# Detect an unexpected exit of a launched harness process and surface the crash report it wrote.
# Baseline runs before a role's Start-Process and records every crash-report candidate as it was
# before launch, so a report written during startup is never mistaken for a pre-existing one.
# Register binds the caller-captured exact PID and process-start time to that role, and Check
# evaluates every registered role from a separate process.
# Process identity is always the registered PID plus its start time: a live PID whose start time
# does not match the registered one is a recycled PID, not the registered process, and a live value
# alone never establishes identity. There is no process-name search anywhere.
# Register also starts a detached watcher that holds a limited-query handle on the registered
# process, because only a handle opened while the process is still alive lets a separate process
# read its exit code; a watcher that cannot bind the registered identity records no exit code and
# Check then reports the code as unknown.
[CmdletBinding()]
param(
	# Baseline | Register | Check, plus Watch (internal, spawned by Register).
	[Parameter(Mandatory)][string] $Action,
	# Absolute path of this run's JSON state file. Callers place it under the worktree's ignored
	# Temp directory; state is never written into tracked repository content.
	[Parameter(Mandatory)][string] $StatePath,
	[string] $Role,
	# The exact game name the role's executable reports, which forms both the report directory and
	# the report file name. Client and server carry different names.
	[string] $GameName,
	# The role's --app-data-directory when one was configured.
	[string] $ConfiguredAppDataRoot,
	[int] $ProcessId,
	# The StartTime of the caller's own Start-Process -PassThru object, round-trip formatted. Only
	# the launching session can read it reliably, so it is supplied rather than rediscovered.
	[string] $StartTimeUtc
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$MaximumMessageLength = 256
$StateSchemaVersion = 'broken-engine-harness-process-check-state/v1'
$ExitSchemaVersion = 'broken-engine-harness-process-exit/v1'
$PollIntervalMilliseconds = 100
$WatcherBindTimeoutSeconds = 5
$ExitCodeWaitSeconds = 2
$NoExceptionText = '(no exception text)'
$CallstackMarker = '<Begin callstack>'
$VersionPrefix = 'Game version:'
# The registered start time is a formatted round-trip of the live one, so only formatting rounding
# may separate them; anything larger is a different process holding a recycled PID.
$IdentityToleranceMilliseconds = 50

$result = [ordered]@{
	schemaVersion = 'broken-engine-harness-process-check/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Process check did not run.'
	action = $Action
	statePath = $StatePath
	role = $Role
}

function Limit-Text([string] $Value, [int] $Maximum) {
	$normalized = ([string]$Value).Trim()
	if ($normalized.Length -le $Maximum) { return $normalized }
	return $normalized.Substring(0, $Maximum - 3) + '...'
}

function Complete-ProcessCheck([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = Limit-Text $Message $MaximumMessageLength
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 32 -Compress))
	exit $ExitCode
}

# ConvertFrom-Json turns an ISO-8601 timestamp back into a [datetime], so every timestamp read from
# the state file is renormalized to the same round-trip UTC text it was written as. Without this the
# stored and freshly observed forms of the same instant compare as different text.
function ConvertTo-TimestampText($Value) {
	if ($null -eq $Value) { return $null }
	if ($Value -is [datetime]) { return ([datetime]$Value).ToUniversalTime().ToString('o', [cultureinfo]::InvariantCulture) }
	return [string]$Value
}

function ConvertTo-UtcDateTime([string] $Value) {
	if ([string]::IsNullOrWhiteSpace($Value)) { return $null }
	$parsed = [datetime]::MinValue
	$styles = [Globalization.DateTimeStyles]::AdjustToUniversal -bor [Globalization.DateTimeStyles]::AssumeUniversal
	if (-not [datetime]::TryParse($Value, [cultureinfo]::InvariantCulture, $styles, [ref]$parsed)) { return $null }
	return $parsed
}

# The report path the engine builds: the chosen root, a child directory named for the game, and the
# game name with its spaces replaced by dashes. Each root the crash path can choose is a candidate.
function Get-CandidatePaths([string] $GameName, [string] $ConfiguredAppDataRoot) {
	$roots = @()
	if (-not [string]::IsNullOrWhiteSpace($ConfiguredAppDataRoot)) { $roots += $ConfiguredAppDataRoot }
	foreach ($folder in @('ApplicationData', 'Desktop')) {
		# An OS folder lookup that resolves nothing yields an empty string rather than a path.
		$resolved = [Environment]::GetFolderPath($folder)
		if (-not [string]::IsNullOrWhiteSpace($resolved)) { $roots += $resolved }
	}

	$fileName = ($GameName -replace ' ', '-') + '-Crash-Report.txt'
	$paths = @()
	foreach ($root in $roots) {
		$candidate = Join-Path (Join-Path $root $GameName) $fileName
		# A configured root that is already the roaming root would otherwise report the same file twice.
		if (@($paths | Where-Object { $_ -ieq $candidate }).Count -eq 0) { $paths += $candidate }
	}
	return $paths
}

function Get-CandidateSnapshot([string] $Path) {
	$exists = $false
	$byteSize = $null
	$lastWriteTimeUtc = $null
	try {
		$item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
		# A directory at this path has no Length, so it stays recorded as absent in both snapshots.
		$byteSize = [int64]$item.Length
		$lastWriteTimeUtc = $item.LastWriteTimeUtc.ToString('o', [cultureinfo]::InvariantCulture)
		$exists = $true
	}
	catch {
		# A missing or unreadable candidate is the normal pre-launch state, not a failure.
	}
	return [ordered]@{ path = $Path; exists = $exists; byteSize = $byteSize; lastWriteTimeUtc = $lastWriteTimeUtc }
}

function Test-CandidateChanged($Baseline, $Current) {
	if ([bool]$Baseline.exists -ne [bool]$Current.exists) { return $true }
	if (-not [bool]$Current.exists) { return $false }
	if ([string]$Baseline.byteSize -cne [string]$Current.byteSize) { return $true }
	return ([string]$Baseline.lastWriteTimeUtc -cne [string]$Current.lastWriteTimeUtc)
}

# The exception text is the first non-empty line after the game version line and before the
# callstack marker. The report is untrusted input written by a crashing process, so a missing,
# truncated, or unreadable file yields the placeholder instead of throwing.
function Get-ReportHeadline([string] $Path) {
	try {
		$stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
		try {
			$reader = [IO.StreamReader]::new($stream)
			$afterVersion = $false
			while ($null -ne ($line = $reader.ReadLine())) {
				$trimmed = $line.Trim()
				if (-not $afterVersion) {
					if ($trimmed.StartsWith($VersionPrefix, [StringComparison]::Ordinal)) { $afterVersion = $true }
					continue
				}
				if ($trimmed -ceq $CallstackMarker) { break }
				if ($trimmed.Length -gt 0) { return Limit-Text $trimmed $MaximumMessageLength }
			}
		}
		finally { $stream.Dispose() }
	}
	catch {
		return $NoExceptionText
	}
	return $NoExceptionText
}

function Test-RegisteredProcessAlive([int] $ProcessId, [string] $StartTimeUtc) {
	$process = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
	if ($null -eq $process) { return $false }
	$registeredStart = ConvertTo-UtcDateTime $StartTimeUtc
	if ($null -eq $registeredStart) { return $false }
	try {
		$liveStart = $process.StartTime.ToUniversalTime()
	}
	catch {
		# A PID whose start time cannot be read is not the registered child this run launched.
		return $false
	}
	return ([Math]::Abs(($liveStart - $registeredStart).TotalMilliseconds) -le $IdentityToleranceMilliseconds)
}

function ConvertTo-RoleRecord($Value) {
	return [ordered]@{
		role = [string]$Value.role
		gameName = [string]$Value.gameName
		baselineUtc = ConvertTo-TimestampText $Value.baselineUtc
		candidates = @(foreach ($candidate in @($Value.candidates) ) {
			[ordered]@{
				path = [string]$candidate.path
				exists = [bool]$candidate.exists
				byteSize = $candidate.byteSize
				lastWriteTimeUtc = ConvertTo-TimestampText $candidate.lastWriteTimeUtc
			}
		})
		processId = $Value.processId
		startTimeUtc = ConvertTo-TimestampText $Value.startTimeUtc
		registeredUtc = ConvertTo-TimestampText $Value.registeredUtc
	}
}

function Read-CheckState([string] $Path) {
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
	$text = [IO.File]::ReadAllText($Path)
	$parsed = $text | ConvertFrom-Json -Depth 32
	# The leading comma keeps an empty role list an empty array instead of collapsing it to $null,
	# which callers here read as "no state file at all".
	return ,@(foreach ($role in @($parsed.roles)) { ConvertTo-RoleRecord $role })
}

function Write-CheckState([string] $Path, $Roles) {
	$state = [ordered]@{
		schemaVersion = $StateSchemaVersion
		updatedUtc = [datetime]::UtcNow.ToString('o', [cultureinfo]::InvariantCulture)
		roles = @($Roles)
	}
	$directory = Split-Path -Parent $Path
	if ($directory) { [IO.Directory]::CreateDirectory($directory) | Out-Null }
	[IO.File]::WriteAllText($Path, ($state | ConvertTo-Json -Depth 32), [Text.UTF8Encoding]::new($false))
}

# One sidecar per registered PID, so a watcher left over from an earlier launch of the same role can
# never be read as the current one.
function Get-ExitSidecarPath([string] $EvidencePath, [string] $Role, [int] $ProcessId) {
	return Join-Path $EvidencePath "$Role-$ProcessId-exit.json"
}

function Read-ExitSidecar([string] $Path) {
	try {
		if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
		$parsed = [IO.File]::ReadAllText($Path) | ConvertFrom-Json -Depth 32
		foreach ($name in @('schemaVersion', 'role', 'processId', 'startTimeUtc', 'bound', 'exitCode')) {
			if ($null -eq $parsed.PSObject.Properties[$name]) { return $null }
		}
		if ([string]$parsed.schemaVersion -cne $ExitSchemaVersion) { return $null }
		return $parsed
	}
	catch {
		# An unreadable sidecar is treated as absent, and the caller retries within its own budget.
		return $null
	}
}

# Only the sidecar of the exact registered identity may speak for a registration. The sidecar
# directory outlives a run, so after PID reuse a sidecar left by an earlier run can carry the
# registered PID while belonging to a different process; the start time separates them.
function Test-SidecarIdentity($Sidecar, [int] $ProcessId, [string] $StartTimeUtc) {
	if ($null -eq $Sidecar) { return $false }
	if ([int]$Sidecar.processId -ne $ProcessId) { return $false }
	return ((ConvertTo-TimestampText $Sidecar.startTimeUtc) -ceq $StartTimeUtc)
}

function Write-ExitSidecar([string] $Path, $Record) {
	$directory = Split-Path -Parent $Path
	if ($directory) { [IO.Directory]::CreateDirectory($directory) | Out-Null }
	$temporaryPath = "$Path.tmp"
	[IO.File]::WriteAllText($temporaryPath, ($Record | ConvertTo-Json -Depth 32), [Text.UTF8Encoding]::new($false))
	# Publishing by move means a reader never observes a partially written sidecar.
	[IO.File]::Move($temporaryPath, $Path, $true)
}

# The watcher publishes the exit code just after the process disappears, so a gone process gets a
# bounded wait before its code is reported as unknown. A watcher that never bound the registered
# identity ends the wait immediately: no code will ever arrive.
function Get-RegisteredExitCode([string] $EvidencePath, $Record) {
	$sidecarPath = Get-ExitSidecarPath $EvidencePath ([string]$Record.role) ([int]$Record.processId)
	$deadline = [datetime]::UtcNow.AddSeconds($ExitCodeWaitSeconds)
	while ($true) {
		$sidecar = Read-ExitSidecar $sidecarPath
		if (Test-SidecarIdentity $sidecar ([int]$Record.processId) ([string]$Record.startTimeUtc)) {
			if (-not [bool]$sidecar.bound) { return $null }
			if ($null -ne $sidecar.exitCode) { return [int]$sidecar.exitCode }
		}
		if ([datetime]::UtcNow -ge $deadline) { return $null }
		Start-Sleep -Milliseconds $PollIntervalMilliseconds
	}
}

try {
	$statePath = [IO.Path]::GetFullPath($StatePath)
	$result.statePath = $statePath
	$evidencePath = Split-Path -Parent $statePath

	switch -CaseSensitive ($Action) {
		'Baseline' {
			$candidates = @(foreach ($path in (Get-CandidatePaths $GameName $ConfiguredAppDataRoot)) { Get-CandidateSnapshot $path })
			$record = [ordered]@{
				role = $Role
				gameName = $GameName
				baselineUtc = [datetime]::UtcNow.ToString('o', [cultureinfo]::InvariantCulture)
				candidates = $candidates
				processId = $null
				startTimeUtc = $null
				registeredUtc = $null
			}
			# A relaunched role starts from a fresh pre-launch baseline with no process bound to it.
			$roles = @()
			$existing = Read-CheckState $statePath
			if ($null -ne $existing) { $roles = @($existing | Where-Object { $_.role -cne $Role }) }
			$roles += $record
			Write-CheckState $statePath $roles
			Complete-ProcessCheck 0 'pass' 'ok' "Baselined $($candidates.Count) crash-report candidate(s) for role '$Role'."
		}
		'Register' {
			$roles = Read-CheckState $statePath
			if ($null -eq $roles) {
				Complete-ProcessCheck 1 'error' 'register.state-missing' "No process-check state exists at '$statePath'; Baseline must run before Register."
			}
			$record = @($roles | Where-Object { $_.role -ceq $Role })
			if ($record.Count -eq 0) {
				Complete-ProcessCheck 1 'error' 'register.role-missing' "Role '$Role' has no pre-launch baseline in '$statePath'."
			}
			$startTime = ConvertTo-UtcDateTime $StartTimeUtc
			if ($null -eq $startTime) {
				Complete-ProcessCheck 1 'error' 'register.start-time-unreadable' "StartTimeUtc '$StartTimeUtc' is not a readable timestamp, so process identity could not be bound."
			}
			# Rebinding replaces any earlier process for this role while keeping its pre-launch baseline.
			$record[0].processId = $ProcessId
			$record[0].startTimeUtc = $startTime.ToString('o', [cultureinfo]::InvariantCulture)
			$record[0].registeredUtc = [datetime]::UtcNow.ToString('o', [cultureinfo]::InvariantCulture)
			Write-CheckState $statePath $roles
			# Start-Process joins ArgumentList items, so every path-valued argument is quoted.
			Start-Process pwsh -ArgumentList @(
				'-NoProfile', '-File', ('"' + $PSCommandPath + '"'),
				'-Action', 'Watch',
				'-StatePath', ('"' + $statePath + '"'),
				'-Role', ('"' + $Role + '"'),
				'-ProcessId', $ProcessId,
				'-StartTimeUtc', ('"' + $record[0].startTimeUtc + '"')
			) -WindowStyle Hidden
			# The watcher publishes its sidecar as soon as it has bound or failed to bind, so a short
			# wait here tells the caller whether an exit code can be expected at all.
			$sidecarPath = Get-ExitSidecarPath $evidencePath $Role $ProcessId
			$bindDeadline = [datetime]::UtcNow.AddSeconds($WatcherBindTimeoutSeconds)
			$sidecar = $null
			while ($true) {
				$sidecar = Read-ExitSidecar $sidecarPath
				# A sidecar carrying any other identity is an earlier run's leftover, so keep polling for
				# this watcher's own publication instead of answering from it.
				if (Test-SidecarIdentity $sidecar $ProcessId $record[0].startTimeUtc) { break }
				if ([datetime]::UtcNow -ge $bindDeadline) { $sidecar = $null; break }
				Start-Sleep -Milliseconds $PollIntervalMilliseconds
			}
			$watcherBound = ($null -ne $sidecar -and [bool]$sidecar.bound)
			$result.watcherBound = $watcherBound
			$watcherText = 'exit-code watcher not bound'
			if ($watcherBound) { $watcherText = 'exit-code watcher bound' }
			Complete-ProcessCheck 0 'pass' 'ok' ("Registered role '$Role' as PID $ProcessId started " +
				"$($record[0].startTimeUtc) ($watcherText).")
		}
		'Check' {
			$roles = Read-CheckState $statePath
			if ($null -eq $roles) {
				Complete-ProcessCheck 1 'error' 'check.state-missing' "No process-check state exists at '$statePath'; Baseline and Register must run before Check."
			}
			$registered = @($roles | Where-Object { $null -ne $_.processId })
			if ($registered.Count -eq 0) {
				# A state file with no registered process proves nothing, so it is a setup failure rather than a pass.
				Complete-ProcessCheck 1 'error' 'check.no-registered-roles' "No role in '$statePath' has a registered process; Register must run before Check."
			}
			$findings = @()
			$exitedRoles = @()
			foreach ($record in $registered) {
				if (Test-RegisteredProcessAlive ([int]$record.processId) $record.startTimeUtc) { continue }
				$exitCode = Get-RegisteredExitCode $evidencePath $record
				$exitCodeText = 'unknown'
				if ($null -ne $exitCode) { $exitCodeText = [string]$exitCode }
				$exitedRoles += "$($record.role) (exit code $exitCodeText)"
				# The registered process is gone. Only a candidate that differs from the pre-launch
				# baseline is this run's report; an unchanged one predates the launch and is stale.
				$changed = @(foreach ($candidate in @($record.candidates)) {
					$current = Get-CandidateSnapshot ([string]$candidate.path)
					if (Test-CandidateChanged $candidate $current) { $current }
				})
				if ($changed.Count -eq 0) {
					# No report changed, so the exit is reported with no report evidence rather than invented evidence.
					$findings += [ordered]@{ role = $record.role; reportPath = $null; headline = $null; exitCode = $exitCode; evidencePath = $evidencePath }
					continue
				}
				foreach ($report in $changed) {
					$findings += [ordered]@{
						role = $record.role
						reportPath = $report.path
						headline = Get-ReportHeadline ([string]$report.path)
						exitCode = $exitCode
						evidencePath = $evidencePath
					}
				}
			}
			$result.findings = @($findings)
			if ($findings.Count -gt 0) {
				$reported = @()
				foreach ($finding in $findings) {
					if ($null -eq $finding.reportPath) { continue }
					$reported += "$($finding.role) -> $($finding.reportPath): $($finding.headline)"
				}
				$reportDetail = ' No crash report changed since launch.'
				if ($reported.Count -gt 0) { $reportDetail = " Crash report(s): $($reported -join '; ')." }
				Complete-ProcessCheck 2 'blocked' 'check.unexpected-exit' ("Registered role(s) $($exitedRoles -join ', ') " +
					"exited unexpectedly.$reportDetail Evidence: '$evidencePath'.")
			}
			Complete-ProcessCheck 0 'pass' 'ok' "All $($registered.Count) registered role(s) are alive with their registered process identity."
		}
		'Watch' {
			# Spawned detached by Register, and the only reader of its result is Check's sidecar read,
			# so this branch writes nothing to stdout and reports no failure.
			$sidecarPath = Get-ExitSidecarPath $evidencePath $Role $ProcessId
			$exitRecord = [ordered]@{
				schemaVersion = $ExitSchemaVersion
				role = $Role
				processId = $ProcessId
				startTimeUtc = $StartTimeUtc
				bound = $false
				exitCode = $null
				exitedUtc = $null
			}
			try {
				Add-Type -Namespace BrokenEngineHarness -Name NativeProcess -MemberDefinition @'
[DllImport("kernel32.dll", SetLastError = true)]
public static extern IntPtr OpenProcess(uint desiredAccess, bool inheritHandle, uint processId);
[DllImport("kernel32.dll", SetLastError = true)]
public static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);
[DllImport("kernel32.dll", SetLastError = true)]
[return: MarshalAs(UnmanagedType.Bool)]
public static extern bool GetExitCodeProcess(IntPtr handle, out int exitCode);
[DllImport("kernel32.dll", SetLastError = true)]
[return: MarshalAs(UnmanagedType.Bool)]
public static extern bool CloseHandle(IntPtr handle);
'@
				# PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE. Opening the handle before the
				# identity is verified keeps the PID from being recycled between that check and the
				# wait, and the open handle keeps the exit code readable after the process ends.
				$handle = [BrokenEngineHarness.NativeProcess]::OpenProcess(0x1000 -bor 0x00100000, $false, [uint32]$ProcessId)
				if ($handle -ne [IntPtr]::Zero) {
					if (Test-RegisteredProcessAlive $ProcessId $StartTimeUtc) {
						$exitRecord.bound = $true
						Write-ExitSidecar $sidecarPath $exitRecord
						[void][BrokenEngineHarness.NativeProcess]::WaitForSingleObject($handle, [uint32]::MaxValue)
						$observedExitCode = 0
						if ([BrokenEngineHarness.NativeProcess]::GetExitCodeProcess($handle, [ref]$observedExitCode)) {
							$exitRecord.exitCode = $observedExitCode
							$exitRecord.exitedUtc = [datetime]::UtcNow.ToString('o', [cultureinfo]::InvariantCulture)
						}
					}
					[void][BrokenEngineHarness.NativeProcess]::CloseHandle($handle)
				}
				Write-ExitSidecar $sidecarPath $exitRecord
			}
			catch {
				# Nothing reads a watcher failure; Check reports the exit code as unknown instead.
			}
			exit 0
		}
		default {
			Complete-ProcessCheck 1 'error' 'action.unknown' "Action '$Action' is not one of Baseline, Register, or Check."
		}
	}
}
catch {
	Complete-ProcessCheck 1 'error' 'internal.error' $_.Exception.Message
}
