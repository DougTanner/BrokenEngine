# Dumps one trace-time window of an ETL to text with `xperf -a dumper -range`, then prints the trace origin as a
# QPC value read from the window's first VSyncDPC row. Give the window either as -FromUs/-ToUs trace times, or as
# -CenterQpc, a QueryPerformanceCounter value the engine logged, with the origin taken from -OriginQpc (the
# capture script's startQpc, which only approximates the true origin, so the window is widened by
# -SlackMilliseconds) or, without it, from a probe dump of the trace's first second.
[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)]
	[string] $EtlPath,

	[Parameter(Mandatory = $true)]
	[string] $OutPath,

	[long] $FromUs = -1,

	[long] $ToUs = -1,

	[long] $CenterQpc = 0,

	[long] $OriginQpc = 0,

	[int] $HalfWidthMilliseconds = 50,

	[int] $SlackMilliseconds = 250,

	[long] $QpcFrequency = [System.Diagnostics.Stopwatch]::Frequency
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TraceDump.psm1') -Force

$xperf = (Get-Command xperf -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$etl = (Resolve-Path -LiteralPath $EtlPath).ProviderPath
$out = [System.IO.Path]::GetFullPath($OutPath, (Get-Location).ProviderPath)
New-Item -ItemType Directory -Force -Path ([System.IO.Path]::GetDirectoryName($out)) | Out-Null
$vsyncName = 'Microsoft-Windows-DxgKrnl/VSyncDPC/win:Info'

function Invoke-Dumper([long] $From, [long] $To, [string] $Target) {
	& $xperf -i $etl -o $Target -a dumper -range $From $To | Out-Null
	if ($LASTEXITCODE -ne 0) { throw "xperf dumper failed with exit $LASTEXITCODE for range $From..$To." }
}

function Get-DumpOrigin([string] $Dump) {
	$frameQpcIndex = Get-TraceFieldIndex (Read-TraceDumpHeader $Dump) $vsyncName 'FrameQPCTime'
	if ($frameQpcIndex -lt 0) { return $null }
	$vsync = Get-TraceDumpRows $Dump | Where-Object Name -eq $vsyncName | Select-Object -First 1
	if ($null -eq $vsync) { return $null }
	Get-TraceOriginQpc $vsync $frameQpcIndex $QpcFrequency
}

function ConvertTo-TraceMicroseconds([long] $Qpc, [long] $Origin) {
	[long] ([decimal] ($Qpc - $Origin) * 1000000 / $QpcFrequency)
}

if ($CenterQpc -gt 0) {
	$slack = 0
	if ($OriginQpc -le 0) {
		$probe = "$out.clock.txt"
		Invoke-Dumper 0 1000000 $probe
		# Untyped, because assigning $null to the [long] parameter would silently make the origin 0.
		try { $probeOrigin = Get-DumpOrigin $probe }
		finally { Remove-Item -LiteralPath $probe -Force }
		if ($null -eq $probeOrigin) { throw 'The first trace second has no VSyncDPC row (a circular trace may have overwritten it); pass the capture''s startQpc as -OriginQpc, or dump any window with -FromUs/-ToUs and pass its printed traceOriginQpc as -OriginQpc.' }
		$OriginQpc = $probeOrigin
	}
	else {
		$slack = $SlackMilliseconds * 1000
	}
	$center = ConvertTo-TraceMicroseconds $CenterQpc $OriginQpc
	$FromUs = [Math]::Max(0L, $center - $HalfWidthMilliseconds * 1000 - $slack)
	$ToUs = $center + $HalfWidthMilliseconds * 1000 + $slack
}
elseif ($FromUs -lt 0 -or $ToUs -le $FromUs) {
	throw 'Pass -CenterQpc, or -FromUs and -ToUs with ToUs after FromUs.'
}

Invoke-Dumper $FromUs $ToUs $out
$origin = Get-DumpOrigin $out
$centerText = if ($CenterQpc -gt 0 -and $null -ne $origin) { " centerUs=$(ConvertTo-TraceMicroseconds $CenterQpc $origin)" } else { '' }
"out=$out fromUs=$FromUs toUs=$ToUs traceOriginQpc=$origin qpcFrequency=$QpcFrequency bytes=$((Get-Item -LiteralPath $out).Length)$centerText"
