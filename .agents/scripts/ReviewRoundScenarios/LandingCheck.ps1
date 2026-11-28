# The landing check: the -Landing inventory, Test-ReviewRoundCoverage, and Invoke-FinalizeCoretide.ps1 against closed
# review rounds, and their agreement with the step script. Each headed case creates its own scratch repositories.
param([Parameter(Mandatory)][string] $ScratchRoot)
. (Join-Path $PSScriptRoot 'ScenarioCommon.ps1')
$coretideScript = Join-Path $script:Wt '.agents/skills/finalize-changes/scripts/Invoke-FinalizeCoretide.ps1'

# The round planner's verdict at a commit, as landing would need it: Get-ReviewRoundState with -Head and -Landing.
function Get-PlannerAt([string] $Repo, [string] $Base, [string] $Head) {
	$text = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); `$s = Get-ReviewRoundState -RepositoryRoot $(ConvertTo-Literal $Repo) -Baseline $Base -Head $Head -Landing; `"done=`$(`$s.Plan.Done) phase=`$(`$s.Plan.Phase) keys=`$(@(`$s.Plan.Keys) -join ',') failure=`$(`$s.Failure)`""
	return "$text".Trim()
}
function Invoke-Gate([string] $Repo, [string] $Arguments) {
	$out = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); `$r = Test-ReviewRoundCoverage -RepositoryRoot $(ConvertTo-Literal $Repo) $Arguments; if (`$null -eq `$r) { 'none' } else { `"`$(`$r.Code) `$(`$r.Message)`" }" 2>&1
	return "$out".Trim()
}
# A Coretide clone at Repo/Projects/Coretide of a bare repository seeded with Files.
function New-CoretideClone([string] $Repo, [string] $Name, [hashtable] $Files) {
	$bare = Join-Path $ScratchRoot "$Name-coretide.git"
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
function Close-Clean([string] $Repo, [string] $Text) {
	$sha = Get-RoundHead $Text
	$template = [regex]::Match($Text, "-Results '([^']*)'").Groups[1].Value
	return Invoke-Steps @('-RepositoryRoot', $Repo, '-CloseRound', '-Head', $sha, '-Results', ($template -replace '<severity>', 'none'))
}

'# Landing staleness versus exempt paths'

'## ADV001 a reviewed Plan rename cannot land as a copy'
$repo = New-Case 'adv001' @{ 'Documents/Plans/Area/One.md' = "# One`n`nA plan body that is long enough to pair as a rename.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
git -C $repo mv Documents/Plans/Area/One.md Documents/Plans/Area/Two.md
$s = Start-Round $repo $base 2 @('-Landing')
"round owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $repo commit -q -m rename
$j = Invoke-Landing $repo $base
Assert ($j.status -eq 'pass') 'control: the reviewed rename lands'
git -C $repo checkout -q $base -- Documents/Plans/Area/One.md
git -C $repo commit -q -m restore
$j = Invoke-Landing $repo $base
Assert ($j.code -eq 'inventory.review-round-unfinished') 'restoring the rename source is refused as an owed round'

'## ADV010 a reviewed Plan copy lands as a rename'
$repo = New-Case 'adv010' @{ 'Documents/Plans/Area/One.md' = "# One`n`nA plan body that is long enough to pair as a rename.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Copy-Item -LiteralPath "$repo/Documents/Plans/Area/One.md" "$repo/Documents/Plans/Area/Two.md"
$s = Start-Round $repo $base 2 @('-Landing')
"round 1 owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $repo add -A; git -C $repo commit -q -m copy
git -C $repo rm -q Documents/Plans/Area/One.md; git -C $repo commit -q -m 'delete source'
$candidate = (git -C $repo rev-parse HEAD).Trim()
"candidate rows from base: $((git -C $repo diff --name-status -M $base $candidate) -join ' | ')"
"snapshot-to-candidate rows: $((git -C $repo diff --name-status -M $s.Sha $candidate) -join ' | ')"
$j = Invoke-Landing $repo $base
"planner at candidate: $(Get-PlannerAt $repo $base $candidate)"
# ADV010 was rejected: the lone exempt Plan deletion after the reviewed copy lands, the verdict the planner gives too.
Assert ($j.status -eq 'pass') 'ADV010 rejected, expected difference: the gate passes the exempt Plan deletion after the reviewed copy'

'## COH203 a binary Plan support file deleted after the round'
$repo = New-Case 'coh203' @{ 'doc.md' = "# Doc`n`nOne.`n" } @{ 'Documents/Plans/Area/Name/image.bin' = [byte[]] @(0, 1, 2, 0, 255, 0, 7, 0) }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base 2 @('-Landing')
"round 1 owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $repo rm -q Documents/Plans/Area/Name/image.bin
git -C $repo add -A; git -C $repo commit -q -m session
$candidate = (git -C $repo rev-parse HEAD).Trim()
$j = Invoke-Landing $repo $base
$planner = Get-PlannerAt $repo $base $candidate
"planner at candidate: $planner"
Assert ($j.status -ne 'pass' -and $j.code -like 'inventory.review-round-*') 'COH203: the landing gate refuses the unreviewed binary deletion'
Assert (($j.status -eq 'pass' -or $j.code -like 'inventory.review-round-*') -and $planner -match '^done=(True|False) phase=\S' -and $planner -match 'failure=$' -and (($j.status -eq 'pass') -eq ($planner -match '^done=True'))) 'the gate and the planner at the candidate agree'

'## COH03 a reviewed Plan deleted after its round is not stale'
$repo = New-Case 'coh03' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
New-Item -ItemType Directory -Force "$repo/Documents/Plans/Area" | Out-Null
Write-Text "$repo/Documents/Plans/Area/Claimed.md" "# Claimed`n"
$s = Start-Round $repo $base 2 @('-Landing')
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
Remove-Item -LiteralPath "$repo/Documents/Plans/Area/Claimed.md"
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2', '-Landing')
$done = $r.Text -match 'Review rounds: done'
git -C $repo add -A
git -C $repo commit -q -m session
$j = Invoke-Landing $repo $base
"planner done: $done"
Assert ($done -and $j.status -eq 'pass') 'the planner reports done and the landing gate passes'

'## control: a Plan added, reviewed, then deleted lands'
$repo = New-Case 'ctl-coh03' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
New-Item -ItemType Directory -Force "$repo/Documents/Plans/Area" | Out-Null
Write-Text "$repo/Documents/Plans/Area/Claimed.md" "# Claimed`n"
$s = Start-Round $repo $base 2 @('-Landing')
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
Remove-Item -LiteralPath "$repo/Documents/Plans/Area/Claimed.md"
git -C $repo add -A; git -C $repo commit -q -m session
$j = Invoke-Landing $repo $base
Assert ($j.status -eq 'pass') 'the deleted reviewed Plan lands'

'## control: a rebase over a primary change to another file lands; one to a reviewed file is stale'
foreach ($variant in @('other', 'same')) {
	$repo = New-Case "ctl-rebase-$variant" @{ 'doc.md' = "# Doc`n`nOne.`n`nTwo.`n`nThree.`n`nFour.`n"; 'other.md' = "# Other`n" }
	$base = (git -C $repo rev-parse HEAD).Trim()
	git -C $repo switch -q -c session
	Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n`nTwo.`n`nThree.`n`nFour.`n"
	$s = Start-Round $repo $base 2 @('-Landing')
	[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
	git -C $repo commit -q -am session
	git -C $repo switch -q main
	if ($variant -eq 'other') { Write-Text "$repo/other.md" "# Other`n`nPrimary edit.`n" } else { Write-Text "$repo/doc.md" "# Doc`n`nOne.`n`nTwo.`n`nThree.`n`nFour, primary edit.`n" }
	git -C $repo commit -q -am primary
	$primary = (git -C $repo rev-parse HEAD).Trim()
	git -C $repo switch -q session
	git -C $repo rebase -q main
	$j = Invoke-Landing $repo $primary
	if ($variant -eq 'other') { Assert ($j.status -eq 'pass') 'a primary change to a file the round did not review lands' }
	else { Assert ($j.code -eq 'inventory.review-round-unfinished') 'a primary edit to another hunk of a reviewed file is refused as an owed round' }
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
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $newBase, '-Tier', '2', '-Landing')
Assert ($j.status -eq 'pass' -and $r.Text -match 'Review rounds: done') 'control: a clean rebase of the unchanged patch over a primary edit to another file owes nothing and lands'
Write-Text "$repo/other.md" "# Other`n`nOne.`n"
git -C $repo commit -q -am 'restore other.md'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $newBase, '-Tier', '2', '-Landing')
$j = Invoke-Landing $repo $newBase
$s2 = Start-Round $repo $newBase 2 @('-Landing')
"round 2: $(@(Get-ReviewRows $s2.Text) -join ' || ')"
Assert ($r.Text -match '-StartRound' -and $r.Text -notmatch 'Review rounds: done' -and (Get-ReviewRows $s2.Text) -match '/coherence-review') 'ADV024: the step script owes /coherence-review for the new session path'
Assert ($j.code -eq 'inventory.review-round-unfinished') 'ADV024: landing refuses the unreviewed session path'

'## COH403 the landing check and the step script agree at the landing candidate'
# Tier 2, -Landing: round 1 reviews a comment-only a.cpp edit and a README edit and closes clean with /comment-review
# withheld; restoring the comment leaves nothing owed. The step script reports done and landing must pass.
$cpp = "// Adds.`nint Add(int a, int b) { return a + b; }`n"
$repo = New-Case 'coh403' @{ 'a.cpp' = $cpp; 'README.md' = "# Readme`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/a.cpp" "// Adds two integers.`nint Add(int a, int b) { return a + b; }`n"
Write-Text "$repo/README.md" "# Readme`n`nOne, edited.`n"
$s1 = Start-Round $repo $base 2 @('-Landing')
"round 1 owes $(Get-Owed $s1.Text)"
[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
Write-Text "$repo/a.cpp" $cpp
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2', '-Landing')
$done = $r.Text -match 'Review rounds: done'
git -C $repo add -A; git -C $repo commit -q -m session
$j = Invoke-Landing $repo $base
"step script done: $done"
Assert ($done) 'control: the step script reports done at the working tree'
Assert ($done -eq ($j.status -eq 'pass')) 'COH403: landing gives the step script''s verdict'

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

'## a Tier-3 chain whose last round recorded Tier 2'
# The Get-ChangeWorkflowSteps.ps1 header says the landing check evaluates at the tier the last non-sweep round
# recorded, so a tier raised after the last round takes effect there at the next -StartRound.
$repo = Join-Path $ScratchRoot 'tier-raised'
New-ScratchRepo $repo
Write-Text "$repo/.gitignore" "Temp/`n"
Write-Text "$repo/doc.md" "# Doc`n`nOne.`n"
git -C $repo add -A; git -C $repo commit -q -m baseline
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s1 = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2', '-StartRound', '-Landing')
Show 'round 1 at -Tier 2' $s1
"round 1 message Tier line: $((git -C $repo log -1 --format=%B (Get-RoundHead $s1.Text) | Select-String '^Tier:') -join '')"
Show 'close round 1' (Close-Clean $repo $s1.Text)
git -C $repo commit -q -am session
# The change is now classified Tier 3; no round has recorded it yet.
$j = Invoke-Landing $repo $base
Assert ($j.status -eq 'pass') 'the landing check evaluates at the recorded Tier 2 and passes'
$d = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '3', '-Landing')
Show 'default -Tier 3 run' $d
Assert ($d.Text -match '-StartRound' -and $d.Text -notmatch 'Review rounds: done') 'a default -Tier 3 run owes a round (/adversarial-review) the landing check does not yet see'
$s2 = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '3', '-StartRound', '-Landing')
Show 'round 2 at -Tier 3' $s2
$tier2 = "$((git -C $repo log -1 --format=%B (Get-RoundHead $s2.Text) | Select-String '^Tier:') -join '')"
"round 2 message Tier line: $tier2"
Assert ($s2.Text -match 'reviewer /adversarial-review:' -and $tier2 -ceq 'Tier: 3') 'the next -StartRound prints /adversarial-review and records Tier 3'
Show 'close round 2' (Close-Clean $repo $s2.Text)
$j = Invoke-Landing $repo $base
Assert ($j.status -eq 'pass') 'control: the closed Tier-3 round lands'
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited again.`n"
git -C $repo commit -q -am again
$j = Invoke-Landing $repo $base
Assert ($j.code -eq 'inventory.review-round-unfinished' -and $j.message -match '/adversarial-review') 'after round 2 the landing check owes /adversarial-review: the raised tier took effect at that -StartRound'

'# The Coretide side'

'## ADV002 a Coretide rebase onto an upstream AGENTS.md edit'
$repo = New-Case 'adv002' @{ 'doc.md' = "# Doc`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
$bare = Join-Path $ScratchRoot 'adv002-coretide.git'
git init -q --bare -b main $bare
$seed = "$bare-seed"
git clone -q $bare $seed 2>$null
git -C $seed config user.name scratch; git -C $seed config user.email scratch@example.invalid
Write-Text "$seed/notes.md" "# Notes`n"
Write-Text "$seed/AGENTS.md" "# Coretide`n`nOne rule.`n"
git -C $seed add -A; git -C $seed commit -q -m seed; git -C $seed push -q origin main 2>$null
$clone = "$repo/Projects/Coretide"
git clone -q $bare $clone 2>$null
git -C $clone config user.name scratch; git -C $clone config user.email scratch@example.invalid
Write-Text "$clone/notes.md" "# Notes`n`nEdited in the session.`n"
$s = Start-Round $repo $base 2 @('-Landing')
"round owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $clone commit -q -am 'session notes'
Write-Text "$seed/AGENTS.md" "# Coretide`n`nOne rule, upstream edit.`n"
git -C $seed commit -q -am upstream; git -C $seed push -q origin main 2>$null
git -C $clone fetch -q origin
git -C $clone rebase -q origin/main
$candidate = (git -C $clone rev-parse HEAD).Trim()
$a = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); `$r = Test-ReviewRoundCoverage -RepositoryRoot $(ConvertTo-Literal $repo) -CoretideCandidate $candidate; if (`$null -eq `$r) { 'none' } else { `"`$(`$r.Code) `$(`$r.Message)`" }"
"gate: $a"
Assert ("$a".Trim() -eq 'none') 'the rebased clone commit of the reviewed bytes passes'

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
# Invoke-FinalizeLanding.ps1 Assert-LandingReviewRoundCoverage gives review-round exactly to an inventory.review-round-*
# code.
Assert ($g -match 'could not be evaluated') 'control: the gate reports an evaluation failure'
Assert (-not $code.StartsWith('inventory.review-round-')) "PD011: the evaluation failure ($code) maps to the terminal disposition, not review-round"

'## PD012 the Coretide result states a disposition'
$repo = New-Case 'pd012' @{ 'doc.md' = "# Doc`n" }
$clone = New-CoretideClone $repo 'pd012' @{ 'notes.md' = "# Notes`n" }
$message = Join-Path $ScratchRoot 'pd012-message.txt'
Write-Text $message "Coretide change`n"
$out = pwsh -NoProfile -NonInteractive -File $coretideScript -Prepare -CurrentWorktree $repo -CommitMessageFile $message 2>&1
"prepare result: $out"
$json = "$out" | ConvertFrom-Json
Assert ($json.schemaVersion -eq 'broken-engine-finalize-coretide/v3' -and $json.PSObject.Properties.Name -contains 'disposition' -and $json.disposition -eq 'terminal') 'PD012: the v3 result carries the landing disposition field'
