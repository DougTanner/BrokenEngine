# Acceptance check 4: the landing gate, continuing in check 3's repository (loop done with -Landing).
. (Join-Path $PSScriptRoot 'lib.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'repo'))
$baseline = (git -C $repo rev-list --max-parents=0 HEAD).Trim()
$landingJson = 'Temp/loop-scenario/landing.json'

function Invoke-Landing([string] $Repo, [string] $Base, [string[]] $Extra = @()) {
	$out = & pwsh -NoProfile -NonInteractive -File .agents/scripts/Get-SessionChangeInventory.ps1 -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $landingJson @Extra 2>&1
	$json = Get-Content -Raw $landingJson | ConvertFrom-Json
	"landing: exit $LASTEXITCODE status=$($json.status) code=$($json.code) message=$($json.message)" | Out-Host
	return $json
}

git -C $repo switch -q -c session
git -C $repo add -A
git -C $repo commit -q -m session
$passing = (git -C $repo rev-parse HEAD).Trim()
$main = (git -C $repo rev-parse main).Trim()
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'pass') 'committed session after the loop: pass'

git -C $repo switch -q main
Write-Text "$repo/other.md" "# Other`n"
git -C $repo add other.md
git -C $repo commit -q -m other
$main = (git -C $repo rev-parse main).Trim()
git -C $repo switch -q session
git -C $repo rebase -q main
$passing = (git -C $repo rev-parse HEAD).Trim()
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'pass') 'rebase over an unrelated primary change: pass'

$text = [IO.File]::ReadAllText("$repo/doc.md")
Write-Text "$repo/doc.md" ($text + "`nLate paragraph.`n")
git -C $repo commit -q -am late
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-unfinished') 'one more doc.md edit: an owed round'
git -C $repo reset -q --hard $passing
git -C $repo checkout -q $baseline -- doc.md
"doc.md vs baseline after restore: '$(git -C $repo diff --stat $baseline -- doc.md)'"
git -C $repo commit -q -am restore
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-unfinished') 'doc.md restored to baseline bytes: an owed round'
git -C $repo reset -q --hard $passing

git -C $repo switch -q main
$text = [IO.File]::ReadAllText("$repo/doc.md")
Write-Text "$repo/doc.md" $text.Replace('# Doc', '# Document')
git -C $repo commit -q -am heading
$main = (git -C $repo rev-parse main).Trim()
git -C $repo switch -q session
git -C $repo rebase -q main
"rebase exit $LASTEXITCODE"
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-unfinished') 'clean same-patch rebase over a primary edit to doc.md: an owed round'

$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $main, '-Tier', '2', '-Landing', '-StartRound')
Show 'start round without closing' $r
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-unfinished') 'open round: unfinished'

git -C $repo update-ref refs/worktree/review-rounds HEAD
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-missing') 'ref at HEAD: missing'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $main, '-Tier', '2', '-Landing')
Show 'default run with the ref at HEAD' $r
Assert ($r.Text -match 'main -StartRound' -and $r.Text -notmatch 'Review rounds:') 'a default run prints a first round -StartRound row'

'## second repository'
$repo2 = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'repo2'))
New-ScratchRepo $repo2
New-Item -ItemType Directory -Force "$repo2/Documents/Investigations/ChangeWorkflow" | Out-Null
Write-Text "$repo2/.gitignore" "Temp/`n"
Write-Text "$repo2/doc.md" "# Doc`n"
Write-Text "$repo2/Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md" "# Incidents`n"
git -C $repo2 add -A
git -C $repo2 commit -q -m baseline
$main2 = (git -C $repo2 rev-parse HEAD).Trim()
git -C $repo2 switch -q -c session
Write-Text "$repo2/doc.md" "# Doc edited`n"
git -C $repo2 commit -q -am doc
$j = Invoke-Landing $repo2 $main2
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-missing') 'no ref, doc.md edit: missing'
git -C $repo2 reset -q --hard $main2
Write-Text "$repo2/Documents/Investigations/ChangeWorkflow/WorkflowIncidents.md" "# Incidents`n`n| row |`n"
git -C $repo2 commit -q -am incidents
$j = Invoke-Landing $repo2 $main2
Assert ($j.status -eq 'pass') 'no ref, WorkflowIncidents.md modification only: pass'
git -C $repo2 reset -q --hard $main2
New-Item -ItemType Directory -Force "$repo2/Documents/Plans/ChangeWorkflow" | Out-Null
Write-Text "$repo2/Documents/Plans/ChangeWorkflow/Saved.md" "# Saved`n"
git -C $repo2 add -A
git -C $repo2 commit -q -m saved
$j = Invoke-Landing $repo2 $main2 @('-SavePlanPaths', 'Documents/Plans/ChangeWorkflow/Saved.md')
Assert ($j.status -eq 'pass') 'no ref, new saved Plan with -SavePlanPaths: pass'
$j = Invoke-Landing $repo2 $main2
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-missing') 'control: the same saved Plan without -SavePlanPaths: missing'
