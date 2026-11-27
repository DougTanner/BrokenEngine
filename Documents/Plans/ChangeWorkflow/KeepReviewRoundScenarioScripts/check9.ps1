# Round-2 diagnosis cases: one reproducing case per round-2 finding of the three failed-retest classes. -Scripts is
# the directory holding the .agents tree under test; -Tag names Temp/loop-scenario/c9-<Tag>, which must not exist yet.
# Run from the worktree root. Each case prints its observed values; a FAIL line is the red signal.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$script:Steps = Join-Path $Scripts '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$inventoryCli = Join-Path $Scripts '.agents/scripts/Get-SessionChangeInventory.ps1'
$inventoryModule = Join-Path $Scripts '.agents/scripts/SessionChangeInventory.psm1'
$top = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c9-$Tag"))
if (Test-Path -LiteralPath $top) { throw "scratch path exists: $top" }
New-Item -ItemType Directory -Force $top | Out-Null
"scripts: $Scripts"

function New-Case([string] $Name, [hashtable] $Files, [hashtable] $Binary = @{}) {
	$repo = Join-Path $top $Name
	New-ScratchRepo $repo
	Write-Text "$repo/.gitignore" "Temp/`nProjects/Coretide/`n"
	foreach ($path in $Files.Keys) {
		$full = Join-Path $repo $path
		New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
		Write-Text $full $Files[$path]
	}
	foreach ($path in $Binary.Keys) {
		$full = Join-Path $repo $path
		New-Item -ItemType Directory -Force (Split-Path $full) | Out-Null
		[IO.File]::WriteAllBytes($full, [byte[]] $Binary[$path])
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
function Invoke-Landing([string] $Repo, [string] $Base) {
	$json = Join-Path $top 'landing.json'
	& pwsh -NoProfile -NonInteractive -File $inventoryCli -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $json 2>&1 | Out-Null
	return Get-Content -Raw $json | ConvertFrom-Json
}
# The round planner's verdict at a commit, as landing would need it: Get-ReviewRoundState with -Head and -Landing.
function Get-PlannerAt([string] $Repo, [string] $Base, [string] $Head) {
	$text = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; `$s = Get-ReviewRoundState -RepositoryRoot '$Repo' -Baseline $Base -Head $Head -Landing; `"done=`$(`$s.Plan.Done) phase=`$(`$s.Plan.Phase) keys=`$(@(`$s.Plan.Keys) -join ',') failure=`$(`$s.Failure)`""
	return "$text".Trim()
}
function Get-ReviewRows([string] $Text) { return @($Text -split "`n" | Where-Object { $_ -match '; Head ' }) }
function Get-Owed([string] $Text) { return @(Get-ReviewRows $Text | ForEach-Object { [regex]::Match($_, ' (/[a-z-]+)').Groups[1].Value }) -join ',' }
function Set-Result([string] $Template, [hashtable] $Values) {
	$out = $Template
	foreach ($key in $Values.Keys) { $out = $out -replace ([regex]::Escape("$key=<severity>")), "$key=$($Values[$key])" }
	return $out -replace '<severity>', 'none'
}
# The comment-only probe: repoCodeReview on a -Since run from a closed round over a.cpp after writing Text.
function New-CommentProbe([string] $Name, [string] $Before) {
	$repo = New-Case $Name @{ 'a.cpp' = $Before }
	$base = (git -C $repo rev-parse HEAD).Trim()
	Write-Text "$repo/a.cpp" ($Before + "int Extra = 0;`n")
	$s = Start-Round $repo $base 2
	[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
	return [pscustomobject] @{ Repo = $repo; Base = $base; Since = $s.Sha; Prefix = ($Before + "int Extra = 0;`n") }
}
function Test-CommentProbe([object] $Probe, [string] $Text) {
	Write-Text "$($Probe.Repo)/a.cpp" $Text
	$v = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; (Get-SessionChangeInventory -RepositoryRoot '$($Probe.Repo)' -Baseline $($Probe.Base) -Since $($Probe.Since) -IncludeUntracked).triggers.repoCodeReview"
	return "$v".Trim()
}

'# Class 1: landing staleness versus exempt paths'

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
"gate: status=$($j.status) code=$($j.code)"
"planner at candidate: $(Get-PlannerAt $repo $base $candidate)"
# ADV010 was rejected (fixed decision 1): the expected difference from the filed case is that the lone exempt Plan
# deletion lands, the verdict the planner gives too.
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
"gate: status=$($j.status) code=$($j.code)"
$planner = Get-PlannerAt $repo $base $candidate
"planner at candidate: $planner"
Assert ($j.status -ne 'pass') 'COH203: the landing gate refuses the unreviewed binary deletion'
Assert (($j.status -eq 'pass') -eq ($planner -match '^done=True')) 'the gate and the planner at the candidate agree'

'# Class 2: owed retests displaced by later routing'

'## COH201/ADV011 Tier 2: a document-only fix after a High code close'
$repo = New-Case 'coh201t2' @{ 'a.cpp' = "// Adds.`nint Add(int a, int b) { return a + b; }`n"; 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base 2
"round 1 owes $(Get-Owed $s.Text)"
"close 1: $((Close-Round $repo $s.Sha (Set-Result $s.Template @{ '/repo-code-review' = 'High' })).Text)"
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited again.`n"
$s2 = Start-Round $repo $base 2
"round 2 owes $(Get-Owed $s2.Text)"
Assert ((Get-Owed $s2.Text) -match '/repo-code-review') 'round 2 retests /repo-code-review'
"close 2: $((Close-Round $repo $s2.Sha ($s2.Template -replace '<severity>', 'none')).Text)"
$s3 = Start-Round $repo $base 2
"round 3 owes $(Get-Owed $s3.Text); $(($s3.Text -split "`n" | Select-Object -Last 1))"

'## COH201 Tier 1: a combined round clears the owed /comment-review'
$repo = New-Case 'coh201t1' @{ 'a.cpp' = "// Adds.`nint Add(int a, int b) { return a + b; }`n"; 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base 1 @('-Landing')
"round 1 owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha (Set-Result $s.Template @{ '/repo-code-review' = 'High' }))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited again.`n"
$s2 = Start-Round $repo $base 1 @('-Landing')
"round 2 owes $(Get-Owed $s2.Text); phase line: $((git -C $repo log -1 --format=%B $s2.Sha | Select-String '^Phase:') -join '')"
[void] (Close-Round $repo $s2.Sha ($s2.Template -replace '<severity>', 'none'))
$s3 = Start-Round $repo $base 1 @('-Landing')
$printed = "$(Get-Owed $s.Text),$(Get-Owed $s2.Text),$(Get-Owed $s3.Text)"
"round 3 owes $(Get-Owed $s3.Text); $(($s3.Text -split "`n" | Select-Object -Last 1))"
Assert ((Get-Owed $s2.Text) -match '/repo-code-review') 'round 2 retests /repo-code-review'
Assert ($printed -match '/comment-review') '/comment-review is printed by some round before the loop is done'

'## ADV014 an added High /adversarial-review is retested with its scope'
$repo = New-Case 'adv014' @{ 'a.cpp' = "// Adds.`nint Add(int a, int b) { return a + b; }`n"; 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"
$s = Start-Round $repo $base 2
"round 1 owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha (($s.Template -replace '<severity>', 'none') + ',/adversarial-review=High'))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s2 = Start-Round $repo $base 2
$adv = @(Get-ReviewRows $s2.Text | Where-Object { $_ -match '/adversarial-review' }) -join ''
"round 2 owes $(Get-Owed $s2.Text)"
"round 2 adversarial row: $adv"
Assert ($adv -match 'a\.cpp|every changed artifact type') 'the /adversarial-review retest covers a.cpp, where its finding arose (whole files naming it, or the first-pass scope)'

'# Class 3: comment-text-only predicate'

'## COH202 an unchanged splice joining two slashes'
$p = New-CommentProbe 'coh202' "/\`n/ note /*`nint Value = 1;`n// */`n"
$red = Test-CommentProbe $p ("/\`n/ note /*`nint Value = 2;`n// */`n" + "int Extra = 0;`n")
"repoCodeReview: initializer-change=$red"
Assert ($red -eq 'True') 'changing the live initializer the splice exposes owes /repo-code-review'

'## ADV012 an unchanged splice joining R to a raw string'
$p = New-CommentProbe 'adv012' "const char* Text = R\`n`"d(a`" /* x */ `")d`";`n"
$red = Test-CommentProbe $p ("const char* Text = R\`n`"d(a`" /* y */ `")d`";`n" + "int Extra = 0;`n")
"repoCodeReview: raw-payload-change=$red"
Assert ($red -eq 'True') 'changing raw-string payload owes /repo-code-review'

'## C++23 whitespace splice inside the changed comment (P2223R2)'
$p = New-CommentProbe 'wsplice' "/* note * `n/ int Leak = 1; /* tail */`n"
$red = Test-CommentProbe $p ("/* note *\ `n/ int Leak = 1; /* tail */`n" + "int Extra = 0;`n")
"repoCodeReview: backslash-space-newline=$red"
Assert ($red -eq 'True') 'a changed block comment holding *\<space><LF>/ owes /repo-code-review'

'## Class 2 variant: a High words close followed by a code-only fix'
$repo = New-Case 'wordshigh' @{ 'a.cpp' = "// Adds.`nint Add(int a, int b) { return a + b; }`n"; 'AGENTS.md' = "# Rules`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/AGENTS.md" "# Rules`n`nOne, edited.`n"
$s = Start-Round $repo $base 2
"round 1 owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha (Set-Result $s.Template @{ '/progressive-disclosure-review' = 'High' }))
Write-Text "$repo/a.cpp" "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"
$printed = ''
for ($i = 2; $i -le 5; $i++) {
	$si = Start-Round $repo $base 2
	if ($null -eq $si.Sha) { "round ${i}: $(($si.Text -split "`n" | Select-Object -Last 1))"; break }
	"round $i owes $(Get-Owed $si.Text)"
	$printed += ",$(Get-Owed $si.Text)"
	[void] (Close-Round $repo $si.Sha ($si.Template -replace '<severity>', 'none'))
}
Assert ($printed -match '/progressive-disclosure-review') 'the High /progressive-disclosure-review is retested before the loop is done'

'# Class 1 controls: verdicts the shared classification must keep'

'## control: a reviewed Plan rename whose source is restored is stale (round-1 ADV001)'
$repo = New-Case 'ctl-adv001' @{ 'Documents/Plans/Area/One.md' = "# One`n`nA plan body that is long enough to pair as a rename.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
git -C $repo mv Documents/Plans/Area/One.md Documents/Plans/Area/Two.md
$s = Start-Round $repo $base 2 @('-Landing')
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $repo commit -q -m rename
git -C $repo checkout -q $base -- Documents/Plans/Area/One.md
git -C $repo commit -q -m restore
$j = Invoke-Landing $repo $base
"gate: status=$($j.status) code=$($j.code) message=$($j.message)"
Assert ($j.code -eq 'inventory.review-round-unfinished') 'restoring the rename source is refused as an owed round'

'## control: a Plan added, reviewed, then deleted lands (round-1 COH03)'
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
"gate: status=$($j.status) code=$($j.code)"
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
	"$variant gate: status=$($j.status) code=$($j.code)"
	if ($variant -eq 'other') { Assert ($j.status -eq 'pass') 'a primary change to a file the round did not review lands' }
	else { Assert ($j.code -eq 'inventory.review-round-unfinished') 'a primary edit to another hunk of a reviewed file is refused as an owed round' }
}

'# Item 4: removed paths in a round scope'

# Rounds 1 (code) and 2 (words) over a.cpp and b.cpp close clean; then Change runs and the rounds run to done.
function Invoke-RemovalCase([string] $Name, [scriptblock] $Change) {
	$repo = New-Case $Name @{ 'a.cpp' = "// Adds.`nint Add(int a, int b) { return a + b; }`n"; 'b.cpp' = "// Subtracts.`nint Sub(int a, int b) { return a - b; }`n" }
	$base = (git -C $repo rev-parse HEAD).Trim()
	Write-Text "$repo/a.cpp" "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"
	Write-Text "$repo/b.cpp" "// Subtracts.`nint Sub(int a, int b) { return a - b - 0; }`n"
	for ($i = 1; $i -le 2; $i++) { $si = Start-Round $repo $base 2; [void] (Close-Round $repo $si.Sha ($si.Template -replace '<severity>', 'none')) }
	& $Change $repo
	$rows = [Collections.Generic.List[string]]::new()
	$last = ''
	for ($i = 3; $i -le 7; $i++) {
		$si = Start-Round $repo $base 2
		if ($null -eq $si.Sha) { $last = ($si.Text -split "`n" | Select-Object -Last 1); break }
		foreach ($row in (Get-ReviewRows $si.Text)) { $rows.Add("round ${i}: $($row -replace '; Head .*$', '')") }
		[void] (Close-Round $repo $si.Sha ($si.Template -replace '<severity>', 'none'))
	}
	return [pscustomobject] @{ Rows = $rows; Last = $last }
}

'## a deletion-only words round prints the /comment-review row (round-3 revision)'
$r = Invoke-RemovalCase 'item4-delete' { param($Repo) Remove-Item -LiteralPath "$Repo/b.cpp" }
$r.Rows; $r.Last
Assert (@($r.Rows | Where-Object { $_ -match '/comment-review: changed C\+\+ and GLSL$' }).Count -eq 1) 'the round after the deletion prints /comment-review with the first-pass scope'
Assert (@($r.Rows | Where-Object { $_ -match '/repo-code-review: changed C\+\+(;|$)' }).Count -eq 1) 'the /repo-code-review retest prints the first-pass scope'
Assert ($r.Last -match '^Review rounds: done at round') 'the loop finishes'

'# ADV013: a sweep round reads only in its one form'
$repo = New-Case 'adv013' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
$tree = (git -C $repo rev-parse "$base^{tree}").Trim()
$forge = { param([string] $Kind, [string] $Close)
	$file = Join-Path $top 'forged.txt'
	Write-Text $file "Review round 1`n`nRound: 1`nBaseline: $base`nPhase: sweep`nKind: $Kind`n"
	$commit = (git -C $repo commit-tree $tree -p $base -F $file).Trim()
	if ($Close) {
		Write-Text $file $Close
		$commit = (git -C $repo commit-tree $tree -p $commit -F $file).Trim()
	}
	git -C $repo update-ref refs/worktree/review-rounds $commit
	$text = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; `$s = Get-ReviewRoundState -RepositoryRoot '$repo' -ChainOnly; `"`$(`$s.Readable)`""
	return "$text".Trim() -eq 'True'
}
$checkKind = & $forge 'check' "Review round 1 closed`n`nCloses: 1`nNext: done`n"
$withResult = & $forge 'ordinary' "Review round 1 closed`n`nCloses: 1`nResult: /adversarial-review none`nNext: done`n"
$open = & $forge 'ordinary' ''
$valid = & $forge 'ordinary' "Review round 1 closed`n`nCloses: 1`nNext: done`n"
"readable: checkKind=$checkKind withResult=$withResult open=$open valid=$valid"
Assert (-not $checkKind -and -not $withResult -and -not $open) 'a check-kind, result-carrying, or open sweep round does not read'
Assert ($valid) 'control: the sweep round Invoke-Sweep.ps1 writes reads'
