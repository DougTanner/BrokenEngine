# Citation check for Plan and Investigation prose: finds every backticked `path:line` or `path:start-end`
# citation of a C++ file, checks deterministically that the file is tracked and present and the range is
# inside it, then asks Jev through Invoke-Jev.ps1 three yes-means-bad questions about the sentence that cites
# them: is something it attributes to the region absent, do the lines do the opposite, does it describe a
# different region. Each question sees the cited lines and their enclosing function or class. A citation's
# `problemProbability` is the highest of the three; at 0.5 or above it is flagged. The result lists citations
# a human should open first, highest `problemProbability` first, and never changes a file. Without a key or a
# reachable service the result is `blocked` and the citations stay unchecked, which is the behaviour the
# workflow has without Jev.
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
Import-Module (Join-Path $PSScriptRoot 'AgentScriptCommon.psm1') -Force

$script:CitationPattern = '`(?<path>[A-Za-z0-9_][A-Za-z0-9_/.-]*\.(?:cpp|h|inl)):(?<ranges>\d+(?:-\d+)?(?:,\s*\d+(?:-\d+)?)*)`'
# A sentence ends at punctuation followed by whitespace; the dot inside `File.cpp` is never followed by
# whitespace, so a citation stays inside its sentence.
$script:SentencePattern = '(?<=[.!?;])\s+'
$script:ShiftDistance = 40
$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:MaximumBlockLines = 120
$script:FlagProbability = 0.5
$script:Setting = 'The claim is a sentence from a planning document that cites one or more regions of C++ source by file and line range; `citation` names the one region under test, `cited` is exactly the cited lines, and `context` is the function or class that encloses them (or the cited lines with a few leading lines when no single short one does). '
# Three yes-means-bad questions, one per way a citation goes wrong; merging them into one choice let "lacks
# something" and "says the opposite" share an option (Documents/Investigations/JevEvidenceCitationCheck.md).
$script:Questions = [ordered]@{
	absent = [ordered]@{
		type = 'noul'
		instructions = $script:Setting + 'Does the claim attribute to the cited region a named symbol, behaviour, or structure that is not in `cited`?'
		criteria = [ordered]@{
			true = 'Something the claim says is in the cited region is missing from the cited lines'
			false = 'Everything the claim attributes to the cited region is present in the cited lines'
		}
	}
	opposite = [ordered]@{
		type = 'noul'
		instructions = $script:Setting + 'Do the lines in `cited` do the opposite of what the claim says about the cited region?'
		criteria = [ordered]@{
			true = 'The cited lines cover what the claim describes but behave the opposite way, such as accepting a value the claim says is rejected'
			false = 'The cited lines do not contradict what the claim says about them'
		}
	}
	elsewhere = [ordered]@{
		type = 'noul'
		instructions = $script:Setting + 'Does the claim describe a different region than the cited one, such as a neighbouring function in `context` or another part of the file?'
		criteria = [ordered]@{
			true = 'The claim is about other code than the cited lines'
			false = 'The claim is about the cited lines'
		}
	}
}

$result = [ordered]@{
	schemaVersion = 'broken-engine-citation-support/v2'
	status = 'error'
	code = 'internal.error'
	message = 'Citation check did not run.'
	counts = [ordered]@{ citations = 0; missingFile = 0; outsideFile = 0; tooLong = 0; asked = 0; passed = 0; flagged = 0 }
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
		"$Status $Code $Message (citations $($c.citations), asked $($c.asked), passed $($c.passed), flagged $($c.flagged)) -> $OutputPath"
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
function Get-Context([string[]] $Lines, [int] $Start, [int] $End) {
	# Allman shape, no brace parser: the opener is the column-0 `{` found walking up from the range start
	# without crossing a column-0 closer, else looking down through non-blank lines; the block runs from the
	# non-blank line before it through the first column-0 `}` or `};` after it.
	$opener = 0
	for ($number = $Start; $number -ge 1; $number--) {
		if ($number -lt $Start -and $Lines[$number - 1] -cmatch '^\};?\s*$') { break }
		if (Test-AgentBlockOpener $Lines $number) { $opener = $number; break }
	}
	if ($opener -eq 0) {
		for ($number = $Start + 1; $number -le $Lines.Count; $number++) {
			if ([string]::IsNullOrWhiteSpace($Lines[$number - 1]) -or $Lines[$number - 1] -cmatch '^\};?\s*$') { break }
			if (Test-AgentBlockOpener $Lines $number) { $opener = $number; break }
		}
	}
	if ($opener -gt 0) {
		$declaration = $opener
		for ($number = $opener - 1; $number -ge 1; $number--) {
			if (-not [string]::IsNullOrWhiteSpace($Lines[$number - 1])) { $declaration = $number; break }
		}
		$closer = 0
		for ($number = $opener + 1; $number -le $Lines.Count; $number++) {
			if ($Lines[$number - 1] -cmatch '^\};?\s*$') { $closer = $number; break }
		}
		if ($closer -gt 0 -and $declaration -le $Start -and $End -le $closer -and $closer - $declaration + 1 -le $script:MaximumBlockLines) {
			return (($Lines[($declaration - 1)..($closer - 1)]) -join "`n")
		}
	}
	return Get-Window $Lines $Start $End
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
	$requests.Add([ordered]@{ state = [ordered]@{ claim = $candidate.claim; citation = $candidate.citation; file = $resolved; lines = "$($candidate.start)-$($candidate.end)"; cited = (($fileLines[($candidate.start - 1)..($candidate.end - 1)]) -join "`n"); context = (Get-Context $fileLines $candidate.start $candidate.end) }; questions = $script:Questions })
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
		$requests.Add([ordered]@{ state = [ordered]@{ claim = $candidate.claim; citation = $candidate.citation; file = $resolved; lines = "$shiftedStart-$shiftedEnd"; cited = (($fileLines[($shiftedStart - 1)..($shiftedEnd - 1)]) -join "`n"); context = (Get-Context $fileLines $shiftedStart $shiftedEnd) }; questions = $script:Questions })
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

$controlCounts = [ordered]@{ asked = 0; passed = 0; flagged = 0 }
for ($i = 0; $i -lt $asked.Count; $i++) {
	$candidate = $asked[$i]
	$response = $jev.responses[$i]
	if ($null -ne $response.error) { $result.skipped += [ordered]@{ document = $candidate.document; documentLine = $candidate.documentLine; citation = $candidate.citation; reason = "request-failed: $($response.error)" }; continue }
	$row = [ordered]@{
		document = $candidate.document
		documentLine = $candidate.documentLine
		citation = $candidate.citation
		file = $candidate.resolvedPath
		lines = "$($candidate.start)-$($candidate.end)"
	}
	foreach ($key in $script:Questions.Keys) { $row[$key] = [Math]::Round([double]$response.answers.$key.noul, 3) }
	$row.problemProbability = [Math]::Max([Math]::Max($row.absent, $row.opposite), $row.elsewhere)
	$row.claim = $candidate.claim
	$counts = if ($candidate.control) { $controlCounts } else { $result.counts }
	$counts.asked++
	if ($candidate.control) { $row.control = $true }
	if ($row.problemProbability -ge $script:FlagProbability) { $counts.flagged++; $result.flagged += $row } else { $counts.passed++; $result.passed += $row }
}
# Reading order: flagged rows first, and within each list the most likely problem first; -Stable keeps ties
# in citation order. The rows are dictionaries, so the sort key is a script block; a bare property name would
# not sort them.
$result.flagged = @($result.flagged | Sort-Object -Property { $_['problemProbability'] } -Descending -Stable)
$result.passed = @($result.passed | Sort-Object -Property { $_['problemProbability'] } -Descending -Stable)
if ($IncludeShiftedControls) { $result.controls = $controlCounts }
$flaggedCitations = @($result.flagged | Where-Object { -not $_.Contains('control') }).Count
$failedRequests = @($result.skipped | Where-Object { $_['reason'].StartsWith('request-failed') }).Count
if ($jevExit -ne 0) { Complete-CitationSupport 0 'ok' 'citations.partial' "$($result.counts.asked) citations checked; $failedRequests requests failed and are listed under skipped; $flaggedCitations need a human read." }
Complete-CitationSupport 0 'ok' 'citations.checked' "$($result.counts.asked) citations checked; $flaggedCitations need a human read."
