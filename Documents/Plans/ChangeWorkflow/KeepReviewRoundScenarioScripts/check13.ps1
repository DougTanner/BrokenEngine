# Round-5 script-fix cases: one case per accepted round-5 script finding, against the scripts under -Scripts. -Tag
# names Temp/loop-scenario/c13-<Tag>, which must not exist yet. Run from the worktree root. A FAIL line is the red
# signal; each Assert states the behavior the fix must show.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$script:Steps = Join-Path $Scripts '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$inventoryCli = Join-Path $Scripts '.agents/scripts/Get-SessionChangeInventory.ps1'
$inventoryModule = Join-Path $Scripts '.agents/scripts/SessionChangeInventory.psm1'
$coretideScript = Join-Path $Scripts '.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1'
$top = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c13-$Tag"))
if (Test-Path -LiteralPath $top) { throw "scratch path exists: $top" }
New-Item -ItemType Directory -Force $top | Out-Null
"scripts: $Scripts"

function New-Case([string] $Name, [hashtable] $Files) {
	$repo = Join-Path $top $Name
	New-ScratchRepo $repo
	Write-Text "$repo/.gitignore" "Temp/`nProjects/Coretide/`n"
	foreach ($path in $Files.Keys) {
		$full = Join-Path $repo $path
		New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
		Write-Text $full $Files[$path]
	}
	git -C $repo add -A
	git -C $repo commit -q -m baseline
	return $repo
}
function New-CoretideClone([string] $Repo, [string] $Name, [hashtable] $Files) {
	$bare = Join-Path $top "$Name-coretide.git"
	git init -q --bare -b main $bare
	$seed = "$bare-seed"
	git clone -q $bare $seed 2>$null
	git -C $seed config user.name scratch; git -C $seed config user.email scratch@example.invalid
	foreach ($path in $Files.Keys) {
		$full = Join-Path $seed $path
		New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
		Write-Text $full $Files[$path]
	}
	git -C $seed add -A; git -C $seed commit -q -m seed; git -C $seed push -q origin main 2>$null
	$clone = "$Repo/Projects/Coretide"
	git clone -q $bare $clone 2>$null
	git -C $clone config user.name scratch; git -C $clone config user.email scratch@example.invalid
	return $clone
}
function Start-Round([string] $Repo, [string] $Baseline, [int] $Tier, [string[]] $Extra = @()) {
	$r = Invoke-Steps (@('-RepositoryRoot', $Repo, '-Baseline', $Baseline, '-Tier', "$Tier", '-StartRound') + $Extra)
	return [pscustomobject] @{ Code = $r.Code; Text = $r.Text; Sha = Get-RoundHead $r.Text; Template = [regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value }
}
function Close-Round([string] $Repo, [string] $Sha, [string] $Results) {
	return Invoke-Steps @('-RepositoryRoot', $Repo, '-CloseRound', '-Head', $Sha, '-Results', $Results)
}
function Get-ReviewRows([string] $Text) { return @($Text -split "`n" | Where-Object { $_ -match '; Head ' }) }
function Set-Result([string] $Template, [hashtable] $Values) {
	$out = $Template
	foreach ($key in $Values.Keys) { $out = $out -replace ([regex]::Escape("$key=<severity>")), "$key=$($Values[$key])" }
	return $out -replace '<severity>', 'none'
}
function Invoke-Landing([string] $Repo, [string] $Base) {
	$json = Join-Path $top 'landing.json'
	& pwsh -NoProfile -NonInteractive -File $inventoryCli -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $json 2>&1 | Out-Null
	return Get-Content -Raw $json | ConvertFrom-Json
}
function Invoke-Gate([string] $Repo, [string] $Arguments) {
	$out = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; `$r = Test-ReviewRoundCoverage -RepositoryRoot '$Repo' $Arguments; if (`$null -eq `$r) { 'none' } else { `"`$(`$r.Code) `$(`$r.Message)`" }" 2>&1
	return "$out".Trim()
}

'## COH501 a landing-mode Tier-1 combined round, then a run without -Landing owes /verify-acceptance'
$repo = New-Case 'coh501' @{ 'README.md' = "# Readme`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/README.md" "# Readme`n`nOne, edited.`n"
$s1 = Start-Round $repo $base 1 @('-Landing')
"round 1: $(@(Get-ReviewRows $s1.Text) -join ' || ')"
[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
$landing = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '1', '-Landing')
Assert ($landing.Text -match 'Review rounds: done') 'control: the -Landing run is done after the clean combined round'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '1')
Show 'default run without -Landing' $r
$s2 = Start-Round $repo $base 1
"round 2: $(@(Get-ReviewRows $s2.Text) -join ' || ')"
Assert ($r.Text -match '-StartRound' -and $r.Text -notmatch 'Review rounds: done' -and (Get-ReviewRows $s2.Text) -match 'reviewer /verify-acceptance') 'COH501: without -Landing the stage owes the /verify-acceptance round the landing-mode round did not carry'
if ($null -ne $s2.Sha) { [void] (Close-Round $repo $s2.Sha ($s2.Template -replace '<severity>', 'none')) }
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '1')
Assert ($r.Text -match 'Review rounds: done') 'control: the closed acceptance round finishes the loop'

'## COH502 a High combined close that carried skill validation, the package restored to baseline'
$repo = New-Case 'coh502' @{ 'README.md' = "# Readme`n`nOne.`n"; '.agents/skills/demo/SKILL.md' = "---`nname: demo`n---`n`n# Demo`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/README.md" "# Readme`n`nOne, edited.`n"
Write-Text "$repo/.agents/skills/demo/SKILL.md" "---`nname: demo`n---`n`n# Demo, edited`n"
$s1 = Start-Round $repo $base 1 @('-Landing')
"round 1: $(@(Get-ReviewRows $s1.Text) -join ' || ')"
Assert ($s1.Text -match '/external-skill-creator') 'control: round 1 is combined and carries /external-skill-creator'
[void] (Close-Round $repo $s1.Sha (Set-Result $s1.Template @{ '/coherence-review' = 'High' }))
Write-Text "$repo/.agents/skills/demo/SKILL.md" "---`nname: demo`n---`n`n# Demo`n"
Write-Text "$repo/README.md" "# Readme`n`nOne, fixed.`n"
$s2 = Start-Round $repo $base 1 @('-Landing')
"round 2: $(@(Get-ReviewRows $s2.Text) -join ' || ')"
Assert ($null -ne $s2.Sha -and $s2.Text -notmatch '/external-skill-creator') 'COH502: with the package restored no row carries skill validation'
$c2 = Close-Round $repo $s2.Sha ($s2.Template -replace '<severity>', 'none')
Show 'close round 2' $c2
Assert ($c2.Code -eq 0) 'COH502: round 2 closes'

'## COH502 sibling a Tier-2 skill package validated clean, then restored to baseline'
foreach ($restore in $true, $false) {
	$repo = New-Case "coh502s-$restore" @{ 'README.md' = "# Readme`n`nOne.`n"; '.agents/skills/demo/SKILL.md' = "---`nname: demo`n---`n`n# Demo`n" }
	$base = (git -C $repo rev-parse HEAD).Trim()
	Write-Text "$repo/.agents/skills/demo/SKILL.md" "---`nname: demo`n---`n`n# Demo, edited`n"
	Write-Text "$repo/README.md" "# Readme`n`nOne, edited.`n"
	$s1 = Start-Round $repo $base 2 @('-Landing')
	$severity = if ($restore) { @{ '/coherence-review' = 'High' } } else { @{ '/external-skill-creator' = 'High' } }
	[void] (Close-Round $repo $s1.Sha (Set-Result $s1.Template $severity))
	if ($restore) { Write-Text "$repo/.agents/skills/demo/SKILL.md" "---`nname: demo`n---`n`n# Demo`n" }
	Write-Text "$repo/README.md" "# Readme`n`nOne, fixed.`n"
	$s2 = Start-Round $repo $base 2 @('-Landing')
	"round 2 (restored=$restore): $(@(Get-ReviewRows $s2.Text | ForEach-Object { $_ -replace '; Head .*$', '' }) -join ' || ')"
	if ($restore) { Assert ($null -ne $s2.Sha -and $s2.Text -notmatch '/external-skill-creator') 'COH502 sibling: a package validated clean and then restored owes no skill validation' }
	else { Assert (@(Get-ReviewRows $s2.Text | Where-Object { $_ -match '/external-skill-creator' }).Count -gt 0)'control: a package still changed in the session keeps its validation owed after a High' }
}

'## ADV024 a path newly in the session diff with the snapshot''s bytes is owed'
$repo = New-Case 'adv024' @{ 'doc.md' = "# Doc`n`nOne.`n"; 'other.md' = "# Other`n`nOne.`n" }
git -C $repo switch -q -c session
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$base = (git -C $repo rev-parse main).Trim()
$s1 = Start-Round $repo $base 2 @('-Landing')
[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
git -C $repo commit -q -am session
git -C $repo switch -q main
Write-Text "$repo/other.md" "# Other`n`nOne, primary edit.`n"
git -C $repo commit -q -am primary
git -C $repo switch -q session
git -C $repo rebase -q main
$newBase = (git -C $repo rev-parse main).Trim()
$j = Invoke-Landing $repo $newBase
"control landing after the clean rebase: status=$($j.status) code=$($j.code)"
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $newBase, '-Tier', '2', '-Landing')
Assert ($j.status -eq 'pass' -and $r.Text -match 'Review rounds: done') 'control: a clean rebase of the unchanged patch over a primary edit to another file owes nothing and lands'
Write-Text "$repo/other.md" "# Other`n`nOne.`n"
git -C $repo commit -q -am 'restore other.md'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $newBase, '-Tier', '2', '-Landing')
$j = Invoke-Landing $repo $newBase
"landing after restoring other.md: status=$($j.status) code=$($j.code) message=$($j.message)"
$s2 = Start-Round $repo $newBase 2 @('-Landing')
"round 2: $(@(Get-ReviewRows $s2.Text) -join ' || ')"
Assert ($r.Text -match '-StartRound' -and $r.Text -notmatch 'Review rounds: done' -and (Get-ReviewRows $s2.Text) -match '/coherence-review') 'ADV024: the step script owes /coherence-review for the new session path'
Assert ($j.code -eq 'inventory.review-round-unfinished') 'ADV024: landing refuses the unreviewed session path'

'## ADV025 engine baseline advanced, Coretide-only landing agrees with the step script'
$repo = New-Case 'adv025' @{ 'doc.md' = "# Doc`n`nOne.`n"; 'other.md' = "# Other`n`nOne.`n" }
git -C $repo switch -q -c session
$base = (git -C $repo rev-parse main).Trim()
$clone = New-CoretideClone $repo 'adv025' @{ 'notes.md' = "# Notes`n"; 'Documents/Plans/Area/Done.md' = "# Done`n" }
Write-Text "$clone/notes.md" "# Notes`n`nEdited in the session.`n"
$s1 = Start-Round $repo $base 2 @('-Landing')
"round 1 rows: $(@(Get-ReviewRows $s1.Text | ForEach-Object { $_ -replace '; Head .*$', '' }) -join ' || ')"
[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
git -C $repo switch -q main
Write-Text "$repo/other.md" "# Other`n`nOne, primary edit.`n"
git -C $repo commit -q -am primary
git -C $repo switch -q session
git -C $repo reset -q --hard main
$newBase = (git -C $repo rev-parse main).Trim()
Remove-Item -LiteralPath "$clone/Documents/Plans/Area/Done.md"
git -C $clone add -A; git -C $clone commit -q -m 'session notes and Plan deletion'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $newBase, '-Tier', '2', '-Landing')
$done = $r.Text -match 'Review rounds: done'
$candidate = (git -C $clone rev-parse HEAD).Trim()
$g = Invoke-Gate $repo "-CoretideCandidate $candidate"
"step script done: $done; Coretide-only gate: $g"
Assert ($done) 'control: the step script at the current engine baseline owes no review'
Assert ($g -eq 'none') 'ADV025: the Coretide-only landing gate agrees with the step script'

'## PD011 a failure to evaluate the rounds at the landing candidate is terminal'
$repo = New-Case 'pd011' @{ 'doc.md' = "# Doc`n`nOne.`n" }
git -C $repo switch -q -c session
$base = (git -C $repo rev-parse main).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s1 = Start-Round $repo $base 2 @('-Landing')
[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
git -C $repo commit -q -am session
# A Coretide clone without an upstream makes every inventory at the candidate fail.
$clone = "$repo/Projects/Coretide"
git init -q -b main $clone
git -C $clone config user.name scratch; git -C $clone config user.email scratch@example.invalid
Write-Text "$clone/notes.md" "# Notes`n"
git -C $clone add -A; git -C $clone commit -q -m seed
$candidate = (git -C $repo rev-parse session).Trim()
$g = Invoke-Gate $repo "-EngineBase $base -EngineCandidate $candidate"
"engine gate: $g"
$code = ($g -split ' ')[0]
# Invoke-FinalizeLanding.ps1 Assert-LandingReviewRoundCoverage gives recovery exactly to an inventory.review-round-* code.
Assert ($g -match 'could not be evaluated') 'control: the gate reports an evaluation failure'
Assert (-not $code.StartsWith('inventory.review-round-')) "PD011: the evaluation failure ($code) maps to the terminal disposition, not recovery"

'## PD012 the Coretide result states a disposition'
$repo = New-Case 'pd012' @{ 'doc.md' = "# Doc`n" }
$clone = New-CoretideClone $repo 'pd012' @{ 'notes.md' = "# Notes`n" }
$message = Join-Path $top 'pd012-message.txt'
Write-Text $message "Coretide change`n"
$out = pwsh -NoProfile -NonInteractive -File $coretideScript -Prepare -CurrentWorktree $repo -CommitMessageFile $message 2>&1
"prepare result: $out"
$json = "$out" | ConvertFrom-Json
Assert ($json.schemaVersion -eq 'broken-engine-finalize-coretide/v2' -and $json.PSObject.Properties.Name -contains 'disposition' -and $json.disposition -eq 'terminal') 'PD012: the v2 result carries the landing disposition field'

'## COH602/ADV030 a closed Tier-3 chain, the reviewed file restored to baseline'
foreach ($variant in 'restored', 'plan-deletion', 'control') {
	$repo = New-Case "coh602-$variant" @{ 'README.md' = "# Readme`n`nOne.`n"; 'Documents/Plans/Area/Done.md' = "# Done`n" }
	git -C $repo switch -q -c session
	$base = (git -C $repo rev-parse main).Trim()
	Write-Text "$repo/README.md" "# Readme`n`nOne, edited.`n"
	$s1 = Start-Round $repo $base 3 @('-Landing')
	"round 1 ($variant): $(@(Get-ReviewRows $s1.Text | ForEach-Object { $_ -replace '; Head .*$', '' }) -join ' || ')"
	[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
	$readme = if ($variant -eq 'control') { "# Readme`n`nOne, fixed.`n" } else { "# Readme`n`nOne.`n" }
	Write-Text "$repo/README.md" $readme
	if ($variant -eq 'plan-deletion') { Remove-Item -LiteralPath "$repo/Documents/Plans/Area/Done.md" }
	git -C $repo add -A; git -C $repo commit -q --allow-empty -m "session $variant"
	$candidate = (git -C $repo rev-parse session).Trim()
	$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '3', '-Landing')
	$g = Invoke-Gate $repo "-EngineBase $base -EngineCandidate $candidate"
	"planner ($variant): $(@($r.Text -split "`n" | Where-Object { $_ -match 'Review rounds|-StartRound' }) -join ' || '); gate: $g"
	if ($variant -eq 'control') {
		$s2 = Start-Round $repo $base 3 @('-Landing')
		"round 2 (control): $(@(Get-ReviewRows $s2.Text | ForEach-Object { $_ -replace '; Head .*$', '' }) -join ' || ')"
		Assert ($r.Text -notmatch 'Review rounds: done' -and @(Get-ReviewRows $s2.Text | Where-Object { $_ -match 'reviewer /adversarial-review' }).Count -gt 0 -and $g -match '^inventory\.review-round-') 'control: a Tier-3 session with a remaining change still owes /adversarial-review'
	}
	else { Assert ($r.Text -match 'Review rounds: done' -and $g -eq 'none') "COH602/ADV030 ($variant): the planner is done and the landing check passes" }
}
