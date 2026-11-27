# Acceptance check 3: loop rules, continuing in check 2's repository with -Landing (acceptance rounds are check 2's).
. (Join-Path $PSScriptRoot 'lib.ps1')
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'repo'))
$baseline = (git -C $repo rev-list --max-parents=0 HEAD).Trim()
$common = @('-RepositoryRoot', $repo, '-Baseline', $baseline, '-Tier', '2', '-Landing')

function Start-Round([string] $Title) {
	$r = Invoke-Steps ($common + '-StartRound')
	Show $Title $r | Out-Host
	return [pscustomobject] @{ Text = $r.Text; Code = $r.Code; Sha = (Get-RoundHead $r.Text) }
}
function Close-Round([string] $Sha, [string] $Results) {
	$r = Invoke-Steps @('-RepositoryRoot', $repo, '-CloseRound', '-Head', $Sha, '-Results', $Results)
	Show "close $Results" $r | Out-Host
	return $r
}
function Get-Owes([string] $Text) { return @([regex]::Matches($Text, "-Results '([^']*)'") | ForEach-Object { $_.Groups[1].Value })[0] -replace '<severity>', 'none' }
# Starts and closes rounds with none until the loop is done.
function Complete-Loop() {
	for ($i = 0; $i -lt 6; $i++) {
		$s = Start-Round 'drain'
		if ($null -eq $s.Sha) { return }
		[void] (Close-Round $s.Sha (Get-Owes $s.Text))
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
$s = Start-Round 'comment-only a.cpp'
Assert ($s.Text -match 'mechanic /comment-review: changed C\+\+ and GLSL;' -and $s.Text -notmatch '/repo-code-review' -and (Get-Message $s.Sha) -match 'Phase: words') 'comment-only a.cpp edit: words round, /comment-review with the first-pass scope, no /repo-code-review'
[void] (Close-Round $s.Sha (Get-Owes $s.Text)); Complete-Loop
Edit-File 'a.cpp' "#define FIRST 1`n#define SECOND 2" '#define FIRST 1 #define SECOND 2'
$s = Start-Round 'joined #define lines'
Assert ($s.Text -match 'fresh reviewer /repo-code-review') 'joining the #define lines prints /repo-code-review'
[void] (Close-Round $s.Sha (Get-Owes $s.Text)); Complete-Loop
Edit-File 'b.cpp' '// Subtracts.' '// Subtracts two values.'
$s = Start-Round 'comment-only b.cpp, new to the rounds'
Assert ($s.Text -match 'fresh reviewer /repo-code-review') 'comment-only edit of b.cpp that no round froze prints /repo-code-review'
[void] (Close-Round $s.Sha (Get-Owes $s.Text)); Complete-Loop
Edit-File 'b.cpp' '// Subtracts two values.' '// Subtracts.'
$s = Start-Round 'b.cpp restored to baseline bytes'
Assert ($null -ne $s.Sha -and $s.Text -match '/comment-review: changed C\+\+ and GLSL;') 'restoring b.cpp to baseline bytes prints a /comment-review round with the first-pass scope'
[void] (Close-Round $s.Sha (Get-Owes $s.Text)); Complete-Loop

'## (b)'
Edit-File 'doc.md' 'Second paragraph.' 'Second paragraph, edited.'
$s1 = Start-Round 'doc.md edit'
$c = Close-Round $s1.Sha '/coherence-review=High'
Assert ($c.Text -match 'closed: repeat') 'High closes repeat'
$s2 = Start-Round 'retest with no edit'
$row1 = ($s1.Text -split "`n" | Where-Object { $_ -match '/coherence-review coherence mode' }) -replace '; Head .*$', ''
$row2 = ($s2.Text -split "`n" | Where-Object { $_ -match '/coherence-review coherence mode' }) -replace '; Head .*$', ''
"row1: $row1"; "row2: $row2"
Assert ($null -ne $s2.Sha -and $row1 -eq $row2) 'with no edit -StartRound prints the same /coherence-review row'
$c = Close-Round $s2.Sha '/coherence-review=High'
Edit-File 'doc.md' 'Third paragraph.' 'Third paragraph, edited.'
$s3 = Start-Round 'second doc.md edit'
$reviewLines = @($s3.Text -split "`n" | Where-Object { $_ -match '; Head ' })
Assert ($reviewLines.Count -eq 1 -and $reviewLines[0] -match '/coherence-review coherence mode: changed non-C\+\+ artifacts;') 'next round prints only /coherence-review with the first-pass scope'
$c = Close-Round $s3.Sha '/coherence-review=High'
Assert ($c.Text -match '/external-diagnose-bug') 'third consecutive High prints the /external-diagnose-bug row'
$r = Invoke-Steps $common
Show 'default run after third High' $r
Assert ($r.Text -match '/external-diagnose-bug') 'a default run repeats the diagnosis row'
Complete-Loop

'## (c)'
Edit-File 'doc.md' 'First paragraph, edited.' 'First paragraph, edited twice.'
$s = Start-Round 'ordinary round'
$c = Close-Round $s.Sha '/coherence-review=Medium'
Assert ($c.Text -match 'closed: check') 'ordinary Medium closes check'
Edit-File 'doc.md' 'edited twice.' 'edited thrice.'
$s = Start-Round 'check round'
Assert ((Get-Message $s.Sha) -match 'Kind: check') 'round after the fix records Kind: check'
$c = Close-Round $s.Sha '/coherence-review=Medium'
Assert ($c.Text -match 'closed: done' -and $c.Text -match '/create-follow-up-plans') 'check Medium closes done with the /create-follow-up-plans clause'
Complete-Loop
Edit-File 'doc.md' 'edited thrice.' 'edited four times.'
$s = Start-Round 'ordinary round 2'
[void] (Close-Round $s.Sha '/coherence-review=Low')
Edit-File 'doc.md' 'edited four times.' 'edited five times.'
$s = Start-Round 'check round 2'
$c = Close-Round $s.Sha '/coherence-review=High'
Assert ($c.Text -match 'closed: repeat') 'check High closes repeat'
Complete-Loop

'## (d)'
Edit-File 'a.cpp' 'return a + b + 0;' 'return a + b + 1;'
$s = Start-Round 'code round'
$c = Close-Round $s.Sha (Get-Owes $s.Text).Replace('/repo-code-review=none', '/repo-code-review=Medium')
Assert ($c.Text -match 'closed: check') 'code round Medium closes check'
Edit-File 'a.cpp' '// Adds two integer values.' '// Adds two int values.'
$s = Start-Round 'comment-only fix after check'
Assert ((Get-Message $s.Sha) -match 'Phase: words' -and (Get-Message $s.Sha) -match 'Kind: check' -and $s.Text -match 'mechanic /comment-review') 'comment-only fix: words round, Kind: check, /comment-review'
[void] (Close-Round $s.Sha (Get-Owes $s.Text)); Complete-Loop

'## (e)'
Edit-File 'doc.md' 'Second paragraph, edited.' 'Second paragraph, edited again.'
$s = Start-Round 'tier 2 round'
$c = Close-Round $s.Sha '/coherence-review=none,/foo=none'
Assert ($c.Code -eq 2) 'adding /foo=none exits 2'
$c = Close-Round $s.Sha '/coherence-review=none,/adversarial-review=High'
Assert ($c.Text -match 'closed: repeat') 'an added /adversarial-review=High closes repeat'
Complete-Loop
$r = Invoke-Steps $common
Show 'final default run' $r
