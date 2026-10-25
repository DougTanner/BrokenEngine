# Capture ModelViewer PNGs without showing or activating the viewer: start the Debug viewer hidden in its
# agent mode on a fresh app-data directory, post F9 to its own window, read each completion line from its
# log, and close it by exact PID on every exit path after the start.
# The viewer binds no port, so the capture takes no harness lock and runs no process check.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	# Absolute and must not exist; the script creates it, so the viewer starts from compiled defaults.
	[Parameter(Mandatory)][string] $AppDataDirectory,
	[ValidateNotNullOrEmpty()][string] $ModelPath,
	# Capture until two consecutive captures share their SHA-256; otherwise return after the first.
	[switch] $RequireStable
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$MaximumMessageLength = 256
# Launch through the last capture, sized for a Debug cold start on a fresh app-data directory.
$CaptureTimeoutSeconds = 180
$CloseTimeoutSeconds = 60
$PollMilliseconds = 250
# The viewer never binds it; the value only selects its agent mode and stays clear of the harness ports.
$AgentPort = 27103
$WindowClass = 'Broken Engine Model Viewer'
$WmClose = 0x0010
$WmKeyDown = 0x0100
$VkF9 = 0x78
$CodeExits = @{
	'ok' = 0
	'capture.failed' = 1
	'internal.error' = 1
	'capture.executable-missing' = 2
	'capture.timeout' = 2
	'capture.unstable' = 2
	'process.unexpected-exit' = 2
	'close.forced' = 2
}

$result = [ordered]@{
	schemaVersion = 'broken-engine-model-viewer-capture/v1'
	status = 'error'
	code = 'internal.error'
	message = 'ModelViewer capture did not run.'
	pid = $null
	exitCode = $null
	captures = [Collections.Generic.List[object]]::new()
	stable = $(if ($RequireStable) { $false } else { $null })
	windowVisible = $false
	logFile = $null
	crashReports = @()
}

function Limit-Text([string] $Value, [int] $Maximum) {
	$normalized = ([string]$Value).Trim()
	if ($normalized.Length -le $Maximum) { return $normalized }
	return $normalized.Substring(0, $Maximum - 3) + '...'
}

function Complete-ModelViewerCapture([string] $Code, [string] $Message) {
	$exitCode = $CodeExits[$Code]
	$result.status = @('pass', 'error', 'blocked')[$exitCode]
	$result.code = $Code
	$result.message = Limit-Text $Message $MaximumMessageLength
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 8 -Compress))
	exit $exitCode
}

function New-Outcome([string] $Code, [string] $Message) {
	return [pscustomobject]@{ Code = $Code; Message = $Message }
}

function Find-ViewerWindow([int] $ProcessId) {
	$candidate = [IntPtr]::Zero
	while ($true) {
		# NullString: PowerShell would pass $null to a string parameter as '', matching only untitled windows.
		$candidate = [BrokenEngineHarness.ModelViewerWindow]::FindWindowEx([IntPtr]::Zero, $candidate, $WindowClass, [NullString]::Value)
		if ($candidate -eq [IntPtr]::Zero) { return [IntPtr]::Zero }
		$ownerId = [uint32]0
		[void][BrokenEngineHarness.ModelViewerWindow]::GetWindowThreadProcessId($candidate, [ref]$ownerId)
		if ($ownerId -eq $ProcessId) { return $candidate }
	}
}

# The viewer holds its log open for writing for its whole life, so the read shares write access, and only
# newline-terminated text is returned so a line still being written is never parsed.
function Read-ViewerLog([string] $Path) {
	if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return '' }
	$stream = [IO.FileStream]::new($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
	try {
		$text = [IO.StreamReader]::new($stream, [Text.Encoding]::UTF8).ReadToEnd()
	}
	finally {
		$stream.Dispose()
	}
	return $text.Substring(0, $text.LastIndexOf("`n") + 1)
}

function Get-DeadlineOutcome {
	if ($RequireStable -and $result.captures.Count -ge 2) {
		return New-Outcome 'capture.unstable' "No two consecutive captures of $($result.captures.Count) matched within $CaptureTimeoutSeconds s."
	}
	return New-Outcome 'capture.timeout' "The capture did not finish within $CaptureTimeoutSeconds s ($($result.captures.Count) captures completed)."
}

function Get-ExitOutcome($Process) {
	return New-Outcome 'process.unexpected-exit' "The viewer exited with code $($Process.ExitCode) before the capture finished."
}

function Invoke-ViewerCapture($Process, [string] $LogPath, [Diagnostics.Stopwatch] $Clock) {
	$deadline = [TimeSpan]::FromSeconds($CaptureTimeoutSeconds)
	while ($script:Window -eq [IntPtr]::Zero) {
		$script:Window = Find-ViewerWindow $Process.Id
		if ($script:Window -ne [IntPtr]::Zero) { break }
		if ($Process.HasExited) { return Get-ExitOutcome $Process }
		if ($Clock.Elapsed -ge $deadline) { return Get-DeadlineOutcome }
		Start-Sleep -Milliseconds $PollMilliseconds
	}

	$previousHash = $null
	for ($sequence = 1; ; ++$sequence) {
		# lParam 1: repeat count 1 with the previous-key-state bit clear, so the viewer does not treat it as a repeat.
		if (-not [BrokenEngineHarness.ModelViewerWindow]::PostMessage($script:Window, $WmKeyDown, [IntPtr]$VkF9, [IntPtr]1)) {
			throw "PostMessage F9 to the viewer window failed with Win32 error $([Runtime.InteropServices.Marshal]::GetLastWin32Error())."
		}
		$pattern = "ModelViewer screenshot complete sequence=$sequence token=-?\d+ result=(\{.*\})\r?$"
		while ($true) {
			$completion = [regex]::Match((Read-ViewerLog $LogPath), $pattern, [Text.RegularExpressions.RegexOptions]::Multiline)
			if ($completion.Success) { break }
			if ($Process.HasExited) { return Get-ExitOutcome $Process }
			if ($Clock.Elapsed -ge $deadline) { return Get-DeadlineOutcome }
			Start-Sleep -Milliseconds $PollMilliseconds
		}
		$capture = $completion.Groups[1].Value | ConvertFrom-Json
		if ($capture.PSObject.Properties.Name -contains 'error') {
			return New-Outcome 'capture.failed' "Capture $sequence failed: $($capture.error)"
		}
		$hash = (Get-FileHash -LiteralPath $capture.path -Algorithm SHA256).Hash
		$result.captures.Add([ordered]@{ path = $capture.path; width = $capture.width; height = $capture.height; sha256 = $hash })
		if ([BrokenEngineHarness.ModelViewerWindow]::IsWindowVisible($script:Window)) { $result.windowVisible = $true }
		if (-not $RequireStable) {
			return New-Outcome 'ok' "Captured 1 PNG from PID $($Process.Id)."
		}
		if ($hash -ceq $previousHash) {
			$result.stable = $true
			return New-Outcome 'ok' "Captures $($sequence - 1) and $sequence matched after $sequence captures from PID $($Process.Id)."
		}
		$previousHash = $hash
	}
}

try {
	if (-not [IO.Path]::IsPathRooted($AppDataDirectory)) { throw "AppDataDirectory must be absolute: '$AppDataDirectory'." }

	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
	if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentScriptCommon.psm1'))) {
		$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
	}
	Import-Module (Join-Path $sharedScripts 'AgentScriptCommon.psm1') -Force
	$root = Get-AgentCanonicalPath $RepositoryRoot
	$executable = Join-Path $root 'ModelViewer\Platforms\VisualStudio2026\Output\ModelViewer.Debug.exe'
	if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
		Complete-ModelViewerCapture 'capture.executable-missing' "The ModelViewer Debug executable is missing: '$executable'."
	}

	Add-Type -Namespace BrokenEngineHarness -Name ModelViewerWindow -MemberDefinition @'
[DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr childAfter, string className, string windowName);
[DllImport("user32.dll")]
public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
[DllImport("user32.dll", SetLastError = true)]
[return: MarshalAs(UnmanagedType.Bool)]
public static extern bool PostMessage(IntPtr window, uint message, IntPtr wordParameter, IntPtr longParameter);
[DllImport("user32.dll")]
[return: MarshalAs(UnmanagedType.Bool)]
public static extern bool IsWindowVisible(IntPtr window);
'@

	# Canonical paths carry no trailing separator, which would otherwise escape the closing quote of a
	# quoted viewer argument.
	$appDataRoot = Get-AgentCanonicalPath $AppDataDirectory
	$logPath = Join-Path $appDataRoot 'ModelViewer.log'
	$result.logFile = $logPath
	# No -Force: an existing directory throws here, before any process starts, so the log cannot hold an
	# earlier run's completion line.
	[void](New-Item -ItemType Directory -Path $appDataRoot)

	$Window = [IntPtr]::Zero
	$outcome = $null
	$clock = [Diagnostics.Stopwatch]::StartNew()
	# Start-Process joins ArgumentList items, so every path-valued argument is quoted.
	$viewerArguments = @(
		'--agent-port', "$AgentPort",
		'--log-file', ('"' + $logPath + '"'),
		'--app-data-directory', ('"' + $appDataRoot + '"')
		if ($PSBoundParameters.ContainsKey('ModelPath')) {
			'--scene-path', ('"' + $ModelPath + '"')
		})
	$process = Start-Process -FilePath $executable -ArgumentList $viewerArguments -WorkingDirectory $appDataRoot -WindowStyle Hidden -PassThru
	try {
		$result.pid = $process.Id
		# Holding the handle from the start keeps the exit code readable after the process ends.
		[void]$process.Handle
		$outcome = Invoke-ViewerCapture $process $logPath $clock
	}
	catch {
		$outcome = New-Outcome 'internal.error' $_.Exception.Message
	}
	finally {
		if (-not $process.HasExited) {
			$closed = $false
			if ($Window -ne [IntPtr]::Zero -and [BrokenEngineHarness.ModelViewerWindow]::PostMessage($Window, $WmClose, [IntPtr]::Zero, [IntPtr]::Zero)) {
				$closed = $process.WaitForExit($CloseTimeoutSeconds * 1000)
			}
			if (-not $closed) {
				# The viewer can exit between the wait and the stop; the held handle keeps its PID from reuse.
				try {
					Stop-Process -Id $process.Id
				}
				catch {
					if (-not $process.HasExited) { throw }
				}
				$process.WaitForExit()
				if ($null -ne $outcome -and $outcome.Code -ceq 'ok') {
					$outcome = New-Outcome 'close.forced' "The viewer did not close within $CloseTimeoutSeconds s of WM_CLOSE, or no window was found; stopped PID $($process.Id)."
				}
			}
		}
		$result.exitCode = $process.ExitCode
	}

	$result.crashReports = @(
		(Join-Path $appDataRoot 'Broken Engine Model Viewer\Broken-Engine-Model-Viewer-Crash-Report.txt'),
		(Join-Path $appDataRoot 'Crash-Report.txt') | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
	Complete-ModelViewerCapture $outcome.Code $outcome.Message
}
catch {
	Complete-ModelViewerCapture 'internal.error' $_.Exception.Message
}
