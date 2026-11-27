# Round-1 fix cases: one reproducing case per fixed finding. -Scripts is the directory holding the .agents tree under
# test (the worktree root for the fixed scripts, Temp/loop-scenario/before for the round-1 snapshot's); -Tag names the
# scratch directory Temp/loop-scenario/c8-<Tag>, which must not exist yet. Run from the worktree root.
param([Parameter(Mandatory)][string] $Scripts, [Parameter(Mandatory)][string] $Tag)
. (Join-Path $PSScriptRoot 'lib.ps1')
$Scripts = [IO.Path]::GetFullPath($Scripts)
$script:Steps = Join-Path $Scripts '.agents/scripts/Get-ChangeWorkflowSteps.ps1'
$inventoryCli = Join-Path $Scripts '.agents/scripts/Get-SessionChangeInventory.ps1'
$inventoryModule = Join-Path $Scripts '.agents/scripts/SessionChangeInventory.psm1'
$roundsModule = Join-Path $Scripts '.agents/scripts/ReviewRounds.psm1'
$top = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "c8-$Tag"))
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
function Start-Round([string] $Repo, [string] $Baseline, [string[]] $Extra = @()) {
	$r = Invoke-Steps (@('-RepositoryRoot', $Repo, '-Baseline', $Baseline, '-Tier', '2', '-StartRound') + $Extra)
	return [pscustomobject] @{ Code = $r.Code; Text = $r.Text; Sha = Get-RoundHead $r.Text; Template = [regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value }
}
function Close-Round([string] $Repo, [string] $Sha, [string] $Results) {
	return Invoke-Steps @('-RepositoryRoot', $Repo, '-CloseRound', '-Head', $Sha, '-Results', $Results)
}
function Get-Chain([string] $Repo) {
	$text = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; `$s = Get-ReviewRoundState -RepositoryRoot '$Repo' -ChainOnly; `"`$(`$s.Readable)`""
	return "$text".Trim() -eq 'True'
}
function Invoke-Landing([string] $Repo, [string] $Base) {
	$json = Join-Path $top 'landing.json'
	& pwsh -NoProfile -NonInteractive -File $inventoryCli -RepositoryRoot $Repo -Baseline $Base -Head session -Landing -OutputPath $json 2>&1 | Out-Null
	$result = Get-Content -Raw $json | ConvertFrom-Json
	"landing: status=$($result.status) code=$($result.code) message=$($result.message)" | Out-Host
	return $result
}
function Get-Owed([string] $Text) { return @($Text -split "`n" | Where-Object { $_ -match '; Head ' } | ForEach-Object { [regex]::Match($_, ' (/[a-z-]+)').Groups[1].Value }) -join ',' }

'## ADV009/COH09 diagnosis at exactly the third consecutive Critical or High close'
$repo = New-Case 'adv009' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$diag = @()
for ($i = 1; $i -le 4; $i++) {
	$s = Start-Round $repo $base
	$c = Close-Round $repo $s.Sha '/coherence-review=High'
	$diag += ($c.Text -match '/external-diagnose-bug')
}
"diagnosis per close: $($diag -join ' ')"
Assert (-not $diag[0] -and -not $diag[1] -and $diag[2] -and -not $diag[3]) 'diagnosis row at the third close only, not the fourth'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2')
Assert ($r.Text -notmatch '/external-diagnose-bug') 'a default run after the fourth close prints no diagnosis row'

'## ADV008 uppercase baseline'
$repo = New-Case 'adv008' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base.ToUpperInvariant()
"start exit $($s.Code)"
Assert ($s.Code -eq 0 -and (Get-Chain $repo)) 'a round started with an uppercase -Baseline reads'
Assert ((git -C $repo log -1 --format=%B (Get-RefTip $repo) | Out-String) -cmatch "(?m)^Baseline: $base") 'its message records the lowercase resolved baseline'

'## COH01 a pending code retest is not replaced by a words round'
$repo = New-Case 'coh01' @{ 'a.cpp' = "// Adds.`nint Add(int a, int b) { return a + b; }`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" "// Adds.`nint Add(int a, int b) { return a + b + 0; }`n"
$s = Start-Round $repo $base
"round 1 owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha (($s.Template) -replace '/repo-code-review=<severity>', '/repo-code-review=High' -replace '<severity>', 'none'))
$s2 = Start-Round $repo $base
"round 2 owes $(Get-Owed $s2.Text)"
Assert ($s2.Text -match 'fresh reviewer /repo-code-review' -and $s2.Text -notmatch 'mechanic /comment-review') 'after a High code close and no edit, round 2 retests /repo-code-review and withholds /comment-review'

'## COH05 an added /adversarial-review High is retested'
$repo = New-Case 'coh05' @{ 'doc.md' = "# Doc`n`nOne.`n`nTwo.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n`nTwo.`n"
$s = Start-Round $repo $base
[void] (Close-Round $repo $s.Sha '/coherence-review=none,/adversarial-review=High')
$s2 = Start-Round $repo $base
"no-edit round owes $(Get-Owed $s2.Text)"
Assert ($s2.Text -match 'reviewer /adversarial-review') 'no edit: the repeat round prints /adversarial-review'
[void] (Close-Round $repo $s2.Sha ($s2.Template -replace '/adversarial-review=<severity>', '/adversarial-review=High' -replace '<severity>', 'none'))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n`nTwo, edited.`n"
$s3 = Start-Round $repo $base
"edited round owes $(Get-Owed $s3.Text)"
Assert ($s3.Text -match 'reviewer /adversarial-review') 'with a doc edit: the next round prints /adversarial-review'

'## ADV004/COH02 a line splice in changed comment text keeps the code review'
$repo = New-Case 'adv004' @{ 'a.cpp' = "/* note`ntail */`nint Add(int a, int b) { return a + b; }`n// plain`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/a.cpp" "/* note`ntail */`nint Add(int a, int b) { return a + b + 0; }`n// plain`n"
$s = Start-Round $repo $base
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
$since = $s.Sha
$probe = { param([string] $Text)
	Write-Text "$repo/a.cpp" $Text
	$v = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; (Get-SessionChangeInventory -RepositoryRoot '$repo' -Baseline $base -Since $since -IncludeUntracked).triggers.repoCodeReview"
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
$s = Start-Round $repo $base
$snapTree = (git -C $repo rev-parse "$($s.Sha)^{tree}").Trim()
$otherTree = (git -C $repo rev-parse "$base^{tree}").Trim()
$forge = { param([string] $Tree, [string] $Message)
	$file = Join-Path $top 'forged.txt'
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
Assert (-not $noResults -and -not $wrongTree -and -not $wrongNext -and -not $foreign) 'a close with no owed result, another tree, a wrong Next, or a foreign key does not read'
Assert ($valid) 'control: the close -CloseRound would write reads'

'## ADV006 a symbolic review ref'
$repo = New-Case 'adv006' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
git -C $repo symbolic-ref refs/worktree/review-rounds refs/heads/main
$s = Start-Round $repo $base
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
$s = Start-Round $repo $base
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $repo update-ref refs/heads/chain (Get-RefTip $repo)
git -C $repo symbolic-ref refs/worktree/review-rounds refs/heads/chain
Assert (-not (Get-Chain $repo)) 'a symbolic ref naming a valid chain does not read'

'## ADV005/COH04 one observed tip serves planning and the compare-and-swap'
$repo = New-Case 'adv005' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
$s = Start-Round $repo $base
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited twice.`n"
# The -StartRound sequence with a concurrent round started between its chain read and its old-value read: the script's
# own read order, replayed in-process with the modules under test.
$emulate = @"
Import-Module '$inventoryModule'; Import-Module '$roundsModule'
`$chain = Get-ReviewRoundState -RepositoryRoot '$repo' -ChainOnly
`$tree = New-ReviewRoundTree '$repo'
`$concurrent = New-ReviewRoundCommit '$repo' `$tree `$chain.Last.Close.Commit (Format-ReviewRoundSnapshotMessage 2 '$base' 'code' 'ordinary' 2 @('/coherence-review') `$null)
git -C '$repo' update-ref refs/worktree/review-rounds `$concurrent
`$old = if (`$chain.PSObject.Properties.Name -contains 'Tip') { `$chain.Tip } else { & (Get-Module ReviewRounds) { Get-ReviewRoundRefValue '$repo' } }
`$mine = New-ReviewRoundCommit '$repo' `$tree `$chain.Last.Close.Commit (Format-ReviewRoundSnapshotMessage 2 '$base' 'words' 'ordinary' 2 @('/coherence-review') `$null)
`$moved = Update-ReviewRoundRef '$repo' `$mine `$old
"cas=`$moved tipIsConcurrent=`$((git -C '$repo' rev-parse refs/worktree/review-rounds).Trim() -eq `$concurrent)"
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
$s = Start-Round $repo $base
git -C $repo config --unset core.hooksPath
"start exit $($s.Code): $($s.Text)"
Assert ((Test-Path -LiteralPath "$repo/Temp/hook-ran") -and $s.Code -eq 2 -and (Get-RefTip $repo) -eq $base) 'a ref moved during the freeze fails -StartRound and keeps the other writer''s value'

'## ADV001 a reviewed Plan rename cannot land as a copy'
$repo = New-Case 'adv001' @{ 'Documents/Plans/Area/One.md' = "# One`n`nA plan body that is long enough to pair as a rename.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
git -C $repo mv Documents/Plans/Area/One.md Documents/Plans/Area/Two.md
$s = Start-Round $repo $base @('-Landing')
"round owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $repo commit -q -m rename
$j = Invoke-Landing $repo $base
Assert ($j.status -eq 'pass') 'control: the reviewed rename lands'
git -C $repo checkout -q $base -- Documents/Plans/Area/One.md
git -C $repo commit -q -m restore
$j = Invoke-Landing $repo $base
Assert ($j.code -eq 'inventory.review-round-unfinished') 'restoring the rename source is refused as an owed round'

'## COH03 a reviewed Plan deleted after its round is not stale'
$repo = New-Case 'coh03' @{ 'doc.md' = "# Doc`n`nOne.`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
git -C $repo switch -q -c session
Write-Text "$repo/doc.md" "# Doc`n`nOne, edited.`n"
New-Item -ItemType Directory -Force "$repo/Documents/Plans/Area" | Out-Null
Write-Text "$repo/Documents/Plans/Area/Claimed.md" "# Claimed`n"
$s = Start-Round $repo $base @('-Landing')
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
Remove-Item -LiteralPath "$repo/Documents/Plans/Area/Claimed.md"
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-Baseline', $base, '-Tier', '2', '-Landing')
$done = $r.Text -match 'Review rounds: done'
git -C $repo add -A
git -C $repo commit -q -m session
$j = Invoke-Landing $repo $base
"planner done: $done"
Assert ($done -and $j.status -eq 'pass') 'the planner reports done and the landing gate passes'

'## ADV002 a Coretide rebase onto an upstream AGENTS.md edit'
$repo = New-Case 'adv002' @{ 'doc.md' = "# Doc`n" }
$base = (git -C $repo rev-parse HEAD).Trim()
$bare = Join-Path $top 'adv002-coretide.git'
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
$s = Start-Round $repo $base @('-Landing')
"round owes $(Get-Owed $s.Text)"
[void] (Close-Round $repo $s.Sha ($s.Template -replace '<severity>', 'none'))
git -C $clone commit -q -am 'session notes'
Write-Text "$seed/AGENTS.md" "# Coretide`n`nOne rule, upstream edit.`n"
git -C $seed commit -q -am upstream; git -C $seed push -q origin main 2>$null
git -C $clone fetch -q origin
git -C $clone rebase -q origin/main
$candidate = (git -C $clone rev-parse HEAD).Trim()
$a = pwsh -NoProfile -NonInteractive -Command "Import-Module '$inventoryModule'; `$r = Test-ReviewRoundCoverage -RepositoryRoot '$repo' -CoretideCandidate $candidate; if (`$null -eq `$r) { 'none' } else { `"`$(`$r.Code) `$(`$r.Message)`" }"
"gate: $a"
Assert ("$a".Trim() -eq 'none') 'the rebased clone commit of the reviewed bytes passes'
