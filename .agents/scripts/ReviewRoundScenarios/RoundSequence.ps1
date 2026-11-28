# A sequence of review rounds: round snapshots for a stage that does not land, the loop rules with -Landing, the
# landing gate, and Coretide snapshots and the split save. The round snapshot, loop rule, landing gate, and Coretide
# snapshot parts each continue in the repository the previous one left, so they stay one file; the second repository
# and split save parts start their own.
param([Parameter(Mandatory)][string] $ScratchRoot)
. (Join-Path $PSScriptRoot 'ScenarioCommon.ps1')

'# Round snapshots for a stage that does not land'
$repo = Join-Path $ScratchRoot 'repo'
New-ScratchRepo $repo
Write-Text "$repo/.gitignore" "Temp/`nProjects/Coretide/`n"
Write-Text "$repo/a.cpp" "// Adds two values.`n#define FIRST 1`n#define SECOND 2`nint Add(int a, int b)`n{`n`treturn a + b;`n}`n"
Write-Text "$repo/b.cpp" "// Subtracts.`nint Sub(int a, int b)`n{`n`treturn a - b;`n}`n"
Write-Text "$repo/doc.md" "# Doc`n`nFirst paragraph.`n`nSecond paragraph.`n`nThird paragraph.`n"
New-Item -ItemType Directory -Force "$repo/sparse" | Out-Null
Write-Text "$repo/sparse/keep.txt" "keep`n"
git -C $repo add -A
git -C $repo commit -q -m baseline
$baseline = (git -C $repo rev-parse HEAD).Trim()
git -C $repo sparse-checkout set --no-cone '/*' '!/sparse/'
Write-Text "$repo/a.cpp" "// Adds two values.`n#define FIRST 1`n#define SECOND 2`nint Add(int a, int b)`n{`n`treturn a + b + 0;`n}`n"
Write-Text "$repo/doc.md" "# Doc`n`nFirst paragraph, edited.`n`nSecond paragraph.`n`nThird paragraph.`n"
Write-Text "$repo/new.md" "# New`n"
$head0 = (git -C $repo rev-parse HEAD).Trim()
$index0 = (git -C $repo ls-files -s --sparse | Out-String)
$status0 = (git -C $repo status --porcelain | Out-String)
"baseline $baseline"
"sparse ls-files -t: $((git -C $repo ls-files -t -- sparse/keep.txt) -join ' ')"

$common = @('-RepositoryRoot', $repo, '-Baseline', $baseline, '-Tier', '2')
$r = Invoke-Steps $common
Show 'default run 1' $r
Assert ($r.Code -eq 0 -and $r.Text -match '-StartRound' -and $r.Text -notmatch '; Head ') 'default run prints prepare rows and a -StartRound row and no review row'
Assert ($null -eq (Get-RefTip $repo)) 'ref stays absent after a default run'

$r = Invoke-Steps ($common + '-StartRound')
Show 'start round 1' $r
$sha = Get-RoundHead $r.Text
Assert ($null -ne $sha -and $sha -eq (Get-RefTip $repo)) 'review rows end Head <ref tip>'
Assert ($r.Text -match 'fresh reviewer /repo-code-review' -and $r.Text -match '/coherence-review coherence mode' -and $r.Text -notmatch 'mechanic /comment-review') 'round 1 prints /repo-code-review and /coherence-review and no /comment-review'
$tree = git -C $repo ls-tree -r $sha
$keepBlob = (git -C $repo rev-parse "${baseline}:sparse/keep.txt").Trim()
Assert ((@($tree) -join "`n") -match "$keepBlob\tsparse/keep.txt" -and (@($tree) -join "`n") -match "\tnew\.md") 'snapshot holds sparse/keep.txt at its baseline blob and new.md'
$msg = (git -C $repo log -1 --format=%B $sha | Out-String)
"snapshot message:"; $msg
Assert ($msg -match '(?m)^Owes: ' -and $msg -notmatch 'a\.cpp|doc\.md|new\.md|changed') 'message holds Owes lines and no scope text or file path'
Assert ((git -C $repo rev-parse HEAD).Trim() -eq $head0 -and (git -C $repo ls-files -s --sparse | Out-String) -eq $index0 -and (git -C $repo status --porcelain | Out-String) -eq $status0) 'HEAD, index, and status unchanged'
$first = $r.Text
$r = Invoke-Steps $common
Show 'default rerun (open round)' $r
Assert ($r.Text -eq $first) 'a default rerun prints the same rows'

$r = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $sha, '-Results', '/repo-code-review=none')
Show 'close missing coherence' $r
Assert ($r.Code -eq 2 -and $r.Text -match '/coherence-review') 'close missing a key exits 2 naming /coherence-review'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $baseline, '-Results', '/repo-code-review=none,/coherence-review=none')
Show 'close with wrong head' $r
Assert ($r.Code -eq 2 -and $r.Text -match 'is not round 1''s snapshot') 'close with -Head at the baseline exits 2'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $sha, '-Results', '/repo-code-review=none,/coherence-review=none')
Show 'close round 1' $r
$tip = Get-RefTip $repo
Assert ($r.Code -eq 0 -and $r.Text -match 'closed: done') 'close prints closed: done'
Assert ((git -C $repo rev-parse "$tip^").Trim() -eq $sha -and (git -C $repo rev-parse "$tip^{tree}").Trim() -eq (git -C $repo rev-parse "$sha^{tree}").Trim()) 'close parent is the snapshot with the same tree'
$cmsg = (git -C $repo log -1 --format=%B $tip | Out-String)
"close message:"; $cmsg
Assert ($cmsg -match '(?m)^Result: /repo-code-review none' -and $cmsg -match '(?m)^Next: done') 'close holds Result lines and Next: done'
git -C $repo cat-file -e $sha
Assert ($LASTEXITCODE -eq 0) 'snapshot object exists'

$r = Invoke-Steps $common
Show 'default run after round 1' $r
Assert ($r.Text -match '/update-claude-docs' -and $r.Text -match '-StartRound' -and $r.Text -notmatch 'new\.md') 'next default run prints /update-claude-docs and -StartRound and no row for new.md'
$r = Invoke-Steps ($common + '-StartRound')
Show 'start round 2' $r
$sha2 = Get-RoundHead $r.Text
Assert ($r.Text -match 'mechanic /comment-review' -and (git -C $repo log -1 --format=%B $sha2 | Out-String) -match 'Phase: words') 'round 2 is a words round with /comment-review'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $sha2, '-Results', '/comment-review=none')
Show 'close round 2' $r
$r = Invoke-Steps ($common + '-Landing')
Show 'default run -Landing after round 2' $r
Assert ($r.Text -match 'Review rounds: done at round 2') 'with -Landing: done at round 2'
$r = Invoke-Steps $common
Show 'default run without -Landing after round 2' $r
Assert ($r.Text -match '-StartRound' -and $r.Text -notmatch 'Review rounds: done') 'without -Landing: an acceptance round is owed'
$r = Invoke-Steps ($common + '-StartRound')
Show 'start round 3' $r
$sha3 = Get-RoundHead $r.Text
Assert ($r.Text -match '/verify-acceptance') 'round 3 prints /verify-acceptance'
$r = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $sha3, '-Results', '/verify-acceptance=none')
Show 'close round 3' $r
$r = Invoke-Steps $common
Show 'default run after round 3' $r
Assert ($r.Text -match 'Review rounds: done at round 3') 'done at round 3'
"baseline=$baseline"

'# Loop rules with -Landing (the acceptance rounds are above)'
$common = @('-RepositoryRoot', $repo, '-Baseline', $baseline, '-Tier', '2', '-Landing')

function Start-LoopRound([string] $Title) {
	$r = Invoke-Steps ($common + '-StartRound')
	Show $Title $r | Out-Host
	return [pscustomobject] @{ Text = $r.Text; Code = $r.Code; Sha = (Get-RoundHead $r.Text) }
}
function Close-LoopRound([string] $Sha, [string] $Results) {
	$r = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $Sha, '-Results', $Results)
	Show "close $Results" $r | Out-Host
	return $r
}
function Get-Owes([string] $Text) { return @([regex]::Matches($Text, "-Results '([^']*)'") | ForEach-Object { $_.Groups[1].Value })[0] -replace '<severity>', 'none' }
# Starts and closes rounds with none until the loop is done.
function Complete-Loop() {
	for ($i = 0; $i -lt 6; $i++) {
		$s = Start-LoopRound 'drain'
		if ($null -eq $s.Sha) { return }
		[void] (Close-LoopRound $s.Sha (Get-Owes $s.Text))
	}
}
function Get-Message([string] $Sha) { return (git -C $repo log -1 --format=%B $Sha | Out-String) }
function Edit-File([string] $Name, [string] $From, [string] $To) {
	$path = Join-Path $repo $Name
	$text = [IO.File]::ReadAllText($path)
	if (-not $text.Contains($From)) { throw "$Name lacks '$From'" }
	Write-Text $path $text.Replace($From, $To)
}

'## (a)'
Edit-File 'a.cpp' '// Adds two values.' '// Adds two integer values.'
$s = Start-LoopRound 'comment-only a.cpp'
Assert ($s.Text -match 'mechanic /comment-review: changed C\+\+ and GLSL;' -and $s.Text -notmatch '/repo-code-review' -and (Get-Message $s.Sha) -match 'Phase: words') 'comment-only a.cpp edit: words round, /comment-review with the first-pass scope, no /repo-code-review'
[void] (Close-LoopRound $s.Sha (Get-Owes $s.Text)); Complete-Loop
Edit-File 'a.cpp' "#define FIRST 1`n#define SECOND 2" '#define FIRST 1 #define SECOND 2'
$s = Start-LoopRound 'joined #define lines'
Assert ($s.Text -match 'fresh reviewer /repo-code-review') 'joining the #define lines prints /repo-code-review'
[void] (Close-LoopRound $s.Sha (Get-Owes $s.Text)); Complete-Loop
Edit-File 'b.cpp' '// Subtracts.' '// Subtracts two values.'
$s = Start-LoopRound 'comment-only b.cpp, new to the rounds'
Assert ($s.Text -match 'fresh reviewer /repo-code-review') 'comment-only edit of b.cpp that no round froze prints /repo-code-review'
[void] (Close-LoopRound $s.Sha (Get-Owes $s.Text)); Complete-Loop
Edit-File 'b.cpp' '// Subtracts two values.' '// Subtracts.'
$s = Start-LoopRound 'b.cpp restored to baseline bytes'
Assert ($null -ne $s.Sha -and $s.Text -match '/comment-review: changed C\+\+ and GLSL;') 'restoring b.cpp to baseline bytes prints a /comment-review round with the first-pass scope'
[void] (Close-LoopRound $s.Sha (Get-Owes $s.Text)); Complete-Loop

'## (b)'
Edit-File 'doc.md' 'Second paragraph.' 'Second paragraph, edited.'
$s1 = Start-LoopRound 'doc.md edit'
$c = Close-LoopRound $s1.Sha '/coherence-review=High'
Assert ($c.Text -match 'closed: repeat') 'High closes repeat'
$s2 = Start-LoopRound 'retest with no edit'
$row1 = ($s1.Text -split "`n" | Where-Object { $_ -match '/coherence-review coherence mode' }) -replace '; Head .*$', ''
$row2 = ($s2.Text -split "`n" | Where-Object { $_ -match '/coherence-review coherence mode' }) -replace '; Head .*$', ''
"row1: $row1"; "row2: $row2"
Assert ($null -ne $s2.Sha -and $row1 -eq $row2) 'with no edit -StartRound prints the same /coherence-review row'
$c = Close-LoopRound $s2.Sha '/coherence-review=High'
Edit-File 'doc.md' 'Third paragraph.' 'Third paragraph, edited.'
$s3 = Start-LoopRound 'second doc.md edit'
$reviewLines = @($s3.Text -split "`n" | Where-Object { $_ -match '; Head ' })
Assert ($reviewLines.Count -eq 1 -and $reviewLines[0] -match '/coherence-review coherence mode: changed non-C\+\+ artifacts;') 'next round prints only /coherence-review with the first-pass scope'
$c = Close-LoopRound $s3.Sha '/coherence-review=High'
Assert ($c.Text -match '/external-diagnose-bug') 'third consecutive High prints the /external-diagnose-bug row'
$r = Invoke-Steps $common
Show 'default run after third High' $r
Assert ($r.Text -match '/external-diagnose-bug') 'a default run repeats the diagnosis row'
Complete-Loop

'## (c)'
Edit-File 'doc.md' 'First paragraph, edited.' 'First paragraph, edited twice.'
$s = Start-LoopRound 'ordinary round'
$c = Close-LoopRound $s.Sha '/coherence-review=Medium'
Assert ($c.Text -match 'closed: check') 'ordinary Medium closes check'
Edit-File 'doc.md' 'edited twice.' 'edited thrice.'
$s = Start-LoopRound 'check round'
Assert ((Get-Message $s.Sha) -match 'Kind: check') 'round after the fix records Kind: check'
$c = Close-LoopRound $s.Sha '/coherence-review=Medium'
Assert ($c.Text -match 'closed: done' -and $c.Text -match '/create-follow-up-plans') 'check Medium closes done with the /create-follow-up-plans clause'
Complete-Loop
Edit-File 'doc.md' 'edited thrice.' 'edited four times.'
$s = Start-LoopRound 'ordinary round 2'
[void] (Close-LoopRound $s.Sha '/coherence-review=Low')
Edit-File 'doc.md' 'edited four times.' 'edited five times.'
$s = Start-LoopRound 'check round 2'
$c = Close-LoopRound $s.Sha '/coherence-review=High'
Assert ($c.Text -match 'closed: repeat') 'check High closes repeat'
Complete-Loop

'## (d)'
Edit-File 'a.cpp' 'return a + b + 0;' 'return a + b + 1;'
$s = Start-LoopRound 'code round'
$c = Close-LoopRound $s.Sha (Get-Owes $s.Text).Replace('/repo-code-review=none', '/repo-code-review=Medium')
Assert ($c.Text -match 'closed: check') 'code round Medium closes check'
Edit-File 'a.cpp' '// Adds two integer values.' '// Adds two int values.'
$s = Start-LoopRound 'comment-only fix after check'
Assert ((Get-Message $s.Sha) -match 'Phase: words' -and (Get-Message $s.Sha) -match 'Kind: check' -and $s.Text -match 'mechanic /comment-review') 'comment-only fix: words round, Kind: check, /comment-review'
[void] (Close-LoopRound $s.Sha (Get-Owes $s.Text)); Complete-Loop

'## (e)'
Edit-File 'doc.md' 'Second paragraph, edited.' 'Second paragraph, edited again.'
$s = Start-LoopRound 'tier 2 round'
$c = Close-LoopRound $s.Sha '/coherence-review=none,/foo=none'
Assert ($c.Code -eq 2 -and $c.Text -match 'printed no /foo row') 'adding /foo=none exits 2'
$c = Close-LoopRound $s.Sha '/coherence-review=none,/adversarial-review=High'
Assert ($c.Text -match 'closed: repeat') 'an added /adversarial-review=High closes repeat'
Complete-Loop
$r = Invoke-Steps $common
Show 'final default run' $r

'# The landing gate (the first repository''s loop is done with -Landing)'
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
$repo2 = Join-Path $ScratchRoot 'repo2'
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

'# Coretide snapshots (the first repository after the landing gate) and the split save'
$main = (git -C $repo rev-parse main).Trim()
function New-CoretideClone([string] $Bare, [string] $Engine) {
	git init -q --bare -b main $Bare
	$seed = "$Bare-seed"
	git clone -q $Bare $seed 2>$null
	git -C $seed config user.name scratch; git -C $seed config user.email scratch@example.invalid
	Write-Text "$seed/notes.md" "# Notes`n"
	git -C $seed add -A; git -C $seed commit -q -m seed; git -C $seed push -q origin main 2>$null
	git clone -q $Bare "$Engine/Projects/Coretide" 2>$null
	$clone = "$Engine/Projects/Coretide"
	git -C $clone config user.name scratch; git -C $clone config user.email scratch@example.invalid
	$top = [IO.Path]::GetFullPath((git -C $clone rev-parse --show-toplevel).Trim())
	if ($top -ne [IO.Path]::GetFullPath($clone)) { throw "clone toplevel mismatch $top" }
	return $clone
}

$clone = New-CoretideClone (Join-Path $ScratchRoot 'coretide.git') $repo
Write-Text "$clone/notes.md" "# Notes`n`nEdited in the session.`n"
$cHead = (git -C $clone rev-parse HEAD).Trim(); $cIndex = (git -C $clone ls-files -s | Out-String); $cStatus = (git -C $clone status --porcelain | Out-String)
$common = @('-RepositoryRoot', $repo, '-Baseline', $main, '-Tier', '2', '-Landing')
$r = Invoke-Steps ($common + '-StartRound')
Show 'start round with a Coretide clone' $r
$sha = Get-RoundHead $r.Text
$cTip = Get-RefTip $clone
$msg = (git -C $repo log -1 --format=%B $sha | Out-String)
Assert ($null -ne $cTip -and $msg -match "(?m)^Coretide-Snapshot: $cTip" -and $r.Text -match "Coretide head $cTip") 'engine message and rows name the clone ref tip as Coretide-Snapshot'
Assert ((git -C $clone rev-parse HEAD).Trim() -eq $cHead -and (git -C $clone ls-files -s | Out-String) -eq $cIndex -and (git -C $clone status --porcelain | Out-String) -eq $cStatus) 'clone HEAD, index, and status unchanged'
$inv = pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); (Get-SessionChangeInventory -RepositoryRoot $(ConvertTo-Literal $repo) -Baseline $main -Head $sha).coretide.headSha"
Assert ("$inv".Trim() -eq $cTip) "inventory -Head <engine snapshot> reports coretide.headSha $inv"
Assert ((git -C $repo log --format=%B refs/worktree/review-rounds | Out-String) -notmatch 'Projects/Coretide|notes\.md') 'engine chain messages name no Coretide path'
$owes = ([regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value) -replace '<severity>', 'none'
$c = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $sha, '-Results', $owes)
Show 'close' $c
for ($i = 0; $i -lt 4; $i++) {
	$r = Invoke-Steps ($common + '-StartRound')
	$s = Get-RoundHead $r.Text
	if ($null -eq $s) { break }
	Show 'drain' $r
	[void] (Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $s, '-Results', (([regex]::Match($r.Text, "-Results '([^']*)'").Groups[1].Value) -replace '<severity>', 'none')))
}
Show 'loop state' (Invoke-Steps $common)
git -C $clone commit -q -am 'session notes'
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'pass') 'clone commit of the reviewed bytes: pass'
git -C $clone reset -q --hard HEAD~1
Write-Text "$clone/notes.md" "# Notes`n`nDifferent bytes.`n"
git -C $clone commit -q -am 'different notes'
$j = Invoke-Landing $repo $main
Assert ($j.status -eq 'blocked' -and $j.code -eq 'inventory.review-round-unfinished') 'clone commit of different bytes: an owed round'

'## split save'
$repo3 = Join-Path $ScratchRoot 'repo3'
New-ScratchRepo $repo3
Write-Text "$repo3/.gitignore" "Temp/`nProjects/Coretide/`n"
Write-Text "$repo3/doc.md" "# Doc`n"
git -C $repo3 add -A; git -C $repo3 commit -q -m baseline
$main3 = (git -C $repo3 rev-parse HEAD).Trim()
$clone3 = New-CoretideClone (Join-Path $ScratchRoot 'coretide3.git') $repo3
git -C $repo3 switch -q -c session
foreach ($root in @($repo3, $clone3)) {
	New-Item -ItemType Directory -Force "$root/Documents/Plans/ChangeWorkflow" | Out-Null
	Write-Text "$root/Documents/Plans/ChangeWorkflow/Saved.md" "# Saved`n"
	git -C $root add -A; git -C $root commit -q -m saved
}
$session3 = (git -C $repo3 rev-parse session).Trim()
$cand3 = (git -C $clone3 rev-parse HEAD).Trim()
$both = 'Documents/Plans/ChangeWorkflow/Saved.md,Projects/Coretide/Documents/Plans/ChangeWorkflow/Saved.md'
$call = { param([string] $Arguments) pwsh -NoProfile -NonInteractive -Command "Import-Module $(ConvertTo-Literal $inventoryModule); `$r = Test-ReviewRoundCoverage -RepositoryRoot $(ConvertTo-Literal $repo3) $Arguments; if (`$null -eq `$r) { 'none' } else { `$r.Code }" }
$a = & $call "-EngineBase $main3 -EngineCandidate $session3 -CoretideCandidate $cand3 -SavePlanPaths '$both'"
Assert ("$a".Trim() -eq 'none') "both sides with both paths authorized: $a"
$a = & $call "-EngineBase $main3 -EngineCandidate $session3 -SavePlanPaths '$both'"
Assert ("$a".Trim() -eq 'none') "engine side alone with both paths authorized: $a"
$a = & $call "-EngineBase $main3 -EngineCandidate $session3 -SavePlanPaths 'Projects/Coretide/Documents/Plans/ChangeWorkflow/Saved.md'"
Assert ("$a".Trim() -eq 'inventory.review-round-missing') "engine side alone with only the Coretide path authorized: $a"
