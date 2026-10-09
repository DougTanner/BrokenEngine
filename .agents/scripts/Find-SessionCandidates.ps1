# Candidate scanner for /code-style-review: by default it reports candidate temporary
# instrumentation and style-rule candidates on lines this session added, so a review never has to
# separate them from pre-existing code by hand. The scan reports candidates only —
# it never decides whether a hit is temporary or a row is a violation, never edits a source file, and
# writes only the caller's -OutputPath result and one OS temp file holding the inventory, deleted before
# it exits (GIT_OPTIONAL_LOCKS=0 keeps Git from refreshing the index). Stdout carries the result
# document, or with -OutputPath one summary line. With -Path it instead scans every line of the named
# tracked C++ files for the style-rule-<n> kinds only.
[CmdletBinding(DefaultParameterSetName = 'Session')]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory, ParameterSetName = 'Session')][string] $Baseline,
	[Parameter(ParameterSetName = 'Session')][string] $Head,
	[Parameter(ParameterSetName = 'Session')][switch] $IncludeUntracked,
	[Parameter(ParameterSetName = 'Session')][string[]] $PathPrefix,
	[Parameter(Mandatory, ParameterSetName = 'WholeFile')][string[]] $Path,
	[string] $OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

$script:MaximumHits = 400
$script:MaximumTextLength = 200
$script:MaximumMessageLength = 256
$script:MaximumOutputBytes = 131072

$script:InventoryScript = Join-Path $PSScriptRoot 'Get-SessionChangeInventory.ps1'
$script:CppClasses = @('cpp', 'dual-language-header')
# One entry per candidate kind: the residue kinds .agents/skills/code-style-review/references/worker.md
# step 16 removes, and one style-rule-<n> kind per rule of Documents/C++StyleGuide.txt that its step 9
# adjudicates. A line reports every kind that matches it, each as its own row. An entry's Except clears a
# match that is one of the rule's permitted forms or a form another kind reports. The style-rule-61,
# style-rule-22, style-rule-59, style-rule-62 and style-rule-51 kinds are not in this table: each needs
# another head-side line too, so Test-Rule61Line, Test-Rule22Line, Test-Rule59Line, Test-Rule62Line and
# Test-Rule51Line decide theirs. That worker's step 6 hand-read list, its step 16 rule 70 check, and these
# style-rule-<n> kinds together make the review's style mandate, and a rule is on two lists when each covers
# a different form, so update step 6 when a kind changes.
$script:ScalarType = '(?:(?:unsigned|signed)\s+)?(?:bool|char|wchar_t|short|int|long(?:\s+long)?|float|double)|unsigned|u?int(?:8|16|32|64)_t|size_t|u?intptr_t|ptrdiff_t'
$script:IntegerType = '(?:(?:unsigned|signed)\s+)?(?:short|int|long(?:\s+long)?)|unsigned|u?int(?:8|16|32|64)_t|size_t|ptrdiff_t'
# The prose-prone kinds share style-rule-2's comment-and-string alternative, so a line holding a comment
# or a quote character is never reported for them.
$script:CommentOrQuote = '(?://|/\*|\*/|["''])'
# The prefix walks the line past string and character literals, treating a quote directly after a digit as a
# digit separator, and stops at a comment, so a kind whose pattern starts with it reports only a match in code
# before any comment.
$script:CodePrefix = '^(?:[^"''/]|/(?![/*])|(?<=\d)''|''(?:\\.|[^''\\])*''|"(?:\\.|[^"\\])*")*?'
$script:CandidatePatterns = @(
	@{ Kind = 'log'; Pattern = '\bLOG\s*\(' }
	@{ Kind = 'printf'; Pattern = '\bprintf\s*\(' }
	@{ Kind = 'debug-break'; Pattern = '\bDEBUG_BREAK\s*\(\s*\)' }
	# The engine spells the macro ASSERT, so the always-failing assertion is matched without case.
	@{ Kind = 'assert-false'; Pattern = '(?i)\bassert\s*\(\s*false\s*\)' }
	@{ Kind = 'fixme'; Pattern = '(?://|/\*|^\s*\*).*\bFIXME\b' }
	@{ Kind = 'hack'; Pattern = '(?://|/\*|^\s*\*).*\bHACK\b' }
	@{ Kind = 'style-rule-2'; Pattern = '^\s*(?:[A-Za-z_]\w*(?:::[A-Za-z_]\w*)?(?:<[^{};]*>)?\s+)+(?:[*&]\s*)?[A-Za-z_]\w*(?:\s*\[[^\]]*\])?\s*\{\s*$'; Except = '(?://|/\*|\*/|["''])|^\s*(?:class|struct|union|enum|namespace|return|if|else|for|while|switch|try|catch|do)\b' }
	@{ Kind = 'style-rule-15'; Pattern = $script:CodePrefix + '\bauto\b'; Except = 'auto\s*&?&?\s*\[|\bauto\s+(?:vec|mat)[A-Z]|\bauto\s*&?\s+(?:it|\w+It)\b|=\s*\[|=\s*[&*]?(?:[\w:.]|->)*<[^<>]*>\s*[({]|\bdecltype\s*\(\s*auto\s*\)' }
	@{ Kind = 'style-rule-18'; Pattern = '^\s*(?:static\s+)?const\s+(?![^=({;<]*(?:<[^;]*>)?[^=({;<]*[&*])(?:[A-Za-z_][\w:]*(?:<[^;]*>)?\s+)+[A-Za-z_]\w*\s*[={(]|^\s*(?:static\s+)?const\s+auto\s*\[|^\s*(?:[A-Za-z_][\w:]*(?:<[^;]*>)?\s*\*?\s+)+const\s+[A-Za-z_]\w*\s*[={(]|\bfor\s*\(\s*const\s+(?:[A-Za-z_][\w:]*(?:<[^;]*>)?\s+)+[A-Za-z_]\w*\s*:' }
	@{ Kind = 'style-rule-19'; Pattern = '\btemplate\s*<[^>]*(?:\bclass\b|[\w.]\s+[A-Z0-9_]*[a-z]\w*\s*(?:[,=>]|$))' }
	@{ Kind = 'style-rule-27'; Pattern = $script:CodePrefix + '(?:\b\d+\.(?:\d+(?:[eE][-+]?\d+)?)?(?:[^\w.]|$)|\b\d+\.f\b|(?<![\w.])\.\d+(?:f|\b))' }
	@{ Kind = 'style-rule-68'; Pattern = $script:CodePrefix + '(?:\bstatic_cast\s*<\s*(?:std::)?(?:u?int(?:16|32|64)_t|uint8_t)\s*>\s*\(\s*-?(?:0[xX][0-9a-fA-F'']+|\d[\d'']*)\s*\)|(?<![\w:])(?:std::)?(?:u?int(?:16|32|64)_t|uint8_t)\s*\{\s*-?(?:0[xX][0-9a-fA-F'']+|\d[\d'']*)\s*\}|(?<![\w.''])(?:0[xX][0-9a-fA-F'']+|\d[\d'']*)(?:[uU]?(?:ll|LL)|(?:ll|LL)[uU]|[uU][lL]?|[lL][uU])\b)' }
	@{ Kind = 'style-rule-28'; Pattern = $script:CodePrefix + '\bNULL\b' }
	@{ Kind = 'style-rule-29'; Pattern = '\bvirtual\b.*\)\s*(?:const\s*)?(?:noexcept(?:\s*\([^)]*\))?\s*)?(?:;|\{|$)'; Except = '\boverride\b|\bfinal\b' }
	@{ Kind = 'style-rule-32'; Pattern = '\bstd::map\s*<' }
	@{ Kind = 'style-rule-41'; Pattern = '\busing\s+namespace\s+[\w:]+\s*;'; Except = 'using\s+namespace\s+(?:DirectX|std::chrono_literals)\s*;' }
	@{ Kind = 'style-rule-50'; Pattern = '\b(?:if|while)\s*\((?:.*(?:&&|\|\||\(|;))?\s*!?\s*(?<!\bsizeof\s*\(\s*)(?:[\w.>-]*(?:->|\.))?[gms]?p[A-Z]\w*\s*(?:\)|&&|\|\|)|\b(?:if|while)\s*\((?:.*(?:&&|\|\||(?<![\w\])]\s*)\(|;))?\s*!?\s*(?:[\w.>-]*(?:->|\.))?[gms]?p(?:p|c|ui|i|b|e|f[234]?|vec|mat)[A-Z]\w*\s*(?:\)|&&|\|\|)|\b(?:if|while)\s*\(\s*(?:const\s+)?[A-Za-z_][\w:]*(?:<[^;]*>)?\s*\*+\s*(?:const\s+)?[A-Za-z_]\w*\s*=[^;]*\)\s*(?:[{/].*)?$' }
	@{ Kind = 'style-rule-52'; Pattern = $script:CodePrefix + '[\w>]\{' }
	@{ Kind = 'style-rule-57'; Pattern = '\b\w+(?:Impl|Internal)\s*\(' }
	@{ Kind = 'style-rule-58'; Pattern = '^\s*#\s*(?:el)?ifn?def\b' }
	@{ Kind = 'style-rule-1'; Pattern = '^ +[^ ]'; Except = '^ +\*(?:\s|/|$)' }
	@{ Kind = 'style-rule-5'; Pattern = $script:CodePrefix + '(?:\bnew\s+[A-Za-z_]|\bdelete\b|\b(?:malloc|calloc|realloc|free|_aligned_(?:malloc|realloc|free))\s*\()'; Except = '^\s*\*(?:\s|/|$)|=\s*delete\b|\boperator\s+(?:new|delete)\b' }
	@{ Kind = 'style-rule-6'; Pattern = '\bBT_(?:DEBUG|RELEASE|PROFILE)\b|^\s*#\s*(?:el)?if\b.*\bkb[A-Z]'; Except = $script:CommentOrQuote }
	@{ Kind = 'style-rule-10'; Pattern = '^\s*#\s*include\s*["<][^">]*\\' }
	@{ Kind = 'style-rule-39'; Pattern = '\(\s*void\s*\)\s*[A-Za-z_]\w*\s*;|\bstatic_cast\s*<\s*void\s*>\s*\(\s*[A-Za-z_]\w*\s*\)\s*;|\bUNREFERENCED_PARAMETER\s*\(|\bstd::ignore\s*=\s*[A-Za-z_]\w*\s*;' }
	# A `(void)name;` or `static_cast<void>(name);` discard is style-rule-39's row, not a rule 11 cast.
	@{ Kind = 'style-rule-11'; Pattern = '(?<!\b(?:alignas|alignof|sizeof|decltype)\s*)\((?:const\s+)?(?:void|' + $script:ScalarType + '|[A-Za-z_][\w:]*(?=\s*(?:const\s*)?\*))(?:\s*const)?(?:\s*\*)*\s*\)\s*(?!(?:const|override|noexcept|final|volatile|mutable)\b)[\w(]|\bstatic_cast\s*<\s*void\s*>\s*\('; Except = '^\s*//|\(\s*void\s*\)\s*[A-Za-z_]\w*\s*;|\bstatic_cast\s*<\s*void\s*>\s*\(\s*[A-Za-z_]\w*\s*\)\s*;' }
	@{ Kind = 'style-rule-17'; Pattern = '\b(?:' + $script:IntegerType + ')\s+[A-Za-z_]\w*\s*[={][^;]*\.size\s*\(\s*\)|\bfor\s*\(\s*(?:' + $script:IntegerType + ')\s+[A-Za-z_]\w*[^;]*;[^;]*\.size\s*\(\s*\)|\b(?!int64_t\b)(?:' + $script:IntegerType + ')\s+[A-Za-z_]\w*\s*[={][^;]*\bstd::ssize\s*\(|\bfor\s*\(\s*(?!int64_t\b)(?:' + $script:IntegerType + ')\s+[A-Za-z_]\w*[^;]*;[^;]*\bstd::ssize\s*\(' }
	@{ Kind = 'style-rule-20'; Pattern = '^\s*(?:(?:static|inline|constexpr|const|thread_local|mutable)\s+)*(?:' + $script:ScalarType + ')\s+[A-Za-z_]\w*\s*(?:=\s*)?\{|^\s*(?:(?:static|inline|constexpr|const|thread_local|mutable)\s+)*[A-Za-z_][\w:]*(?:<[^;]*>)?\s*\*+\s*(?:const\s+)?[A-Za-z_]\w*\s*(?:=\s*)?\{|(?<![\w:]|(?:->|:)\s*)(?:std::)?(?:' + $script:ScalarType + ')\s*\{' }
	@{ Kind = 'style-rule-23'; Pattern = $script:CodePrefix + '\btypedef\b'; Except = '^\s*\*(?:\s|/|$)' }
	@{ Kind = 'style-rule-25'; Pattern = $script:CodePrefix + '\bconstexpr\b'; Except = '\b(?:static|inline)\s+(?:(?:static|inline|const|thread_local)\s+)*constexpr\b|\bconstexpr\s+(?:static|inline)\b|\bif\s+constexpr\b|\bconstexpr\s+[^=;{]*[\w)*&>]\s*\(' }
	@{ Kind = 'style-rule-26'; Pattern = '\busing\s+enum\b' }
	@{ Kind = 'style-rule-30'; Pattern = '>\s+>' }
	@{ Kind = 'style-rule-33'; Pattern = '\b(?:CHAR_BIT|MB_LEN_MAX|S?CHAR_MIN|S?CHAR_MAX|UCHAR_MAX|SHRT_MIN|SHRT_MAX|USHRT_MAX|INT_MIN|INT_MAX|UINT_MAX|LONG_MIN|LONG_MAX|ULONG_MAX|LLONG_MIN|LLONG_MAX|ULLONG_MAX|_I(?:8|16|32|64)_(?:MIN|MAX)|_UI(?:8|16|32|64)_MAX)\b' }
	@{ Kind = 'style-rule-34'; Pattern = $script:CodePrefix + '(?<![\w.''])\d{4,}(?![\d''])'; Except = '^\s*#\s*(?:pragma|line)\b|^\s*\*(?:\s|/|$)' }
	@{ Kind = 'style-rule-35'; Pattern = $script:CodePrefix + '\b(?:(?:CreateDirectory|RemoveDirectory|DeleteFile|GetFileAttributes(?:Ex)?|SetFileAttributes|PathFileExists|FindFirstFile(?:Ex)?|FindNextFile)[AW]?|FindClose|_w?mkdir|_w?rmdir|_w?unlink|_w?access(?:_s)?|_w?stat(?:32|64|i64)?|_w?findfirst(?:32|64)?|_w?findnext(?:32|64)?|_w?rename|_wremove|mkdir|rmdir|unlink|opendir|readdir)\s*\('; Except = '^\s*\*(?:\s|/|$)' }
	@{ Kind = 'style-rule-36'; Pattern = '^\s*(?:(?:static|inline|thread_local|volatile|mutable)\s+)*(?:const\s+)?(?:(?:' + $script:ScalarType + ')(?:\s*\*+\s*|\s+)|(?!(?:return|delete|co_return|throw|goto|case|else|do)\b)[A-Za-z_][\w:]*(?:<[^;]*>)?\s*\*+\s*)(?:const\s+)?[A-Za-z_]\w*\s*;|^\s*extern\s+(?!"|template\b)[^;(]*;' }
	@{ Kind = 'style-rule-37'; Pattern = '\bstd::get\s*<\s*\d+\s*>\s*\(|\bstd::tie\s*\(|&\s*[A-Za-z_]\w*\s*=\s*[^;]*\.(?:first|second)\s*;' }
	@{ Kind = 'style-rule-40'; Pattern = '[(,]\s*(?:\[\[[^\]]*\]\]\s*)?(?:const\s+(?:(?:char|wchar_t)\s*\*|std::w?string\s*&)|(?:char|wchar_t)\s+const\s*\*|std::w?string\s+const\s*&)'; Except = '\bfor\s*\(\s*(?:const\s+(?:(?:char|wchar_t)\s*\*|std::w?string\s*&)|(?:char|wchar_t)\s+const\s*\*|std::w?string\s+const\s*&)' }
	@{ Kind = 'style-rule-44'; Pattern = '\bXM(?:Load|Store)(?:Float(?:2|3|4|3x4|4x3|4x4)|Int(?:2|3|4))\s*\(' }
	@{ Kind = 'style-rule-46'; Pattern = '\bXM\w*Est\s*\(' }
	@{ Kind = 'style-rule-54'; Pattern = '^\s*(?:(?:static|inline|const|mutable)\s+)*Vk[A-Z]\w*\s+[A-Za-z_]\w*\s*(?:\[[^\]]*\]\s*)*(?:[=;{]|$)'; Except = '^\s*(?:(?:static|inline|const|mutable)\s+)*Vk([A-Z]\w*)\s+[A-Za-z_]\w*Vk\1s?\s*(?:\[[^\]]*\]\s*)*(?:[=;{]|$)' }
	@{ Kind = 'style-rule-55'; Pattern = '^\s*enum\b(?!\s+(?:class|struct)\b)' }
	@{ Kind = 'style-rule-66'; Pattern = '^\s*namespace\s*(?:\{.*)?$' }
	@{ Kind = 'style-rule-43'; Pattern = $script:CodePrefix + '(?:\btypeid\s*\(|\bdynamic_cast\s*<)'; Except = '^\s*\*(?:\s|/|$)' }
	# `Num` as its own word in an identifier, so Number and Enumerate stay clear; a name reached through ::, ->
	# or . belongs to another API.
	@{ Kind = 'style-rule-14'; Pattern = '(?<!(?:::|->|\.)\s*)\b(?:\w*[a-z0-9_])?Num(?![a-z])'; Except = $script:CommentOrQuote }
	@{ Kind = 'style-rule-72'; Pattern = $script:CodePrefix + '(?:\.|->)k[a-z0-9]*[A-Z]'; Except = '^\s*\*(?:\s|/|$)' }
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
	$json = $result | ConvertTo-Json -Depth 32 -Compress
	$text = $json
	if (-not [string]::IsNullOrWhiteSpace($OutputPath)) {
		[IO.File]::WriteAllText($OutputPath, $json, $script:Utf8)
		$text = "$Status $Code $($result.message) -> $OutputPath`n"
	}
	$stream = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes($text)
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
	if ($PathPrefix) { $arguments += @('-PathPrefix', ($PathPrefix -join ',')) }
	$shell = [Environment]::ProcessPath
	if ([string]::IsNullOrEmpty($shell)) { $shell = 'pwsh' }
	# The file result is never truncated, so every added line is scanned.
	$inventoryFile = [IO.Path]::GetTempFileName()
	try {
		$run = Invoke-AgentProcess $shell ($arguments + @('-OutputPath', $inventoryFile)) $script:Root
		$text = if (Test-Path -LiteralPath $inventoryFile -PathType Leaf) { [IO.File]::ReadAllText($inventoryFile, $script:Utf8) } else { '' }
	}
	finally {
		Remove-Item -LiteralPath $inventoryFile -ErrorAction SilentlyContinue
	}
	$document = $null
	if (-not [string]::IsNullOrWhiteSpace($text)) { $document = $text | ConvertFrom-Json }
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

function Get-IfCondition([string] $Path, [int] $Line) {
	# Walks an `if` condition from the first `(` of head-side line $Line over later head-side lines until its
	# parentheses close, outside comments and literals. Returns $null when that line has no `(` or the file
	# ends first; otherwise the closing line's number, its code after the closing `)` with literals masked, and
	# whether the condition has a `||` at depth one.
	$lines = Get-NewSideLine $Path
	$depth = 0
	$opened = $false
	$hasOr = $false
	$number = $Line
	while ($true) {
		if ($number -gt $lines.Count) { return $null }
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
		if (-not $opened) { return $null }
		if ($depth -eq 0) { return [pscustomobject] @{ Line = $number; Rest = $code.Substring($index + 1); HasOr = $hasOr } }
		$number++
	}
}

function Test-Rule61Line([string] $Path, [int] $Line, [string] $Text) {
	# An `if` (an `else if` counts as its `if`) or `else` line breaks rule 61 when a statement follows the
	# condition's closing `)`, which may be on a later head-side line, or follows the `else`, or when the next
	# non-blank head-side line after that is not `{`. A trailing `{` is a brace, and a leading
	# `[[likely]]`/`[[unlikely]]` is an attribute, not a statement.
	if ($Text -cnotmatch '^\s*(?:else\s+)?if\b|^\s*else\b') { return $false }
	$number = $Line
	if ($Text -cmatch '^\s*(?:else\s+)?if\b') {
		$condition = Get-IfCondition $Path $Line
		if ($null -eq $condition) { return $false }
		$rest = $condition.Rest
		$number = $condition.Line
	}
	else {
		$rest = ($Text -replace '//.*$', '') -replace '^\s*else\b', ''
	}
	$rest = $rest.Trim() -creplace '^\[\[(?:likely|unlikely)\]\]\s*', ''
	if ($rest.StartsWith('{')) { return $false }
	if ($rest.Length -gt 0) { return $true }
	$lines = Get-NewSideLine $Path
	for ($number++; $number -le $lines.Count; $number++) {
		$next = $lines[$number - 1].Trim()
		if ($next.Length -eq 0) { continue }
		return -not $next.StartsWith('{')
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
	# An `if` (an `else if` counts as its `if`) line is a rule 62 candidate when its condition, which may
	# continue over later head-side lines until its parentheses close, has a `||` at depth one outside
	# comments and literals, and its braced body is one `return`, `continue` or `break` statement.
	if ($Text -cnotmatch '^\s*(?:else\s+)?if\b') { return $false }
	$condition = Get-IfCondition $Path $Line
	if ($null -eq $condition -or -not $condition.HasOr -or $condition.Rest.Trim().Length -gt 0) { return $false }
	$lines = Get-NewSideLine $Path
	$body = [Collections.Generic.List[string]]::new()
	for ($number = $condition.Line + 1; $number -le $lines.Count -and $body.Count -lt 3; $number++) {
		$next = ($lines[$number - 1] -replace '//.*$', '').Trim()
		if ($next.Length -gt 0) { $body.Add($next) }
	}
	return $body.Count -eq 3 -and $body[0] -ceq '{' -and $body[1] -cmatch '^(?:return\b[^;]*|continue|break)\s*;$' -and $body[2] -ceq '}'
}

function Test-Rule51Line([string] $Path, [int] $Line, [string] $Text) {
	# A line breaks rule 51 when, outside comments and literals, it ends in an assignment operator, so a
	# declaration or assignment wraps, or it leaves a parenthesis open and ends in `,` or `(`, so a call's or
	# declaration's arguments wrap, unless the next non-blank head-side line starts with `{`.
	# That exempts every next-line `{`, including a one-line braced list rule 51 keeps on the call line; the
	# code-style-review hand read of rule 51 catches that case. A line-ending `(` counts only after a word
	# character, `>`, `]`, `)` or an operator-function name, and not after `return`, since a `(` after
	# another operator, `=`, `(` or `,` groups an expression rather than opening an argument list. A line-ending
	# `=` after `=`, `!`, `<` or `>` is a comparison, except the `<<=` and `>>=` assignments.
	if ($Text.Trim() -cmatch '^(?:#|/\*|\*)') { return $false }
	$code = (($Text -replace '"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*''', '""') -replace '//.*$', '').Trim()
	if ($code -cnotmatch '(?:<<|>>|(?<![=!<>]))=$') {
		if ($code -cnotmatch '[,(]$') { return $false }
		# Counted by length rather than a pipeline, whose one-match result has no Count under strict mode.
		if ($code.Length - $code.Replace('(', '').Length -le $code.Length - $code.Replace(')', '').Length) { return $false }
		if ($code.EndsWith('(')) {
			$before = $code.Substring(0, $code.Length - 1).TrimEnd()
			if (($before -cnotmatch '[\w>\])]$' -and $before -cnotmatch '\boperator\s*\S+$') -or $before -cmatch '\breturn$') { return $false }
		}
	}
	$lines = Get-NewSideLine $Path
	for ($number = $Line + 1; $number -le $lines.Count; $number++) {
		$next = $lines[$number - 1].Trim()
		if ($next.Length -eq 0) { continue }
		return -not $next.StartsWith('{')
	}
	return $false
}

function Test-CandidatePattern([string] $Path, [string] $Text) {
	$kinds = [Collections.Generic.List[string]]::new()
	foreach ($pattern in $script:ScannedPatterns) {
		if ($Text -cnotmatch $pattern.Pattern) { continue }
		if ($pattern.ContainsKey('Except') -and $Text -cmatch $pattern.Except) { continue }
		# Rule 25 governs function scope and header global scope, so a column-0 constexpr in a .cpp file is a
		# permitted namespace-scope form.
		if ($pattern.Kind -ceq 'style-rule-25' -and $Path -cmatch '\.cpp$' -and $Text -cmatch '^constexpr\b') { continue }
		# A .cpp extern variable needs its defining file and sharing files to judge, so the hand-read covers it.
		if ($pattern.Kind -ceq 'style-rule-36' -and $Path -cnotmatch '\.h$' -and $Text -cmatch '^\s*extern\b') { continue }
		$kinds.Add($pattern.Kind)
	}
	return , $kinds
}

try {
	$script:Root = Get-AgentCanonicalPath $RepositoryRoot
	if (-not (Test-Path -LiteralPath $script:Root -PathType Container)) {
		Complete-SessionCandidates 2 'blocked' 'candidates.repository-root-invalid' "-RepositoryRoot must be an existing directory: '$RepositoryRoot'."
	}
	if ($PSCmdlet.ParameterSetName -ceq 'WholeFile') {
		# Whole-file mode reports style kinds only.
		$script:ScannedPatterns = @($script:CandidatePatterns | Where-Object { $_.Kind.StartsWith('style-rule-') })
		$scannedLines = Get-FileLine $Path
		$scope = "$(@($Path).Count) named C++ file(s)"
	}
	else {
		if (-not (Test-Path -LiteralPath $script:InventoryScript -PathType Leaf)) {
			Complete-SessionCandidates 2 'blocked' 'candidates.inventory-missing' "The session change inventory script is missing: '$($script:InventoryScript)'."
		}
		$inventory = Get-InventoryDocument
		$script:HeadSha = if ([string]::IsNullOrWhiteSpace($inventory.headSha)) { '' } else { $inventory.headSha }
		$scannedLines = Get-AddedLine $inventory
		$scope = 'session-added C++ lines'
	}

	$hits = [Collections.Generic.List[object]]::new()
	foreach ($line in $scannedLines) {
		$kinds = [Collections.Generic.List[string]]::new()
		if (Test-Rule61Line $line.Path $line.Line $line.Text) { $kinds.Add('style-rule-61') }
		if (Test-Rule22Line $line.Path $line.Line $line.Text) { $kinds.Add('style-rule-22') }
		if (Test-Rule59Line $line.Path $line.Line $line.Text) { $kinds.Add('style-rule-59') }
		$kinds.AddRange((Test-CandidatePattern $line.Path $line.Text))
		if (Test-Rule62Line $line.Path $line.Line $line.Text) { $kinds.Add('style-rule-62') }
		if (Test-Rule51Line $line.Path $line.Line $line.Text) { $kinds.Add('style-rule-51') }
		if ($kinds.Count -eq 0) { continue }
		$text = $line.Text.Trim()
		if ($text.Length -gt $script:MaximumTextLength) { $text = $text.Substring(0, $script:MaximumTextLength) }
		foreach ($kind in $kinds) { $hits.Add([ordered]@{ path = $line.Path; line = $line.Line; kind = $kind; text = $text }) }
	}
	$sorted = [Collections.Generic.List[object]]::new($hits)
	$sorted.Sort([Comparison[object]] {
		param($left, $right)
		$compare = [string]::CompareOrdinal($left.path, $right.path)
		if ($compare -ne 0) { return $compare }
		if ($left.line -ne $right.line) { return $left.line - $right.line }
		return [string]::CompareOrdinal($left.kind, $right.kind)
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
	# A file result has no stdout budget, so it carries every hit.
	$toFile = -not [string]::IsNullOrWhiteSpace($OutputPath)
	$hitLimit = if ($toFile) { $sorted.Count } else { $script:MaximumHits }
	$emitted = [Collections.Generic.List[object]]::new()
	foreach ($hit in ($sorted | Select-Object -First $hitLimit)) { $emitted.Add($hit) }
	while ($true) {
		$result.hits = [object[]] $emitted.ToArray()
		$result.truncated = $emitted.Count -lt $sorted.Count
		if ($toFile -or $script:Utf8.GetByteCount(($result | ConvertTo-Json -Depth 32 -Compress)) -le $script:MaximumOutputBytes) { break }
		if ($emitted.Count -eq 0) { break }
		$drop = [Math]::Max(1, [int] [Math]::Ceiling($emitted.Count * 0.1))
		$emitted.RemoveRange($emitted.Count - $drop, $drop)
	}
	Complete-SessionCandidates 0 'pass' 'ok' "Scanned the $scope and found $($sorted.Count) candidate(s)."
}
catch {
	Complete-SessionCandidates 1 'error' 'internal.error' $_.Exception.Message
}
