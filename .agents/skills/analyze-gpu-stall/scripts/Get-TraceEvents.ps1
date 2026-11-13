# Lists (Mode list) or counts by process and event (Mode count) the rows of an `xperf -a dumper` text file inside
# a trace-time window. Include and Exclude are regular expressions matched against each printed row, which reads
# "<time> <event> pid=<pid> tid=<tid> <payload>".
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string] $DumpPath,

	[Parameter(Mandatory = $true)]
	[long] $From,

	[Parameter(Mandatory = $true)]
	[long] $To,

	[ValidateSet('list', 'count')]
	[string] $Mode = 'list',

	[string] $Include = '.',

	# High-rate scheduler noise that hides the events a stall window is read for.
	[string] $Exclude = 'AttemptPreemption|VSyncSmoothened',

	[int] $Max = 400
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TraceDump.psm1') -Force

$header = Read-TraceDumpHeader $DumpPath
$counts = @{}
$matched = 0
foreach ($row in Get-TraceDumpRows $DumpPath $From $To) {
	$name = $row.Name -replace '^Microsoft-Windows-DxgKrnl/', ''
	# A manifest layout's payload follows its four etw: columns; any other layout's follows the CPU column.
	$first = if ((Get-TraceFieldIndex $header $row.Name 'etw:ActivityId') -ge 0) { 9 } else { 5 }
	$last = [Math]::Min($row.Columns.Count - 1, $first + 15)
	$payload = if ($last -ge $first) { ($row.Columns[$first..$last] -join ',') -replace '\s+', '' } else { '' }
	$text = "$($row.Time) $name pid=$($row.ProcessId) tid=$($row.ThreadId) $payload"
	if ($text -notmatch $Include -or $text -match $Exclude) { continue }
	if ($Mode -eq 'count') {
		$key = "pid=$($row.ProcessId) $name"
		$counts[$key] = 1 + [int] $counts[$key]
	}
	elseif ($matched -lt $Max) {
		$text.Substring(0, [Math]::Min(230, $text.Length))
	}
	$matched++
}

if ($Mode -eq 'count') {
	$counts.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First $Max | ForEach-Object { "$($_.Value) $($_.Key)" }
}
"matched=$matched listed=$([Math]::Min($matched, $Max))"
