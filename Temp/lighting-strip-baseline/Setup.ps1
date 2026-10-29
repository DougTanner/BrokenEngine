param(
	[int]$TargetTick = 400,
	[string]$SaveFile = 'lighting-strip-baseline-t400'
)
$ErrorActionPreference = 'Stop'
$Harness = 'C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad\Tools\AgentHarness\Platforms\VisualStudio2026\Output\AgentHarness.exe'
$Owner = '97ca3cb8-2b56-42a8-acc7-6e0009465041'

function Invoke-Server([string]$Json)
{
	$Response = $Json | & $Harness --owner $Owner --port 27100 --timeout-ms 15000 -
	if ($LASTEXITCODE -ne 0) { throw "server command failed ($LASTEXITCODE): $Json -> $Response" }
	return ($Response | ConvertFrom-Json -Depth 50).result
}

function Get-Tick { return [int64](Invoke-Server '{"cmd":"status"}').tick }

Invoke-Server '{"cmd":"reset"}' | Out-Null
Invoke-Server '{"cmd":"pause","params":{"paused":true}}' | Out-Null
$T0 = Get-Tick
"reset+pause tick=$T0"
if ($T0 -ge 190) { throw "Reset pause tick $T0 too late for tick-200 injection." }

$Entries = @(
	'{"coord":[0,0],"type":"SpawnOmni","tick":200,"vecPosition":[20,-60],"alignment":"random","eController":"RandomSpawn"}',
	'{"coord":[0,0],"type":"SpawnOmni","tick":200,"vecPosition":[60,-170],"alignment":"random","eController":"RandomSpawn"}',
	'{"coord":[0,0],"type":"SpawnOmni","tick":200,"vecPosition":[110,-60],"alignment":"random","eController":"RandomSpawn"}',
	'{"coord":[0,0],"type":"SpawnFighter","tick":200,"vecPosition":[30,-140],"alignment":"fleet","eController":"RandomSpawn"}',
	'{"coord":[0,0],"type":"SpawnFighter","tick":200,"vecPosition":[50,-90],"alignment":"fleet","eController":"RandomSpawn"}',
	'{"coord":[0,0],"type":"SpawnFighter","tick":200,"vecPosition":[90,-150],"alignment":"fleet","eController":"RandomSpawn"}',
	'{"coord":[0,0],"type":"SpawnExplosion","tick":400,"iTypeIndex":0,"vecPosition":[50,-25],"lifecycleFlags":4,"iTrailCount":6,"iParticleCount":16}',
	'{"coord":[0,0],"type":"SpawnExplosion","tick":400,"iTypeIndex":0,"vecPosition":[95,-60],"lifecycleFlags":4,"iTrailCount":6,"iParticleCount":16}'
)
$Inject = '{"cmd":"inject_payload","params":{"entries":[' + ($Entries -join ',') + ']}}'
$InjectResult = Invoke-Server $Inject
"inject: $($InjectResult | ConvertTo-Json -Compress)"

# Run at 1/1 to within 48 ticks of the target, then pause.
Invoke-Server '{"cmd":"pause","params":{"paused":false}}' | Out-Null
while ((Get-Tick) -lt ($TargetTick - 48)) { Start-Sleep -Milliseconds 100 }
Invoke-Server '{"cmd":"pause","params":{"paused":true}}' | Out-Null
"coarse pause tick=$(Get-Tick)"

# Slow to 1/32 and single-step to the exact target tick.
for ($i = 0; $i -lt 8; $i++) {
	$Scale = Invoke-Server '{"cmd":"timescale","params":{"faster":false}}'
	if ($Scale.denominator -eq 32) { break }
}
"timescale=$($Scale.numerator)/$($Scale.denominator)"
$Tick = Get-Tick
while ($Tick -lt $TargetTick) {
	'{"cmd":"pause","params":{"paused":false}}' | & $Harness --owner $Owner --port 27100 --timeout-ms 15000 - | Out-Null
	'{"cmd":"pause","params":{"paused":true}}' | & $Harness --owner $Owner --port 27100 --timeout-ms 15000 - | Out-Null
	$Tick = Get-Tick
}
"exact pause tick=$Tick"
if ($Tick -ne $TargetTick) { throw "Overshot target tick: $Tick" }

# Restore 1/1 while paused.
for ($i = 0; $i -lt 8; $i++) {
	$Scale = Invoke-Server '{"cmd":"timescale","params":{"faster":true}}'
	if ($Scale.denominator -eq 1) { break }
}
"timescale=$($Scale.numerator)/$($Scale.denominator)"

$Save = Invoke-Server ('{"cmd":"save","params":{"file":"' + $SaveFile + '"}}')
"save: $($Save | ConvertTo-Json -Compress)"
"final status: $((Invoke-Server '{"cmd":"status"}') | ConvertTo-Json -Compress)"
