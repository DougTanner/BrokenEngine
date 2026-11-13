# Runs one non-elevated circular ETW session with DxgKrnl at GPUView's present-level selection (log.cmd's
# TRACE_DXC_PRESENT), optionally plus an engine marker provider, and stops it -StopDelayMilliseconds after the
# client log gains a new line matching -StallPattern, or after -MaxSeconds. Such a session needs no elevation, so
# it carries no CPU context switches and no process names; the script therefore writes the processes and
# services seen at start and stop to "<etl>.processes.csv" so trace process IDs can be named. -AgentHarness with
# -Owner heartbeats a harness claim while it waits. The session is always stopped before the script exits.
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string] $EtlPath,

	[string] $ClientLog,

	[string] $StallPattern,

	[int] $MaxSeconds = 300,

	# GUID of a temporary engine ETW provider whose events mark frame points; see references/capture.md.
	[string] $MarkerProvider,

	[string] $AgentHarness,

	[string] $Owner,

	[string] $SessionName = 'BeGpuStall',

	[int] $MaxFileMegabytes = 1024,

	[int] $StopDelayMilliseconds = 1000
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrEmpty($ClientLog) -ne [string]::IsNullOrEmpty($StallPattern)) { throw 'Pass -ClientLog and -StallPattern together.' }
if ([string]::IsNullOrEmpty($AgentHarness) -ne [string]::IsNullOrEmpty($Owner)) { throw 'Pass -AgentHarness and -Owner together.' }
if ($ClientLog -and -not (Test-Path -LiteralPath $ClientLog -PathType Leaf)) { throw "Client log is missing: '$ClientLog'." }

$xperf = (Get-Command xperf -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$etl = [System.IO.Path]::GetFullPath($EtlPath, (Get-Location).ProviderPath)
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($etl)) | Out-Null
$providers = '802ec45a-1e99-4b83-9920-87c98277ba9d:0x208041:5'
if ($MarkerProvider) { $providers += "+$MarkerProvider" }

function Get-ProcessSnapshot([string] $Seen) {
	$services = Get-CimInstance Win32_Service -Filter 'ProcessId <> 0' | Group-Object ProcessId -AsHashTable -AsString
	foreach ($process in Get-CimInstance Win32_Process) {
		$key = [string] $process.ProcessId
		[pscustomobject]@{
			Seen = $Seen
			ProcessId = $process.ProcessId
			Name = $process.Name
			Created = if ($process.CreationDate) { $process.CreationDate.ToUniversalTime().ToString('o') } else { '' }
			Services = if ($services -and $services.ContainsKey($key)) { $services[$key].Name -join ' ' } else { '' }
		}
	}
}

function Get-StallCount {
	if (-not $ClientLog) { return 0 }
	@(Select-String -LiteralPath $ClientLog -Pattern $StallPattern).Count
}

foreach ($controller in Get-CimInstance Win32_VideoController) {
	"display adapter='$($controller.Name)' driver=$($controller.DriverVersion) mode=$($controller.CurrentHorizontalResolution)x$($controller.CurrentVerticalResolution)@$($controller.CurrentRefreshRate)Hz"
}
$snapshot = @(Get-ProcessSnapshot 'start')
$baseline = Get-StallCount
& $xperf -start $SessionName -on $providers -f $etl -BufferSize 1024 -MinBuffers 64 -MaxBuffers 256 -FileMode Circular -MaxFile $MaxFileMegabytes
if ($LASTEXITCODE -ne 0) { throw "xperf -start failed with exit $LASTEXITCODE; a leftover session of the same name stops with: xperf -stop $SessionName" }
# Read after xperf returns: the trace origin precedes it by a few milliseconds, while a read before the start
# can precede the origin by hundreds of milliseconds of xperf start-up.
$startQpc = [System.Diagnostics.Stopwatch]::GetTimestamp()
$started = [datetime]::UtcNow
$lastHeartbeat = $started
$caught = $false
try {
	while (([datetime]::UtcNow - $started).TotalSeconds -lt $MaxSeconds) {
		Start-Sleep -Milliseconds 400
		if ((Get-StallCount) -gt $baseline) {
			Start-Sleep -Milliseconds $StopDelayMilliseconds
			$caught = $true
			break
		}
		if ($AgentHarness -and ([datetime]::UtcNow - $lastHeartbeat).TotalSeconds -gt 60) {
			& $AgentHarness lock heartbeat --key default --owner $Owner | Out-Null
			$lastHeartbeat = [datetime]::UtcNow
		}
	}
}
finally {
	& $xperf -stop $SessionName | Out-Null
	$stopExit = $LASTEXITCODE
	$stopped = [datetime]::UtcNow
	$snapshot += @(Get-ProcessSnapshot 'stop')
	$snapshot | Export-Csv -LiteralPath "$etl.processes.csv" -NoTypeInformation
}

"caught=$caught stopExit=$stopExit elapsedSeconds=$([int] ($stopped - $started).TotalSeconds) started=$($started.ToString('o')) stopped=$($stopped.ToString('o')) startQpc=$startQpc qpcFrequency=$([System.Diagnostics.Stopwatch]::Frequency) etl=$etl etlBytes=$((Get-Item -LiteralPath $etl).Length)"
if ($ClientLog) {
	Select-String -LiteralPath $ClientLog -Pattern $StallPattern | Select-Object -Skip $baseline | ForEach-Object { $_.Line }
}
