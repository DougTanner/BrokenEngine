# check7p.ps1 plus the VS014 recovery case, with -Scripts (the tree whose .agents/scripts and .agents/skills/sweep are
# copied) and -Tag (scratch Temp/loop-scenario/c7r-<Tag>). Run from the worktree root.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c7r-$Tag"))
New-ScratchRepo $repo
Write-Text "$repo/.gitignore" "Temp/`n"
Write-Text "$repo/doc.md" "# Doc`n`nOne paragraph.`n`nA.`n`nB.`n`nC.`n`nD.`n`nTwo paragraph.`n"
New-Item -ItemType Directory -Force "$repo/.agents/skills" | Out-Null
Copy-Item -Recurse -LiteralPath (Join-Path $Scripts '.agents/scripts') -Destination "$repo/.agents/scripts"
Copy-Item -Recurse -LiteralPath (Join-Path $Scripts '.agents/skills/sweep') -Destination "$repo/.agents/skills/sweep"
New-Item -ItemType Directory -Force "$repo/Documents/Investigations/ChangeWorkflow" | Out-Null
Write-Text "$repo/Documents/Investigations/ChangeWorkflow/Sweep.md" "# Sweep`n`n## Sweep`n`n- Type: ``document```n- Targets: ``doc.md```n"
git -C $repo add -A
git -C $repo commit -q -m baseline
$main = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne paragraph, swept.`n`nA.`n`nB.`n`nC.`n`nD.`n`nTwo paragraph.`n"
$spec = 'Documents/Investigations/ChangeWorkflow/Sweep.md'
$script:Steps = Join-Path $repo '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$log = Join-Path $PSScriptRoot "check7r-$Tag-sweep.txt"
function Invoke-SweepRound([string[]] $Mode = @('-ReviewRound')) {
	$p = Start-Process pwsh -WorkingDirectory $repo -Wait -PassThru -NoNewWindow -RedirectStandardOutput $log -RedirectStandardError "$log.err" -ArgumentList (@('-NoProfile', '-NonInteractive', '-File', '.agents/skills/sweep/scripts/Invoke-Sweep.ps1', '-Spec', $spec) + $Mode)
	"sweep $($Mode -join ' ') exit $($p.ExitCode): $((Get-Content -Raw $log)) $((Get-Content -Raw "$log.err"))" | Out-Host
	return $p.ExitCode
}
$landingJson = "Temp/loop-scenario/landing7r-$Tag.json"
function Invoke-Landing([string] $Base = $main) {
	& pwsh -NoProfile -NonInteractive -File (Join-Path $repo '.agents/scripts/Get-SessionChangeInventory.ps1') -RepositoryRoot $repo -Baseline $Base -Head session -Landing -OutputPath $landingJson 2>&1 | Out-Null
	$json = Get-Content -Raw $landingJson | ConvertFrom-Json
	"landing: status=$($json.status) code=$($json.code) message=$($json.message)" | Out-Host
	return $json
}

$code = Invoke-SweepRound
Assert ($code -ne 0 -and $null -eq (Get-RefTip $repo)) 'no Temp/Sweep: exits non-zero and writes no ref'
git -C $repo switch -q -c session
git -C $repo commit -q -am swept
$j = Invoke-Landing
Assert ($j.code -eq 'inventory.review-round-missing') 'the bare committed edit: missing'
git -C $repo reset -q HEAD~1
New-Item -ItemType Directory -Force "$repo/Temp/Sweep" | Out-Null
Write-Text "$repo/Temp/Sweep/Baseline.txt" "$main`n$spec`n"
Write-Text "$repo/Temp/Sweep/Status.txt" "RUNNING close $(Get-Date -Format s)`n"
$code = Invoke-SweepRound
Assert ($code -ne 0 -and $null -eq (Get-RefTip $repo)) 'Status RUNNING close: exits non-zero and writes no ref'
Write-Text "$repo/Temp/Sweep/Status.txt" "CLOSE-DONE units=1 applied=1 deferred=0 ordering=0 cpp=no builds=none profile=no`n"
$code = Invoke-SweepRound
$tip = Get-RefTip $repo
$tipMsg = (git -C $repo log -1 --format=%B $tip | Out-String)
$parentMsg = (git -C $repo log -1 --format=%B "$tip^" | Out-String)
"tip message:`n$tipMsg`nparent message:`n$parentMsg"
Assert ($code -eq 0 -and (Get-Content -Raw $log) -match 'round 1 closed: done') 'CLOSE-DONE: exits 0 and prints round 1 closed: done'
Assert ($tipMsg -match '(?m)^Next: done' -and $tipMsg -notmatch '(?m)^Result:' -and $parentMsg -match '(?m)^Phase: sweep' -and $parentMsg -notmatch '(?m)^Owes:') 'tip Next: done, no Result; parent Phase: sweep, no Owes'
git -C $repo commit -q -am swept
$j = Invoke-Landing
Assert ($j.status -eq 'pass') 'committed sweep edit after its round: pass'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $main, '-Tier', '2', '-SweepRound')
Show 'step script with -SweepRound' $r
Assert ($r.Code -ne 0 -and $r.Text -match 'parameter') 'the step script rejects -SweepRound at parameter binding'

'## VS014 a rebase over a primary edit to another region of a swept file, then a second sweep round'
git -C $repo switch -q main
Write-Text "$repo/doc.md" "# Doc`n`nOne paragraph.`n`nA.`n`nB.`n`nC.`n`nD.`n`nTwo paragraph, primary edit.`n"
git -C $repo commit -q -am primary
git -C $repo switch -q session
git -C $repo rebase -q main
$newMain = (git -C $repo rev-parse main).Trim()
"rebased doc.md: $((Get-Content -Raw "$repo/doc.md") -replace "`n", ' | ')"
$j = Invoke-Landing $newMain
Assert ($j.code -eq 'inventory.review-round-unfinished') 'VS014 red precondition: landing refuses the rebased candidate'
$code = Invoke-SweepRound
Assert ($code -eq 0 -and (Get-Content -Raw $log) -match 'round 2 closed: done') 'VS014: -ReviewRound freezes a second sweep round on the recovered branch'
$j = Invoke-Landing $newMain
Assert ($j.status -eq 'pass') 'VS014: the landing inventory passes after the second sweep round'
$code = Invoke-SweepRound @('-Model', 'none', '-Batch', 'doc.md')
Assert ($code -ne 0 -and (Get-Content -Raw "$log.err") -match 'belongs to commit') 'control: -Batch still refuses a HEAD other than the sweep baseline'
[void] (Invoke-SweepRound @('-Model', 'none', '-Close'))
$status = Get-Content -Raw "$repo/Temp/Sweep/Status.txt"
"Status.txt after -Close: $status"
Assert ($status -match '^CLOSE-FAILED Baseline .*belongs to commit') 'control: -Close still refuses a HEAD other than the sweep baseline'
git -C $repo switch -q --detach $main
Write-Text "$repo/Temp/Sweep/Status.txt" "CLOSE-DONE units=1 applied=1 deferred=0 ordering=0 cpp=no builds=none profile=no`n"
Write-Text "$repo/Temp/Sweep/Baseline.txt" "$newMain`n$spec`n"
$code = Invoke-SweepRound
Assert ($code -ne 0 -and (Get-Content -Raw "$log.err") -match 'belongs to commit') 'control: -ReviewRound refuses a HEAD that does not descend from the sweep baseline'
