# Prints one row per vsync interval of an `xperf -a dumper` text file (offsets in microseconds after that vsync):
# the DWMVsyncSignal payload, the compositor's first event, event count, and retrieved present tokens of any
# process (none means it ran no composition pass), the engine's present calls and the moments the compositor
# retrieved an engine present token, engine queue submissions and ETW marker events, flip events, and the other
# processes with the most rows of any event, counted by each row's process ID (a DPC or completion row carries the
# interrupted thread's process, not the GPU work's owner); then summary statistics and the trace origin as a QPC value.
# An engine token is the Token of an engine PresentHistory*/win:Start row; it counts as retrieved at the next
# PresentHistory/win:Stop row of the compositor carrying the same Token (tokens are pooled and reused).
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string] $DumpPath,

	[Parameter(Mandatory = $true)]
	[int] $EnginePid,

	# The desktop compositor (dwm.exe) process ID.
	[Parameter(Mandatory = $true)]
	[int] $DwmPid,

	[long] $From = 0,

	[long] $To = [long]::MaxValue,

	[int] $TopOthers = 3,

	[int] $MaxRows = 200,

	[long] $QpcFrequency = [System.Diagnostics.Stopwatch]::Frequency
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TraceDump.psm1') -Force

$prefix = 'Microsoft-Windows-DxgKrnl/'
$vsyncName = "${prefix}VSyncDPC/win:Info"
$header = Read-TraceDumpHeader $DumpPath
$frameIndex = Get-TraceFieldIndex $header $vsyncName 'FrameNumber'
$frameQpcIndex = Get-TraceFieldIndex $header $vsyncName 'FrameQPCTime'
if ($frameIndex -lt 0 -or $frameQpcIndex -lt 0) { throw "The dump header has no $vsyncName layout; the trace lacks DxgKrnl vsync events." }
$signalIndex = Get-TraceFieldIndex $header "${prefix}DWMVsyncSignal/win:Info" 'DWMCount'
$stopTokenIndex = Get-TraceFieldIndex $header "${prefix}PresentHistory/win:Stop" 'Token'
$startTokenIndex = @{}
foreach ($name in @("${prefix}PresentHistory/win:Start", "${prefix}PresentHistoryDetailed/win:Start")) {
	$index = Get-TraceFieldIndex $header $name 'Token'
	if ($index -ge 0) { $startTokenIndex[$name] = $index }
}
$ignoredProcesses = @(-1, 0, 4, $EnginePid, $DwmPid)

$intervals = [System.Collections.Generic.List[object]]::new()
$current = $null
$pendingTokens = @{}
$presentToRetrieved = [System.Collections.Generic.List[long]]::new()
$originQpc = $null
foreach ($row in Get-TraceDumpRows $DumpPath $From $To) {
	if ($row.Name -eq $vsyncName) {
		if ($null -eq $originQpc) { $originQpc = Get-TraceOriginQpc $row $frameQpcIndex $QpcFrequency }
		$current = [pscustomobject]@{
			Time = $row.Time; Frame = $row.Columns[$frameIndex].Trim(); Signal = ''; DwmFirst = $null; DwmEvents = 0; DwmTokens = 0
			Presents = [System.Collections.Generic.List[long]]::new(); Retrieved = [System.Collections.Generic.List[long]]::new()
			Submits = 0; Markers = [System.Collections.Generic.List[string]]::new(); Flips = 0; Others = @{}
		}
		$intervals.Add($current)
		continue
	}

	$tokenIndex = $startTokenIndex[$row.Name]
	if ($row.ProcessId -eq $EnginePid -and $null -ne $tokenIndex) { $pendingTokens[$row.Columns[$tokenIndex].Trim()] = $row.Time }
	$retrieved = $false
	$dwmTokenStop = $row.ProcessId -eq $DwmPid -and $row.Name -eq "${prefix}PresentHistory/win:Stop"
	if ($dwmTokenStop -and $stopTokenIndex -ge 0) {
		$token = $row.Columns[$stopTokenIndex].Trim()
		if ($pendingTokens.ContainsKey($token)) {
			$presentToRetrieved.Add($row.Time - $pendingTokens[$token])
			$pendingTokens.Remove($token)
			$retrieved = $true
		}
	}
	if ($null -eq $current) { continue }

	$offset = $row.Time - $current.Time
	if ($row.Name -eq "${prefix}DWMVsyncSignal/win:Info" -and $current.Signal -eq '' -and $signalIndex -ge 0) {
		$current.Signal = ($row.Columns[$signalIndex..($signalIndex + 4)] | ForEach-Object { $_.Trim() }) -join ','
	}
	if ($row.Name -like "${prefix}MMIOFlip*") { $current.Flips++ }
	if ($retrieved) { $current.Retrieved.Add($offset) }
	if ($dwmTokenStop) { $current.DwmTokens++ }
	if ($row.ProcessId -eq $DwmPid) {
		if ($null -eq $current.DwmFirst) { $current.DwmFirst = $offset }
		$current.DwmEvents++
	}
	elseif ($row.ProcessId -eq $EnginePid) {
		switch ($row.Name) {
			"${prefix}Present/win:Info" { $current.Presents.Add($offset) }
			"${prefix}QueuePacket/win:Start" { $current.Submits++ }
			'UnknownEvent/Crimson' { $current.Markers.Add("$($row.ThreadId)@$offset") }
		}
	}
	elseif ($ignoredProcesses -notcontains $row.ProcessId) {
		$current.Others[$row.ProcessId] = 1 + [int] $current.Others[$row.ProcessId]
	}
}

if ($intervals.Count -lt 2) { throw "Fewer than two vsyncs between $From and $To; widen the window." }

function Format-Stats([string] $Label, $Values) {
	if ($Values.Count -eq 0) { return "${Label}: none" }
	$sorted = @($Values | Sort-Object)
	'{0}: n={1} min={2} p50={3} p95={4} max={5}' -f $Label, $sorted.Count, $sorted[0], $sorted[[int] ($sorted.Count / 2)], $sorted[[Math]::Min($sorted.Count - 1, [int] ($sorted.Count * 0.95))], $sorted[-1]
}

$durations = @(for ($i = 0; $i + 1 -lt $intervals.Count; $i++) { $intervals[$i + 1].Time - $intervals[$i].Time })
$medianDuration = @($durations | Sort-Object)[[int] ($durations.Count / 2)]
$retrievedToNextVsync = [System.Collections.Generic.List[long]]::new()
$tokensPerInterval = @{}
for ($i = 0; $i + 1 -lt $intervals.Count; $i++) {
	$interval = $intervals[$i]
	$duration = $durations[$i]
	foreach ($offset in $interval.Retrieved) { $retrievedToNextVsync.Add($duration - $offset) }
	$tokensPerInterval[$interval.Retrieved.Count] = 1 + [int] $tokensPerInterval[$interval.Retrieved.Count]
	if ($i -ge $MaxRows) { continue }
	$flags = @()
	if ($duration -gt 1.5 * $medianDuration) { $flags += 'LONG' }
	if ($interval.DwmTokens -eq 0) { $flags += 'NOCOMPOSE' }
	if ($interval.Retrieved.Count -eq 0) { $flags += 'NOTOKEN' }
	$others = ($interval.Others.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First $TopOthers | ForEach-Object { "$($_.Key):$($_.Value)" }) -join ' '
	$dwmFirst = if ($null -eq $interval.DwmFirst) { 'none' } else { "+$($interval.DwmFirst)" }
	'V{0} t={1} dt={2} signal=[{3}] dwmFirst={4} dwmEvents={5} dwmTokens={6} presents=[{7}] retrieved=[{8}] submits={9} markers=[{10}] flips={11} others=[{12}] {13}' -f $interval.Frame, $interval.Time, $duration, $interval.Signal, $dwmFirst, $interval.DwmEvents, $interval.DwmTokens, ($interval.Presents -join ' '), ($interval.Retrieved -join ' '), $interval.Submits, ($interval.Markers -join ' '), $interval.Flips, $others, ($flags -join ' ')
}

Format-Stats 'vsyncIntervalUs' $durations
Format-Stats 'presentToRetrievedUs' $presentToRetrieved
Format-Stats 'retrievedToNextVsyncUs' $retrievedToNextVsync
'engineTokensRetrievedPerInterval: ' + (($tokensPerInterval.GetEnumerator() | Sort-Object Key | ForEach-Object { "$($_.Key)x$($_.Value)" }) -join ' ')
"intervals=$($durations.Count) longIntervals=$(@($durations | Where-Object { $_ -gt 1.5 * $medianDuration }).Count) traceOriginQpc=$originQpc qpcFrequency=$QpcFrequency"
