# Round-4 diagnosis cases: one case per accepted round-4 finding group, against the scripts under -Scripts and the
# instruction text under -Docs (default: the worktree root). -Tag names Temp/loop-scenario/c11-<Tag>, which must not
# exist yet. Run from the worktree root. A FAIL line is the red signal; each Assert states the behavior a fix must show.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag, [string] $Docs = '.')
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$Docs = [IO.Path]::GetFullPath($Docs)
$script:Steps = Join-Path $Scripts '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$inventoryCli = Join-Path $Scripts '.agents/scripts/Get-SessionChangeInventory.ps1'
$inventoryModule = Join-Path $Scripts '.agents/scripts/SessionChangeInventory.psm1'
$top = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c11-$Tag"))
if (Test-Path -LiteralPath $top) { throw "scratch path exists: $top" }
New-Item -ItemType Directory -Force $top | Out-Null
"scripts: $Scripts; docs: $Docs"

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
function Start-Round([string] $Repo, [string] $Baseline, [int] $Tier, [string[]] $Extra = @()) {
	$r = Invoke-Steps (@('-RepositoryRoot', $Repo, '-Baseline', $Baseline, '-Tier', "$Tier", '-StartRound') + $Extra)
	return [pscustomobject] @{ Code = $r.Code; Text = $r.Text; Sha = Get-RoundHead $r.Text; Template = [regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value }
}
function Close-Round([string] $Repo, [string] $Sha, [string] $Results) {
	return Invoke-Steps @('-RepositoryRoot', $Repo, '-CloseRound', '-Head', $Sha, '-Results', $Results)
}
function Get-ReviewRows([string] $Text) { return @($Text -split "`n" | Where-Object { $_ -match '; Head ' }) }
function Get-Owed([string] $Text) { return @(Get-ReviewRows $Text | ForEach-Object { [regex]::Match($_, ' (/[a-z-]+)').Groups[1].Value }) -join ',' }
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
$cpp = "// Adds.`nint Add(int a, int b) { return a + b; }`n"
$cppEdited = "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"

'## COH401/VS010 a combined round''s High close owes only the checks that round carried'
# Tier 1, README only, -Landing: round 1 is combined and carries no check. Its High close must not make round 2 carry
# /external-skill-creator (no skill package exists) or /progressive-disclosure-review (no instruction doc changed).
$repo = New-Case 'coh401' @{ 'README.md' = "# Readme`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/README.md" "# Readme`n`nOne, edited.`n"
$s1 = Start-Round $repo $base 1 @('-Landing')
"round 1: $(@(Get-ReviewRows $s1.Text) -join ' || ')"
[void] (Close-Round $repo $s1.Sha (Set-Result $s1.Template @{ '/coherence-review' = 'High' }))
Write-Text "$repo/README.md" "# Readme`n`nOne, fixed.`n"
$s2 = Start-Round $repo $base 1 @('-Landing')
"round 2: $(@(Get-ReviewRows $s2.Text) -join ' || ')"
Assert ($s2.Text -notmatch '/external-skill-creator|/progressive-disclosure-review') 'COH401/VS010: round 2 carries no check round 1 did not carry'
if ($null -ne $s2.Sha) { [void] (Close-Round $repo $s2.Sha ($s2.Template -replace '<severity>', 'none')) }
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '1', '-Landing')
Assert ($r.Text -match 'Review rounds: done') 'the clean round-2 close finishes the loop'

'## COH402/ADV021 Tier 1: a combined retest reads the whole session change for every check it carries'
# Round 1 (code) clears coherence over A.md; round 2 (words) closes progressive disclosure High on A.md; a fix edits
# B.md only. The combined round 3 carries the progressive-disclosure retest; no row may narrow it to B.md.
$repo = New-Case 'adv021' @{ 'a.cpp' = $cpp; '.agents/references/A.md' = "# A`n`nOne.`n"; '.agents/references/B.md' = "# B`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" $cppEdited
Write-Text "$repo/.agents/references/A.md" "# A`n`nOne, edited.`n"
$rows = [Collections.Generic.List[string]]::new()
$s1 = Start-Round $repo $base 1 @('-Landing')
foreach ($row in (Get-ReviewRows $s1.Text)) { $rows.Add("round 1: $($row -replace '; Head .*$', '')") }
[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
$s2 = Start-Round $repo $base 1 @('-Landing')
foreach ($row in (Get-ReviewRows $s2.Text)) { $rows.Add("round 2: $($row -replace '; Head .*$', '')") }
[void] (Close-Round $repo $s2.Sha (Set-Result $s2.Template @{ '/progressive-disclosure-review' = 'High' }))
Write-Text "$repo/.agents/references/B.md" "# B`n`nOne, edited.`n"
$s3 = Start-Round $repo $base 1 @('-Landing')
foreach ($row in (Get-ReviewRows $s3.Text)) { $rows.Add("round 3: $($row -replace '; Head .*$', '')") }
$rows
$combined = @($rows | Where-Object { $_ -match '^round 3: .*/coherence-review combined mode' }) -join ''
Assert ($combined -match 'also carries: .*?/progressive-disclosure-review') 'control: round 3 is combined and carries the /progressive-disclosure-review retest'
Assert (@($rows | Where-Object { $_ -match 'whole files' }).Count -eq 0) 'COH402/ADV021: no row narrows its scope to the files since a since point'
Assert ($combined -match 'combined mode: changed non-C\+\+ artifacts') 'COH402/ADV021: the combined row prints the first-pass scope'

'## ADV022 a close-added High /adversarial-review is retested over the whole session change'
# Tier 2: round 1 adds /adversarial-review=none (session-wide), round 2 adds /adversarial-review=High on a.cpp, a fix
# edits doc.md only. The retest row must read the first-pass scope, which holds a.cpp.
$repo = New-Case 'adv022' @{ 'a.cpp' = $cpp; 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" $cppEdited
$s1 = Start-Round $repo $base 2
[void] (Close-Round $repo $s1.Sha (($s1.Template -replace '<severity>', 'none') + ',/adversarial-review=none'))
$s2 = Start-Round $repo $base 2
"round 2 owes $(Get-Owed $s2.Text)"
[void] (Close-Round $repo $s2.Sha (($s2.Template -replace '<severity>', 'none') + ',/adversarial-review=High'))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s3 = Start-Round $repo $base 2
$adv = @(Get-ReviewRows $s3.Text | Where-Object { $_ -match '/adversarial-review' }) -join ''
"round 3 adversarial row: $($adv -replace '; Head .*$', '')"
Assert ($adv -match 'reviewer /adversarial-review: every changed artifact type') 'ADV022: the retest prints the first-pass scope'

'## COH403 the landing check and the step script agree at the landing candidate'
# Tier 2, -Landing: round 1 reviews a comment-only a.cpp edit and a README edit and closes clean with /comment-review
# withheld; restoring the comment leaves nothing owed. The step script reports done and landing must pass.
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
"step script done: $done; landing: status=$($j.status) code=$($j.code) message=$($j.message)"
Assert ($done) 'control: the step script reports done at the working tree'
Assert ($done -eq ($j.status -eq 'pass')) 'COH403: landing gives the step script''s verdict'

'## COH404/VS011/PD009/ADV023 comment-review step 1 keeps a present Coretide source and drops a deletion'
# A Coretide x.cpp edit and an engine b.cpp deletion; step 1's file selection is applied as the worker text states it.
$repo = New-Case 'coh404' @{ 'b.cpp' = $cpp; 'doc.md' = "# Doc`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
$bare = Join-Path $top 'coh404-coretide.git'
git init -q --bare -b main $bare
$seed = "$bare-seed"
git clone -q $bare $seed 2>$null
git -C $seed config user.name scratch; git -C $seed config user.email scratch@example.invalid
Write-Text "$seed/x.cpp" $cpp
git -C $seed add -A; git -C $seed commit -q -m seed; git -C $seed push -q origin main 2>$null
$clone = "$repo/Projects/Coretide"
git clone -q $bare $clone 2>$null
git -C $clone config user.name scratch; git -C $clone config user.email scratch@example.invalid
Write-Text "$clone/x.cpp" $cppEdited
Remove-Item -LiteralPath "$repo/b.cpp"
$s1 = Start-Round $repo $base 2
$head = $s1.Sha
$entries = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; (Get-SessionChangeInventory -RepositoryRoot '$repo' -Baseline $base -Head $head).entries | ForEach-Object { `"`$(`$_.status) `$(`$_.class) `$(`$_.path)`" }"
"inventory at Head: $($entries -join ' | ')"
$worker = Get-Content -Raw (Join-Path $Docs '.agents/skills/comment-review/references/worker.md')
$prescribesHeadLookup = $worker -match 'git cat-file -e <Head>:<path>'
$kept = @(foreach ($line in $entries) {
	$status, $class, $path = $line -split ' ', 3
	if ($class -cnotin @('cpp', 'dual-language-header', 'glsl')) { continue }
	if ($prescribesHeadLookup) {
		git -C $repo cat-file -e "${head}:$path" 2>$null
		if ($LASTEXITCODE -eq 0) { $path }
	}
	elseif ($status -cne 'D') { $path }
})
"worker prescribes the Head lookup: $prescribesHeadLookup; step 1 keeps: $($kept -join ', ')"
Assert ($kept -ccontains 'Projects/Coretide/x.cpp') 'COH404/VS011/PD009/ADV023: the present Coretide source survives step 1'
Assert ($kept -cnotcontains 'b.cpp') 'the deleted engine source is not handed to the scanner'

'## PD010 the landing result states that recovery may proceed'
# Inspection: the review-round refusal is thrown with the recovery disposition, and the worker gates recovery on the
# result's disposition and lock.released instead of a list of codes.
$landingScript = Get-Content -Raw (Join-Path $Docs '.agents/skills/finalize-changes/scripts/Invoke-FinalizeLanding.ps1')
$finalizeWorker = Get-Content -Raw (Join-Path $Docs '.agents/skills/finalize-changes/references/worker.md')
$coverageThrow = [regex]::Match($landingScript, 'Throw-Landing 2 \$coverage\.Code \$coverage\.Message[^\r\n]*').Value
"coverage refusal: $coverageThrow"
Assert ($coverageThrow -match "'recovery'") 'PD010: the review-round refusal carries the recovery disposition'
Assert ($finalizeWorker -notmatch 'Gate dispatch on those') 'PD010: the worker no longer computes recovery from a code list'
