# Launch one harness role: baseline its crash-report candidates, refuse Shared data that differs from
# this worktree's build record, start the game with the harness argument set, register and check the
# process, and wait for its port to answer ping.
# The baseline runs before the data comparison, so a refused launch or relaunch leaves the role
# baselined and unregistered, never holding a stale registration for release's entry check. The data
# is compared once, before the start; a change between that comparison and the game loading its data
# is assumed never to happen, so nothing after the start compares again.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][ValidateSet('server', 'client', 'client-b')][string] $Role,
	# The claim result's gameDataDirectory, or the derived Temp copy of it that agent-harness worker.md
	# step 5 permits.
	[Parameter(Mandatory)][string] $GameDataDirectory,
	[Parameter(Mandatory)][string] $AgentHarness,
	[Parameter(Mandatory)][string] $Owner,
	[Parameter(Mandatory)][string] $ProcessCheckStatePath,
	[Parameter(Mandatory)][string] $AppDataDirectory,
	[Parameter(Mandatory)][string] $LogFile,
	# The role's crash-report game name.
	[Parameter(Mandatory)][string] $GameName,
	[string] $Configuration = 'Debug',
	# Client roles only: omit --windowed 1600x900.
	[switch] $NativeResolution,
	# Client roles only: add --renderdoc.
	[switch] $RenderDoc,
	# Omit --loopback-only for a cross-machine launch.
	[switch] $CrossMachine
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# ValidateSet binds any casing, while the role comparisons below and the process check's role
# matching are case-sensitive.
$Role = $Role.ToLowerInvariant()

$MaximumMessageLength = 256
$PingTimeoutSeconds = 120
$RolePorts = @{ 'server' = 27100; 'client' = 27101; 'client-b' = 27102 }

$result = [ordered]@{
	schemaVersion = 'broken-engine-harness-launch/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Harness launch did not run.'
	role = $Role
	port = $RolePorts[$Role]
	project = $null
	gameDataDirectory = $GameDataDirectory
	pid = $null
	startTimeUtc = $null
	tick = $null
	processCheck = $null
	ping = $null
}

function Limit-Text([string] $Value, [int] $Maximum) {
	$normalized = ([string]$Value).Trim()
	if ($normalized.Length -le $Maximum) { return $normalized }
	return $normalized.Substring(0, $Maximum - 3) + '...'
}

function Complete-HarnessLaunch([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = Limit-Text $Message $MaximumMessageLength
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 32 -Compress))
	exit $ExitCode
}

# Each helper runs as its own process, so its exit code cannot end this run, and its one JSON object
# is parsed from its captured stdout; an unparsable object is null.
function Invoke-HarnessScript([string] $ScriptName, [string[]] $Arguments) {
	$powerShell = [Environment]::ProcessPath
	if ([string]::IsNullOrWhiteSpace($powerShell)) { $powerShell = 'pwsh' }
	$raw = @(& $powerShell -NoProfile -File (Join-Path $PSScriptRoot $ScriptName) @Arguments 2>&1)
	$exitCode = $LASTEXITCODE
	$stdout = [string]::Join([Environment]::NewLine, @($raw | Where-Object { $_ -isnot [System.Management.Automation.ErrorRecord] } | ForEach-Object { [string]$_ }))
	$payload = $null
	if (-not [string]::IsNullOrWhiteSpace($stdout)) {
		try { $payload = $stdout | ConvertFrom-Json -Depth 32 } catch { $payload = $null }
	}
	return [pscustomobject]@{ ExitCode = $exitCode; Payload = $payload }
}

function Get-PayloadField($Payload, [string] $Name) {
	if ($null -eq $Payload -or -not ($Payload.PSObject.Properties.Name -ccontains $Name)) { return $null }
	return $Payload.$Name
}

function Invoke-ProcessCheckAction([string] $Action, [string] $FailureCode, [string[]] $Arguments) {
	$check = Invoke-HarnessScript 'Invoke-AgentHarnessProcessCheck.ps1' (@('-Action', $Action, '-StatePath', $script:StatePath) + $Arguments)
	$result.processCheck = $check.Payload
	if ($check.ExitCode -eq 2 -and $Action -ceq 'Check' -and $null -ne $check.Payload) {
		Complete-HarnessLaunch 2 'blocked' 'check.unexpected-exit' (Get-PayloadField $check.Payload 'message')
	}
	if ($check.ExitCode -ne 0) {
		Complete-HarnessLaunch 1 'error' $FailureCode ("Process-check $Action for role '$Role' failed with exit $($check.ExitCode): " +
			"$(Get-PayloadField $check.Payload 'code') $(Get-PayloadField $check.Payload 'message')")
	}
}

try {
	if ([string]::IsNullOrWhiteSpace($Owner)) { throw 'Owner must not be empty.' }
	if ([string]::IsNullOrWhiteSpace($Configuration)) { throw 'Configuration must not be empty.' }
	if (-not [IO.Path]::IsPathRooted($GameDataDirectory)) { throw "GameDataDirectory must be absolute: '$GameDataDirectory'." }
	if ($Role -ceq 'server' -and ($NativeResolution -or $RenderDoc)) { throw '-NativeResolution and -RenderDoc apply to client roles only.' }

	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
	if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
		$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
	}
	Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force
	$root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not (Test-Path -LiteralPath $root -PathType Container)) {
		Complete-HarnessLaunch 1 'error' 'launch.repository-missing' "RepositoryRoot is not a directory: '$root'."
	}
	# Canonical paths carry no trailing separator, which would otherwise escape the closing quote of a
	# quoted game argument.
	$dataDirectory = Get-AgentCanonicalPath $GameDataDirectory
	$result.gameDataDirectory = $dataDirectory
	$appDataRoot = Get-AgentCanonicalPath $AppDataDirectory
	$logPath = Get-AgentCanonicalPath $LogFile
	$script:StatePath = Get-AgentCanonicalPath $ProcessCheckStatePath

	$launch = Get-AgentHarnessLaunchConfiguration $root $dataDirectory $Configuration
	$result.project = $launch.Project
	if ($null -ne $launch.Failure) { Complete-HarnessLaunch 1 'error' "launch.$($launch.Failure)" $launch.Message }

	Invoke-ProcessCheckAction 'Baseline' 'launch.baseline-failed' @('-Role', $Role, '-GameName', $GameName, '-ConfiguredAppDataRoot', $appDataRoot)

	$recordTarget = if ($Role -ceq 'server') { 'Server' } else { 'Client' }
	$dataChange = Compare-AgentSharedDataRecord $root $dataDirectory $launch.Project $recordTarget $Configuration
	if ($null -ne $dataChange) { Complete-HarnessLaunch 2 'blocked' $dataChange.Code $dataChange.Message }

	if ($Role -ceq 'server') {
		# The server is single-instance per machine, so a harness server started beside any running one
		# exits at once. A relaunch reaches here only after this run's own server PID is gone.
		$runningServers = @(Get-Process -Name "$($launch.ServerBaseName)*" -ErrorAction SilentlyContinue)
		if ($runningServers.Count -gt 0) {
			Complete-HarnessLaunch 2 'blocked' 'launch.server-running' ("A server is already running, so a harness server would exit at once: " +
				"$(@($runningServers | ForEach-Object { "$($_.ProcessName) PID $($_.Id)" }) -join ', ').")
		}
	}

	$executable = Join-Path $launch.OutputDirectory $(if ($Role -ceq 'server') { $launch.ServerExe } else { $launch.ClientExe })
	# Start-Process joins ArgumentList items, so every path-valued argument is quoted.
	$gameArguments = @(
		'--agent-port', "$($result.port)",
		'--data-directory', ('"' + $dataDirectory + '"'),
		'--app-data-directory', ('"' + $appDataRoot + '"'),
		'--log-file', ('"' + $logPath + '"'))
	if (-not $CrossMachine) { $gameArguments += '--loopback-only' }
	if ($Role -cne 'server' -and -not $NativeResolution) { $gameArguments += @('--windowed', '1600x900') }
	if ($RenderDoc) { $gameArguments += '--renderdoc' }
	try {
		$process = Start-Process -FilePath $executable -ArgumentList $gameArguments -WorkingDirectory $appDataRoot -WindowStyle Hidden -PassThru -ErrorAction Stop
	}
	catch {
		Complete-HarnessLaunch 1 'error' 'launch.start-failed' "Role '$Role' could not start '$executable': $($_.Exception.Message)"
	}
	$result.pid = $process.Id
	$result.startTimeUtc = $process.StartTime.ToUniversalTime().ToString('o')

	Invoke-ProcessCheckAction 'Register' 'launch.register-failed' @('-Role', $Role, '-ProcessId', "$($process.Id)", '-StartTimeUtc', $result.startTimeUtc)
	# Checked before the readiness wait, so a process that died during startup is caught against its
	# true pre-launch baseline.
	Invoke-ProcessCheckAction 'Check' 'launch.check-failed' @()

	$ping = Invoke-HarnessScript 'Wait-HarnessPing.ps1' @(
		'-AgentHarness', $AgentHarness, '-Owner', $Owner, '-Port', "$($result.port)",
		'-TimeoutSeconds', "$PingTimeoutSeconds", '-ProcessCheckStatePath', $script:StatePath)
	$result.ping = $ping.Payload
	$result.tick = Get-PayloadField $ping.Payload 'tick'
	$pingCode = Get-PayloadField $ping.Payload 'code'
	$pingMessage = Get-PayloadField $ping.Payload 'message'
	if ($ping.ExitCode -eq 0) {
		Complete-HarnessLaunch 0 'pass' 'ok' "Role '$Role' launched as PID $($process.Id) and port $($result.port) answered ping with tick $($result.tick)."
	}
	if ($ping.ExitCode -eq 2 -and $null -ne $pingCode) { Complete-HarnessLaunch 2 'blocked' $pingCode $pingMessage }
	Complete-HarnessLaunch 1 'error' 'launch.ping-failed' "The readiness wait on port $($result.port) failed with exit $($ping.ExitCode): $pingCode $pingMessage"
}
catch {
	Complete-HarnessLaunch 1 'error' 'internal.error' $_.Exception.Message
}
