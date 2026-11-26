# PostInvocation hook for Antigravity CLI (.agents/hooks.json) and PostToolUse
# hook for Codex CLI (.codex/config.toml). Injects unseen safe in-worktree
# ancestor subdirectory AGENTS.md files (excluding root AGENTS.md, Temp/, and
# symlinks/reparse points) on file access, deduplicated via hook markers in the
# active session transcript with zero OS-temp state.
# Exit codes:
#   0 - always; stdout is compact hook JSON when emitting unseen entries/stubs,
#       or empty on no-op or any fail-open error.

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:MarkerPrefix = '<!-- subdirectory-agents-md:'
$script:MarkerLineRegex = [regex]::new('^<!-- subdirectory-agents-md: ([^>\r\n]+) -->$')
$script:AgyPostInvocationHeaderRegex = [regex]::new('\A(?:The following is a <SYSTEM_MESSAGE>[^\n]*\r?\n\r?\n)?<SYSTEM_MESSAGE>\r?\nThe following system message was injected by a PostInvocation hook:\r?\n')
$script:DirectReadCommands = @('get-content', 'gc', 'cat', 'type')
$script:SkipOutputCommands = @('echo', 'write-output', 'write-host', 'printf')
$script:MetadataPathCommands = @('get-childitem', 'gci', 'ls', 'dir', 'test-path', 'get-item', 'gi', 'resolve-path')
$script:MetadataSearchCommands = @('select-string', 'sls', 'rg', 'grep')
$script:HeadingPatternRegex = [regex]::new('^\^#+(?:\+|\{\d+(?:,\d*)?\})? $')
$script:ApplyPatchHeaderRegex = [regex]::new('(?m)^\*\*\* (?:(?:Update|Add|Delete) File|Move to):\s*(.+?)\s*\r?$')
$script:AgyFullReadRegex = [regex]::new('(?m)^Showing lines 1-(\d+) of \1\r?$')
$script:AgyZeroExitRegex = [regex]::new('(?m)^Command exited with code 0\.\r?$')
$script:AgyNonZeroExitRegex = [regex]::new('(?m)^Command exited with code (?!0\.\r?$)-?\d+\.\r?$')

function Test-JsonInteger($Value, [ref] $OutValue) {
	if ($Value -is [int] -or $Value -is [long]) {
		$OutValue.Value = [long]$Value
		return $true
	}
	return $false
}

function ConvertTo-NormalizedNewlines([string] $Text) {
	return $Text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Get-ComparableParent([string] $ComparablePath) {
	$slash = $ComparablePath.LastIndexOf('/')
	if ($slash -le 0) { return '' }
	return $ComparablePath.Substring(0, $slash)
}

function Test-IsAgentsFilePath([string] $ComparablePath) {
	$slash = $ComparablePath.LastIndexOf('/')
	$leaf = if ($slash -ge 0) { $ComparablePath.Substring($slash + 1) } else { $ComparablePath }
	return $leaf.Equals('AGENTS.md', [StringComparison]::OrdinalIgnoreCase)
}

function Add-MarkersFromText([string] $Text, [Collections.Generic.HashSet[string]] $Seen) {
	if ([string]::IsNullOrEmpty($Text) -or -not $Text.StartsWith($script:MarkerPrefix, [StringComparison]::Ordinal)) { return }
	$sr = [IO.StringReader]::new($Text)
	try {
		while ($null -ne ($line = $sr.ReadLine())) {
			$m = $script:MarkerLineRegex.Match($line)
			if (-not $m.Success) { break }
			$rel = $m.Groups[1].Value
			if (-not [string]::IsNullOrEmpty($rel)) {
				[void] $Seen.Add($rel)
			}
		}
	}
	finally {
		$sr.Dispose()
	}
}

function Get-SafeAgentsCandidate([string] $DirComparable, [string] $CwdComparable, [string] $TempComparable, [hashtable] $Cache) {
	if ($Cache.ContainsKey($DirComparable)) { return $Cache[$DirComparable] }
	$result = $null
	try {
		if (Test-UnderDirectory $DirComparable $CwdComparable) {
			$agentsComparable = "$DirComparable/AGENTS.md"
			if (-not (Test-UnderDirectory $agentsComparable $TempComparable)) {
				$agentsPath = $agentsComparable.Replace('/', [IO.Path]::DirectorySeparatorChar)
				if ([IO.File]::Exists($agentsPath)) {
					$safety = Get-AgentPathSafety $agentsPath
					if ($safety.Status -ceq 'safe' -and -not $safety.Item.PSIsContainer) {
						$text = [IO.File]::ReadAllText($safety.Path)
						if (-not [string]::IsNullOrWhiteSpace($text)) {
							$relPath = $agentsComparable.Substring($CwdComparable.Length + 1)
							$result = [pscustomobject]@{
								RelPath = $relPath
								DirComparable = $DirComparable
								Text = $text
							}
						}
					}
				}
			}
		}
	}
	catch {
		$result = $null
	}
	$Cache[$DirComparable] = $result
	return $result
}

function New-CollectionState() {
	return [pscustomobject]@{
		CandidateCache = [hashtable]::new([StringComparer]::OrdinalIgnoreCase)
		CandidateSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
		OrderedCandidates = [Collections.Generic.List[object]]::new()
		DirectFullReadSet = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	}
}

function Add-AncestorCandidates([string] $StartDirComparable, [string] $CwdComparable, [string] $TempComparable, $State) {
	$dirs = [Collections.Generic.List[string]]::new()
	$dir = $StartDirComparable
	while (-not [string]::IsNullOrEmpty($dir) -and (Test-UnderDirectory $dir $CwdComparable)) {
		$dirs.Add($dir)
		$dir = Get-ComparableParent $dir
	}
	for ($k = $dirs.Count - 1; $k -ge 0; $k--) {
		$cand = Get-SafeAgentsCandidate $dirs[$k] $CwdComparable $TempComparable $State.CandidateCache
		if ($null -ne $cand -and $State.CandidateSet.Add($cand.RelPath)) {
			$State.OrderedCandidates.Add($cand)
		}
	}
}

function Add-DirectFullReadCandidate($Candidate, [string] $CwdComparable, [string] $TempComparable, $State) {
	Add-AncestorCandidates (Get-ComparableParent $Candidate.DirComparable) $CwdComparable $TempComparable $State
	if ($State.CandidateSet.Add($Candidate.RelPath)) {
		$State.OrderedCandidates.Add($Candidate)
	}
	[void] $State.DirectFullReadSet.Add($Candidate.RelPath)
}

function Add-AccessedTarget([string] $ComparablePath, [bool] $IsDirectory, [string] $CwdComparable, [string] $TempComparable, $State) {
	if (-not (Test-UnderDirectory $ComparablePath $CwdComparable)) { return }
	$startDir = if ($IsDirectory) { $ComparablePath } else { Get-ComparableParent $ComparablePath }
	Add-AncestorCandidates $startDir $CwdComparable $TempComparable $State
}

function Split-UnquotedCommaWord($Word, [int] $StartOffset) {
	$pieces = [Collections.Generic.List[string]]::new()
	$val = $Word.Value
	$view = $Word.View
	if ($StartOffset -ge $val.Length) { return , $pieces }
	if ($val.Length -ne $view.Length) {
		$pieces.Add($val.Substring($StartOffset))
		return , $pieces
	}
	$segStart = $StartOffset
	for ($idx = $StartOffset; $idx -lt $val.Length; $idx++) {
		if ($view[$idx] -eq ',') {
			if ($idx -gt $segStart) { $pieces.Add($val.Substring($segStart, $idx - $segStart)) }
			$segStart = $idx + 1
		}
	}
	if ($val.Length -gt $segStart) { $pieces.Add($val.Substring($segStart)) }
	return , $pieces
}

function Get-WordPathCandidates($Word) {
	$pieces = [Collections.Generic.List[string]]::new()
	if ($Word.HasVariable -or $Word.HasSubstitution) { return , $pieces }
	$val = $Word.Value
	if ([string]::IsNullOrWhiteSpace($val)) { return , $pieces }
	$startOffset = 0
	if ($val -notmatch '^[A-Za-z]:[\\/]' -and $val -match '^-?[A-Za-z0-9_-]+[:=]') {
		$startOffset = $Matches[0].Length
	}
	return (Split-UnquotedCommaWord $Word $startOffset)
}

function Get-DirectReadShellPlan([string] $Command, [string] $CwdComparable, [string] $TempComparable, [hashtable] $CandidateCache) {
	try {
		$lex = Get-ShellTokenLists $Command $false
		if ($lex.TokenLists.Count -ne 1 -or $lex.HasRedirection) { return $null }
		$elements = Get-ShellElements $lex.TokenLists[0] $false
		if ($elements.Count -ne 1) { return $null }
		$el = $elements[0]
		if ($el.CommandIndex -lt 0 -or $el.PrecededBy -cne '' -or $el.FollowedBy -cne '') { return $null }
		if ($script:DirectReadCommands -notcontains $el.Name) { return $null }

		$expectPath = $false
		$expectEncoding = $false
		$endOfFlags = $false
		$operands = [Collections.Generic.List[string]]::new()
		for ($i = $el.CommandIndex + 1; $i -lt $el.Words.Count; $i++) {
			$w = $el.Words[$i]
			if ($w.HasVariable -or $w.HasSubstitution -or $w.IsSplat) { return $null }
			$val = $w.Value
			if ($expectEncoding) {
				$expectEncoding = $false
				continue
			}
			if ($expectPath) {
				$pieces = Split-UnquotedCommaWord $w 0
				if ($pieces.Count -eq 0) { return $null }
				foreach ($p in $pieces) { $operands.Add($p) }
				$expectPath = $w.Raw.EndsWith(',')
				continue
			}
			if (-not $endOfFlags -and $val -ceq '--') {
				$endOfFlags = $true
				continue
			}
			if (-not $endOfFlags -and $val.StartsWith('-') -and $val.Length -gt 1) {
				if ($val -notmatch '^-([A-Za-z]+)(?::(.*))?$') { return $null }
				$flag = $Matches[1].ToLowerInvariant()
				$hasColon = $val.IndexOf(':') -ge 0
				$inlineVal = if ($hasColon) { $Matches[2] } else { '' }
				if ($flag -eq 'raw') {
					if ($hasColon -and @('$true', '$false', 'true', 'false', '1', '0') -notcontains $inlineVal.ToLowerInvariant()) { return $null }
					continue
				}
				elseif (@('literalpath', 'lp', 'pspath', 'path') -contains $flag) {
					if ($hasColon) {
						$pieces = Split-UnquotedCommaWord $w ($val.IndexOf(':') + 1)
						if ($pieces.Count -eq 0) { return $null }
						foreach ($p in $pieces) { $operands.Add($p) }
					}
					else {
						$expectPath = $true
					}
					continue
				}
				elseif ($flag -eq 'encoding') {
					if (-not $hasColon) { $expectEncoding = $true }
					continue
				}
				else {
					return $null
				}
			}
			$pieces = Split-UnquotedCommaWord $w 0
			foreach ($p in $pieces) { $operands.Add($p) }
		}
		if ($expectPath -or $expectEncoding -or $operands.Count -eq 0) { return $null }

		$agentsCandidates = [Collections.Generic.List[object]]::new()
		$otherTargets = [Collections.Generic.List[object]]::new()
		foreach ($op in $operands) {
			if ($op.IndexOfAny([char[]]'*?') -ge 0) { return $null }
			try {
				$resolved = ConvertTo-ComparablePath $op $CwdComparable
				if (-not (Test-UnderDirectory $resolved $CwdComparable)) { continue }
				if (Test-IsAgentsFilePath $resolved) {
					$cand = Get-SafeAgentsCandidate (Get-ComparableParent $resolved) $CwdComparable $TempComparable $CandidateCache
					if ($null -ne $cand) { $agentsCandidates.Add($cand) }
				}
				else {
					$native = $resolved.Replace('/', [IO.Path]::DirectorySeparatorChar)
					if ([IO.Directory]::Exists($native)) {
						$otherTargets.Add([pscustomobject]@{ ComparablePath = $resolved; IsDirectory = $true })
					}
					elseif ([IO.File]::Exists($native)) {
						$otherTargets.Add([pscustomobject]@{ ComparablePath = $resolved; IsDirectory = $false })
					}
				}
			}
			catch {
				continue
			}
		}
		if ($agentsCandidates.Count -eq 0) { return $null }
		return [pscustomobject]@{
			AgentsCandidates = $agentsCandidates
			OtherTargets = $otherTargets
		}
	}
	catch {
		return $null
	}
}

function Get-LeafName([string] $PathText) {
	$slash = $PathText.LastIndexOfAny([char[]]'/\')
	if ($slash -ge 0) { return $PathText.Substring($slash + 1) }
	return $PathText
}

function Test-IsContentReadElement($El) {
	if ($El.CommandIndex -lt 0) { return $false }
	if ($script:DirectReadCommands -contains $El.Name) { return $true }
	if ($script:MetadataSearchCommands -contains $El.Name -and -not (Test-IsMetadataLookupElement $El)) { return $true }
	return $false
}

function Get-PreservedMetadataIndices($Tokens, $Elements, [bool] $PreserveAll) {
	$preserved = [Collections.Generic.HashSet[int]]::new()
	if ($Elements.Count -eq 0) { return , $preserved }
	if ($PreserveAll) {
		for ($i = 0; $i -lt $Elements.Count; $i++) { [void] $preserved.Add($i) }
		return , $preserved
	}

	$isContentRead = [bool[]]::new($Elements.Count)
	$hasAnyContentRead = $false
	for ($i = 0; $i -lt $Elements.Count; $i++) {
		if (Test-IsContentReadElement $Elements[$i]) {
			$isContentRead[$i] = $true
			$hasAnyContentRead = $true
		}
	}
	if (-not $hasAnyContentRead) { return , $preserved }

	# Walk tokens in lockstep with Elements to track enclosing content readers
	# and pipeline stages across () and {} groups.
	$nextPipeId = 0
	$stack = [Collections.Generic.List[object]]::new()
	$stack.Add([pscustomobject]@{
		PipeId = $nextPipeId++
		Stage = 0
		AfterPipe = $false
		StageHasContentRead = $false
		OwnerIsContentRead = $false
	})
	$elPositions = [object[]]::new($Elements.Count)
	$maxContentReadStage = [hashtable]::new()
	$elIdx = 0
	$inWordRun = $false

	for ($ti = 0; $ti -lt $Tokens.Count; $ti++) {
		$token = $Tokens[$ti]
		if ($token -is [string]) {
			if ($inWordRun) {
				$elIdx++
				$inWordRun = $false
			}
			$top = $stack[$stack.Count - 1]
			if ($token -ceq '|') {
				$top.Stage++
				$top.AfterPipe = $true
				$top.StageHasContentRead = $false
			}
			elseif ($token -ceq '(' -or $token -ceq '{') {
				$top.AfterPipe = $false
				$stack.Add([pscustomobject]@{
					PipeId = $nextPipeId++
					Stage = 0
					AfterPipe = $false
					StageHasContentRead = $false
					OwnerIsContentRead = ($top.OwnerIsContentRead -or $top.StageHasContentRead)
				})
			}
			elseif ($token -ceq ')' -or $token -ceq '}') {
				if ($stack.Count -gt 1) {
					$stack.RemoveAt($stack.Count - 1)
				}
			}
			elseif ($token -ceq "`n") {
				$keepPipeline = $top.AfterPipe
				if (-not $keepPipeline) {
					$nextIdx = $ti + 1
					while ($nextIdx -lt $Tokens.Count -and $Tokens[$nextIdx] -is [string] -and $Tokens[$nextIdx] -ceq "`n") {
						$nextIdx++
					}
					$keepPipeline = ($nextIdx -lt $Tokens.Count -and $Tokens[$nextIdx] -is [string] -and $Tokens[$nextIdx] -ceq '|')
				}
				if (-not $keepPipeline) {
					$top.PipeId = $nextPipeId++
					$top.Stage = 0
					$top.AfterPipe = $false
					$top.StageHasContentRead = $false
				}
			}
			else {
				$top.PipeId = $nextPipeId++
				$top.Stage = 0
				$top.AfterPipe = $false
				$top.StageHasContentRead = $false
			}
			continue
		}
		if ($token.IsRedirectTarget) { continue }
		if (-not $inWordRun) {
			$inWordRun = $true
			if ($elIdx -lt $Elements.Count) {
				$top = $stack[$stack.Count - 1]
				$top.AfterPipe = $false
				if ($isContentRead[$elIdx]) {
					$top.StageHasContentRead = $true
					foreach ($frame in $stack) {
						if (-not $maxContentReadStage.ContainsKey($frame.PipeId) -or $frame.Stage -gt $maxContentReadStage[$frame.PipeId]) {
							$maxContentReadStage[$frame.PipeId] = $frame.Stage
						}
					}
				}
				if ($top.OwnerIsContentRead) {
					[void] $preserved.Add($elIdx)
				}
				$positions = [Collections.Generic.List[object]]::new($stack.Count)
				foreach ($frame in $stack) {
					$positions.Add([pscustomobject]@{ PipeId = $frame.PipeId; Stage = $frame.Stage })
				}
				$elPositions[$elIdx] = $positions
			}
		}
	}

	for ($i = 0; $i -lt $Elements.Count; $i++) {
		if ($preserved.Contains($i) -or $null -eq $elPositions[$i]) { continue }
		foreach ($pos in $elPositions[$i]) {
			if ($maxContentReadStage.ContainsKey($pos.PipeId) -and $maxContentReadStage[$pos.PipeId] -gt $pos.Stage) {
				[void] $preserved.Add($i)
				break
			}
		}
	}

	return , $preserved
}

function Test-IsMetadataLookupElement($El) {
	if ($script:MetadataPathCommands -contains $El.Name) { return $true }

	if ((Get-LeafName $El.Name).Equals('get-affectedagentsdocs.ps1', [StringComparison]::OrdinalIgnoreCase)) {
		return $true
	}
	if ($El.Name -eq 'pwsh') {
		for ($i = $El.CommandIndex + 1; $i -lt ($El.Words.Count - 1); $i++) {
			$val = $El.Words[$i].Value
			if ($val.Equals('-Command', [StringComparison]::OrdinalIgnoreCase) -or $val.Equals('-c', [StringComparison]::OrdinalIgnoreCase)) {
				break
			}
			if ($val.Equals('-File', [StringComparison]::OrdinalIgnoreCase)) {
				$next = $El.Words[$i + 1]
				if (-not $next.HasVariable -and -not $next.HasSubstitution -and
					(Get-LeafName $next.Value).Equals('get-affectedagentsdocs.ps1', [StringComparison]::OrdinalIgnoreCase)) {
					return $true
				}
				break
			}
		}
		return $false
	}

	if ($script:MetadataSearchCommands -notcontains $El.Name) { return $false }

	if ($El.Name -eq 'rg' -or $El.Name -eq 'grep') {
		if ($El.FollowedBy -ceq '(') { return $false }
		$expectPattern = $false
		$expectOtherValue = $false
		$endOfFlags = $false
		$hasMetadataOutputFlag = $false
		$explicitPatternCount = 0
		$patternValue = $null
		$positionalPattern = $null
		$positionalPatternDynamic = $false

		$metadataShortFlags = @('l', 'c')
		$metadataLongFlags = if ($El.Name -eq 'rg') {
			@('--files-with-matches', '--files', '--count', '--count-matches')
		}
		else {
			@('--files-with-matches', '--count')
		}
		$safeShortBoolFlags = @('i', 'n', 'H', 'h', 'w', 'q', 'a', 's', 'L', 'I')
		$safeShortValueFlags = @('m', 'd')
		$safeLongBoolFlags = @('--ignore-case', '--line-number', '--with-filename', '--no-filename',
			'--word-regexp', '--quiet', '--silent', '--no-messages', '--follow')
		$safeLongValueFlags = @('--max-count', '--color', '--encoding')
		if ($El.Name -eq 'rg') {
			$safeShortBoolFlags += @('S', 'u')
			$safeShortValueFlags += @('g', 't', 'T', 'M', 'j', 'E')
			$safeLongBoolFlags += @('--no-heading', '--heading', '--smart-case', '--case-sensitive',
				'--hidden', '--no-ignore', '--unrestricted')
			$safeLongValueFlags += @('--glob', '--iglob', '--type', '--type-not', '--max-depth',
				'--max-columns', '--threads')
		}
		else {
			$safeShortBoolFlags += @('E', 'r', 'R')
			$safeLongBoolFlags += @('--extended-regexp', '--recursive', '--dereference-recursive')
			$safeLongValueFlags += @('--include', '--exclude', '--exclude-dir', '--directories')
		}

		for ($i = $El.CommandIndex + 1; $i -lt $El.Words.Count; $i++) {
			$w = $El.Words[$i]
			if ($w.IsSplat) { return $false }
			if (-not $endOfFlags -and ($w.HasVariable -or $w.HasSubstitution)) { return $false }
			$val = $w.Value
			if ($expectPattern) {
				$explicitPatternCount++
				$patternValue = $val
				$expectPattern = $false
				continue
			}
			if ($expectOtherValue) {
				$expectOtherValue = $false
				continue
			}
			if (-not $endOfFlags -and $val -ceq '--') {
				$endOfFlags = $true
				continue
			}
			if (-not $endOfFlags -and $val.StartsWith('--', [StringComparison]::Ordinal) -and $val.Length -gt 2) {
				$eq = $val.IndexOf('=')
				$flagName = if ($eq -ge 0) { $val.Substring(0, $eq) } else { $val }
				$hasEq = $eq -ge 0
				if ($flagName -ceq '--regexp') {
					if ($hasEq) {
						$explicitPatternCount++
						$patternValue = $val.Substring($eq + 1)
					}
					else {
						$expectPattern = $true
					}
					continue
				}
				if (-not $hasEq -and $metadataLongFlags -ccontains $flagName) {
					$hasMetadataOutputFlag = $true
					continue
				}
				if (-not $hasEq -and $safeLongBoolFlags -ccontains $flagName) {
					continue
				}
				if ($safeLongValueFlags -ccontains $flagName) {
					if (-not $hasEq) { $expectOtherValue = $true }
					continue
				}
				return $false
			}
			if (-not $endOfFlags -and $val.StartsWith('-', [StringComparison]::Ordinal) -and $val.Length -gt 1) {
				for ($ci = 1; $ci -lt $val.Length; $ci++) {
					$ch = [string]$val[$ci]
					if ($ch -ceq 'e') {
						if ($ci + 1 -lt $val.Length) {
							$explicitPatternCount++
							$patternValue = $val.Substring($ci + 1)
						}
						else {
							$expectPattern = $true
						}
						break
					}
					if ($metadataShortFlags -ccontains $ch) {
						$hasMetadataOutputFlag = $true
						continue
					}
					if ($safeShortBoolFlags -ccontains $ch) {
						continue
					}
					if ($safeShortValueFlags -ccontains $ch) {
						if ($ci + 1 -ge $val.Length) { $expectOtherValue = $true }
						break
					}
					return $false
				}
				continue
			}
			if ($explicitPatternCount -eq 0 -and $null -eq $positionalPattern -and -not $positionalPatternDynamic) {
				if ($w.HasVariable -or $w.HasSubstitution) { $positionalPatternDynamic = $true }
				else { $positionalPattern = $val }
			}
		}

		if ($expectPattern -or $expectOtherValue) { return $false }
		if ($hasMetadataOutputFlag) { return $true }
		if ($explicitPatternCount -gt 1) { return $false }
		if ($explicitPatternCount -eq 1) { return $script:HeadingPatternRegex.IsMatch($patternValue) }
		if ($positionalPatternDynamic -or $null -eq $positionalPattern) { return $false }
		return $script:HeadingPatternRegex.IsMatch($positionalPattern)
	}

	# select-string / sls
	$expectPattern = $false
	$expectOtherValue = $false
	$endOfFlags = $false
	$patternCount = 0
	$patternValue = $null
	$positionalPattern = $null
	$positionalPatternDynamic = $false
	$safeValueFlags = @('path', 'literalpath', 'lp', 'pspath', 'encoding', 'include', 'exclude', 'culture')
	$safeSwitchFlags = @('casesensitive', 'allmatches', 'list', 'quiet', 'raw', 'noemphasis')

	for ($i = $El.CommandIndex + 1; $i -lt $El.Words.Count; $i++) {
		$w = $El.Words[$i]
		if ($w.IsSplat) { return $false }
		$val = $w.Value
		$nextStartsWithComma = ($i + 1 -lt $El.Words.Count) -and $El.Words[$i + 1].View.StartsWith(',', [StringComparison]::Ordinal)
		if ($expectPattern) {
			if ($w.HasVariable -or $w.HasSubstitution) { return $false }
			if ($w.View.IndexOf(',') -ge 0 -or $nextStartsWithComma) { return $false }
			$patternCount++
			$patternValue = $val
			$expectPattern = $false
			continue
		}
		if ($expectOtherValue) {
			$expectOtherValue = $w.View.EndsWith(',', [StringComparison]::Ordinal) -or $nextStartsWithComma
			continue
		}
		if (-not $endOfFlags -and $val -ceq '--') {
			$endOfFlags = $true
			continue
		}
		if (-not $endOfFlags -and $val.StartsWith('-', [StringComparison]::Ordinal) -and $val.Length -gt 1) {
			if ($w.HasVariable -or $w.HasSubstitution) { return $false }
			if ($val -notmatch '^-([A-Za-z]+)(?::(.*))?$') { return $false }
			$flag = $Matches[1].ToLowerInvariant()
			$hasColon = $val.IndexOf(':') -ge 0
			$inlineVal = if ($hasColon) { $Matches[2] } else { '' }
			if ($flag -eq 'pattern') {
				if ($hasColon) {
					if ($w.View.IndexOf(',') -ge 0 -or $nextStartsWithComma) { return $false }
					$patternCount++
					$patternValue = $inlineVal
				}
				else {
					$expectPattern = $true
				}
				continue
			}
			if ($safeValueFlags -contains $flag) {
				if ($hasColon) {
					$expectOtherValue = $w.View.EndsWith(',', [StringComparison]::Ordinal) -or $nextStartsWithComma
				}
				else {
					$expectOtherValue = $true
				}
				continue
			}
			if ($safeSwitchFlags -contains $flag) {
				if ($hasColon -and @('$true', '$false', 'true', 'false', '1', '0') -notcontains $inlineVal.ToLowerInvariant()) {
					return $false
				}
				continue
			}
			return $false
		}
		if ($patternCount -eq 0 -and $null -eq $positionalPattern -and -not $positionalPatternDynamic) {
			if ($w.HasVariable -or $w.HasSubstitution -or $w.View.IndexOf(',') -ge 0 -or $nextStartsWithComma) {
				$positionalPatternDynamic = $true
			}
			else {
				$positionalPattern = $val
			}
		}
	}

	if ($expectPattern -or $expectOtherValue -or $patternCount -gt 1) { return $false }
	if ($patternCount -eq 1) { return $script:HeadingPatternRegex.IsMatch($patternValue) }
	if ($positionalPatternDynamic -or $null -eq $positionalPattern) { return $false }
	return $script:HeadingPatternRegex.IsMatch($positionalPattern)
}

function Get-ShellAccessedTargets([string] $Command, [string] $CwdComparable) {
	$targets = [Collections.Generic.List[object]]::new()
	try {
		$tokenLists = (Get-ShellTokenLists $Command $false).TokenLists
	}
	catch {
		return , $targets
	}
	$parsedLists = [Collections.Generic.List[object]]::new($tokenLists.Count)
	$hasContentReaderWithSubstitution = $false
	for ($t = 0; $t -lt $tokenLists.Count; $t++) {
		try {
			$elements = Get-ShellElements $tokenLists[$t] $false
		}
		catch {
			continue
		}
		$parsedLists.Add([pscustomobject]@{ Index = $t; Tokens = $tokenLists[$t]; Elements = $elements })
		if (-not $hasContentReaderWithSubstitution -and $tokenLists.Count -gt 1) {
			foreach ($el in $elements) {
				if (-not (Test-IsContentReadElement $el)) { continue }
				for ($w = $el.CommandIndex + 1; $w -lt $el.Words.Count; $w++) {
					if ($el.Words[$w].HasSubstitution) {
						$hasContentReaderWithSubstitution = $true
						break
					}
				}
				if ($hasContentReaderWithSubstitution) { break }
			}
		}
	}
	foreach ($entry in $parsedLists) {
		$elements = $entry.Elements
		$preserved = Get-PreservedMetadataIndices $entry.Tokens $elements ($entry.Index -gt 0 -and $hasContentReaderWithSubstitution)
		for ($i = 0; $i -lt $elements.Count; $i++) {
			$el = $elements[$i]
			if ($el.CommandIndex -lt 0) { continue }
			if ($script:SkipOutputCommands -contains $el.Name) { continue }
			if ((Test-IsMetadataLookupElement $el) -and -not $preserved.Contains($i)) { continue }
			$isDirectRead = $script:DirectReadCommands -contains $el.Name
			$cmdWord = $el.Words[$el.CommandIndex]
			$wordsToScan = [Collections.Generic.List[object]]::new()
			if ($cmdWord.Value.IndexOfAny([char[]]'/\') -ge 0) {
				$wordsToScan.Add($cmdWord)
			}
			for ($w = $el.CommandIndex + 1; $w -lt $el.Words.Count; $w++) {
				$wordsToScan.Add($el.Words[$w])
			}
			foreach ($word in $wordsToScan) {
				$pieces = Get-WordPathCandidates $word
				foreach ($piece in $pieces) {
					try {
						$resolved = ConvertTo-ComparablePath $piece $CwdComparable
						if (-not (Test-UnderDirectory $resolved $CwdComparable)) { continue }
						$native = $resolved.Replace('/', [IO.Path]::DirectorySeparatorChar)
						if ([IO.Directory]::Exists($native)) {
							$targets.Add([pscustomobject]@{ ComparablePath = $resolved; IsDirectory = $true; IsDirectRead = $isDirectRead })
						}
						elseif ([IO.File]::Exists($native)) {
							$targets.Add([pscustomobject]@{ ComparablePath = $resolved; IsDirectory = $false; IsDirectRead = $isDirectRead })
						}
					}
					catch {
						continue
					}
				}
			}
		}
	}
	return , $targets
}

function Add-ShellCommandTargets([string] $Command, [bool] $IsFailed, [bool] $CanVerifyFull, [string] $ResponseText, [string] $CwdComparable, [string] $TempComparable, $State) {
	$directPlan = Get-DirectReadShellPlan $Command $CwdComparable $TempComparable $State.CandidateCache
	if ($null -ne $directPlan) {
		if ($IsFailed -and $directPlan.OtherTargets.Count -eq 0) { return }
		$normContent = if (-not $IsFailed -and $CanVerifyFull -and -not [string]::IsNullOrEmpty($ResponseText)) {
			ConvertTo-NormalizedNewlines $ResponseText
		}
		else {
			''
		}
		foreach ($cand in $directPlan.AgentsCandidates) {
			$normBody = (ConvertTo-NormalizedNewlines $cand.Text).Trim()
			if (-not $IsFailed -and $CanVerifyFull -and -not [string]::IsNullOrEmpty($normBody) -and $normContent.Contains($normBody, [StringComparison]::Ordinal)) {
				Add-DirectFullReadCandidate $cand $CwdComparable $TempComparable $State
			}
			else {
				Add-AncestorCandidates (Get-ComparableParent $cand.DirComparable) $CwdComparable $TempComparable $State
			}
		}
		foreach ($other in $directPlan.OtherTargets) {
			Add-AccessedTarget $other.ComparablePath $other.IsDirectory $CwdComparable $TempComparable $State
		}
		return
	}

	$targets = Get-ShellAccessedTargets $Command $CwdComparable
	if ($targets.Count -eq 0) { return }
	if ($IsFailed) {
		$allDirectReadAgents = $true
		foreach ($t in $targets) {
			if ($t.IsDirectory -or -not $t.IsDirectRead -or -not (Test-IsAgentsFilePath $t.ComparablePath)) {
				$allDirectReadAgents = $false
				break
			}
		}
		if ($allDirectReadAgents) { return }
	}
	foreach ($t in $targets) {
		if (-not $t.IsDirectory -and $t.IsDirectRead -and (Test-IsAgentsFilePath $t.ComparablePath)) {
			Add-AncestorCandidates (Get-ComparableParent (Get-ComparableParent $t.ComparablePath)) $CwdComparable $TempComparable $State
		}
		else {
			Add-AccessedTarget $t.ComparablePath $t.IsDirectory $CwdComparable $TempComparable $State
		}
	}
}

function Format-EmissionText($State, [Collections.Generic.HashSet[string]] $Seen) {
	$markers = [Collections.Generic.List[string]]::new()
	$bodies = [Collections.Generic.List[string]]::new()
	foreach ($cand in $State.OrderedCandidates) {
		if ($Seen.Contains($cand.RelPath)) { continue }
		$markers.Add("<!-- subdirectory-agents-md: $($cand.RelPath) -->")
		if (-not $State.DirectFullReadSet.Contains($cand.RelPath)) {
			$body = $cand.Text.TrimEnd("`r", "`n")
			$bodies.Add("Contents of $($cand.RelPath) (directory instructions for accessed files):`n$body")
		}
	}
	if ($markers.Count -eq 0) { return '' }
	$header = $markers -join "`n"
	if ($bodies.Count -eq 0) { return $header }
	return "$header`n`n$($bodies -join "`n`n")"
}

function Write-Utf8Stdout([string] $Text) {
	$stdout = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes($Text)
	$stdout.Write($bytes, 0, $bytes.Length)
	$stdout.Flush()
}

function Invoke-AgyPostInvocation($Hook) {
	$cwd = Get-AgentTranscriptField $Hook 'cwd'
	$transcriptPath = Get-AgentTranscriptField $Hook 'transcriptPath'
	$initialNumStepsRaw = Get-AgentTranscriptField $Hook 'initialNumSteps'
	$initialNumSteps = 0L
	if ($cwd -isnot [string] -or [string]::IsNullOrWhiteSpace($cwd)) { return }
	if ($transcriptPath -isnot [string] -or [string]::IsNullOrWhiteSpace($transcriptPath)) { return }
	if (-not (Test-JsonInteger $initialNumStepsRaw ([ref]$initialNumSteps)) -or $initialNumSteps -lt 0) { return }
	if (-not [IO.File]::Exists($transcriptPath)) { return }

	$cwdComparable = ConvertTo-ComparablePath $cwd $cwd
	$tempComparable = "$cwdComparable/Temp"
	$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	$currentToolCalls = [Collections.Generic.List[object]]::new()
	$pendingQueue = [Collections.Generic.Queue[object]]::new()

	$stream = [IO.FileStream]::new($transcriptPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
	$reader = [IO.StreamReader]::new($stream, $script:Utf8)
	try {
		while ($null -ne ($line = $reader.ReadLine())) {
			$hasMarker = $line.IndexOf($script:MarkerPrefix, [StringComparison]::Ordinal) -ge 0
			$hasToolRecord = ($line.IndexOf('"PLANNER_RESPONSE"', [StringComparison]::Ordinal) -ge 0) -or
				($line.IndexOf('"GENERIC"', [StringComparison]::Ordinal) -ge 0)
			if (-not $hasMarker -and -not $hasToolRecord) { continue }
			try {
				$rec = ConvertFrom-Json -InputObject $line -Depth 100 -NoEnumerate -ErrorAction Stop
			}
			catch {
				continue
			}
			if ($rec -isnot [Management.Automation.PSCustomObject]) { continue }
			$type = Get-AgentTranscriptField $rec 'type'
			if ($type -isnot [string]) { continue }

			if ($type -ceq 'SYSTEM_MESSAGE') {
				if (-not $hasMarker) { continue }
				$source = Get-AgentTranscriptField $rec 'source'
				if ($source -is [string] -and $source -ceq 'SYSTEM_SDK') {
					$content = Get-AgentTranscriptField $rec 'content'
					if ($content -is [string]) {
						$hdr = $script:AgyPostInvocationHeaderRegex.Match($content)
						if ($hdr.Success) { $content = $content.Substring($hdr.Length) }
						Add-MarkersFromText $content $seen
					}
				}
				continue
			}

			if ($type -ceq 'PLANNER_RESPONSE' -or $type -ceq 'GENERIC') {
				$stepIndexRaw = Get-AgentTranscriptField $rec 'step_index'
				$stepIndex = 0L
				if (-not (Test-JsonInteger $stepIndexRaw ([ref]$stepIndex))) { continue }
				if ($stepIndex -lt $initialNumSteps) { continue }

				if ($type -ceq 'PLANNER_RESPONSE') {
					$rawCalls = Get-AgentTranscriptField $rec 'tool_calls'
					if ($rawCalls -is [object[]]) {
						foreach ($tc in $rawCalls) {
							if ($tc -isnot [Management.Automation.PSCustomObject]) { continue }
							$tcName = Get-AgentTranscriptField $tc 'name'
							$tcArgs = Get-AgentTranscriptField $tc 'args'
							$entry = [pscustomobject]@{
								Name = if ($tcName -is [string]) { $tcName } else { '' }
								Args = $tcArgs
								Result = $null
							}
							$currentToolCalls.Add($entry)
							$pendingQueue.Enqueue($entry)
						}
					}
				}
				else {
					if ($pendingQueue.Count -gt 0) {
						$paired = $pendingQueue.Dequeue()
						$statusRaw = Get-AgentTranscriptField $rec 'status'
						$errorRaw = Get-AgentTranscriptField $rec 'error'
						$contentRaw = Get-AgentTranscriptField $rec 'content'
						$paired.Result = [pscustomobject]@{
							Status = if ($statusRaw -is [string]) { $statusRaw } else { '' }
							Error = if ($errorRaw -is [string]) { $errorRaw } else { '' }
							Content = if ($contentRaw -is [string]) { $contentRaw } else { '' }
						}
					}
				}
			}
		}
	}
	finally {
		$reader.Dispose()
		$stream.Dispose()
	}

	if ($currentToolCalls.Count -eq 0) { return }
	$state = New-CollectionState

	foreach ($call in $currentToolCalls) {
		$isError = ($null -ne $call.Result) -and (($call.Result.Status -ceq 'ERROR') -or (-not [string]::IsNullOrEmpty($call.Result.Error)))
		if ($isError -and $call.Name -cne 'run_command') { continue }

		if ($call.Name -ceq 'read_file') {
			$path = Get-AgentTranscriptField $call.Args 'Path'
			if ($path -isnot [string] -or [string]::IsNullOrWhiteSpace($path)) { continue }
			try {
				$resolved = ConvertTo-ComparablePath $path $cwdComparable
				if (-not (Test-UnderDirectory $resolved $cwdComparable)) { continue }
				if (Test-IsAgentsFilePath $resolved) {
					$cand = Get-SafeAgentsCandidate (Get-ComparableParent $resolved) $cwdComparable $tempComparable $state.CandidateCache
					if ($null -eq $cand) { continue }
					$hasSlice = ($null -ne (Get-AgentTranscriptField $call.Args 'StartLine')) -or
						($null -ne (Get-AgentTranscriptField $call.Args 'EndLine')) -or
						($null -ne (Get-AgentTranscriptField $call.Args 'ByteOffset'))
					$fullDelivered = (-not $hasSlice) -and
						($null -ne $call.Result) -and
						$script:AgyFullReadRegex.IsMatch($call.Result.Content) -and
						($call.Result.Content -notmatch '(?m)^Partial read \(') -and
						($call.Result.Content -notmatch 'truncated at the \d+-byte limit')
					if ($fullDelivered) {
						Add-DirectFullReadCandidate $cand $cwdComparable $tempComparable $state
					}
					else {
						Add-AncestorCandidates (Get-ComparableParent $cand.DirComparable) $cwdComparable $tempComparable $state
					}
				}
				else {
					$native = $resolved.Replace('/', [IO.Path]::DirectorySeparatorChar)
					if ([IO.Directory]::Exists($native)) {
						Add-AccessedTarget $resolved $true $cwdComparable $tempComparable $state
					}
					elseif ([IO.File]::Exists($native)) {
						Add-AccessedTarget $resolved $false $cwdComparable $tempComparable $state
					}
				}
			}
			catch {
				continue
			}
		}
		elseif ($call.Name -ceq 'edit_file') {
			$path = Get-AgentTranscriptField $call.Args 'Path'
			if ($path -isnot [string] -or [string]::IsNullOrWhiteSpace($path)) { continue }
			try {
				$resolved = ConvertTo-ComparablePath $path $cwdComparable
				if (-not (Test-UnderDirectory $resolved $cwdComparable)) { continue }
				$native = $resolved.Replace('/', [IO.Path]::DirectorySeparatorChar)
				if ([IO.Directory]::Exists($native)) {
					Add-AccessedTarget $resolved $true $cwdComparable $tempComparable $state
				}
				elseif ([IO.File]::Exists($native)) {
					Add-AccessedTarget $resolved $false $cwdComparable $tempComparable $state
				}
			}
			catch {
				continue
			}
		}
		elseif ($call.Name -ceq 'write_file') {
			$path = Get-AgentTranscriptField $call.Args 'Path'
			if ($path -isnot [string] -or [string]::IsNullOrWhiteSpace($path)) { continue }
			try {
				$resolved = ConvertTo-ComparablePath $path $cwdComparable
				if (-not (Test-UnderDirectory $resolved $cwdComparable)) { continue }
				$native = $resolved.Replace('/', [IO.Path]::DirectorySeparatorChar)
				if ([IO.Directory]::Exists($native)) {
					Add-AccessedTarget $resolved $true $cwdComparable $tempComparable $state
				}
				elseif ([IO.File]::Exists($native)) {
					Add-AccessedTarget $resolved $false $cwdComparable $tempComparable $state
				}
				else {
					$parentDir = Get-ComparableParent $resolved
					if (-not [string]::IsNullOrEmpty($parentDir) -and [IO.Directory]::Exists($parentDir.Replace('/', [IO.Path]::DirectorySeparatorChar))) {
						Add-AccessedTarget $resolved $false $cwdComparable $tempComparable $state
					}
				}
			}
			catch {
				continue
			}
		}
		elseif ($call.Name -ceq 'run_command') {
			$cmd = Get-AgentTranscriptField $call.Args 'CommandLine'
			if ($cmd -isnot [string] -or [string]::IsNullOrWhiteSpace($cmd)) { continue }
			$content = if ($null -ne $call.Result) { $call.Result.Content } else { '' }
			$isBg = $content.IndexOf('Tool is running as a background task', [StringComparison]::Ordinal) -ge 0
			$isFailed = $isError -or ((-not $isBg) -and $script:AgyNonZeroExitRegex.IsMatch($content))
			$canVerifyFull = (-not $isFailed) -and ($null -ne $call.Result) -and (-not $isBg) -and $script:AgyZeroExitRegex.IsMatch($content)
			Add-ShellCommandTargets $cmd $isFailed $canVerifyFull $content $cwdComparable $tempComparable $state
		}
	}

	$text = Format-EmissionText $state $seen
	if ([string]::IsNullOrEmpty($text)) { return }
	$payload = ConvertTo-Json -Compress -Depth 6 ([ordered]@{
		injectSteps = @(
			[ordered]@{
				systemMessage = [ordered]@{
					systemMessage = $text
				}
			}
		)
	})
	Write-Utf8Stdout $payload
}

function Invoke-CodexPostToolUse($Hook) {
	$toolName = Get-AgentTranscriptField $Hook 'tool_name'
	if ($toolName -isnot [string] -or @('Bash', 'apply_patch') -cnotcontains $toolName) { return }
	$cwd = Get-AgentTranscriptField $Hook 'cwd'
	if ($cwd -isnot [string] -or [string]::IsNullOrWhiteSpace($cwd)) { return }
	$toolInput = Get-AgentTranscriptField $Hook 'tool_input'
	$command = Get-AgentTranscriptField $toolInput 'command'
	if ($command -isnot [string] -or [string]::IsNullOrWhiteSpace($command)) { return }

	$cwdComparable = ConvertTo-ComparablePath $cwd $cwd
	$tempComparable = "$cwdComparable/Temp"
	$state = New-CollectionState

	if ($toolName -ceq 'apply_patch') {
		foreach ($match in $script:ApplyPatchHeaderRegex.Matches($command)) {
			$rawPath = $match.Groups[1].Value.Trim()
			if ([string]::IsNullOrEmpty($rawPath)) { continue }
			try {
				$resolved = ConvertTo-ComparablePath $rawPath $cwdComparable
				if (-not (Test-UnderDirectory $resolved $cwdComparable)) { continue }
				$native = $resolved.Replace('/', [IO.Path]::DirectorySeparatorChar)
				if ([IO.Directory]::Exists($native)) {
					Add-AccessedTarget $resolved $true $cwdComparable $tempComparable $state
				}
				elseif ([IO.File]::Exists($native)) {
					Add-AccessedTarget $resolved $false $cwdComparable $tempComparable $state
				}
				else {
					$parentDir = Get-ComparableParent $resolved
					if (-not [string]::IsNullOrEmpty($parentDir) -and [IO.Directory]::Exists($parentDir.Replace('/', [IO.Path]::DirectorySeparatorChar))) {
						Add-AccessedTarget $resolved $false $cwdComparable $tempComparable $state
					}
				}
			}
			catch {
				continue
			}
		}
	}
	else {
		$targets = Get-ShellAccessedTargets $command $cwdComparable
		foreach ($t in $targets) {
			Add-AccessedTarget $t.ComparablePath $t.IsDirectory $cwdComparable $tempComparable $state
		}
	}

	# Candidate-first short-circuit: skip transcript streaming when zero candidates exist.
	if ($state.OrderedCandidates.Count -eq 0) { return }

	$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
	$transcriptPath = Get-AgentTranscriptField $Hook 'transcript_path'
	if ($transcriptPath -is [string] -and -not [string]::IsNullOrWhiteSpace($transcriptPath) -and [IO.File]::Exists($transcriptPath)) {
		$stream = [IO.FileStream]::new($transcriptPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
		$reader = [IO.StreamReader]::new($stream, $script:Utf8)
		try {
			while ($null -ne ($line = $reader.ReadLine())) {
				if ($line.IndexOf($script:MarkerPrefix, [StringComparison]::Ordinal) -lt 0) { continue }
				try {
					$rec = ConvertFrom-Json -InputObject $line -Depth 100 -NoEnumerate -ErrorAction Stop
				}
				catch {
					continue
				}
				if ($rec -isnot [Management.Automation.PSCustomObject]) { continue }
				$type = Get-AgentTranscriptField $rec 'type'
				if ($type -isnot [string] -or $type -cne 'response_item') { continue }
				$payloadObj = Get-AgentTranscriptField $rec 'payload'
				if ($payloadObj -isnot [Management.Automation.PSCustomObject]) { continue }
				$payloadType = Get-AgentTranscriptField $payloadObj 'type'
				$payloadRole = Get-AgentTranscriptField $payloadObj 'role'
				if ($payloadType -isnot [string] -or $payloadType -cne 'message') { continue }
				if ($payloadRole -isnot [string] -or $payloadRole -cne 'developer') { continue }
				$contentItems = Get-AgentTranscriptField $payloadObj 'content'
				if ($contentItems -isnot [object[]]) { continue }
				foreach ($item in $contentItems) {
					if ($item -isnot [Management.Automation.PSCustomObject]) { continue }
					$itemType = Get-AgentTranscriptField $item 'type'
					if ($itemType -isnot [string] -or $itemType -cne 'input_text') { continue }
					$itemText = Get-AgentTranscriptField $item 'text'
					if ($itemText -is [string]) { Add-MarkersFromText $itemText $seen }
				}
			}
		}
		finally {
			$reader.Dispose()
			$stream.Dispose()
		}
	}

	$text = Format-EmissionText $state $seen
	if ([string]::IsNullOrEmpty($text)) { return }
	$payload = ConvertTo-Json -Compress -Depth 6 ([ordered]@{
		hookSpecificOutput = [ordered]@{
			hookEventName = 'PostToolUse'
			additionalContext = $text
		}
	})
	Write-Utf8Stdout $payload
}

try {
	Import-Module (Join-Path $PSScriptRoot 'AgentShellLexer.psm1')
	Import-Module (Join-Path $PSScriptRoot 'AgentTranscriptCommon.psm1')

	$reader = [IO.StreamReader]::new([Console]::OpenStandardInput(), $script:Utf8)
	$rawInput = $reader.ReadToEnd()
	if ([string]::IsNullOrWhiteSpace($rawInput)) { exit 0 }
	$hook = ConvertFrom-Json -InputObject $rawInput -Depth 100 -NoEnumerate -ErrorAction Stop
	if ($hook -isnot [Management.Automation.PSCustomObject]) { exit 0 }

	$agyEvent = Get-AgentTranscriptField $hook 'hookEventName'
	if ($agyEvent -is [string] -and $agyEvent -ceq 'PostInvocation') {
		Invoke-AgyPostInvocation $hook
		exit 0
	}

	$codexEvent = Get-AgentTranscriptField $hook 'hook_event_name'
	if ($codexEvent -is [string] -and $codexEvent -ceq 'PostToolUse') {
		Invoke-CodexPostToolUse $hook
		exit 0
	}
}
catch {
	# Fail open: a hook error must never block tool execution or agent turns.
}
exit 0
