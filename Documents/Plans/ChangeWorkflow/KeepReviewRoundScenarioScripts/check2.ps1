# Acceptance check 2: round snapshots for a stage that does not land.
. (Join-Path $PSScriptRoot 'lib.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'repo'))
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
Assert ($r.Code -eq 2) 'close with -Head at the baseline exits 2'
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
