# Candidate scanner for /code-style-review: by default it reports candidate temporary
# instrumentation and style-rule candidates on lines this session added, so a review never has to
# separate them from pre-existing code by hand. The scan reports candidates only —
# it never decides whether a hit is temporary or a row is a violation, never edits a file, and writes
# nothing to disk (GIT_OPTIONAL_LOCKS=0 keeps Git from refreshing the index), so it is safe under a
# read-only sandbox. Stdout carries only the result document. With -Path it instead scans every line of
# the named tracked C++ files for the style-rule-<n> kinds only.
[CmdletBinding(DefaultParameterSetName = 'Session')]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory, ParameterSetName = 'Session')][string] $Baseline,
	[Parameter(ParameterSetName = 'Session')][string] $Head,
	[Parameter(ParameterSetName = 'Session')][switch] $IncludeUntracked,
	[Parameter(Mandatory, ParameterSetName = 'WholeFile')][string[]] $Path
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

$script:MaximumHits = 400
# The regions cap of Get-SessionChangeInventory.ps1: an emitted row count at the cap means the scan
# may have missed added lines the inventory dropped, so the result reports itself truncated. The
# inventory also sheds region rows below that cap to fit its own stdout budget, so a full count above
# the emitted count means the same thing.
$script:InventoryRegionCap = 400
$script:MaximumTextLength = 200
$script:MaximumMessageLength = 256
$script:MaximumOutputBytes = 131072

$script:InventoryScript = Join-Path $PSScriptRoot 'Get-SessionChangeInventory.ps1'
$script:CppClasses = @('cpp', 'dual-language-header')
# One entry per candidate kind: the residue kinds .agents/skills/code-style-review/references/worker.md
# step 17 removes, and one style-rule-<n> kind per rule of Documents/C++StyleGuide.txt that its step 10
# adjudicates. The order is the order a line is attributed: a line reports the first kind that matches
# it. An entry's Except clears a match that is one of the rule's permitted forms. The style-rule-61,
# style-rule-22, style-rule-59, style-rule-62 and style-rule-51 kinds are not in this table: each needs another
# head-side line too, so Test-Rule61Line, Test-Rule22Line and Test-Rule59Line decide theirs, in that order,
# before the table, and Test-Rule62Line then Test-Rule51Line decide theirs after the table, so they hide no
# table kind; style-rule-14 is the table's last entry for the same reason. That worker's step 7 hand-read
# list and these style-rule-<n> kinds together make the review's style mandate, and a rule is on both when
# each covers a different form, so update step 7 when a kind changes.
$script:ScalarType = '(?:(?:unsigned|signed)\s+)?(?:bool|char|wchar_t|short|int|long(?:\s+long)?|float|double)|unsigned|u?int(?:8|16|32|64)_t|size_t|u?intptr_t|ptrdiff_t'
$script:IntegerType = '(?:(?:unsigned|signed)\s+)?(?:short|int|long(?:\s+long)?)|unsigned|u?int(?:8|16|32|64)_t|ptrdiff_t'
# The prose-prone kinds share style-rule-2's comment-and-string alternative, so a line holding a comment
# or a quote character is never reported for them.
$script:CommentOrQuote = '(?://|/\*|\*/|["''])'
$script:CandidatePatterns = @(
	@{ Kind = 'log'; Pattern = '\bLOG\s*\(' }
	@{ Kind = 'printf'; Pattern = '\bprintf\s*\(' }
	@{ Kind = 'debug-break'; Pattern = '\bDEBUG_BREAK\s*\(\s*\)' }
	# The engine spells the macro ASSERT, so the always-failing assertion is matched without case.
	@{ Kind = 'assert-false'; Pattern = '(?i)\bassert\s*\(\s*false\s*\)' }
	@{ Kind = 'fixme'; Pattern = '(?://|/\*|^\s*\*).*\bFIXME\b' }
	@{ Kind = 'hack'; Pattern = '(?://|/\*|^\s*\*).*\bHACK\b' }
	@{ Kind = 'style-rule-2'; Pattern = '^\s*(?:[A-Za-z_]\w*(?:::[A-Za-z_]\w*)?(?:<[^{};]*>)?\s+)+(?:[*&]\s*)?[A-Za-z_]\w*(?:\s*\[[^\]]*\])?\s*\{\s*$'; Except = '(?://|/\*|\*/|["''])|^\s*(?:class|struct|union|enum|namespace|return|if|else|for|while|switch|try|catch|do)\b' }
	@{ Kind = 'style-rule-15'; Pattern = '\bauto\b'; Except = 'auto\s*&?&?\s*\[|\bauto\s+(?:vec|mat)|\bauto\s*&?\s+(?:it|\w+It)\b|=\s*\[|<[^<>]*>\s*[({]|\bdecltype\s*\(\s*auto\s*\)' }
	@{ Kind = 'style-rule-18'; Pattern = '^\s*const\s+(?:[A-Za-z_][\w:]*(?:<[^;]*>)?\s+)+[A-Za-z_]\w*\s*[={(]'; Except = '^\s*const\s+[^=({;<]*(?:<[^;]*>)?[^=({;<]*[&*]' }
	@{ Kind = 'style-rule-19'; Pattern = '\btemplate\s*<[^>]*(?:\bclass\b|\btypename(?:\.\.\.)?\s+[A-Z]*[a-z])' }
	@{ Kind = 'style-rule-27'; Pattern = '\b\d+\.(?:\d+(?:[eE][-+]?\d+)?)?(?:[^\w.]|$)|\b\d+\.f\b|(?:^|[^\w.])\.\d+(?:f|\b)' }
	@{ Kind = 'style-rule-28'; Pattern = '\bNULL\b' }
	@{ Kind = 'style-rule-29'; Pattern = '\bvirtual\b.*\)\s*(?:const\s*)?(?:noexcept\s*)?;'; Except = '\boverride\b|\bfinal\b' }
	@{ Kind = 'style-rule-32'; Pattern = '\bstd::map\s*<' }
	@{ Kind = 'style-rule-41'; Pattern = '\busing\s+namespace\s+[\w:]+\s*;'; Except = 'using\s+namespace\s+DirectX\s*;' }
	@{ Kind = 'style-rule-50'; Pattern = '\b(?:if|while)\s*\((?:.*(?:&&|\|\||\())?\s*!?\s*(?:[\w.>-]*(?:->|\.))?[gms]?p[A-Z]\w*\s*(?:\)|&&|\|\|)' }
	@{ Kind = 'style-rule-52'; Pattern = '\w\{\}' }
	@{ Kind = 'style-rule-57'; Pattern = '\b\w+(?:Impl|Internal)\s*\(' }
	@{ Kind = 'style-rule-58'; Pattern = '^\s*#\s*ifn?def\b' }
	@{ Kind = 'style-rule-1'; Pattern = '^ +\S'; Except = '^ +\*' }
	@{ Kind = 'style-rule-5'; Pattern = '\bnew\s+[A-Za-z_]|\bdelete\b|\b(?:malloc|calloc|realloc|free)\s*\('; Except = $script:CommentOrQuote + '|=\s*delete\b|\boperator\s+(?:new|delete)\b' }
	@{ Kind = 'style-rule-6'; Pattern = '\bBT_(?:DEBUG|RELEASE|PROFILE)\b|^\s*#\s*(?:el)?if\b.*\bkb[A-Z]'; Except = $script:CommentOrQuote }
	@{ Kind = 'style-rule-10'; Pattern = '^\s*#\s*include\s*["<][^">]*\\' }
	# A `(void)name;` discard is also a parenthesized builtin type followed by an operand, so rule 39
	# precedes rule 11.
	@{ Kind = 'style-rule-39'; Pattern = '\(\s*void\s*\)\s*[A-Za-z_]\w*\s*;' }
	@{ Kind = 'style-rule-11'; Pattern = '(?<!\b(?:alignas|alignof|sizeof|decltype)\s*)\((?:const\s+)?(?:void|' + $script:ScalarType + '|[A-Za-z_][\w:]*(?=\s*(?:const\s*)?\*))(?:\s*const)?(?:\s*\*)*\s*\)\s*(?!(?:const|override|noexcept|final|volatile|mutable)\b)[\w(]'; Except = '^\s*//' }
	@{ Kind = 'style-rule-17'; Pattern = '\b(?:' + $script:IntegerType + ')\s+[A-Za-z_]\w*\s*[={][^;]*\.size\s*\(\s*\)|\bfor\s*\(\s*(?:' + $script:IntegerType + ')\s+[A-Za-z_]\w*[^;]*;[^;]*\.size\s*\(\s*\)' }
	@{ Kind = 'style-rule-20'; Pattern = '^\s*(?:(?:static|inline|constexpr|const|thread_local|mutable)\s+)*(?:' + $script:ScalarType + ')\s+[A-Za-z_]\w*\s*\{' }
	@{ Kind = 'style-rule-23'; Pattern = '\btypedef\b'; Except = $script:CommentOrQuote }
	@{ Kind = 'style-rule-25'; Pattern = '\bconstexpr\b'; Except = '\b(?:static|inline)\b|\bif\s+constexpr\b|\bconstexpr\s+[^=;{]*[\w)*&>]\s*\(' }
	@{ Kind = 'style-rule-26'; Pattern = '\busing\s+enum\b' }
	@{ Kind = 'style-rule-30'; Pattern = '>\s+>' }
	@{ Kind = 'style-rule-33'; Pattern = '\b(?:CHAR_BIT|MB_LEN_MAX|S?CHAR_MIN|S?CHAR_MAX|UCHAR_MAX|SHRT_MIN|SHRT_MAX|USHRT_MAX|INT_MIN|INT_MAX|UINT_MAX|LONG_MIN|LONG_MAX|ULONG_MAX|LLONG_MIN|LLONG_MAX|ULLONG_MAX)\b' }
	# The prefix walks the line past string literals, character literals and comments, treating a quote
	# directly after a digit as a digit separator, so only a literal in code is reported.
	@{ Kind = 'style-rule-34'; Pattern = '^(?:[^"''/]|/(?![/*])|(?<=\d)''|''(?:\\.|[^''\\])*''|"(?:\\.|[^"\\])*")*?(?<![\w.''])\d{4,}(?![\d''])'; Except = '^\s*#\s*(?:pragma|line)\b|^\s*\*(?:\s|/|$)' }
	@{ Kind = 'style-rule-35'; Pattern = '\b(?:(?:CreateDirectory|RemoveDirectory|DeleteFile|GetFileAttributes(?:Ex)?|SetFileAttributes|PathFileExists|FindFirstFile(?:Ex)?|FindNextFile)[AW]?|FindClose|_w?mkdir|_w?rmdir|_w?unlink|_w?access(?:_s)?|_w?stat(?:32|64|i64)?|_w?findfirst(?:32|64)?|_w?findnext(?:32|64)?|mkdir|rmdir|unlink|opendir|readdir)\s*\('; Except = $script:CommentOrQuote }
	@{ Kind = 'style-rule-36'; Pattern = '^\s*(?:(?:static|inline|thread_local|volatile|mutable)\s+)*(?:const\s+)?(?:(?:' + $script:ScalarType + ')(?:\s*\*+\s*|\s+)|(?!(?:return|delete|co_return|throw|goto|case|else|do)\b)[A-Za-z_][\w:]*(?:<[^;]*>)?\s*\*+\s*)(?:const\s+)?[A-Za-z_]\w*\s*;' }
	@{ Kind = 'style-rule-37'; Pattern = '\bstd::get\s*<\s*\d+\s*>\s*\(|\bstd::tie\s*\(' }
	@{ Kind = 'style-rule-40'; Pattern = '[(,]\s*const\s+(?:(?:char|wchar_t)\s*\*|std::w?string\s*&)'; Except = '\bfor\s*\(\s*const\s+(?:(?:char|wchar_t)\s*\*|std::w?string\s*&)' }
	@{ Kind = 'style-rule-44'; Pattern = '\bXM(?:Load|Store)Float(?:2|3|4|3x4|4x3|4x4)\s*\(' }
	@{ Kind = 'style-rule-46'; Pattern = '\bXM\w*Est\s*\(' }
	@{ Kind = 'style-rule-54'; Pattern = '^\s*(?:(?:static|inline|const|mutable)\s+)*Vk[A-Z]\w*\s+[A-Za-z_]\w*\s*(?:[=;{]|$)'; Except = '^\s*(?:(?:static|inline|const|mutable)\s+)*Vk([A-Z]\w*)\s+[A-Za-z_]\w*Vk\1\s*(?:[=;{]|$)' }
	@{ Kind = 'style-rule-55'; Pattern = '^\s*enum\b(?!\s+(?:class|struct)\b)'; Except = $script:CommentOrQuote }
	# `Num` as its own word in an identifier, so Number and Enumerate stay clear; a name reached through ::, ->
	# or . belongs to another API.
	@{ Kind = 'style-rule-14'; Pattern = '(?<!(?:::|->|\.)\s*)\b(?:\w*[a-z0-9_])?Num(?![a-z])'; Except = $script:CommentOrQuote }
)
$script:ScannedPatterns = $script:CandidatePatterns
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Root = $null
$script:HeadSha = ''
$script:NewSideLines = @{}

$result = [ordered]@{
	schemaVersion = 'broken-engine-session-candidates/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Session candidate scan did not run.'
	hits = @()
	counts = $null
	truncated = $false
}

function Complete-SessionCandidates([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = if ($Message.Length -gt $script:MaximumMessageLength) { $Message.Substring(0, $script:MaximumMessageLength) } else { $Message }
	$stream = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes(($result | ConvertTo-Json -Depth 32 -Compress))
	$stream.Write($bytes, 0, $bytes.Length)
	$stream.Flush()
	exit $ExitCode
}

function Invoke-CandidateGit([string[]] $Arguments) {
	$run = Invoke-AgentProcess 'git' (@('-C', $script:Root, '--no-pager') + $Arguments) $script:Root
	if ($run.ExitCode -ne 0) { throw "git $($Arguments -join ' ') failed with exit $($run.ExitCode): $($run.Stderr.Trim())" }
	return $run.Stdout
}

function Get-NewSideLine([string] $Path) {
	# Each return wraps the cached array so a one-line file stays an array of one line instead of
	# unrolling to a bare string, whose indexer would hand back a single character.
	if ($script:NewSideLines.ContainsKey($Path)) { return , $script:NewSideLines[$Path] }
	# The added lines belong to the head side of the compared diff: a commit-valued head reads its blob,
	# and a working-tree head (including an untracked addition) reads the file itself.
	$text = if ([string]::IsNullOrEmpty($script:HeadSha)) {
		[IO.File]::ReadAllText((Join-Path $script:Root ($Path -replace '/', [IO.Path]::DirectorySeparatorChar)), $script:Utf8)
	}
	else {
		Invoke-CandidateGit @('show', "$($script:HeadSha):$Path")
	}
	$script:NewSideLines[$Path] = @($text -split "`r`n|`n|`r")
	return , $script:NewSideLines[$Path]
}

function Get-InventoryDocument() {
	$arguments = @('-NoProfile', '-File', $script:InventoryScript, '-RepositoryRoot', $script:Root, '-Baseline', $Baseline, '-Regions')
	if (-not [string]::IsNullOrWhiteSpace($Head)) { $arguments += @('-Head', $Head) }
	if ($IncludeUntracked) {
		# The inventory reports an untracked path only when the caller lists it, so the pass-through
		# switch supplies the whole untracked set in the comma-separated form that script splits.
		$untracked = @((Invoke-CandidateGit @('ls-files', '--others', '--exclude-standard', '-z')) -split "`0" | Where-Object { -not [string]::IsNullOrEmpty($_) })
		if ($untracked.Count -gt 0) { $arguments += @('-IncludeUntracked', ($untracked -join ',')) }
	}
	$shell = [Environment]::ProcessPath
	if ([string]::IsNullOrEmpty($shell)) { $shell = 'pwsh' }
	$run = Invoke-AgentProcess $shell $arguments $script:Root
	$document = $null
	if (-not [string]::IsNullOrWhiteSpace($run.Stdout)) { $document = $run.Stdout | ConvertFrom-Json }
	if ($run.ExitCode -ne 0 -or $null -eq $document -or $document.status -cne 'pass') {
		$reason = if ($null -ne $document) { "$($document.code): $($document.message)" } else { $run.Stderr.Trim() }
		Complete-SessionCandidates 2 'blocked' 'candidates.inventory-unavailable' "The session change inventory did not produce a scannable result: $reason"
	}
	return $document
}

function Get-AddedLine([object] $Inventory) {
	# Only C++ classes are scanned, and only the head side of each added or modified region: with the
	# inventory's -U0 regions, every line in a region's head-side range is a line this session added.
	$cppPaths = [Collections.Generic.HashSet[string]]::new([string[]] @())
	foreach ($entry in $Inventory.entries) {
		if ($script:CppClasses -ccontains $entry.class) { [void] $cppPaths.Add($entry.path) }
	}
	$added = [Collections.Generic.List[object]]::new()
	if ($null -eq $Inventory.regions) { return $added }
	foreach ($region in $Inventory.regions) {
		if (-not $cppPaths.Contains($region.path) -or $null -eq $region.startLine) { continue }
		$lines = Get-NewSideLine $region.path
		for ($number = [int] $region.startLine; $number -le [int] $region.endLine; $number++) {
			if ($number -lt 1 -or $number -gt $lines.Count) { continue }
			$added.Add([pscustomobject] @{ Path = $region.path; Line = $number; Text = $lines[$number - 1] })
		}
	}
	return $added
}

function Get-FileLine([string[]] $Paths) {
	# Whole-file mode scans every working-tree line of each named file, which must be a tracked C++ file.
	$relative = @($Paths | ForEach-Object { $_ -replace '\\', '/' })
	$tracked = [Collections.Generic.HashSet[string]]::new([string[]] @((Invoke-CandidateGit (@('--literal-pathspecs', 'ls-files', '-z', '--') + $relative)) -split "`0" | Where-Object { -not [string]::IsNullOrEmpty($_) }))
	$scanned = [Collections.Generic.List[object]]::new()
	foreach ($file in $relative) {
		if ($file -cnotmatch '\.(?:h|cpp)$' -or -not $tracked.Contains($file) -or -not (Test-Path -LiteralPath (Join-Path $script:Root $file) -PathType Leaf)) {
			Complete-SessionCandidates 2 'blocked' 'candidates.path-invalid' "-Path must name tracked *.h or *.cpp files relative to the repository root: '$file'."
		}
		$lines = Get-NewSideLine $file
		for ($number = 1; $number -le $lines.Count; $number++) {
			$scanned.Add([pscustomobject] @{ Path = $file; Line = $number; Text = $lines[$number - 1] })
		}
	}
	return $scanned
}

function Test-Rule61Line([string] $Path, [int] $Line, [string] $Text) {
	# An `if` (an `else if` counts as its `if`) or `else` line breaks rule 61 when a statement follows the
	# condition on the same line, or when the next non-blank head-side line is neither `{` nor a `&&`/`||`
	# continuation of the condition. A trailing `{` is a brace, not a statement.
	if ($Text -cnotmatch '^\s*(?:else\s+)?if\b|^\s*else\b') { return $false }
	$code = $Text -replace '//.*$', ''
	$rest = ''
	if ($code -cmatch '^\s*(?:else\s+)?if\b') {
		$depth = 0
		for ($index = $code.IndexOf('('); $index -ge 0 -and $index -lt $code.Length; $index++) {
			if ($code[$index] -eq '(') { $depth++ }
			elseif ($code[$index] -eq ')') {
				$depth--
				if ($depth -eq 0) { $rest = $code.Substring($index + 1); break }
			}
		}
	}
	else {
		$rest = $code -replace '^\s*else\b', ''
	}
	$rest = $rest.Trim()
	if ($rest.StartsWith('{')) { return $false }
	if ($rest.Length -gt 0) { return $true }
	$lines = Get-NewSideLine $Path
	for ($number = $Line + 1; $number -le $lines.Count; $number++) {
		$next = $lines[$number - 1].Trim()
		if ($next.Length -eq 0) { continue }
		return -not ($next.StartsWith('{') -or $next.StartsWith('&&') -or $next.StartsWith('||'))
	}
	return $false
}

function Test-Rule22Line([string] $Path, [int] $Line, [string] $Text) {
	# A line breaks rule 22 when it ends without a comma and the next non-blank head-side line starts with
	# the closing `}`. A line ending in `;`, `{`, `}`, `:` or `\` is a statement, a brace, a label or a macro
	# continuation, not a list element.
	$code = ($Text -replace '//.*$', '').Trim()
	if ($code.Length -eq 0 -or $code -cmatch '^(?:#|/\*|\*)' -or $code -cmatch '[,;{}:\\]$') { return $false }
	$lines = Get-NewSideLine $Path
	for ($number = $Line + 1; $number -le $lines.Count; $number++) {
		$next = $lines[$number - 1].Trim()
		if ($next.Length -eq 0) { continue }
		return $next.StartsWith('}')
	}
	return $false
}

function Test-Rule59Line([string] $Path, [int] $Line, [string] $Text) {
	# A case or default label breaks rule 59 when its indent is not exactly one tab deeper than the nearest
	# preceding head-side switch line indented no deeper than the label, so a nested switch in an earlier case
	# body is skipped while a label written at its own switch's indent is still caught.
	if (-not ($Text -cmatch '^(\s*)(?:case\b|default\s*:)')) { return $false }
	$indent = $Matches[1]
	$lines = Get-NewSideLine $Path
	for ($number = $Line - 1; $number -ge 1; $number--) {
		if (-not ($lines[$number - 1] -cmatch '^(\s*)switch\b') -or $Matches[1].Length -gt $indent.Length) { continue }
		return $indent -cne "$($Matches[1])`t"
	}
	return $false
}

function Test-Rule62Line([string] $Path, [int] $Line, [string] $Text) {
	# An `if` (an `else if` counts as its `if`) line breaks rule 62 when its condition, which may continue
	# over later head-side lines until its parentheses close, has a `||` at depth one outside comments and
	# literals, and its braced body is one `return`, `continue` or `break` statement.
	if ($Text -cnotmatch '^\s*(?:else\s+)?if\b') { return $false }
	$lines = Get-NewSideLine $Path
	$depth = 0
	$opened = $false
	$hasOr = $false
	$number = $Line
	while ($true) {
		if ($number -gt $lines.Count) { return $false }
		$code = ($lines[$number - 1] -replace '"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*''', '""') -replace '//.*$', ''
		$index = if ($opened) { 0 } else { $code.IndexOf('(') }
		for (; $index -ge 0 -and $index -lt $code.Length; $index++) {
			if ($code[$index] -eq '(') { $depth++; $opened = $true }
			elseif ($code[$index] -eq ')') {
				$depth--
				if ($depth -eq 0) { break }
			}
			elseif ($depth -eq 1 -and $code[$index] -eq '|' -and $index + 1 -lt $code.Length -and $code[$index + 1] -eq '|') { $hasOr = $true; $index++ }
		}
		if (-not $opened) { return $false }
		if ($depth -eq 0) {
			if (-not $hasOr -or $code.Substring($index + 1).Trim().Length -gt 0) { return $false }
			break
		}
		$number++
	}
	$body = [Collections.Generic.List[string]]::new()
	for ($number++; $number -le $lines.Count -and $body.Count -lt 3; $number++) {
		$next = ($lines[$number - 1] -replace '//.*$', '').Trim()
		if ($next.Length -gt 0) { $body.Add($next) }
	}
	return $body.Count -eq 3 -and $body[0] -ceq '{' -and $body[1] -cmatch '^(?:return\b[^;]*|continue|break)\s*;$' -and $body[2] -ceq '}'
}

function Test-Rule51Line([string] $Path, [int] $Line, [string] $Text) {
	# A line breaks rule 51 when it leaves a parenthesis open outside comments and literals and ends in `,` or
	# `(`, so a call's or declaration's arguments wrap, unless the next non-blank head-side line starts with the
	# `{` rule 2 puts on the next line for a lambda or struct literal argument.
	if ($Text.Trim() -cmatch '^(?:#|/\*|\*)') { return $false }
	$code = (($Text -replace '"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*''', '""') -replace '//.*$', '').Trim()
	if ($code -cnotmatch '[,(]$') { return $false }
	# Counted by length rather than a pipeline, whose one-match result has no Count under strict mode.
	if ($code.Length - $code.Replace('(', '').Length -le $code.Length - $code.Replace(')', '').Length) { return $false }
	$lines = Get-NewSideLine $Path
	for ($number = $Line + 1; $number -le $lines.Count; $number++) {
		$next = $lines[$number - 1].Trim()
		if ($next.Length -eq 0) { continue }
		return -not $next.StartsWith('{')
	}
	return $false
}

function Test-CandidatePattern([string] $Text) {
	foreach ($pattern in $script:ScannedPatterns) {
		if ($Text -cnotmatch $pattern.Pattern) { continue }
		if ($pattern.ContainsKey('Except') -and $Text -cmatch $pattern.Except) { continue }
		return $pattern.Kind
	}
	return $null
}

try {
	$script:Root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not (Test-Path -LiteralPath $script:Root -PathType Container)) {
		Complete-SessionCandidates 2 'blocked' 'candidates.repository-root-invalid' "-RepositoryRoot must be an existing directory: '$RepositoryRoot'."
	}
	if ($PSCmdlet.ParameterSetName -ceq 'WholeFile') {
		# A residue kind such as log would otherwise hide a style kind on an existing line.
		$script:ScannedPatterns = @($script:CandidatePatterns | Where-Object { $_.Kind.StartsWith('style-rule-') })
		$scannedLines = Get-FileLine $Path
		$regionsCapped = $false
		$scope = "$(@($Path).Count) named C++ file(s)"
	}
	else {
		if (-not (Test-Path -LiteralPath $script:InventoryScript -PathType Leaf)) {
			Complete-SessionCandidates 2 'blocked' 'candidates.inventory-missing' "The session change inventory script is missing: '$($script:InventoryScript)'."
		}
		$inventory = Get-InventoryDocument
		$script:HeadSha = if ([string]::IsNullOrWhiteSpace($inventory.headSha)) { '' } else { $inventory.headSha }
		# A passing inventory always carries truncation.regions, so both counts are read directly.
		$emittedRegionCount = @($inventory.regions).Count
		$regionsCapped = $emittedRegionCount -ge $script:InventoryRegionCap -or [int] $inventory.truncation.regions.full -gt $emittedRegionCount
		$scannedLines = Get-AddedLine $inventory
		$scope = 'session-added C++ lines'
	}

	$hits = [Collections.Generic.List[object]]::new()
	foreach ($line in $scannedLines) {
		$kind = if (Test-Rule61Line $line.Path $line.Line $line.Text) { 'style-rule-61' }
		elseif (Test-Rule22Line $line.Path $line.Line $line.Text) { 'style-rule-22' }
		elseif (Test-Rule59Line $line.Path $line.Line $line.Text) { 'style-rule-59' }
		else { Test-CandidatePattern $line.Text }
		if ($null -eq $kind -and (Test-Rule62Line $line.Path $line.Line $line.Text)) { $kind = 'style-rule-62' }
		if ($null -eq $kind -and (Test-Rule51Line $line.Path $line.Line $line.Text)) { $kind = 'style-rule-51' }
		if ($null -eq $kind) { continue }
		$text = $line.Text.Trim()
		if ($text.Length -gt $script:MaximumTextLength) { $text = $text.Substring(0, $script:MaximumTextLength) }
		$hits.Add([ordered]@{ path = $line.Path; line = $line.Line; kind = $kind; text = $text })
	}
	$sorted = [Collections.Generic.List[object]]::new($hits)
	$sorted.Sort([Comparison[object]] {
		param($left, $right)
		$compare = [string]::CompareOrdinal($left.path, $right.path)
		if ($compare -ne 0) { return $compare }
		return $left.line - $right.line
	})

	# Counts always describe the complete scan, never the truncated emission.
	$counts = [ordered]@{ total = $sorted.Count }
	foreach ($pattern in $script:CandidatePatterns) { $counts[$pattern.Kind] = @($sorted | Where-Object { $_.kind -ceq $pattern.Kind }).Count }
	$counts['style-rule-61'] = @($sorted | Where-Object { $_.kind -ceq 'style-rule-61' }).Count
	$counts['style-rule-22'] = @($sorted | Where-Object { $_.kind -ceq 'style-rule-22' }).Count
	$counts['style-rule-59'] = @($sorted | Where-Object { $_.kind -ceq 'style-rule-59' }).Count
	$counts['style-rule-62'] = @($sorted | Where-Object { $_.kind -ceq 'style-rule-62' }).Count
	$counts['style-rule-51'] = @($sorted | Where-Object { $_.kind -ceq 'style-rule-51' }).Count
	$result.counts = $counts
	$emitted = [Collections.Generic.List[object]]::new()
	foreach ($hit in ($sorted | Select-Object -First $script:MaximumHits)) { $emitted.Add($hit) }
	while ($true) {
		$result.hits = [object[]] $emitted.ToArray()
		$result.truncated = $emitted.Count -lt $sorted.Count -or $regionsCapped
		if ($script:Utf8.GetByteCount(($result | ConvertTo-Json -Depth 32 -Compress)) -le $script:MaximumOutputBytes) { break }
		if ($emitted.Count -eq 0) { break }
		$drop = [Math]::Max(1, [int] [Math]::Ceiling($emitted.Count * 0.1))
		$emitted.RemoveRange($emitted.Count - $drop, $drop)
	}
	Complete-SessionCandidates 0 'pass' 'ok' "Scanned the $scope and found $($sorted.Count) candidate(s)."
}
catch {
	Complete-SessionCandidates 1 'error' 'internal.error' $_.Exception.Message
}
