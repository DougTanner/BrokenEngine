# Round-3 non-convergence diagnosis cases: one reproducing case per class the round-3 Criticals fall in, against the
# scripts under -Scripts. -Tag names Temp/loop-scenario/c10-<Tag>, which must not exist yet. Run from the worktree root.
# A FAIL line is the red signal.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$script:Steps = Join-Path $Scripts '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$top = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c10-$Tag"))
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
function Start-Round([string] $Repo, [string] $Baseline, [int] $Tier, [string[]] $Extra = @()) {
	$r = Invoke-Steps (@('-RepositoryRoot', $Repo, '-Baseline', $Baseline, '-Tier', "$Tier", '-StartRound') + $Extra)
	return [pscustomobject] @{ Code = $r.Code; Text = $r.Text; Sha = Get-RoundHead $r.Text; Template = [regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value }
}
function Close-Round([string] $Repo, [string] $Sha, [string] $Results) {
	return Invoke-Steps @('-RepositoryRoot', $Repo, '-CloseRound', '-Head', $Sha, '-Results', $Results)
}
function Get-ReviewRows([string] $Text) { return @($Text -split "`n" | Where-Object { $_ -match '; Head ' }) }
function Get-Owed([string] $Text) { return @(Get-ReviewRows $Text | ForEach-Object { [regex]::Match($_, ' (/[a-z-]+)').Groups[1].Value }) -join ',' }
function Get-Phase([string] $Repo, [string] $Sha) { if ($null -eq $Sha) { return '' } return "$((git -C $Repo log -1 --format=%B $Sha | Select-String '^Phase:') -join '')" }
function Set-Result([string] $Template, [hashtable] $Values) {
	$out = $Template
	foreach ($key in $Values.Keys) { $out = $out -replace ([regex]::Escape("$key=<severity>")), "$key=$($Values[$key])" }
	return $out -replace '<severity>', 'none'
}
$cpp = "// Adds.`nint Add(int a, int b) { return a + b; }`n"
$cppEdited = "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"

'# Class A: phase interleaving (B and W baselines, withheld words, combined counted as W)'

'## ADV016/COH301 Tier 1: a combined retest erases the withheld /comment-review'
$repo = New-Case 'adv016' @{ 'a.cpp' = $cpp; 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" $cppEdited
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s1 = Start-Round $repo $base 1 @('-Landing')
"round 1 owes $(Get-Owed $s1.Text); $(Get-Phase $repo $s1.Sha)"
"close 1: $((Close-Round $repo $s1.Sha (Set-Result $s1.Template @{ '/coherence-review' = 'High' })).Text)"
Write-Text "$repo/doc.md" "# Doc`n`nOne, fixed.`n"
$s2 = Start-Round $repo $base 1 @('-Landing')
"round 2 owes $(Get-Owed $s2.Text); $(Get-Phase $repo $s2.Sha)"
$owedComment = (Get-Owed $s2.Text) -match '/comment-review'
if ($null -ne $s2.Sha) {
	[void] (Close-Round $repo $s2.Sha ($s2.Template -replace '<severity>', 'none'))
	$s3 = Start-Round $repo $base 1 @('-Landing')
	"round 3 owes $(Get-Owed $s3.Text); last line: $(($s3.Text -split "`n" | Select-Object -Last 1))"
	if ((Get-Owed $s3.Text) -match '/comment-review') { $owedComment = $true }
}
Assert $owedComment 'ADV016: /comment-review on a.cpp (never run) is printed before the loop reports done'

'## COH303 a source deletion after a words round never prints /update-claude-docs'
$repo = New-Case 'coh303' @{ 'a.cpp' = $cpp; 'b.cpp' = "// Subtracts.`nint Sub(int a, int b) { return a - b; }`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" $cppEdited
$sync = $false
$rows = @()
for ($n = 1; $n -le 6; $n++) {
	if ($n -eq 3) { Remove-Item -LiteralPath "$repo/b.cpp" }
	$preview = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2', '-Landing')
	if ($n -ge 3 -and $preview.Text -match '/update-claude-docs') { $sync = $true }
	$s = Start-Round $repo $base 2 @('-Landing')
	if ($null -eq $s.Sha) { $rows += "round ${n}: $(($s.Text -split "`n" | Select-Object -Last 1))"; break }
	$rows += "round ${n}: owes $(Get-Owed $s.Text); $(Get-Phase $repo $s.Sha)"
	[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
}
$rows
Assert $sync 'COH303: a default run after the deletion prints /update-claude-docs before the loop is done'

'# Class B: carried retest scopes rebuilt from diffs (no stored scope)'

'## ADV018 a main-added /adversarial-review retest loses the file the finding arose in'
$repo = New-Case 'adv018' @{ 'a.cpp' = $cpp; 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" $cppEdited
$s1 = Start-Round $repo $base 2
"round 1 owes $(Get-Owed $s1.Text); $(Get-Phase $repo $s1.Sha)"
[void] (Close-Round $repo $s1.Sha ($s1.Template -replace '<severity>', 'none'))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s2 = Start-Round $repo $base 2
"round 2 owes $(Get-Owed $s2.Text); $(Get-Phase $repo $s2.Sha)"
"close 2 (+/adversarial-review=High): $((Close-Round $repo $s2.Sha ((Set-Result $s2.Template @{}) + ',/adversarial-review=High')).Text)"
Write-Text "$repo/doc.md" "# Doc`n`nOne, fixed.`n"
$s3 = Start-Round $repo $base 2
$adv = @(Get-ReviewRows $s3.Text | Where-Object { $_ -match '/adversarial-review' })
"round 3 adversarial row: $($adv -join ' | ')"
Assert (@($adv).Count -eq 1 -and ($adv[0] -match 'a\.cpp' -or $adv[0] -match 'every changed artifact type')) 'ADV018: the retest scope includes a.cpp, where the session-wide review ran'

'## ADV017/COH302 Tier 1: a combined retest drops the carried validator its High result covered'
$repo = New-Case 'adv017' @{ '.agents/skills/demo/SKILL.md' = "# Demo`n`nRuns tool.ps1.`n"; '.agents/scripts/tool.ps1' = "'one'`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/.agents/skills/demo/SKILL.md" "# Demo`n`nRuns tool.ps1 with a flag.`n"
$s1 = Start-Round $repo $base 1 @('-Landing')
"round 1 rows: $((Get-ReviewRows $s1.Text) -join ' | ')"
"close 1: $((Close-Round $repo $s1.Sha (Set-Result $s1.Template @{ '/coherence-review' = 'High' })).Text)"
Write-Text "$repo/.agents/scripts/tool.ps1" "param([switch] `$Flag)`n'one'`n"
$s2 = Start-Round $repo $base 1 @('-Landing')
"round 2 rows: $((Get-ReviewRows $s2.Text) -join ' | ')"
Assert (($s1.Text -notmatch 'carries: /external-skill-creator') -or ($s2.Text -match '/external-skill-creator')) 'ADV017: the validator carried by round 1, whose result was High, runs again in round 2'

'# Class C: removed paths in scopes'

'## added in round 1, deleted in round 2: listed nowhere, dependents still reviewed'
$repo = New-Case 'addremove' @{ 'a.cpp' = $cpp; 'b.cpp' = "int Use() { return Add(1, 2); }`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/x.cpp" "// Extra.`nint Extra() { return 1; }`n"
$rows = @()
for ($n = 1; $n -le 6; $n++) {
	if ($n -eq 3) { Remove-Item -LiteralPath "$repo/x.cpp"; Write-Text "$repo/b.cpp" "int Use() { return Add(2, 2); }`n" }
	$s = Start-Round $repo $base 2 @('-Landing')
	if ($null -eq $s.Sha) { $rows += "round ${n}: $(($s.Text -split "`n" | Select-Object -Last 1))"; break }
	foreach ($row in (Get-ReviewRows $s.Text)) { $rows += "round ${n}: $($row -replace '; Head .*$', '')" }
	[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
}
$rows
$after = @($rows | Where-Object { $_ -match '^round [3-6]: ' })
Assert (@($after | Where-Object { $_ -match 'x\.cpp' }).Count -eq 0) 'x.cpp is listed under neither whole files nor removed after its deletion'
Assert (@($after | Where-Object { $_ -match '/repo-code-review: changed C\+\+(;|$)' }).Count -eq 1 -and @($after | Where-Object { $_ -match '/comment-review: changed C\+\+ and GLSL$' }).Count -eq 1) 'the dependent b.cpp change still schedules /repo-code-review and /comment-review with the first-pass scope'

'# COH304: acceptance waits for /agent-harness'
'## and rule 6: one acceptance round after every other key is clear; a High result retests'
$repo = New-Case 'coh304' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$startOf = { param([string] $Text) @($Text -split "`n" | Where-Object { $_ -match '^- main -StartRound:' }) -join '' }
$p1 = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2', '-HarnessChecks')
"preview 1: $(& $startOf $p1.Text)"
$seq = @()
$s = Start-Round $repo $base 2
$seq += "1:$(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
$p2 = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2', '-HarnessChecks')
"preview 2: $(& $startOf $p2.Text)"
$s = Start-Round $repo $base 2
$seq += "2:$(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha (Set-Result $s.Template @{ '/verify-acceptance' = 'High' }))
Write-Text "$repo/doc.md" "# Doc`n`nOne, fixed.`n"
for ($n = 3; $n -le 6; $n++) {
	$s = Start-Round $repo $base 2
	if ($null -eq $s.Sha) { $seq += "${n}:done"; break }
	$seq += "${n}:$(Get-Owed $s.Text)"
	[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
}
"sequence: $($seq -join ' | ')"
Assert ((& $startOf $p1.Text) -notmatch '/agent-harness') 'control: a code round''s -StartRound row does not wait for /agent-harness'
Assert ((& $startOf $p2.Text) -match 'after .*/agent-harness') 'COH304: the -StartRound row of an acceptance round waits for /agent-harness'
Assert (($seq -join ' | ') -ceq '1:/coherence-review | 2:/verify-acceptance | 3:/coherence-review | 4:/verify-acceptance | 5:done') 'rule 6: acceptance runs after the other keys clear, and its High result retests it after the fix round'
$repo = New-Case 'coh304t1' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$p = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '1', '-HarnessChecks')
"tier 1 preview: $(& $startOf $p.Text)"
Assert ((& $startOf $p.Text) -match 'after .*/agent-harness') 'COH304: the -StartRound row of a combined round carrying /verify-acceptance waits for /agent-harness'

'# Check-kind round: a check pass that closes High re-enters the loop with the right keys and scopes'
$repo = New-Case 'checkhigh' @{ 'a.cpp' = $cpp; 'b.cpp' = "int Use() { return Add(1, 2); }`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" $cppEdited
$seq = @()
$s = Start-Round $repo $base 2 @('-Landing')
$seq += "1 $(Get-Phase $repo $s.Sha) $((git -C $repo log -1 --format=%B $s.Sha | Select-String '^Kind:') -join ''): $((Get-ReviewRows $s.Text | ForEach-Object { $_ -replace '; Head .*$', '' }) -join ' / ')"
[void] (Close-Round $repo $s.Sha (Set-Result $s.Template @{ '/repo-code-review' = 'Medium' }))
Write-Text "$repo/b.cpp" "int Use() { return Add(1, 3); }`n"
$s = Start-Round $repo $base 2 @('-Landing')
$kind2 = "$((git -C $repo log -1 --format=%B $s.Sha | Select-String '^Kind:') -join '')"
$seq += "2 ${kind2}: $((Get-ReviewRows $s.Text | ForEach-Object { $_ -replace '; Head .*$', '' }) -join ' / ')"
[void] (Close-Round $repo $s.Sha (Set-Result $s.Template @{ '/repo-code-review' = 'High' }))
$s = Start-Round $repo $base 2 @('-Landing')
$kind3 = "$((git -C $repo log -1 --format=%B $s.Sha | Select-String '^Kind:') -join '')"
$rows3 = @(Get-ReviewRows $s.Text | ForEach-Object { $_ -replace '; Head .*$', '' })
$seq += "3 ${kind3}: $($rows3 -join ' / ')"
$seq
Assert ($kind2 -ceq 'Kind: check' -and $kind3 -ceq 'Kind: ordinary') 'the fix round after a Medium close is a check pass; the round after its High is ordinary'
Assert (@($rows3 | Where-Object { $_ -match '/repo-code-review: changed C\+\+(;|$)' }).Count -eq 1 -and @($rows3 | Where-Object { $_ -match '/comment-review' }).Count -eq 0) 'the unchanged-tree retest prints /repo-code-review with the first-pass scope and still withholds /comment-review'

'# COH305/COH306: a deletion-only first pass prints /comment-review and landing names the same handoff'
$repo = New-Case 'coh306' @{ 'a.cpp' = $cpp; 'b.cpp' = "// Subtracts.`nint Sub(int a, int b) { return a - b; }`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Remove-Item -LiteralPath "$repo/b.cpp"
$rows = @()
for ($n = 1; $n -le 5; $n++) {
	$s = Start-Round $repo $base 2 @('-Landing')
	if ($null -eq $s.Sha) { $rows += "round ${n}: $(($s.Text -split "`n" | Select-Object -Last 1))"; break }
	foreach ($row in (Get-ReviewRows $s.Text)) { $rows += "round ${n}: $($row -replace '; Head .*$', '')" }
	[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
}
$rows
git -C $repo add -A; git -C $repo commit -q -m session
$json = Join-Path $top 'coh306-landing.json'
& pwsh -NoProfile -NonInteractive -File (Join-Path $Scripts '.agents/scripts/Get-SessionChangeInventory.ps1') -RepositoryRoot $repo -Baseline $base -Head session -Landing -OutputPath $json 2>&1 | Out-Null
$j = Get-Content -Raw $json | ConvertFrom-Json
$skeleton = @($j.landing.acceptanceSkeleton | ForEach-Object { $_.check })
"landing: status=$($j.status) code=$($j.code) skeleton=$($skeleton -join ', ')"
Assert (@($rows | Where-Object { $_ -match '/comment-review: changed C\+\+ and GLSL' }).Count -eq 1) 'COH305: the deletion-only first pass prints the /comment-review row'
Assert ($j.status -eq 'pass' -and $skeleton -ccontains '/comment-review') 'COH306: landing passes and its skeleton names the /comment-review handoff the planner printed'
