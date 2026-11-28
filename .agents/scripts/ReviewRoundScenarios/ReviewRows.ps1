# Review rows and round results: what Get-ChangeWorkflowSteps.ps1 prints for each round, how its closes route the
# next round, and which review-round chains read. Each headed case creates its own scratch repositories.
param([Parameter(Mandatory)][string] $ScratchRoot)
. (Join-Path $PSScriptRoot 'ScenarioCommon.ps1')

function Get-Chain([string] $Repo) {
	$text = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); `$s = Get-ReviewRoundState -RepositoryRoot $(ConvertTo-Literal $Repo) -ChainOnly; `"`$(`$s.Readable)`""
	# $null when the child printed neither True nor False, so a failed read is not taken for an unreadable chain.
	$value = "$text".Trim()
	if ($value -eq 'True') { return $true }
	if ($value -eq 'False') { return $false }
	return $null
}
function Set-Result([string] $Template, [hashtable] $Values) {
	$out = $Template
	foreach ($key in $Values.Keys) { $out = $out -replace ([regex]::Escape("$key=<severity>")), "$key=$($Values[$key])" }
	return $out -replace '<severity>', 'none'
}
function Get-Phase([string] $Repo, [string] $Sha) { if ($null -eq $Sha) { return '' } return "$((git -C $Repo log -1 --format=%B $Sha | Select-String '^Phase:') -join '')" }
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
	$v = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); (Get-SessionChangeInventory -RepositoryRoot $(ConvertTo-Literal $Probe.Repo) -Baseline $($Probe.Base) -Since $($Probe.Since) -IncludeUntracked).triggers.repoCodeReview"
	return "$v".Trim()
}
$cpp = "// Adds.`nint Add(int a, int b) { return a + b; }`n"
$cppEdited = "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"

'# Close routing and chain reads'

'## ADV009/COH09 diagnosis at exactly the third consecutive Critical or High close'
$repo = New-Case 'adv009' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$diag = @()
for ($i = 1; $i -le 4; $i++) {
	$s = Start-Round $repo $base 2
	$c = Close-Round $repo $s.Sha '/coherence-review=High'
	$diag += ($c.Text -match '/external-diagnose-bug')
}
"diagnosis per close: $($diag -join ' ')"
Assert ($null -ne $s.Sha -and $c.Code -eq 0 -and -not $diag[0] -and -not $diag[1] -and $diag[2] -and -not $diag[3]) 'diagnosis row at the third close only, not the fourth'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2')
Assert ($r.Code -eq 0 -and $r.Text -match '-StartRound' -and $r.Text -notmatch '/external-diagnose-bug') 'a default run after the fourth close prints no diagnosis row'

'## ADV008 uppercase baseline'
$repo = New-Case 'adv008' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base.ToUpperInvariant() 2
"start exit $($s.Code)"
Assert ($s.Code -eq 0 -and (Get-Chain $repo)) 'a round started with an uppercase -Baseline reads'
Assert ((git -C $repo log -1 --format=%B (Get-RefTip $repo) | Out-String) -cmatch "(?m)^Baseline: $base") 'its message records the lowercase resolved baseline'

'## COH01 a pending code retest is not replaced by a words round'
$repo = New-Case 'coh01' @{ 'a.cpp' = "// Adds.`nint Add(int a, int b) { return a + b; }`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"
$s = Start-Round $repo $base 2
"round 1 owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha (($s.Template) -replace '/repo-code-review=<severity>', '/repo-code-review=High' -replace '<severity>', 'none'))
$s2 = Start-Round $repo $base 2
"round 2 owes $(Get-Owed $s2.Text)"
Assert ($s2.Text -match 'fresh reviewer /repo-code-review' -and $s2.Text -notmatch 'mechanic /comment-review') 'after a High code close and no edit, round 2 retests /repo-code-review and withholds /comment-review'

'## COH05 an added /adversarial-review High is retested'
$repo = New-Case 'coh05' @{ 'doc.md' = "# Doc`n`nOne.`n`nTwo.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n`nTwo.`n"
$s = Start-Round $repo $base 2
[void] (Close-Round $repo $s.Sha '/coherence-review=none,/adversarial-review=High')
$s2 = Start-Round $repo $base 2
"no-edit round owes $(Get-Owed $s2.Text)"
Assert ($s2.Text -match 'reviewer /adversarial-review') 'no edit: the repeat round prints /adversarial-review'
[void] (Close-Round $repo $s2.Sha ($s2.Template -replace '/adversarial-review=<severity>', '/adversarial-review=High' -replace '<severity>', 'none'))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n`nTwo, edited.`n"
$s3 = Start-Round $repo $base 2
"edited round owes $(Get-Owed $s3.Text)"
Assert ($s3.Text -match 'reviewer /adversarial-review') 'with a doc edit: the next round prints /adversarial-review'

'## ADV004/COH02 a line splice in changed comment text keeps the code review'
$repo = New-Case 'adv004' @{ 'a.cpp' = "/* note`ntail */`nint Add(int a, int b) { return a + b; }`n// plain`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" "/* note`ntail */`nint Add(int a, int b) { return a + b + 0; }`n// plain`n"
$s = Start-Round $repo $base 2
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
$since = $s.Sha
$probe = { param([string] $Text)
	Write-Text "$repo/a.cpp" $Text
	$v = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); (Get-SessionChangeInventory -RepositoryRoot $(ConvertTo-Literal $repo) -Baseline $base -Since $since -IncludeUntracked).triggers.repoCodeReview"
	return "$v".Trim()
}
$plain = & $probe "/* note`ntail */`nint Add(int a, int b) { return a + b + 0; }`n// plain, edited`n"
$splice = & $probe "/* note *\`n/ int Leak = 1; /* tail */`nint Add(int a, int b) { return a + b + 0; }`n// plain`n"
$lineSplice = & $probe "/* note`ntail */`nint Add(int a, int b) { return a + b + 0; }`n// plain \`n"
"repoCodeReview: plain=$plain splice=$splice lineSplice=$lineSplice"
Assert ($plain -eq 'False') 'control: a plain comment edit is comment-text-only'
Assert ($splice -eq 'True') 'a block comment edit holding *\<LF>/ owes /repo-code-review'
Assert ($lineSplice -eq 'True') 'a line comment edit ending in a backslash owes /repo-code-review'

'## ADV003 a malformed close does not read'
$repo = New-Case 'adv003' @{ 'doc.md' = "# Doc`n`nOne.`n"; 'other.md' = "# Other`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base 2
$snapTree = (git -C $repo rev-parse "$($s.Sha)^{tree}").Trim()
$otherTree = (git -C $repo rev-parse "$base^{tree}").Trim()
$forge = { param([string] $Tree, [string] $Message)
	$file = Join-Path $ScratchRoot 'forged.txt'
	Write-Text $file $Message
	$commit = (git -C $repo commit-tree $Tree -p $s.Sha -F $file).Trim()
	git -C $repo update-ref refs/worktree/review-rounds $commit
	return Get-Chain $repo
}
$noResults = & $forge $snapTree "Review round 1 closed`n`nCloses: 1`nNext: done`n"
$wrongTree = & $forge $otherTree "Review round 1 closed`n`nCloses: 1`nResult: /coherence-review none`nNext: done`n"
$wrongNext = & $forge $snapTree "Review round 1 closed`n`nCloses: 1`nResult: /coherence-review High`nNext: done`n"
$foreign = & $forge $snapTree "Review round 1 closed`n`nCloses: 1`nResult: /coherence-review none`nResult: /repo-code-review none`nNext: done`n"
$valid = & $forge $snapTree "Review round 1 closed`n`nCloses: 1`nResult: /coherence-review none`nNext: done`n"
"readable: noResults=$noResults wrongTree=$wrongTree wrongNext=$wrongNext foreign=$foreign valid=$valid"
Assert ($noResults -eq $false -and $wrongTree -eq $false -and $wrongNext -eq $false -and $foreign -eq $false) 'a close with no owed result, another tree, a wrong Next, or a foreign key does not read'
Assert ($valid) 'control: the close -CloseRound would write reads'

'## ADV006 a symbolic review ref'
$repo = New-Case 'adv006' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
git -C $repo symbolic-ref refs/worktree/review-rounds refs/heads/main
$s = Start-Round $repo $base 2
"start exit $($s.Code): $($s.Text -split "`n" | Select-Object -Last 1)"
$mainTip = (git -C $repo rev-parse refs/heads/main).Trim()
git -C $repo symbolic-ref -q refs/worktree/review-rounds 2>$null | Out-Null
$stillSymbolic = $LASTEXITCODE -eq 0
"main $mainTip; ref symbolic after: $stillSymbolic"
Assert ($mainTip -eq $base) 'the session branch main is not moved'
Assert ($s.Code -eq 0 -and -not $stillSymbolic -and (Get-Chain $repo)) 'the round replaces the symbolic ref with a direct one that reads'
$repo = New-Case 'adv006read' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base 2
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
$direct = Get-Chain $repo
git -C $repo update-ref refs/heads/chain (Get-RefTip $repo)
git -C $repo symbolic-ref refs/worktree/review-rounds refs/heads/chain
Assert ($direct -and (Get-Chain $repo) -eq $false) 'a symbolic ref naming a valid chain does not read'

'## ADV005/COH04 one observed tip serves planning and the compare-and-swap'
$repo = New-Case 'adv005' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base 2
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited twice.`n"
# The -StartRound sequence with a concurrent round started between its chain read and its old-value read: the script's
# own read order, replayed in-process with the modules under test.
$repoLiteral = ConvertTo-Literal $repo
$emulate = @"
Import-Module $(ConvertTo-Literal $inventoryModule); Import-Module $(ConvertTo-Literal $roundsModule)
`$chain = Get-ReviewRoundState -RepositoryRoot $repoLiteral -ChainOnly
`$tree = New-ReviewRoundTree $repoLiteral
`$concurrent = New-ReviewRoundCommit $repoLiteral `$tree `$chain.Last.Close.Commit (Format-ReviewRoundSnapshotMessage 2 '$base' 'code' 'ordinary' 2 @('/coherence-review') `$null)
git -C $repoLiteral update-ref refs/worktree/review-rounds `$concurrent
`$old = if (`$chain.PSObject.Properties.Name -contains 'Tip') { `$chain.Tip } else { & (Get-Module ReviewRounds) { Get-ReviewRoundRefValue $repoLiteral } }
`$mine = New-ReviewRoundCommit $repoLiteral `$tree `$chain.Last.Close.Commit (Format-ReviewRoundSnapshotMessage 2 '$base' 'words' 'ordinary' 2 @('/coherence-review') `$null)
`$moved = Update-ReviewRoundRef $repoLiteral `$mine `$old
"cas=`$moved tipIsConcurrent=`$((git -C $repoLiteral rev-parse refs/worktree/review-rounds).Trim() -eq `$concurrent)"
"@
$out = pwsh -NoProfile -NonInteractive -Command $emulate
"$out"
Assert ("$out" -match 'cas=False tipIsConcurrent=True') 'the compare-and-swap fails and the concurrent open round stays'
# The script itself: a post-index-change hook advances the ref while -StartRound builds its tree.
$repo = New-Case 'adv005hook' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
New-Item -ItemType Directory -Force "$repo/Temp/hooks" | Out-Null
Write-Text "$repo/Temp/hooks/post-index-change" "#!/bin/sh`nif [ ! -e Temp/hook-ran ]; then touch Temp/hook-ran; git update-ref refs/worktree/review-rounds HEAD; fi`n"
git -C $repo config core.hooksPath Temp/hooks
$s = Start-Round $repo $base 2
git -C $repo config --unset core.hooksPath
"start exit $($s.Code): $($s.Text)"
Assert ((Test-Path -LiteralPath "$repo/Temp/hook-ran") -and $s.Code -eq 2 -and (Get-RefTip $repo) -eq $base) 'a ref moved during the freeze fails -StartRound and keeps the other writer''s value'

'## ADV013: a sweep round reads only in its one form'
$repo = New-Case 'adv013' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
$tree = (git -C $repo rev-parse "$base^{tree}").Trim()
$forge = { param([string] $Kind, [string] $Close)
	$file = Join-Path $ScratchRoot 'forged.txt'
	Write-Text $file "Review round 1`n`nRound: 1`nBaseline: $base`nPhase: sweep`nKind: $Kind`n"
	$commit = (git -C $repo commit-tree $tree -p $base -F $file).Trim()
	if ($Close) {
		Write-Text $file $Close
		$commit = (git -C $repo commit-tree $tree -p $commit -F $file).Trim()
	}
	git -C $repo update-ref refs/worktree/review-rounds $commit
	return Get-Chain $repo
}
$checkKind = & $forge 'check' "Review round 1 closed`n`nCloses: 1`nNext: done`n"
$withResult = & $forge 'ordinary' "Review round 1 closed`n`nCloses: 1`nResult: /adversarial-review none`nNext: done`n"
$open = & $forge 'ordinary' ''
$valid = & $forge 'ordinary' "Review round 1 closed`n`nCloses: 1`nNext: done`n"
"readable: checkKind=$checkKind withResult=$withResult open=$open valid=$valid"
Assert ($checkKind -eq $false -and $withResult -eq $false -and $open -eq $false) 'a check-kind, result-carrying, or open sweep round does not read'
Assert ($valid) 'control: the sweep round Invoke-Sweep.ps1 writes reads'

'# Owed retests later routing must not displace'

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

'## COH201 Tier 1: a document-only fix after a High code close keeps the owed /comment-review'
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

'## a High words close followed by a code-only fix'
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

'# Comment-text-only predicate'

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

'# Removed paths in a round scope'

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

'## a deletion-only words round prints the /comment-review row'
$r = Invoke-RemovalCase 'item4-delete' { param($Repo) Remove-Item -LiteralPath "$Repo/b.cpp" }
$r.Rows; $r.Last
Assert (@($r.Rows | Where-Object { $_ -match '/comment-review: changed C\+\+ and GLSL$' }).Count -eq 1) 'the round after the deletion prints /comment-review with the first-pass scope'
Assert (@($r.Rows | Where-Object { $_ -match '/repo-code-review: changed C\+\+(;|$)' }).Count -eq 1) 'the /repo-code-review retest prints the first-pass scope'
Assert ($r.Last -match '^Review rounds: done at round') 'the loop finishes'

'## added in round 1, deleted in round 3: listed nowhere, dependents still reviewed'
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

'## COH305/COH306: a deletion-only first pass prints /comment-review and landing names the same handoff'
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
$j = Invoke-Landing $repo $base
$skeleton = @($j.landing.acceptanceSkeleton | ForEach-Object { $_.check })
"landing: skeleton=$($skeleton -join ', ')"
Assert (@($rows | Where-Object { $_ -match '/comment-review: changed C\+\+ and GLSL' }).Count -eq 1) 'COH305: the deletion-only first pass prints the /comment-review row'
Assert ($j.status -eq 'pass' -and $skeleton -ccontains '/comment-review') 'COH306: landing passes and its skeleton names the /comment-review handoff the planner printed'

'# Phase interleaving (B and W baselines, withheld words, combined counted as W)'

'## ADV016/COH301 Tier 1: the words retest after a High code-round close keeps the withheld /comment-review owed'
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

'## COH303 a source deletion after a words round still prints /update-claude-docs'
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

'# Carried retest scopes rebuilt from diffs (no stored scope)'

'## ADV018 a main-added /adversarial-review retest keeps the file the finding arose in'
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

'## ADV017/COH302 Tier 1: a combined retest runs again the carried validator its High result covered'
$repo = New-Case 'adv017' @{ '.agents/skills/demo/SKILL.md' = "# Demo`n`nRuns tool.ps1.`n"; '.agents/scripts/tool.ps1' = "'one'`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/.agents/skills/demo/SKILL.md" "# Demo`n`nRuns tool.ps1 with a flag.`n"
$s1 = Start-Round $repo $base 1 @('-Landing')
"round 1 rows: $((Get-ReviewRows $s1.Text) -join ' | ')"
"close 1: $((Close-Round $repo $s1.Sha (Set-Result $s1.Template @{ '/coherence-review' = 'High' })).Text)"
Write-Text "$repo/.agents/scripts/tool.ps1" "param([switch] `$Flag)`n'one'`n"
$s2 = Start-Round $repo $base 1 @('-Landing')
"round 2 rows: $((Get-ReviewRows $s2.Text) -join ' | ')"
Assert ($s1.Code -eq 0 -and $s2.Code -eq 0 -and $null -ne $s1.Sha -and $null -ne $s2.Sha -and $s1.Text -match 'carries: /external-skill-creator' -and $s2.Text -match '/external-skill-creator') 'ADV017: the validator carried by round 1, whose result was High, runs again in round 2'

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
Assert ($null -ne $s2.Sha -and $s2.Text -notmatch '/external-skill-creator|/progressive-disclosure-review') 'COH401/VS010: round 2 carries no check round 1 did not carry'
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

'# Acceptance rounds and check passes'

'## COH304: acceptance waits for /agent-harness, and rule 6: one acceptance round after every other key is clear; a High result retests'
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
Assert ($p1.Code -eq 0 -and (& $startOf $p1.Text) -ne '' -and (& $startOf $p1.Text) -notmatch '/agent-harness') 'control: a code round''s -StartRound row does not wait for /agent-harness'
Assert ((& $startOf $p2.Text) -match 'after .*/agent-harness') 'COH304: the -StartRound row of an acceptance round waits for /agent-harness'
Assert (($seq -join ' | ') -ceq '1:/coherence-review | 2:/verify-acceptance | 3:/coherence-review | 4:/verify-acceptance | 5:done') 'rule 6: acceptance runs after the other keys clear, and its High result retests it after the fix round'
$repo = New-Case 'coh304t1' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$p = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '1', '-HarnessChecks')
"tier 1 preview: $(& $startOf $p.Text)"
Assert ((& $startOf $p.Text) -match 'after .*/agent-harness') 'COH304: the -StartRound row of a combined round carrying /verify-acceptance waits for /agent-harness'

'## Check-kind round: a check pass that closes High re-enters the loop with the right keys and scopes'
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

'## COH404/VS011/PD009/ADV023 comment-review step 1 keeps a present Coretide source and drops a deletion'
# A Coretide x.cpp edit and an engine b.cpp deletion; step 1 keeps the C++ and GLSL entries the inventory at the round
# Head does not list as deleted.
$repo = New-Case 'coh404' @{ 'b.cpp' = $cpp; 'doc.md' = "# Doc`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
$bare = Join-Path $ScratchRoot 'coh404-coretide.git'
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
$entries = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); (Get-SessionChangeInventory -RepositoryRoot $(ConvertTo-Literal $repo) -Baseline $base -Head $head).entries | ForEach-Object { `"`$(`$_.status) `$(`$_.class) `$(`$_.path)`" }"
"inventory at Head: $($entries -join ' | ')"
$kept = @(foreach ($line in $entries) {
	$status, $class, $path = $line -split ' ', 3
	if ($class -cnotin @('cpp', 'dual-language-header', 'glsl')) { continue }
	if ($status -cne 'D') { $path }
})
"step 1 keeps: $($kept -join ', ')"
Assert ($kept -ccontains 'Projects/Coretide/x.cpp') 'COH404/VS011/PD009/ADV023: the present Coretide source survives step 1'
Assert ($kept -cnotcontains 'b.cpp') 'the deleted engine source is not handed to the scanner'
