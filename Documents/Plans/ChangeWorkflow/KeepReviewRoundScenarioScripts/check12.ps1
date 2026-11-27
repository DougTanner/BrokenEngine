# Round-4 fix added case: a Tier-3 change whose last round recorded Tier 2. The Get-ChangeWorkflowSteps.ps1 header
# says the landing check evaluates at the tier the last non-sweep round recorded, so a tier raised after the last
# round takes effect there at the next -StartRound. -Scripts and -Tag as in check11.ps1. Run from the worktree root.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$script:Steps = Join-Path $Scripts '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$inventoryCli = Join-Path $Scripts '.agents/scripts/Get-SessionChangeInventory.ps1'
$top = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c12-$Tag"))
if (Test-Path -LiteralPath $top) { throw "scratch path exists: $top" }
New-Item -ItemType Directory -Force $top | Out-Null
"scripts: $Scripts"
function Invoke-Landing([string] $Repo, [string] $Base) {
	$json = Join-Path $top 'landing.json'
	& pwsh -NoProfile -NonInteractive -File $inventoryCli -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $json 2>&1 | Out-Null
	return Get-Content -Raw $json | ConvertFrom-Json
}
function Close-Clean([string] $Repo, [string] $Text) {
	$sha = Get-RoundHead $Text
	$template = [regex]::Match($Text, "-Results '([^']*)'").Groups[1].Value
	return Invoke-Steps @('-RepositoryRoot', $Repo, '-CloseRound', '-Head', $sha, '-Results', ($template -replace '<severity>', 'none'))
}

'## a Tier-3 chain whose last round recorded Tier 2'
$repo = Join-Path $top 'tier-raised'
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
"landing after the tier rise: status=$($j.status) code=$($j.code) message=$($j.message)"
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
"landing after round 2: status=$($j.status) code=$($j.code)"
Assert ($j.status -eq 'pass') 'control: the closed Tier-3 round lands'
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited again.`n"
git -C $repo commit -q -am again
$j = Invoke-Landing $repo $base
"landing after a later edit: status=$($j.status) code=$($j.code) message=$($j.message)"
Assert ($j.code -eq 'inventory.review-round-unfinished' -and $j.message -match '/adversarial-review') 'after round 2 the landing check owes /adversarial-review: the raised tier took effect at that -StartRound'
