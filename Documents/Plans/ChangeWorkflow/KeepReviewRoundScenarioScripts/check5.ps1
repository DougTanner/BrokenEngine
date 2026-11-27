# Acceptance check 5: Coretide snapshots and the split save, continuing in check 4's repository.
. (Join-Path $PSScriptRoot 'lib.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'repo'))
$main = (git -C $repo rev-parse main).Trim()
$landingJson = 'Temp/loop-scenario/landing.json'
function Invoke-Landing([string] $Repo, [string] $Base) {
	$out = & pwsh -NoProfile -NonInteractive -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $landingJson 2>&1
	$json = Get-Content -Raw $landingJson | ConvertFrom-Json
	"landing: status=$($json.status) code=$($json.code) message=$($json.message)" | Out-Host
	return $json
}
function New-CoretideClone([string] $Bare, [string] $Engine) {
	git init -q --bare -b main $Bare
	$seed = "$Bare-seed"
	git clone -q $Bare $seed 2>$null
	git -C $seed config user.name scratch; git -C $seed config user.email scratch@example.invalid
	Write-Text "$seed/notes.md" "# Notes`n"
	git -C $seed add -A; git -C $seed commit -q -m seed; git -C $seed push -q origin main 2>$null
	git clone -q $Bare "$Engine/Projects/Coretide" 2>$null
	$clone = "$Engine/Projects/Coretide"
	git -C $clone config user.name scratch; git -C $clone config user.email scratch@example.invalid
	$top = [IO.Path]::GetFullPath((git -C $clone rev-parse --show-toplevel).Trim())
	if ($top -ne [IO.Path]::GetFullPath($clone)) { throw "clone toplevel mismatch $top" }
	return $clone
}

$clone = New-CoretideClone ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'coretide.git'))) $repo
Write-Text "$clone/notes.md" "# Notes`n`nEdited in the session.`n"
$cHead = (git -C $clone rev-parse HEAD).Trim(); $cIndex = (git -C $clone ls-files -s | Out-String); $cStatus = (git -C $clone status --porcelain | Out-String)
$common = @('-RepositoryRoot', $repo, '-Baseline', $main, '-Tier', '2', '-Landing')
$r = Invoke-Steps ($common + '-StartRound')
Show 'start round with a Coretide clone' $r
$sha = Get-RoundHead $r.Text
$cTip = Get-RefTip $clone
$msg = (git -C $repo log -1 --format=%B $sha | Out-String)
Assert ($null -ne $cTip -and $msg -match "(?m)^Coretide-Snapshot: $cTip" -and $r.Text -match "Coretide head $cTip") 'engine message and rows name the clone ref tip as Coretide-Snapshot'
Assert ((git -C $clone rev-parse HEAD).Trim() -eq $cHead -and (git -C $clone ls-files -s | Out-String) -eq $cIndex -and (git -C $clone status --porcelain | Out-String) -eq $cStatus) 'clone HEAD, index, and status unchanged'
$inv = pwsh -NoProfile -NonInteractive -Command "Import-Module ./.agents/scripts/SessionChangeInventory.psm1; (Get-SessionChangeInventory -RepositoryRoot '$repo' -Baseline $main -Head $sha).coretide.headSha"
Assert ("$inv".Trim() -eq $cTip) "inventory -Head <engine snapshot> reports coretide.headSha $inv"
Assert ((git -C $repo log --format=%B refs/worktree/review-rounds | Out-String) -notmatch 'Projects/Coretide|notes\.md') 'engine chain messages name no Coretide path'
$owes = ([regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value) -replace '<severity>', 'none'
$c = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $sha, '-Results', $owes)
Show 'close' $c
for ($i = 0; $i -lt 4; $i++) {
	$r = Invoke-Steps ($common + '-StartRound')
	$s = Get-RoundHead $r.Text
	if ($null -eq $s) { break }
	Show 'drain' $r
	[void] (Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $s, '-Results', (([regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value) -replace '<severity>', 'none')))
}
Show 'loop state' (Invoke-Steps $common)
git -C $clone commit -q -am 'session notes'
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'pass') 'clone commit of the reviewed bytes: pass'
git -C $clone reset -q --hard HEAD~1
Write-Text "$clone/notes.md" "# Notes`n`nDifferent bytes.`n"
git -C $clone commit -q -am 'different notes'
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-unfinished') 'clone commit of different bytes: an owed round'

'## split save'
$repo3 = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'repo3'))
New-ScratchRepo $repo3
Write-Text "$repo3/.gitignore" "Temp/`nProjects/Coretide/`n"
Write-Text "$repo3/doc.md" "# Doc`n"
git -C $repo3 add -A; git -C $repo3 commit -q -m baseline
$main3 = (git -C $repo3 rev-parse HEAD).Trim()
$clone3 = New-CoretideClone ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'coretide3.git'))) $repo3
git -C $repo3 switch -q -c session
foreach ($root in @($repo3, $clone3)) {
	New-Item -ItemType Directory -Force "$root/Documents/Plans/ChangeWorkflow" | Out-Null
	Write-Text "$root/Documents/Plans/ChangeWorkflow/Saved.md" "# Saved`n"
	git -C $root add -A; git -C $root commit -q -m saved
}
$session3 = (git -C $repo3 rev-parse session).Trim()
$cand3 = (git -C $clone3 rev-parse HEAD).Trim()
$both = 'Documents/Plans/ChangeWorkflow/Saved.md,Projects/Coretide/Documents/Plans/ChangeWorkflow/Saved.md'
$call = { param([string] $Arguments) pwsh -NoProfile -NonInteractive -Command "Import-Module ./.agents/scripts/SessionChangeInventory.psm1; `$r = Test-ReviewRoundCoverage -RepositoryRoot '$repo3' $Arguments; if (`$null -eq `$r) { 'none' } else { `$r.Code }" }
$a = & $call "-EngineBase $main3 -EngineCandidate $session3 -CoretideCandidate $cand3 -SavePlanPaths '$both'"
Assert ("$a".Trim() -eq 'none') "both sides with both paths authorized: $a"
$a = & $call "-EngineBase $main3 -EngineCandidate $session3 -SavePlanPaths '$both'"
Assert ("$a".Trim() -eq 'none') "engine side alone with both paths authorized: $a"
$a = & $call "-EngineBase $main3 -EngineCandidate $session3 -SavePlanPaths 'Projects/Coretide/Documents/Plans/ChangeWorkflow/Saved.md'"
Assert ("$a".Trim() -eq 'inventory.review-round-missing') "engine side alone with only the Coretide path authorized: $a"
