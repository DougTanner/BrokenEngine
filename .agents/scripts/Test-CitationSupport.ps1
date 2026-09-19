# Citation check for Plan and Investigation prose: finds every backticked `path:line` or `path:start-end`
# citation of a C++ file, checks deterministically that the file is tracked and present and the range is
# inside it, then asks Jev through Invoke-Jev.ps1 whether the cited lines support the sentence that cites
# them. The result lists citations a human should open first — `says_nothing` and `contradicts` answers,
# then low-probability `supports` — and never changes a file. Without a key or a reachable service the
# result is `blocked` and the citations stay unchecked, which is the behaviour the workflow has without Jev.
[CmdletBinding()]
param(
	[string[]] $Path = @('Documents/Plans'),
	[string] $OutputPath,
	[int] $ContextLines = 2,
	[int] $MaximumRangeLines = 60,
	[int] $MaximumClaimLength = 800,
	# Adds one synthetic negative per citation: a window of the same length in the same file at least 40
	# lines away. Used to measure how well the answers separate real citations from shifted ones.
	[switch] $IncludeShiftedControls
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:CitationPattern = '`(?<path>[A-Za-z0-9_][A-Za-z0-9_/.-]*\.(?:cpp|h|inl)):(?<ranges>\d+(?:-\d+)?(?:,\s*\d+(?:-\d+)?)*)`'
# A sentence ends at punctuation followed by whitespace; the dot inside `File.cpp` is never followed by
# whitespace, so a citation stays inside its sentence.
$script:SentencePattern = '(?<=[.!?;])\s+'
$script:ShiftDistance = 40
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:Question = [ordered]@{
	relation = [ordered]@{
		type = 'choice'
		instructions = 'The claim is a sentence from a planning document that cites one or more regions of C++ source by file and line range; `citation` names the one region under test, and `code` is that region with a few lines of leading context. How does the code relate to what the claim says about the cited region?'
		criteria = [ordered]@{
			supports = 'The code shown is what the claim describes: the named symbols, behaviour, or structure the claim attributes to this region are present in it'
			contradicts = 'The code covers the same behaviour or symbols but does the opposite of what is claimed, or lacks something the claim says is there'
			says_nothing = 'The code is unrelated to the claim, or is a different region than the one the claim describes'
		}
	}
}

$result = [ordered]@{
	schemaVersion = 'broken-engine-citation-support/v1'
	status = 'error'
	code = 'internal.error'
	message = 'Citation check did not run.'
	counts = [ordered]@{ citations = 0; missingFile = 0; outsideFile = 0; tooLong = 0; asked = 0; supports = 0; contradicts = 0; saysNothing = 0 }
	controls = $null
	inputTokens = 0
	flagged = @()
	passed = @()
	skipped = @()
}

function Complete-CitationSupport([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = $Message
	$json = $result | ConvertTo-Json -Depth 16 -Compress
	if ([string]::IsNullOrWhiteSpace($OutputPath)) {
		$stream = [Console]::OpenStandardOutput()
		$bytes = $script:Utf8.GetBytes($json)
		$stream.Write($bytes, 0, $bytes.Length)
		$stream.Flush()
	}
	else {
		[IO.File]::WriteAllText($OutputPath, $json, $script:Utf8)
		$c = $result.counts
		"$Status $Code $Message (citations $($c.citations), asked $($c.asked), supports $($c.supports), contradicts $($c.contradicts), says_nothing $($c.saysNothing)) -> $OutputPath"
	}
	exit $ExitCode
}

$topLevel = @(& git rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or $topLevel.Count -eq 0) { Complete-CitationSupport 1 'error' 'git.no-repository' 'The current directory is not inside a Git repository.' }
$RepositoryRoot = [IO.Path]::GetFullPath(([string]$topLevel[0]).Trim())

# A citation may name a file by a bare name or a partial path; resolve through the tracked file list so a
# unique suffix match still counts as the deterministic pre-check passing.
$trackedFiles = @(& git -C $RepositoryRoot ls-files -- '*.cpp' '*.h' '*.inl' 2>$null)
$fileTextCache = @{}
function Get-FileLines([string] $RelativePath) {
	if (-not $fileTextCache.ContainsKey($RelativePath)) {
		$bytes = [IO.File]::ReadAllBytes((Join-Path $RepositoryRoot $RelativePath))
		$offset = if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { 3 } else { 0 }
		$text = [Text.UTF8Encoding]::new($false).GetString($bytes, $offset, $bytes.Length - $offset)
		$fileTextCache[$RelativePath] = $text.Replace("`r`n", "`n").Split("`n")
	}
	return $fileTextCache[$RelativePath]
}
function Resolve-CitedPath([string] $Cited) {
	$normalized = $Cited -replace '\\', '/'
	$resolved = $null
	if ($trackedFiles -ccontains $normalized) { $resolved = $normalized }
	else {
		$matches = @($trackedFiles | Where-Object { $_.EndsWith("/$normalized") })
		if ($matches.Count -eq 1) { $resolved = $matches[0] }
	}
	# A tracked file can be absent from a sparse worktree; that is a skip, not a crash.
	if ($null -ne $resolved -and -not (Test-Path -LiteralPath (Join-Path $RepositoryRoot $resolved) -PathType Leaf)) { $resolved = $null }
	return $resolved
}
function Get-Window([string[]] $Lines, [int] $Start, [int] $End) {
	$first = [Math]::Max(1, $Start - $ContextLines)
	return (($Lines[($first - 1)..($End - 1)]) -join "`n")
}

$markdownFiles = @(foreach ($entry in $Path) {
	$full = if ([IO.Path]::IsPathRooted($entry)) { $entry } else { Join-Path $RepositoryRoot $entry }
	if (Test-Path -LiteralPath $full -PathType Container) { Get-ChildItem -LiteralPath $full -Recurse -Filter '*.md' -File | Where-Object { $_.Name -ne 'AGENTS.md' } | ForEach-Object { $_.FullName } }
	elseif (Test-Path -LiteralPath $full -PathType Leaf) { $full }
})
if ($markdownFiles.Count -eq 0) { Complete-CitationSupport 1 'error' 'path.empty' 'No markdown files found under the given paths.' }

$candidates = [Collections.Generic.List[object]]::new()
foreach ($markdownFile in $markdownFiles) {
	$relativeMarkdown = [IO.Path]::GetRelativePath($RepositoryRoot, $markdownFile) -replace '\\', '/'
	$lines = (Get-Content -LiteralPath $markdownFile -Raw).Replace("`r`n", "`n").Split("`n")
	for ($lineIndex = 0; $lineIndex -lt $lines.Count; $lineIndex++) {
		$matchSet = [regex]::Matches($lines[$lineIndex], $script:CitationPattern)
		if ($matchSet.Count -eq 0) { continue }
		# The claim is the sentence holding the citation, found inside the paragraph (the run of non-blank
		# lines) so a sentence wrapped across lines is whole.
		$top = $lineIndex; while ($top -gt 0 -and -not [string]::IsNullOrWhiteSpace($lines[$top - 1])) { $top-- }
		$bottom = $lineIndex; while ($bottom -lt $lines.Count - 1 -and -not [string]::IsNullOrWhiteSpace($lines[$bottom + 1])) { $bottom++ }
		$paragraph = (($lines[$top..$bottom] -join ' ') -replace '\s+', ' ').Trim()
		$sentences = @($paragraph -split $script:SentencePattern)
		foreach ($match in $matchSet) {
			$claim = @($sentences | Where-Object { $_.Contains($match.Value) })[0]
			if ($null -eq $claim) { $claim = $paragraph }
			if ($claim.Length -gt $MaximumClaimLength) { $claim = $claim.Substring(0, $MaximumClaimLength) }
			foreach ($range in ($match.Groups['ranges'].Value -split ',\s*')) {
				$bounds = $range -split '-'
				$start = [int]$bounds[0]
				$end = if ($bounds.Count -gt 1) { [int]$bounds[1] } else { $start }
				$candidates.Add([ordered]@{
					document = $relativeMarkdown
					documentLine = $lineIndex + 1
					citation = "$($match.Groups['path'].Value):$range"
					citedPath = $match.Groups['path'].Value
					start = $start
					end = $end
					claim = $claim
				})
			}
		}
	}
}
$result.counts.citations = $candidates.Count

$requests = [Collections.Generic.List[object]]::new()
$asked = [Collections.Generic.List[object]]::new()
foreach ($candidate in $candidates) {
	$resolved = Resolve-CitedPath $candidate.citedPath
	if ($null -eq $resolved) { $result.counts.missingFile++; $result.skipped += [ordered]@{ document = $candidate.document; documentLine = $candidate.documentLine; citation = $candidate.citation; reason = 'missing-file' }; continue }
	$fileLines = Get-FileLines $resolved
	if ($candidate.end -lt $candidate.start -or $candidate.end -gt $fileLines.Count) { $result.counts.outsideFile++; $result.skipped += [ordered]@{ document = $candidate.document; documentLine = $candidate.documentLine; citation = $candidate.citation; reason = 'outside-file' }; continue }
	$length = $candidate.end - $candidate.start + 1
	if ($length -gt $MaximumRangeLines) { $result.counts.tooLong++; $result.skipped += [ordered]@{ document = $candidate.document; documentLine = $candidate.documentLine; citation = $candidate.citation; reason = 'too-long' }; continue }
	$candidate.resolvedPath = $resolved
	$candidate.control = $false
	$requests.Add([ordered]@{ state = [ordered]@{ claim = $candidate.claim; citation = $candidate.citation; file = $resolved; lines = "$($candidate.start)-$($candidate.end)"; code = (Get-Window $fileLines $candidate.start $candidate.end) }; questions = $script:Question })
	$asked.Add($candidate)
	if ($IncludeShiftedControls) {
		# Shift forward when the file has room, otherwise backward; skip files too short for either.
		$shiftedStart = $candidate.start + $length + $script:ShiftDistance
		if ($shiftedStart + $length - 1 -gt $fileLines.Count) { $shiftedStart = $candidate.start - $length - $script:ShiftDistance }
		if ($shiftedStart -lt 1) { continue }
		$shiftedEnd = $shiftedStart + $length - 1
		$control = [ordered]@{} + $candidate
		$control.control = $true
		$control.start = $shiftedStart
		$control.end = $shiftedEnd
		$requests.Add([ordered]@{ state = [ordered]@{ claim = $candidate.claim; citation = $candidate.citation; file = $resolved; lines = "$shiftedStart-$shiftedEnd"; code = (Get-Window $fileLines $shiftedStart $shiftedEnd) }; questions = $script:Question })
		$asked.Add($control)
	}
}
if ($asked.Count -eq 0) { Complete-CitationSupport 0 'ok' 'citations.none' 'No citation passed the deterministic pre-check, so Jev was not asked.' }

$requestFile = [IO.Path]::GetTempFileName()
$responseFile = [IO.Path]::GetTempFileName()
try {
	[IO.File]::WriteAllText($requestFile, ($requests | ConvertTo-Json -Depth 16 -Compress), $script:Utf8)
	& (Join-Path $PSScriptRoot 'Invoke-Jev.ps1') -RequestPath $requestFile -OutputPath $responseFile | Out-Null
	$jevExit = $LASTEXITCODE
	$jev = Get-Content -LiteralPath $responseFile -Raw | ConvertFrom-Json -Depth 64
}
finally {
	Remove-Item -LiteralPath $requestFile, $responseFile -ErrorAction SilentlyContinue
}
if ($jev.status -eq 'blocked') { Complete-CitationSupport 2 'blocked' $jev.code "Jev did not run, so no citation was checked: $($jev.message)" }
$result.inputTokens = [int64]$jev.inputTokens

$controlCounts = [ordered]@{ asked = 0; supports = 0; contradicts = 0; saysNothing = 0 }
for ($i = 0; $i -lt $asked.Count; $i++) {
	$candidate = $asked[$i]
	$response = $jev.responses[$i]
	if ($null -ne $response.error) { $result.skipped += [ordered]@{ document = $candidate.document; documentLine = $candidate.documentLine; citation = $candidate.citation; reason = "request-failed: $($response.error)" }; continue }
	$answer = $response.answers.relation
	$row = [ordered]@{
		document = $candidate.document
		documentLine = $candidate.documentLine
		citation = $candidate.citation
		file = $candidate.resolvedPath
		lines = "$($candidate.start)-$($candidate.end)"
		relation = $answer.choice
		supportsProbability = [Math]::Round([double]$answer.probabilities.supports, 3)
		confidence = [Math]::Round([double]$answer.confidence, 3)
		claim = $candidate.claim
	}
	$counts = if ($candidate.control) { $controlCounts } else { $result.counts }
	$counts.asked++
	switch ($answer.choice) {
		'supports' { $counts.supports++ }
		'contradicts' { $counts.contradicts++ }
		'says_nothing' { $counts.saysNothing++ }
	}
	if ($candidate.control) { $row.control = $true }
	if ($answer.choice -eq 'supports') { $result.passed += $row } else { $result.flagged += $row }
}
# Reading order: flagged rows first, and within each list the least-supported citation first.
# The rows are dictionaries, so the sort key is a script block; a bare property name would not sort them.
$result.flagged = @($result.flagged | Sort-Object -Property { $_['supportsProbability'] })
$result.passed = @($result.passed | Sort-Object -Property { $_['supportsProbability'] })
if ($IncludeShiftedControls) { $result.controls = $controlCounts }
if ($jevExit -ne 0) { Complete-CitationSupport 1 'error' 'jev.partial' "Some requests failed; their citations are listed under skipped." }
$flaggedCitations = @($result.flagged | Where-Object { -not $_.Contains('control') }).Count
Complete-CitationSupport 0 'ok' 'citations.checked' "$($result.counts.asked) citations checked; $flaggedCitations need a human read."
