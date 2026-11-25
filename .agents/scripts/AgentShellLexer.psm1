Set-StrictMode -Version Latest

# Shared shell tokenizer and path comparison helpers used by
# Invoke-PreToolUseHook.ps1 and Invoke-SubdirectoryAgentsHook.ps1.

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
	[bool] $HasRedirection = $false
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
				$this.HasRedirection = $true
				$this.ReadHeredocStart()
				continue
			}
			$redirectPattern = if ($this.Bash) { [ShellLexer]::BashRedirect } else { [ShellLexer]::PowerShellRedirect }
			$redirect = $redirectPattern.Match($this.S, $this.I)
			if ($redirect.Success) {
				$this.HasRedirection = $true
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
		if ($inner.HasRedirection) { $this.HasRedirection = $true }
		$Value.Append($this.S, $this.I, $end + 1 - $this.I)
		$View.Append([char]2)
		$Word.HasSubstitution = $true
		$this.I = $end + 1
	}
}

$script:LeadingKeywords = @('if', 'elif', 'then', 'else', 'do', 'while', 'until', '!', 'time')

function Get-ShellTokenLists([string] $Command, [bool] $IsBash) {
	$lexer = [ShellLexer]::new($Command, $IsBash)
	$tokenLists = [System.Collections.Generic.List[object]]::new()
	$tokenLists.Add($lexer.ReadTokens($false))
	$tokenLists.AddRange($lexer.Nested)
	return [pscustomobject]@{
		TokenLists = $tokenLists
		HasRedirection = $lexer.HasRedirection
	}
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

Export-ModuleMember -Function Get-ShellTokenLists, Get-ShellElements, ConvertTo-ComparablePath, Test-UnderDirectory
