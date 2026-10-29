param(
	[Parameter(Mandatory)][string]$OutDir,
	[Parameter(Mandatory)][string]$Owner,
	[string]$Harness = 'C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad\Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe',
	[string]$SaveFile = 'lighting-strip-baseline-t400',
	[string]$Prefix = 'baseline',
	# Client launch result startTimeUtc; the sun angle advances with client wall time from session start.
	[Parameter(Mandatory)][datetime]$ClientStartUtc,
	[int]$CloseAtSeconds = 60,
	[int]$WideAtSeconds = 75
)
function Wait-Until([int]$Seconds)
{
	$Target = $ClientStartUtc.ToUniversalTime().AddSeconds($Seconds)
	$Remaining = ($Target - [datetime]::UtcNow).TotalMilliseconds
	if ($Remaining -lt 0) { throw "Missed the $Seconds s capture point by $(-$Remaining) ms." }
	Start-Sleep -Milliseconds ([int]$Remaining)
	"capture point +$Seconds s at $([datetime]::UtcNow.ToString('o'))"
}
$ErrorActionPreference = 'Stop'

function Invoke-Port([int]$Port, [string]$Json, [int]$TimeoutMs = 15000)
{
	$Response = $Json | & $Harness --owner $Owner --port $Port --timeout-ms $TimeoutMs -
	if ($LASTEXITCODE -ne 0) { throw "command failed ($LASTEXITCODE) on ${Port}: $Json -> $Response" }
	return ($Response | ConvertFrom-Json -Depth 100).result
}

function Get-Scene([bool]$Units)
{
	$Json = if ($Units) { '{"cmd":"describe_scene","params":{"maxUnits":200}}' } else { '{"cmd":"describe_scene","params":{"includeUnits":false}}' }
	return Invoke-Port 27101 $Json
}

function Wait-Settled
{
	$Previous = ''
	for ($i = 0; $i -lt 60; $i++) {
		Start-Sleep -Milliseconds 250
		$Scene = Get-Scene $false
		$Key = "$($Scene.tick)|$($Scene.camera.eye -join ',')|$($Scene.cellStates[0].lastRenderedTick)"
		if ($Key -eq $Previous) { return $Scene }
		$Previous = $Key
	}
	throw 'Client tick/camera did not settle.'
}

function Save-Shot([string]$Name)
{
	$Path = Join-Path $OutDir "$Prefix-$Name.png"
	$PathJson = $Path | ConvertTo-Json
	$Shot = Invoke-Port 27101 ('{"cmd":"screenshot","params":{"path":' + $PathJson + ',"format":"png","maxWidth":0}}') 30000
	$Scene = Get-Scene $true
	$Scene | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath (Join-Path $OutDir "$Prefix-$Name-scene.json") -Encoding utf8
	$Cell = $Scene.cellStates | Where-Object { ($_.coord -join ',') -eq '0,0' }
	"shot $Name path=$($Shot.path) size=$($Shot.width)x$($Shot.height) clientTick=$($Scene.tick) confirmed=$($Cell.confirmedTick) tail=$($Cell.tailTick) lastRendered=$($Cell.lastRenderedTick) eye=$($Scene.camera.eye -join ',') visibleArea=$($Scene.camera.visibleArea -join ',') lod=$($Scene.camera.lod) counts=$($Scene.counts | ConvertTo-Json -Compress) unitsInView=$(@($Scene.units).Count)"
}

$Load = Invoke-Port 27100 ('{"cmd":"load","params":{"file":"' + $SaveFile + '","pauseAfterLoad":true}}')
"load: $($Load | ConvertTo-Json -Compress)"
if ($Load.resetToFresh) { throw 'Save was rejected (resetToFresh:true).' }
"server status: $((Invoke-Port 27100 '{"cmd":"status"}') | ConvertTo-Json -Compress)"
"server frame: $((Invoke-Port 27100 '{"cmd":"query_frame","params":{"coord":[0,0]}}') | ConvertTo-Json -Compress)"
$Settled = Wait-Settled
"settled after load: tick=$($Settled.tick) eye=$($Settled.camera.eye -join ',')"

# Close view: zoom fully in (clamps at the minimum eye height).
Invoke-Port 27101 '{"cmd":"wheel","params":{"notches":40}}' | Out-Null
Wait-Settled | Out-Null
Wait-Until $CloseAtSeconds
Save-Shot 'close'

# Wide view: 60 notches out from the clamped minimum.
Invoke-Port 27101 '{"cmd":"wheel","params":{"notches":-60}}' | Out-Null
Wait-Settled | Out-Null
Wait-Until $WideAtSeconds
Save-Shot 'wide'

$Profile = Invoke-Port 27101 '{"cmd":"query_profile","params":{}}'
$Profile | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath (Join-Path $OutDir "$Prefix-query_profile.json") -Encoding utf8
"lighting: $($Profile.lighting | ConvertTo-Json -Compress -Depth 100)"
