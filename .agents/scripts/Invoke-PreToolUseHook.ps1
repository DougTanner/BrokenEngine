# Claude Code PreToolUse hook for the Bash and PowerShell tools, registered in
# .claude/settings.json. Reads the hook JSON from stdin and blocks the shell
# forms the root AGENTS.md rules forbid:
#   - directory change: cd, chdir, pushd, popd, Set-Location, sl,
#     Push-Location, or Pop-Location as a command word;
#   - -ExecutionPolicy Bypass, including the -ep and -ex... abbreviations;
#   - a .ps1 or .psm1 path rooted under the input cwd but outside <cwd>/Temp/;
#   - git stash bare, pop, save, clear, push without both -u and -m, or apply
#     without a hexadecimal SHA;
#   - sed -i/--in-place or perl -i on a file outside Temp/;
#   - a deletion whose targets are computed rather than written literally:
#     rm, rmdir, Remove-Item and its aliases, git rm, or git clean with a
#     wildcard, a [...] pair (allowed in a -LiteralPath value or a quoted Bash
#     rm argument), -Include/-Exclude/-Filter, a variable or substitution,
#     Bash brace expansion, a PowerShell (...) or splat argument, or pipeline
#     input; git clean without an exact path; find -delete or -exec rm; xargs rm.
# Each command is tokenized per shell, so quoted text, here-strings, and heredoc
# bodies never count as commands; the script-path and deletion checks still
# read quoted arguments. A command inside a quoted string (bash -c '...',
# pwsh -Command "...") or inside a script the command runs is not inspected:
# the hook backs up the prose rules rather than replacing them.
# Exit codes:
#   0 - allowed, including every non-shell tool, input that cannot be parsed,
#       and any unexpected script error
#   2 - blocked; one stderr line names the rule and the correct form

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

class ShellWord {
	[string] $Raw = ''
	[string] $Value = ''
	# Value with quoted or escaped characters as U+0001 and each variable or
	# substitution as U+0002, so glob and brace checks see only unquoted text.
	[string] $View = ''
	[bool] $HasVariable
	[bool] $HasSubstitution
	[bool] $IsSplat
	[bool] $IsRedirectTarget
}

# Produces words and operator strings (';', '&&', '||', '|', '&', newline,
# '(', ')', '{', '}'). Throws on text whose extent it cannot find: an
# unterminated quote, here-string, heredoc, or substitution.
class ShellLexer {
	static [regex] $BashRedirect = [regex]::new('\G(?:\d+|&)?(?:>>?|<<<|<)(?:&(?:\d+|-)?)?')
	static [regex] $PowerShellRedirect = [regex]::new('\G(?:\d+|\*)?(?:>>?|<)(?:&\d+)?')

	[string] $S
	[int] $I = 0
	[bool] $Bash
	[char] $Escape
	[System.Collections.Generic.List[hashtable]] $Heredocs = [System.Collections.Generic.List[hashtable]]::new()
	# Token lists of every $(...) and Bash backtick substitution, nested ones included.
	[System.Collections.Generic.List[object]] $Nested = [System.Collections.Generic.List[object]]::new()

	ShellLexer([string] $Text, [bool] $IsBash) {
		$this.S = $Text
		$this.Bash = $IsBash
		$this.Escape = if ($IsBash) { [char]'\' } else { [char]'`' }
	}

	[char] Peek([int] $Offset) {
		$index = $this.I + $Offset
		if ($index -lt $this.S.Length) { return $this.S[$index] }
		return [char]0
	}

	[int] LineEndLength([int] $Index) {
		if ($Index -lt $this.S.Length -and $this.S[$Index] -eq "`n") { return 1 }
		if ($Index + 1 -lt $this.S.Length -and $this.S[$Index] -eq "`r" -and $this.S[$Index + 1] -eq "`n") { return 2 }
		return 0
	}

	[bool] IsWordEnd([char] $C) {
		if (" `t`r`n;&|()<>".IndexOf($C) -ge 0) { return $true }
		return -not $this.Bash -and ($C -eq '{' -or $C -eq '}')
	}

	[System.Collections.Generic.List[object]] ReadTokens([bool] $UntilParen) {
		$tokens = [System.Collections.Generic.List[object]]::new()
		$depth = 0
		$expectTarget = $false
		while ($this.I -lt $this.S.Length) {
			$c = $this.S[$this.I]
			$lineEnd = $this.LineEndLength($this.I + 1)
			if ($c -eq $this.Escape -and $lineEnd -gt 0) { $this.I += 1 + $lineEnd; continue }
			if ($c -eq ' ' -or $c -eq "`t" -or $c -eq "`r") { $this.I++; continue }
			if ($c -eq "`n") {
				$this.I++
				$tokens.Add("`n")
				$expectTarget = $false
				$this.ReadHeredocBodies()
				continue
			}
			if ($c -eq '#') {
				while ($this.I -lt $this.S.Length -and $this.S[$this.I] -ne "`n") { $this.I++ }
				continue
			}
			if (-not $this.Bash -and $c -eq '<' -and $this.Peek(1) -eq '#') {
				$end = $this.S.IndexOf('#>', $this.I + 2)
				if ($end -lt 0) { throw 'unterminated block comment' }
				$this.I = $end + 2
				continue
			}
			$pair = if ($this.I + 1 -lt $this.S.Length) { $this.S.Substring($this.I, 2) } else { '' }
			if ($pair -eq '&&' -or $pair -eq '||') {
				$tokens.Add($pair)
				$this.I += 2
				$expectTarget = $false
				continue
			}
			if ($this.Bash -and $pair -eq '<<' -and $this.Peek(2) -ne '<') {
				$this.ReadHeredocStart()
				continue
			}
			$redirectPattern = if ($this.Bash) { [ShellLexer]::BashRedirect } else { [ShellLexer]::PowerShellRedirect }
			$redirect = $redirectPattern.Match($this.S, $this.I)
			if ($redirect.Success) {
				$this.I += $redirect.Length
				$expectTarget = $redirect.Value -notmatch '&(\d+|-)$'
				continue
			}
			if ($c -eq '|') {
				$tokens.Add('|')
				$this.I += $(if ($this.Bash -and $this.Peek(1) -eq '&') { 2 } else { 1 })
				$expectTarget = $false
				continue
			}
			if ($c -eq ';' -or ($c -eq '&' -and $this.Bash)) {
				$tokens.Add([string]$c)
				$this.I++
				$expectTarget = $false
				continue
			}
			if ($c -eq '(') {
				$tokens.Add('(')
				$depth++
				$this.I++
				$expectTarget = $false
				continue
			}
			if ($c -eq ')') {
				$this.I++
				if ($UntilParen -and $depth -eq 0) { return $tokens }
				$tokens.Add(')')
				$depth--
				$expectTarget = $false
				continue
			}
			if (-not $this.Bash) {
				# The PowerShell call operator, kept as a word so the command after it is found.
				if ($c -eq '&') {
					$call = [ShellWord]::new()
					$call.Raw = '&'
					$call.Value = '&'
					$call.View = '&'
					$tokens.Add($call)
					$this.I++
					continue
				}
				if ($c -eq '@' -and ($this.Peek(1) -eq '(' -or $this.Peek(1) -eq '{')) {
					$open = [string]$this.Peek(1)
					$tokens.Add($open)
					if ($open -eq '(') { $depth++ }
					$this.I += 2
					$expectTarget = $false
					continue
				}
				if ($c -eq '{' -or $c -eq '}') {
					$tokens.Add([string]$c)
					$this.I++
					$expectTarget = $false
					continue
				}
			}
			elseif (($c -eq '{' -or $c -eq '}') -and ($this.I + 1 -ge $this.S.Length -or " `t`r`n;&|)".IndexOf($this.S[$this.I + 1]) -ge 0)) {
				# Bash braces group commands only as separate words; {a,b} stays a word.
				$tokens.Add([string]$c)
				$this.I++
				$expectTarget = $false
				continue
			}
			$word = $this.ReadWord()
			$word.IsRedirectTarget = $expectTarget
			$expectTarget = $false
			$tokens.Add($word)
		}
		if ($UntilParen) { throw 'unterminated substitution' }
		if ($this.Heredocs.Count -gt 0) { throw 'unterminated heredoc' }
		return $tokens
	}

	[void] ReadHeredocStart() {
		$this.I += 2
		$stripTabs = $this.Peek(0) -eq '-'
		if ($stripTabs) { $this.I++ }
		while ($this.Peek(0) -eq ' ' -or $this.Peek(0) -eq "`t") { $this.I++ }
		if ($this.I -ge $this.S.Length -or $this.IsWordEnd($this.S[$this.I])) { throw 'missing heredoc delimiter' }
		$delimiter = $this.ReadWord()
		$this.Heredocs.Add(@{ Delimiter = $delimiter.Value; StripTabs = $stripTabs })
	}

	# Skips each pending heredoc body; called just past the newline that ends
	# the line holding the heredoc operators.
	[void] ReadHeredocBodies() {
		foreach ($heredoc in $this.Heredocs) {
			while ($true) {
				if ($this.I -ge $this.S.Length) { throw 'unterminated heredoc' }
				$newline = $this.S.IndexOf("`n", $this.I)
				$end = if ($newline -lt 0) { $this.S.Length } else { $newline }
				$line = $this.S.Substring($this.I, $end - $this.I).TrimEnd([char]13)
				if ($heredoc.StripTabs) { $line = $line.TrimStart([char]9) }
				$this.I = if ($newline -lt 0) { $this.S.Length } else { $newline + 1 }
				if ($line -ceq $heredoc.Delimiter) { break }
			}
		}
		$this.Heredocs.Clear()
	}

	[ShellWord] ReadWord() {
		$word = [ShellWord]::new()
		$start = $this.I
		$value = [Text.StringBuilder]::new()
		$view = [Text.StringBuilder]::new()
		if (-not $this.Bash -and $this.S[$this.I] -eq '@') {
			$quote = $this.Peek(1)
			if (($quote -eq "'" -or $quote -eq '"') -and $this.IsHereStringStart($this.I + 2)) {
				$open = $this.S.IndexOf("`n", $this.I)
				$close = $this.S.IndexOf("`n$quote@", $open)
				if ($close -lt 0) { throw 'unterminated here-string' }
				$value.Append($this.S, $open + 1, $close - $open - 1)
				$view.Append([char]1)
				$this.I = $close + 3
				$word.Raw = $this.S.Substring($start, $this.I - $start)
				$word.Value = $value.ToString()
				$word.View = $view.ToString()
				return $word
			}
			if ([char]::IsLetterOrDigit($quote) -or $quote -eq '_') { $word.IsSplat = $true }
		}
		while ($this.I -lt $this.S.Length) {
			$c = $this.S[$this.I]
			if ($this.IsWordEnd($c)) { break }
			if ($c -eq "'") { $this.ReadSingleQuoted($value, $view) }
			elseif ($c -eq '"') { $this.ReadDoubleQuoted($word, $value, $view) }
			elseif ($c -eq $this.Escape) {
				$lineEnd = $this.LineEndLength($this.I + 1)
				if ($lineEnd -gt 0) { $this.I += 1 + $lineEnd }
				elseif ($this.I + 1 -lt $this.S.Length) {
					$value.Append($this.S[$this.I + 1])
					$view.Append([char]1)
					$this.I += 2
				}
				else {
					$value.Append($c)
					$view.Append($c)
					$this.I++
				}
			}
			elseif ($c -eq '$') { $this.ReadDollar($word, $value, $view) }
			elseif ($this.Bash -and $c -eq '`') { $this.ReadBacktick($word, $value, $view) }
			else {
				$value.Append($c)
				$view.Append($c)
				$this.I++
			}
		}
		if ($this.I -eq $start) {
			$value.Append($this.S[$this.I])
			$view.Append($this.S[$this.I])
			$this.I++
		}
		$word.Raw = $this.S.Substring($start, $this.I - $start)
		$word.Value = $value.ToString()
		$word.View = $view.ToString()
		return $word
	}

	[bool] IsHereStringStart([int] $Index) {
		while ($Index -lt $this.S.Length -and " `t`r".IndexOf($this.S[$Index]) -ge 0) { $Index++ }
		return $Index -lt $this.S.Length -and $this.S[$Index] -eq "`n"
	}

	[void] ReadSingleQuoted([Text.StringBuilder] $Value, [Text.StringBuilder] $View) {
		while ($true) {
			$close = $this.S.IndexOf("'", $this.I + 1)
			if ($close -lt 0) { throw 'unterminated quote' }
			$length = $close - $this.I - 1
			$Value.Append($this.S, $this.I + 1, $length)
			$View.Append([char]1, $length)
			$this.I = $close + 1
			# PowerShell reads '' inside a single-quoted string as one literal quote.
			if ($this.Bash -or $this.Peek(0) -ne "'") { return }
			$Value.Append("'")
			$View.Append([char]1)
		}
	}

	[void] ReadDoubleQuoted([ShellWord] $Word, [Text.StringBuilder] $Value, [Text.StringBuilder] $View) {
		$this.I++
		while ($true) {
			if ($this.I -ge $this.S.Length) { throw 'unterminated quote' }
			$c = $this.S[$this.I]
			if ($c -eq '"') {
				if ($this.Bash -or $this.Peek(1) -ne '"') {
					$this.I++
					return
				}
				$Value.Append('"')
				$View.Append([char]1)
				$this.I += 2
			}
			elseif ($c -eq $this.Escape) {
				$next = $this.Peek(1)
				if ($next -eq "`n") { $this.I += 2 }
				elseif ($this.Bash -and '$`"\'.IndexOf($next) -lt 0) {
					$Value.Append($c)
					$View.Append([char]1)
					$this.I++
				}
				else {
					$Value.Append($next)
					$View.Append([char]1)
					$this.I += 2
				}
			}
			elseif ($c -eq '$') { $this.ReadDollar($Word, $Value, $View) }
			elseif ($this.Bash -and $c -eq '`') { $this.ReadBacktick($Word, $Value, $View) }
			else {
				$Value.Append($c)
				$View.Append([char]1)
				$this.I++
			}
		}
	}

	[void] ReadDollar([ShellWord] $Word, [Text.StringBuilder] $Value, [Text.StringBuilder] $View) {
		$start = $this.I
		$next = $this.Peek(1)
		if ($next -eq '(') {
			$this.I += 2
			$this.Nested.Add($this.ReadTokens($true))
			$Word.HasSubstitution = $true
		}
		elseif ($next -eq '{') {
			$close = $this.S.IndexOf('}', $this.I + 2)
			if ($close -lt 0) { throw 'unterminated variable' }
			$this.I = $close + 1
			$Word.HasVariable = $true
		}
		elseif ([char]::IsLetterOrDigit($next) -or $next -eq '_') {
			$end = $this.I + 1
			while ($end -lt $this.S.Length -and ([char]::IsLetterOrDigit($this.S[$end]) -or $this.S[$end] -eq '_' -or
					(-not $this.Bash -and $this.S[$end] -eq ':' -and $end + 1 -lt $this.S.Length -and [char]::IsLetterOrDigit($this.S[$end + 1])))) {
				$end++
			}
			$name = $this.S.Substring($this.I + 1, $end - $this.I - 1)
			$this.I = $end
			# $true, $false, and $null are PowerShell constants, as in -Confirm:$false.
			if (-not $this.Bash -and @('true', 'false', 'null') -contains $name) {
				$Value.Append('$' + $name)
				$View.Append('$' + $name)
				return
			}
			$Word.HasVariable = $true
		}
		else {
			$Value.Append('$')
			$View.Append('$')
			$this.I++
			return
		}
		$Value.Append($this.S, $start, $this.I - $start)
		$View.Append([char]2)
	}

	[void] ReadBacktick([ShellWord] $Word, [Text.StringBuilder] $Value, [Text.StringBuilder] $View) {
		$end = $this.I + 1
		while ($end -lt $this.S.Length -and $this.S[$end] -ne '`') {
			if ($this.S[$end] -eq '\') { $end++ }
			$end++
		}
		if ($end -ge $this.S.Length) { throw 'unterminated substitution' }
		$inner = [ShellLexer]::new($this.S.Substring($this.I + 1, $end - $this.I - 1), $true)
		$this.Nested.Add($inner.ReadTokens($false))
		$this.Nested.AddRange($inner.Nested)
		$Value.Append($this.S, $this.I, $end + 1 - $this.I)
		$View.Append([char]2)
		$Word.HasSubstitution = $true
		$this.I = $end + 1
	}
}

$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:LeadingKeywords = @('if', 'elif', 'then', 'else', 'do', 'while', 'until', '!', 'time')
$script:DirectoryCommands = @('cd', 'chdir', 'pushd', 'popd', 'set-location', 'sl', 'push-location', 'pop-location')
$script:DeletionCommands = @('rm', 'rmdir', 'remove-item', 'ri', 'del', 'erase', 'rd')
$script:RootedPattern = '^(?:[A-Za-z]:[\\/]|[\\/]|~(?:[\\/]|$))'
$script:ScriptPathPattern = [regex]::new('(?i)(?:^|(?<=[\s''"&=(,;|]))((?:[a-z]:[\\/]|~[\\/]|[\\/])[^\s''"<>|;&()]*\.psm?1)(?=$|[\s''",;)|&])')
$script:Messages = @{
	DirectoryChange = 'do not change the working directory (cd, Set-Location, Push-Location, or their aliases); run every command from the worktree root and address files by path.'
	ExecutionPolicy = 'never use -ExecutionPolicy Bypass; run a repository script as pwsh -NoProfile -File <repo-relative script path> [arguments].'
	AbsoluteScriptPath = 'never run a repository script by absolute path; use pwsh -NoProfile -File <repo-relative script path> [arguments] from the worktree root.'
	Stash = 'the git stash stack is shared by every worktree; use only git stash push -u -m <unique-tag>, git stash list, git stash apply <sha>, and git stash drop.'
	InPlaceEdit = 'do not edit files in place with sed or perl outside Temp/; change tracked files with the host Edit tool.'
	ComputedDeletion = 'delete only by exact literal paths, never by wildcard, pattern filter, pipeline, find, xargs, variable, or substitution; name each path literally (in PowerShell, -LiteralPath ''<path>''); a directory may be deleted recursively.'
}

function Write-Stderr([string] $Message) {
	$stderr = [Console]::OpenStandardError()
	$bytes = $script:Utf8.GetBytes($Message)
	$stderr.Write($bytes, 0, $bytes.Length)
	$stderr.Flush()
}

function Get-WordsFrom($Words, [int] $Start) {
	return , $Words.GetRange($Start, $Words.Count - $Start)
}

# An element is the run of words between two operators; CommandIndex points at
# its command word past leading keywords, call operators, and assignments.
function New-ShellElement($Words, [string] $PrecededBy, [string] $FollowedBy, [bool] $IsBash) {
	$index = 0
	while ($index -lt $Words.Count) {
		$raw = $Words[$index].Raw
		if ($script:LeadingKeywords -contains $raw -or $raw -eq '&' -or $raw -eq '.') { $index++ }
		elseif ($raw -match '^\$[\w:]+$' -and $index + 1 -lt $Words.Count -and $Words[$index + 1].Raw -eq '=') {
			$index += 2
		}
		elseif ($IsBash -and $raw -match '^[A-Za-z_]\w*=') { $index++ }
		else { break }
	}
	$name = ''
	if ($index -lt $Words.Count) {
		$word = $Words[$index]
		# A quoted PowerShell word not after & is a string expression, not a command.
		$isExpression = -not $IsBash -and ($word.Raw.StartsWith("'") -or $word.Raw.StartsWith('"')) -and
			($index -eq 0 -or $Words[$index - 1].Raw -ne '&')
		if (-not $isExpression) { $name = $word.Value.ToLowerInvariant() -replace '\.exe$', '' }
	}
	else { $index = -1 }
	return [pscustomobject]@{
		Words = $Words
		PrecededBy = $PrecededBy
		FollowedBy = $FollowedBy
		CommandIndex = $index
		Name = $name
	}
}

function Get-ShellElements([System.Collections.Generic.List[object]] $Tokens, [bool] $IsBash) {
	$elements = [System.Collections.Generic.List[object]]::new()
	$groups = [System.Collections.Generic.Stack[string]]::new()
	$words = $null
	$precededBy = ''
	foreach ($token in $Tokens) {
		if ($token -is [string]) {
			if ($token -eq '(' -or $token -eq '{') { $groups.Push($token) }
			elseif ($token -eq ')' -or $token -eq '}') {
				$open = if ($token -eq ')') { '(' } else { '{' }
				if ($groups.Count -eq 0 -or $groups.Pop() -ne $open) { throw 'unbalanced group' }
			}
			if ($null -ne $words) {
				$elements.Add((New-ShellElement $words $precededBy $token $IsBash))
				$words = $null
				$precededBy = $token
			}
			elseif ($token -ne "`n") { $precededBy = $token }
			continue
		}
		if ($token.IsRedirectTarget) { continue }
		if ($null -eq $words) { $words = [System.Collections.Generic.List[object]]::new() }
		$words.Add($token)
	}
	if ($groups.Count -gt 0) { throw 'unbalanced group' }
	if ($null -ne $words) { $elements.Add((New-ShellElement $words $precededBy '' $IsBash)) }
	return , $elements
}

function ConvertTo-ComparablePath([string] $Path, [string] $BasePath) {
	if ($Path -match '^[\\/]([A-Za-z])(?=[\\/]|$)') { $Path = $Matches[1] + ':' + $Path.Substring(2) }
	elseif ($Path -match '^~(?=[\\/]|$)') { $Path = $HOME + $Path.Substring(1) }
	return [IO.Path]::GetFullPath($Path, $BasePath).Replace('\', '/').TrimEnd('/')
}

function Test-UnderDirectory([string] $Path, [string] $Directory) {
	return $Path.StartsWith($Directory + '/', [StringComparison]::OrdinalIgnoreCase)
}

function Test-AbsoluteScriptPath([string] $Text, [string] $CwdPath) {
	$candidates = [System.Collections.Generic.List[string]]::new()
	if ($Text -match '(?i)\.psm?1$') { $candidates.Add($Text) }
	foreach ($match in $script:ScriptPathPattern.Matches($Text)) { $candidates.Add($match.Groups[1].Value) }
	foreach ($match in [regex]::Matches($Text, "(?i)'([^']+\.psm?1)'")) { $candidates.Add($match.Groups[1].Value) }
	foreach ($candidate in $candidates) {
		if ($candidate -notmatch $script:RootedPattern) { continue }
		$path = ConvertTo-ComparablePath $candidate $CwdPath
		if ((Test-UnderDirectory $path $CwdPath) -and -not (Test-UnderDirectory $path "$CwdPath/Temp")) { return $true }
	}
	return $false
}

function Test-ExecutionPolicyBypass($Words) {
	for ($j = 0; $j -lt $Words.Count; $j++) {
		if ($Words[$j].Value -notmatch '^-([A-Za-z]+)(?::(.*))?$') { continue }
		$parameter = $Matches[1].ToLowerInvariant()
		$setting = $Matches[2]
		if ($parameter -ne 'ep' -and ($parameter.Length -lt 2 -or -not 'executionpolicy'.StartsWith($parameter))) { continue }
		if ($null -eq $setting -and $j + 1 -lt $Words.Count) { $setting = $Words[$j + 1].Value }
		if ($setting -eq 'bypass') { return $true }
	}
	return $false
}

function Get-GitSubcommandIndex($Arguments) {
	for ($j = 0; $j -lt $Arguments.Count; $j++) {
		$value = $Arguments[$j].Value
		if (@('-C', '-c', '--git-dir', '--work-tree', '--namespace', '--config-env') -ccontains $value) { $j++ }
		elseif (-not $value.StartsWith('-')) { return $j }
	}
	return -1
}

function Test-UnsafeStash($Arguments) {
	if ($Arguments.Count -eq 0) { return $true }
	$subcommand = 'push'
	$options = @($Arguments | ForEach-Object Value)
	if (-not $options[0].StartsWith('-')) {
		$subcommand = $options[0].ToLowerInvariant()
		$options = @($options | Select-Object -Skip 1)
	}
	if (@('pop', 'save', 'clear') -contains $subcommand) { return $true }
	if ($subcommand -eq 'push') {
		$untracked = ($options -ccontains '-u') -or ($options -contains '--include-untracked')
		$message = @($options | Where-Object { $_ -cmatch '^-m' -or $_ -match '^--message(=|$)' }).Count -gt 0
		return -not ($untracked -and $message)
	}
	if ($subcommand -eq 'apply') { return @($options | Where-Object { $_ -match '^[0-9a-f]{7,40}$' }).Count -eq 0 }
	return $false
}

function Test-InPlaceEdit([string] $Name, $Arguments, [string] $CwdPath) {
	$inPlace = $false
	$hasScript = $false
	$files = [System.Collections.Generic.List[string]]::new()
	for ($j = 0; $j -lt $Arguments.Count; $j++) {
		$value = $Arguments[$j].Value
		if ($Name -eq 'sed') {
			if ($value -cmatch '^-[Enrsuz]*i' -or $value -match '^--in-place(=|$)') { $inPlace = $true; continue }
			if (@('-e', '-f', '--expression', '--file') -ccontains $value) { $hasScript = $true; $j++; continue }
			if ($value -match '^--(expression|file)=') { $hasScript = $true; continue }
		}
		else {
			if ($value -cmatch '^-[aclnpsuwTtUWX0-9]*i') { $inPlace = $true; continue }
			if ($value -cmatch '^-[A-Za-z]*[eE]$') { $hasScript = $true; $j++; continue }
		}
		if ($value.StartsWith('-') -and $value.Length -gt 1) { continue }
		$files.Add($value)
	}
	if (-not $inPlace) { return $false }
	if (-not $hasScript -and $files.Count -gt 0) { $files.RemoveAt(0) }
	foreach ($file in $files) {
		if (-not (Test-UnderDirectory (ConvertTo-ComparablePath $file $CwdPath) "$CwdPath/Temp")) { return $true }
	}
	return $false
}

function Test-GitCleanPath($Arguments) {
	for ($j = 0; $j -lt $Arguments.Count; $j++) {
		$value = $Arguments[$j].Value
		if ($value -eq '--') { return $j + 1 -lt $Arguments.Count }
		if ($value -ceq '-e' -or $value -eq '--exclude') { $j++ }
		elseif (-not $value.StartsWith('-')) { return $true }
	}
	return $false
}

function Test-ComputedTarget($Arguments, [bool] $IsBash, [bool] $IsGit) {
	$literalNext = $false
	foreach ($word in $Arguments) {
		$isLiteral = $literalNext
		$literalNext = $false
		if (-not $IsBash -and -not $IsGit -and $word.Raw -match '^-([A-Za-z]+)(:?)') {
			$parameter = $Matches[1].ToLowerInvariant()
			if ($parameter.Length -ge 2 -and ('include'.StartsWith($parameter) -or 'exclude'.StartsWith($parameter) -or 'filter'.StartsWith($parameter))) { return $true }
			$isLiteralPath = $parameter -eq 'lp' -or $parameter -eq 'pspath' -or 'literalpath'.StartsWith($parameter)
			if ($Matches[2]) { $isLiteral = $isLiteralPath } else { $literalNext = $isLiteralPath }
		}
		elseif ($isLiteral -and $word.Raw.EndsWith(',')) { $literalNext = $true }
		if ($word.Value.IndexOfAny([char[]]'*?') -ge 0) { return $true }
		# Bash rm reads [...] inside quotes literally; PowerShell -Path and Git pathspecs expand it even quoted.
		$bracketText = if ($IsBash -and -not $IsGit) { $word.View } elseif ($isLiteral) { '' } else { $word.Value }
		if ($bracketText -match '\[[^\]]*\]') { return $true }
		if ($word.HasVariable -or $word.HasSubstitution) { return $true }
		if ($IsBash -and $word.View -match '\{[^{}]*(,|\.\.)[^{}]*\}') { return $true }
		if (-not $IsBash -and $word.IsSplat) { return $true }
	}
	return $false
}

function Test-ComputedDeletion($Element, $Arguments, [bool] $IsBash) {
	$name = $Element.Name
	if ($name -eq 'find') {
		for ($j = 0; $j -lt $Arguments.Count; $j++) {
			$value = $Arguments[$j].Value
			if ($value -ceq '-delete') { return $true }
			if (@('-exec', '-execdir', '-ok', '-okdir') -ccontains $value -and $j + 1 -lt $Arguments.Count -and
				$script:DeletionCommands -contains $Arguments[$j + 1].Value) { return $true }
		}
		return $false
	}
	if ($name -eq 'xargs') {
		for ($j = 0; $j -lt $Arguments.Count; $j++) {
			$value = $Arguments[$j].Value
			if ($script:DeletionCommands -contains $value) { return $true }
			if ($value -eq 'git' -and $j + 1 -lt $Arguments.Count -and @('rm', 'clean') -contains $Arguments[$j + 1].Value) { return $true }
		}
		return $false
	}
	$isGit = $name -eq 'git'
	if ($isGit) {
		$index = Get-GitSubcommandIndex $Arguments
		if ($index -lt 0 -or @('rm', 'clean') -notcontains $Arguments[$index].Value) { return $false }
		$subcommand = $Arguments[$index].Value
		$Arguments = Get-WordsFrom $Arguments ($index + 1)
		if ($subcommand -eq 'clean' -and -not (Test-GitCleanPath $Arguments)) { return $true }
	}
	elseif ($script:DeletionCommands -notcontains $name) { return $false }
	if ($Element.PrecededBy -eq '|' -or $Element.FollowedBy -eq '(') { return $true }
	return Test-ComputedTarget $Arguments $IsBash $isGit
}

function Get-ElementViolation($Element, [bool] $IsBash, [string] $CwdPath) {
	foreach ($word in $Element.Words) {
		if (Test-AbsoluteScriptPath $word.Value $CwdPath) { return $script:Messages.AbsoluteScriptPath }
	}
	if ($Element.CommandIndex -lt 0) { return $null }
	$name = $Element.Name
	$arguments = Get-WordsFrom $Element.Words ($Element.CommandIndex + 1)
	if ($script:DirectoryCommands -contains $name) { return $script:Messages.DirectoryChange }
	if (Test-ExecutionPolicyBypass $arguments) { return $script:Messages.ExecutionPolicy }
	if ($name -eq 'git') {
		$index = Get-GitSubcommandIndex $arguments
		if ($index -ge 0 -and $arguments[$index].Value -eq 'stash' -and (Test-UnsafeStash (Get-WordsFrom $arguments ($index + 1)))) {
			return $script:Messages.Stash
		}
	}
	if (($name -eq 'sed' -or $name -eq 'perl') -and (Test-InPlaceEdit $name $arguments $CwdPath)) { return $script:Messages.InPlaceEdit }
	if (Test-ComputedDeletion $Element $arguments $IsBash) { return $script:Messages.ComputedDeletion }
	return $null
}

function Get-ShellRuleViolation([string] $Command, [bool] $IsBash, [string] $Cwd) {
	$lexer = [ShellLexer]::new($Command, $IsBash)
	$tokenLists = [System.Collections.Generic.List[object]]::new()
	$tokenLists.Add($lexer.ReadTokens($false))
	$tokenLists.AddRange($lexer.Nested)
	$cwdPath = ConvertTo-ComparablePath $Cwd $Cwd
	for ($j = 0; $j -lt $tokenLists.Count; $j++) {
		$elements = Get-ShellElements $tokenLists[$j] $IsBash
		foreach ($element in $elements) {
			$message = Get-ElementViolation $element $IsBash $cwdPath
			if ($message) { return $message }
		}
	}
	return $null
}

try {
	$reader = [IO.StreamReader]::new([Console]::OpenStandardInput(), $script:Utf8)
	$hook = $reader.ReadToEnd() | ConvertFrom-Json
	if (@('Bash', 'PowerShell') -cnotcontains $hook.tool_name) { exit 0 }
	$command = $hook.tool_input.command
	if ($command -isnot [string]) { exit 0 }
	$message = Get-ShellRuleViolation $command ($hook.tool_name -eq 'Bash') ([string]$hook.cwd)
	if ($message) {
		Write-Stderr "Invoke-PreToolUseHook: $message`n"
		exit 2
	}
}
catch {
	# Fail open: a hook bug must never block shell use; the prose rules still apply.
}
exit 0
