# Require the project executables, provision the checkout, resolve AgentHarness, mint an owner
# token, and claim the harness lock.
# A foreign owner is waited out, not stolen from: provisioning runs once and only the claim attempt
# repeats until the wait budget expires, and nothing touches the holder's processes, heartbeat, or
# claim. An expired wait reports the holder record the last claim attempt itself printed.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Session,
	# The packed data the launch will read. Pass the latest /compile result's normalized
	# GameDataDirectory verbatim, so the pairing check judges the same data the game will be given.
	[Parameter(Mandatory)][string] $GameDataDirectory,
	[string] $Key = 'default',
	[string] $Configuration = 'Debug',
	# Agents use the default; a short budget exists only so a test or scenario can reach the
	# expired-wait outcome without waiting out the standard budget.
	[ValidateRange(1, 500)][int] $WaitSeconds = 500
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$MaximumMessageLength = 256
$MaximumOutputCharacters = 2048
$PollMilliseconds = 5000

$result = [ordered]@{
	schemaVersion = 'broken-engine-harness-claim/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Harness claim did not run.'
	repositoryRoot = $RepositoryRoot
	session = $Session
	key = $Key
	gameDataDirectory = $GameDataDirectory
	owner = $null
	agentHarness = $null
	claim = $null
	currentOwner = $null
	packVersion = $null
	provision = $null
}

function Limit-Text([string] $Value, [int] $Maximum) {
	$normalized = ([string]$Value).Trim()
	if ($normalized.Length -le $Maximum) { return $normalized }
	return $normalized.Substring(0, $Maximum - 3) + '...'
}

function Limit-TrailingText([string] $Value, [int] $Maximum) {
	$normalized = ([string]$Value).Trim()
	if ($normalized.Length -le $Maximum) { return $normalized }
	return '...' + $normalized.Substring($normalized.Length - ($Maximum - 3))
}

function Complete-HarnessClaim([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = Limit-Text $Message $MaximumMessageLength
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 32 -Compress))
	exit $ExitCode
}

function Invoke-NativeCapture([string] $FilePath, [string[]] $Arguments) {
	$raw = @(& $FilePath @Arguments 2>&1)
	$exitCode = $LASTEXITCODE
	$standard = @($raw | Where-Object { $_ -isnot [System.Management.Automation.ErrorRecord] })
	$failed = @($raw | Where-Object { $_ -is [System.Management.Automation.ErrorRecord] })
	return [pscustomobject]@{
		ExitCode = $exitCode
		Stdout = [string]::Join([Environment]::NewLine, @($standard | ForEach-Object { [string]$_ }))
		Stderr = [string]::Join([Environment]::NewLine, @($failed | ForEach-Object { [string]$_ }))
	}
}

function ConvertFrom-LockOutput([string] $Text) {
	if ([string]::IsNullOrWhiteSpace($Text)) { return $null }
	try { return $Text | ConvertFrom-Json -Depth 32 } catch { return $null }
}

function Get-LockField($Record, [string] $Name) {
	if ($null -eq $Record -or -not ($Record.PSObject.Properties.Name -ccontains $Name)) { return $null }
	return $Record.$Name
}

# The expected pack version comes from this worktree's own DataFile.h — the source the executables were
# built from — so this proves source-vs-data pairing, not binary-vs-data. Nothing writes
# sizeof(ChunkHeader) literally, so it is rebuilt from the layout the header explicitly pins: the union
# offset assert plus the largest literal sizeof assert among the union's member types. Every anchor is
# required; a header this cannot parse blocks loudly rather than inventing a version the on-disk data
# would then be judged against.
function Get-ExpectedPackVersion([string] $HeaderPath) {
	$expected = [pscustomobject]@{ Version = $null; Detail = $null }
	if (-not (Test-Path -LiteralPath $HeaderPath -PathType Leaf)) {
		$expected.Detail = "'$HeaderPath' is missing."
		return $expected
	}
	# A read failure leaves the text empty so it reports the same unparsable-header detail as a parse failure.
	$text = [string](Get-Content -LiteralPath $HeaderPath -Raw -ErrorAction SilentlyContinue)
	$lines = @($text -split "\r?\n")

	$versionMatch = [regex]::Match($text, '(?m)^\s*static constexpr int64_t kiVersion\s*=\s*(\d+)\s*\+\s*sizeof\(ChunkHeader\);')
	if (-not $versionMatch.Success) {
		$expected.Detail = 'DataHeader::kiVersion is not the expected "<base> + sizeof(ChunkHeader)" form.'
		return $expected
	}
	$unionMatch = [regex]::Match($text, '(?s)struct ChunkHeader\s*\{.*?union\s*\{(.*?)\}\s*;')
	if (-not $unionMatch.Success) {
		$expected.Detail = 'The ChunkHeader union block could not be read.'
		return $expected
	}
	$memberMatches = @([regex]::Matches($unionMatch.Groups[1].Value, '(?m)^\s*(\w+)\s+(\w+)\s*;\s*$'))
	if ($memberMatches.Count -eq 0) {
		$expected.Detail = 'The ChunkHeader union declares no readable members.'
		return $expected
	}
	$firstMember = $memberMatches[0].Groups[2].Value
	$offsetMatch = [regex]::Match($text, "BT_OFFSETOF\(ChunkHeader,\s*$firstMember\)\s*==\s*(\d+)")
	if (-not $offsetMatch.Success) {
		$expected.Detail = "No static_assert pins the offset of ChunkHeader::$firstMember."
		return $expected
	}
	$unionOffset = [int64]$offsetMatch.Groups[1].Value

	$largestMember = [int64]-1
	foreach ($memberMatch in $memberMatches) {
		$type = $memberMatch.Groups[1].Value
		$assertIndex = -1
		for ($index = 0; $index -lt $lines.Count; $index++) {
			if ($lines[$index] -cmatch "static_assert\(sizeof\($type\)\s*==\s*([^,]+),") { $assertIndex = $index; break }
		}
		if ($assertIndex -lt 0) {
			$expected.Detail = "No static_assert pins sizeof($type)."
			return $expected
		}
		$size = $Matches[1].Trim()
		if ($size -cmatch '^\d+$') {
			if ([int64]$size -gt $largestMember) { $largestMember = [int64]$size }
			continue
		}
		# A member pinned against another type instead of a byte literal is usable only because the header
		# states it is not the largest member; without that statement the union footprint is unknown.
		$annotation = ($lines[[Math]::Max(0, $assertIndex - 2)..$assertIndex]) -join ' '
		if ($annotation -cnotmatch 'non-largest union member') {
			$expected.Detail = "sizeof($type) is not a byte literal and is not documented as a non-largest union member."
			return $expected
		}
	}
	if ($largestMember -lt 0) {
		$expected.Detail = 'No ChunkHeader union member pins its size as a byte literal.'
		return $expected
	}
	# Both anchors being 8-byte multiples is what lets the union footprint be added without modelling
	# trailing padding; anything else is a layout this derivation cannot reproduce.
	if (($unionOffset % 8) -ne 0 -or ($largestMember % 8) -ne 0) {
		$expected.Detail = "ChunkHeader union offset $unionOffset and largest member $largestMember are not both 8-byte multiples."
		return $expected
	}
	$expected.Version = [int64]$versionMatch.Groups[1].Value + $unionOffset + $largestMember
	return $expected
}

# Read-only and share-read: a running game or a running export may hold these manifests open, and the
# pairing check must observe them without disturbing that holder.
function Get-PackManifestVersions([string] $DataDirectory) {
	$manifests = [pscustomobject]@{ Entries = @(); Detail = $null }
	if (-not (Test-Path -LiteralPath $DataDirectory -PathType Container)) {
		$manifests.Detail = "'$DataDirectory' is not a directory."
		return $manifests
	}
	$files = @(Get-ChildItem -LiteralPath $DataDirectory -Filter '*.manifest' -File -ErrorAction SilentlyContinue | Sort-Object Name)
	if ($files.Count -eq 0) {
		$manifests.Detail = "'$DataDirectory' contains no .manifest files."
		return $manifests
	}
	$entries = @()
	foreach ($file in $files) {
		$header = [byte[]]::new(24)
		$read = 0
		try {
			$stream = [IO.File]::Open($file.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
			try {
				while ($read -lt $header.Length) {
					$copied = $stream.Read($header, $read, $header.Length - $read)
					if ($copied -le 0) { break }
					$read += $copied
				}
			}
			finally { $stream.Dispose() }
		}
		catch {
			$manifests.Detail = "'$($file.Name)' could not be read: $($_.Exception.Message)"
			return $manifests
		}
		if ($read -ne $header.Length) {
			$manifests.Detail = "'$($file.Name)' is shorter than its $($header.Length)-byte header."
			return $manifests
		}
		if ([BitConverter]::ToInt64($header, 0) -ne 0xDA7AF11EL) {
			$manifests.Detail = "'$($file.Name)' does not start with the pack manifest magic."
			return $manifests
		}
		$entries += [pscustomobject]@{ Name = $file.Name; Version = [BitConverter]::ToInt64($header, 8) }
	}
	$manifests.Entries = $entries
	return $manifests
}

try {
	if ([string]::IsNullOrWhiteSpace($Session)) { throw 'Session must not be empty.' }
	if ([string]::IsNullOrWhiteSpace($Key)) { throw 'Key must not be empty.' }
	if ([string]::IsNullOrWhiteSpace($Configuration)) { throw 'Configuration must not be empty.' }
	if (-not [IO.Path]::IsPathRooted($RepositoryRoot)) { throw "RepositoryRoot must be absolute: '$RepositoryRoot'." }
	if (-not [IO.Path]::IsPathRooted($GameDataDirectory)) { throw "GameDataDirectory must be absolute: '$GameDataDirectory'." }

	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
	if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
		$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
	}
	Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force
	$root = Get-AgentCanonicalPath $RepositoryRoot
	$result.repositoryRoot = $root
	if (-not (Test-Path -LiteralPath $root -PathType Container)) {
		Complete-HarnessClaim 1 'error' 'claim.repository-missing' "RepositoryRoot is not a directory: '$root'."
	}

	$provisioner = [IO.Path]::GetFullPath((Join-Path $sharedScripts 'Provision-WorktreeThirdParty.ps1'))
	if (-not (Test-Path -LiteralPath $provisioner -PathType Leaf)) {
		Complete-HarnessClaim 1 'error' 'claim.provisioner-missing' "Provisioning script is missing: '$provisioner'."
	}

	# The project harness doc owns the executable names and their Output directory, so the launch
	# block stays the single source of truth: only those three assignments are read, and the doc's
	# hardcoded Debug suffix becomes the requested configuration. Missing executables block here,
	# before provisioning and before the lock, so a build routes through /compile and re-enters.
	$harnessDocument = Join-Path $root 'Projects\BrokenEngineSandbox\Documents\AgentHarness.md'
	$launchText = ''
	if (Test-Path -LiteralPath $harnessDocument -PathType Leaf) {
		# A read failure leaves the text empty so it reports the same unreadable-doc code as a parse failure.
		$launchText = [string](Get-Content -LiteralPath $harnessDocument -Raw -ErrorAction SilentlyContinue)
	}
	$outputMatch = [regex]::Match($launchText, '(?m)^\s*\$Output\s*=\s*Join-Path \$ROOT ''([^'']+)''\s*$')
	$serverMatch = [regex]::Match($launchText, '(?m)^\s*\$ServerExe\s*=\s*Join-Path \$Output ''([^'']+)''\s*$')
	$clientMatch = [regex]::Match($launchText, '(?m)^\s*\$ClientExe\s*=\s*Join-Path \$Output ''([^'']+)''\s*$')
	if (-not $outputMatch.Success -or -not $serverMatch.Success -or -not $clientMatch.Success) {
		Complete-HarnessClaim 1 'error' 'claim.launch-doc-unreadable' `
			"Output, ServerExe, and ClientExe could not be read from '$harnessDocument'."
	}
	$outputRelative = $outputMatch.Groups[1].Value
	$outputDirectory = Join-Path $root $outputRelative
	$missingExecutables = @()
	foreach ($executable in @($serverMatch.Groups[1].Value, $clientMatch.Groups[1].Value)) {
		$configured = [regex]::Replace($executable, '\.Debug\.exe$', ".$Configuration.exe")
		if (-not (Test-Path -LiteralPath (Join-Path $outputDirectory $configured) -PathType Leaf)) {
			$missingExecutables += $configured
		}
	}
	if ($missingExecutables.Count -gt 0) {
		Complete-HarnessClaim 2 'blocked' 'claim.executable-missing' `
			"Missing executables in '$outputRelative': $($missingExecutables -join ', ')."
	}

	# Executables expecting one pack version and data carrying another fail deep inside the launch, at
	# the runtime manifest trust boundary, as a hidden modal dialog and then a ping timeout. Proving the
	# pairing here — before provisioning and before the lock — blocks with no owner token or claim left
	# behind, and the caller supplies the same data directory the launch will pass to the game.
	$dataDirectory = Get-AgentCanonicalPath $GameDataDirectory
	$result.gameDataDirectory = $dataDirectory
	$expectedVersion = Get-ExpectedPackVersion (Join-Path $root 'Common\DataFile.h')
	if ($null -eq $expectedVersion.Version) {
		Complete-HarnessClaim 1 'error' 'claim.pack-version-underivable' `
			"The expected pack version could not be derived from Common\DataFile.h: $($expectedVersion.Detail)"
	}
	$manifests = Get-PackManifestVersions $dataDirectory
	if ($null -ne $manifests.Detail) {
		Complete-HarnessClaim 1 'error' 'claim.game-data-unreadable' "Pack manifests could not be read: $($manifests.Detail)"
	}
	$mismatched = @($manifests.Entries | Where-Object { $_.Version -ne $expectedVersion.Version })
	if ($mismatched.Count -gt 0) {
		$result.packVersion = [ordered]@{
			expected = $expectedVersion.Version
			found = @($manifests.Entries.Version | Sort-Object -Unique)
			mismatchedManifests = @($mismatched | ForEach-Object { $_.Name })
			manifestCount = $manifests.Entries.Count
		}
		$mismatchedNames = @($mismatched | Select-Object -First 3 | ForEach-Object { $_.Name })
		Complete-HarnessClaim 2 'blocked' 'claim.pack-version-mismatch' ("Pack version mismatch: Common\DataFile.h expects " +
			"$($expectedVersion.Version) but $($mismatched.Count) of $($manifests.Entries.Count) manifest(s) in " +
			"'$dataDirectory' carry $((@($mismatched.Version | Sort-Object -Unique)) -join ', '), including $($mismatchedNames -join ', ').")
	}

	# The provisioner throws instead of exiting, so it runs as a child process whose exit code
	# reports the failure and whose progress output stays out of this script's stdout.
	$powerShell = [Environment]::ProcessPath
	if ([string]::IsNullOrWhiteSpace($powerShell)) { $powerShell = 'pwsh' }
	$provision = Invoke-NativeCapture $powerShell @(
		'-NoProfile', '-File', $provisioner, '-RepositoryRoot', $root)
	$provisionOutput = Limit-TrailingText (($provision.Stdout, $provision.Stderr) -join [Environment]::NewLine) $MaximumOutputCharacters
	$result.provision = [ordered]@{ exitCode = $provision.ExitCode; output = $provisionOutput }
	if ($provision.ExitCode -ne 0) {
		Complete-HarnessClaim 1 'error' 'claim.provision-failed' "Worktree provisioning failed with exit $($provision.ExitCode): $provisionOutput"
	}

	$agentHarness = Join-Path $root 'Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe'
	if (-not (Test-Path -LiteralPath $agentHarness -PathType Leaf)) {
		Complete-HarnessClaim 1 'error' 'claim.harness-missing' "AgentHarness executable is missing: '$agentHarness'."
	}
	$result.agentHarness = $agentHarness

	$token = Invoke-NativeCapture $agentHarness @('lock', 'token')
	$owner = $token.Stdout.Trim()
	if ($token.ExitCode -ne 0 -or $owner -cnotmatch '^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$') {
		Complete-HarnessClaim 1 'error' 'claim.token-failed' "AgentHarness could not mint an owner token (exit $($token.ExitCode)): $($token.Stderr)"
	}
	$result.owner = $owner

	# Only the claim attempt repeats: provisioning, the harness path, and the owner token above stay
	# valid for the whole wait, and the same owner token reclaims after each foreign-owner refusal.
	$stopwatch = [Diagnostics.Stopwatch]::StartNew()
	$deadlineMilliseconds = $WaitSeconds * 1000
	$attempts = 0
	while ($true) {
		$attempts++
		$claim = Invoke-NativeCapture $agentHarness @(
			'lock', 'claim', '--key', $Key, '--owner', $owner, '--session', $Session, '--worktree', $root)
		$record = ConvertFrom-LockOutput $claim.Stdout
		if ($claim.ExitCode -eq 0) {
			if ($null -eq $record) {
				Complete-HarnessClaim 1 'error' 'claim.metadata-unreadable' 'AgentHarness claimed the lock but did not print a readable lock record.'
			}
			$result.claim = $record
			Complete-HarnessClaim 0 'pass' 'ok' "Harness lock '$Key' claimed by owner $owner after $attempts attempt(s)."
		}
		if ($claim.ExitCode -ne 2) {
			Complete-HarnessClaim 1 'error' 'claim.failed' "AgentHarness lock claim failed with exit $($claim.ExitCode): $($claim.Stderr)"
		}
		# Sleeping a full interval past the deadline would wait longer than the promised budget.
		$remainingMilliseconds = $deadlineMilliseconds - [int]$stopwatch.ElapsedMilliseconds
		if ($remainingMilliseconds -le 0) { break }
		Start-Sleep -Milliseconds ([Math]::Min($PollMilliseconds, $remainingMilliseconds))
	}

	# The wait expired with the lock still held, so the last attempt's record describes the holder.
	# lock claim prints the held record itself, so the holder needs no second lock status call.
	$claimedAt = Get-LockField $record 'claimedAt'
	# ConvertFrom-Json turns an ISO-8601 timestamp into [datetime], so both forms are normalized here.
	$claimedUtc = $null
	if ($claimedAt -is [datetime]) {
		$claimedUtc = ([datetime]$claimedAt).ToUniversalTime()
	}
	elseif ($claimedAt -is [string]) {
		try {
			$claimedUtc = [datetime]::Parse($claimedAt, [cultureinfo]::InvariantCulture,
				[Globalization.DateTimeStyles]::AdjustToUniversal -bor [Globalization.DateTimeStyles]::AssumeUniversal)
		}
		catch { $claimedUtc = $null }
	}
	$holdSeconds = $null
	$claimedAtText = [string]$claimedAt
	if ($null -ne $claimedUtc) {
		$holdSeconds = [Math]::Round(([datetime]::UtcNow - $claimedUtc).TotalSeconds, 3)
		$claimedAtText = $claimedUtc.ToString('yyyy-MM-ddTHH:mm:ss.fffZ', [cultureinfo]::InvariantCulture)
	}
	$result.currentOwner = [ordered]@{
		owner = Get-LockField $record 'owner'
		session = Get-LockField $record 'session'
		worktree = Get-LockField $record 'worktree'
		claimedAt = $claimedAtText
		heartbeatAt = Get-LockField $record 'heartbeatAt'
		holdSeconds = $holdSeconds
		record = $record
	}
	Complete-HarnessClaim 2 'blocked' 'claim.foreign-owner' ("Harness lock '$Key' is still held by owner $(Get-LockField $record 'owner') " +
		"since $claimedAtText ($holdSeconds seconds) after waiting $WaitSeconds seconds over $attempts attempt(s).")
}
catch {
	Complete-HarnessClaim 1 'error' 'internal.error' $_.Exception.Message
}
