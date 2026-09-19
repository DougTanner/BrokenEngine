# Added-lines-only candidate scanner for /code-style-review: it reports candidate temporary
# instrumentation and style-rule candidates on lines this session added, so a review never has to
# separate them from pre-existing code by hand. The scan reports candidates only —
# it never decides whether a hit is temporary or a row is a violation, never edits a file, and writes
# nothing to disk (GIT_OPTIONAL_LOCKS=0 keeps Git from refreshing the index), so it is safe under a
# read-only sandbox. Stdout carries only the result document.
[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Baseline,
	[string] $Head,
	[switch] $IncludeUntracked
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
# it. An entry's Except clears a match that is one of the rule's permitted forms. The style-rule-61 kind
# is not in this table: it needs the next line too, so Test-Rule61Line decides it before the table. That
# worker's step 7 hand-read list is the complement of the style-rule-<n> kinds, so update it with them.
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
)
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

function Test-CandidatePattern([string] $Text) {
	foreach ($pattern in $script:CandidatePatterns) {
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
	if (-not (Test-Path -LiteralPath $script:InventoryScript -PathType Leaf)) {
		Complete-SessionCandidates 2 'blocked' 'candidates.inventory-missing' "The session change inventory script is missing: '$($script:InventoryScript)'."
	}
	$inventory = Get-InventoryDocument
	$script:HeadSha = if ([string]::IsNullOrWhiteSpace($inventory.headSha)) { '' } else { $inventory.headSha }
	# A passing inventory always carries truncation.regions, so both counts are read directly.
	$emittedRegionCount = @($inventory.regions).Count
	$regionsCapped = $emittedRegionCount -ge $script:InventoryRegionCap -or [int] $inventory.truncation.regions.full -gt $emittedRegionCount

	$hits = [Collections.Generic.List[object]]::new()
	foreach ($line in (Get-AddedLine $inventory)) {
		$kind = if (Test-Rule61Line $line.Path $line.Line $line.Text) { 'style-rule-61' } else { Test-CandidatePattern $line.Text }
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
	Complete-SessionCandidates 0 'pass' 'ok' "Scanned the session-added C++ lines and found $($sorted.Count) candidate(s)."
}
catch {
	Complete-SessionCandidates 1 'error' 'internal.error' $_.Exception.Message
}
