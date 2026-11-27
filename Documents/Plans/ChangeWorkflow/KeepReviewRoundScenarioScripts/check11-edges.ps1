# Edge probes for the proposed landing rule (Temp/loop/r4/temp-diagnosis.md ## Landing check): the step script and
# the gate at the candidate, for (a) the COH403 restore at Tier 3 and (b) a file restored to the bytes an earlier
# passing round reviewed. -Scripts and -Tag as in check11.ps1; prints observed verdicts, no assertions.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$script:Steps = Join-Path $Scripts '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$inventoryCli = Join-Path $Scripts '.agents/scripts/Get-SessionChangeInventory.ps1'
$top = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c11e-$Tag"))
if (Test-Path -LiteralPath $top) { throw "scratch path exists: $top" }
New-Item -ItemType Directory -Force $top | Out-Null
function New-Case([string] $Name, [hashtable] $Files) {
	$repo = Join-Path $top $Name
	New-ScratchRepo $repo
	Write-Text "$repo/.gitignore" "Temp/`nProjects/Coretide/`n"
	foreach ($path in $Files.Keys) { $full = Join-Path $repo $path; New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null; Write-Text $full $Files[$path] }
	git -C $repo add -A; git -C $repo commit -q -m baseline
	return $repo
}
function Start-Round([string] $Repo, [string] $Baseline, [int] $Tier) {
	$r = Invoke-Steps @('-RepositoryRoot', $Repo, '-Baseline', $Baseline, '-Tier', "$Tier", '-StartRound', '-Landing')
	return [pscustomobject] @{ Text = $r.Text; Sha = Get-RoundHead $r.Text; Template = [regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value }
}
function Get-Verdicts([string] $Repo, [string] $Base, [int] $Tier) {
	$r = Invoke-Steps @('-RepositoryRoot', $Repo, '-Baseline', $Base, '-Tier', "$Tier", '-Landing')
	$owed = @($r.Text -split "`n" | Where-Object { $_ -match 'Review rounds: done|-StartRound' } | Select-Object -First 1) -join ''
	git -C $Repo add -A; git -C $Repo commit -q -m candidate
	$json = Join-Path $top 'landing.json'
	& pwsh -NoProfile -NonInteractive -File $inventoryCli -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $json 2>&1 | Out-Null
	$j = Get-Content -Raw $json | ConvertFrom-Json
	return "step script: $(if ($owed -match 'done') { 'done' } else { 'round owed' }); gate: $($j.status) $($j.code) $($j.message)"
}
$cpp = "// Adds.`nint Add(int a, int b) { return a + b; }`n"

'## (a) COH403 restore at Tier 3'
$repo = New-Case 'tier3' @{ 'a.cpp' = $cpp; 'README.md' = "# Readme`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/a.cpp" "// Adds two integers.`nint Add(int a, int b) { return a + b; }`n"
Write-Text "$repo/README.md" "# Readme`n`nEdited.`n"
$s = Start-Round $repo $base 3
"round 1 template: $($s.Template)"
[void] (Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $s.Sha, '-Results', ($s.Template -replace '<severity>', 'none')))
Write-Text "$repo/a.cpp" $cpp
Get-Verdicts $repo $base 3

'## (b) control: doc.md restored to the bytes round 1 reviewed after round 2 reviewed other bytes (both must refuse)'
$repo = New-Case 'revert' @{ 'doc.md' = "# Doc`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/doc.md" "# Doc`n`nVersion one.`n"
$s = Start-Round $repo $base 2
[void] (Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $s.Sha, '-Results', ($s.Template -replace '<severity>', 'none')))
Write-Text "$repo/doc.md" "# Doc`n`nVersion two.`n"
$s = Start-Round $repo $base 2
[void] (Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $s.Sha, '-Results', ($s.Template -replace '<severity>', 'none')))
Write-Text "$repo/doc.md" "# Doc`n`nVersion one.`n"
Get-Verdicts $repo $base 2
