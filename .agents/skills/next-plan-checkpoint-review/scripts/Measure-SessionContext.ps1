[CmdletBinding()]
param(
	[string] $SessionId,
	[string] $TranscriptPath,
	[ValidateRange(1, 25)][int] $Top = 10,
	[ValidateRange(1, [int]::MaxValue)][int] $ThresholdChars = 20000
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# The envelope stays small even when a session breaches the threshold constantly; past this many breach
# rows the envelope reports the truncation instead of growing.
$script:BreachRowCap = 25

$result = [ordered]@{ schemaVersion = 'broken-engine-context-efficiency-error/v1'; status = 'error'; code = 'internal.error'; message = 'Measurement did not run.' }

# Every failure message is redacted the same way the envelope body is, because an exception message can
# carry the transcript path and the envelope must never carry one.
function Protect-TranscriptPath([string] $Text) {
	if ([string]::IsNullOrEmpty($Text)) { return '' }
	return [regex]::Replace($Text, '\.claude[\\/]projects[\\/]\S*', '<transcript-path>')
}

function Complete-Measurement([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	$result.message = Protect-TranscriptPath $Message
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 20 -Compress))
	exit $ExitCode
}

# Strict mode makes a missing property an error, and transcript records omit fields freely.
function Get-Field($Object, [string] $Name) {
	if ($null -eq $Object) { return $null }
	# Reading the collection's Name member throws under strict mode when the object has no properties, and the
	# Properties[$Name] indexer is case-insensitive, so match by iterating the collection instead, which stays
	# case-sensitive and works when it is empty.
	foreach ($property in $Object.PSObject.Properties) {
		# The unary comma keeps an array-valued field from unrolling into the caller's pipeline.
		if ($property.Name -ceq $Name) { return , $Object.$Name }
	}
	return $null
}

function Get-ToolUseIdTag([string] $Text) {
	$match = [regex]::Match($Text, '<tool-use-id>([^<]*)</tool-use-id>')
	if ($match.Success) { return $match.Groups[1].Value }
	return ''
}

function Measure-ResultChars($Content) {
	if ($Content -is [string]) { return $Content.Length }
	$total = 0
	if ($Content -is [object[]]) {
		foreach ($element in $Content) {
			if ([string](Get-Field $element 'type') -cne 'text') { continue }
			$text = Get-Field $element 'text'
			if ($text -is [string]) { $total += $text.Length }
		}
	}
	return $total
}

# The summary is the selector a reviewer needs to name the emitting invocation, never result content.
$script:SummaryFields = @('command', 'file_path', 'path', 'pattern', 'url', 'query', 'prompt', 'description')
function Get-InputSummary($ToolInput) {
	$text = ''
	foreach ($field in $script:SummaryFields) {
		$value = Get-Field $ToolInput $field
		if ($value -is [string] -and -not [string]::IsNullOrWhiteSpace($value)) { $text = $value; break }
	}
	if ($text -eq '' -and $null -ne $ToolInput) {
		foreach ($property in $ToolInput.PSObject.Properties) {
			if ($property.Value -is [string] -and -not [string]::IsNullOrWhiteSpace($property.Value)) { $text = $property.Value; break }
		}
	}
	$text = Protect-TranscriptPath (($text -replace '\s+', ' ').Trim())
	if ($text.Length -gt 160) { $text = $text.Substring(0, 157) + '...' }
	return $text
}

try {
	$hasSessionId = $PSBoundParameters.ContainsKey('SessionId')
	$hasTranscriptPath = $PSBoundParameters.ContainsKey('TranscriptPath')
	if ($hasSessionId -and $hasTranscriptPath) { Complete-Measurement 1 'error' 'input.conflicting-source' 'Pass either -SessionId or -TranscriptPath, not both.' }
	if (-not $hasSessionId -and -not $hasTranscriptPath) { Complete-Measurement 1 'error' 'input.missing-source' 'Pass -SessionId or -TranscriptPath.' }

	if ($hasTranscriptPath) {
		$transcript = $TranscriptPath
		$sessionLabel = [IO.Path]::GetFileNameWithoutExtension($transcript)
	}
	else {
		# Claude stores a transcript at ~/.claude/projects/<cwd with `:`, `\`, `/`, and `.` replaced by `-`>/<session id>.jsonl,
		# so the script must run from the session worktree root the transcript belongs to.
		$projectDirectory = ((Get-Location).Path -replace '[:\\/.]', '-')
		$transcript = Join-Path (Join-Path (Join-Path ([Environment]::GetFolderPath('UserProfile')) '.claude/projects') $projectDirectory) "$SessionId.jsonl"
		$sessionLabel = $SessionId
	}
	if (-not (Test-Path -LiteralPath $transcript -PathType Leaf)) { Complete-Measurement 2 'blocked' 'transcript.not-found' 'The session transcript file does not exist.' }

	$toolUses = @{}
	$measured = [Collections.Generic.List[object]]::new()
	$lineCount = 0
	$lineNumber = 0
	foreach ($line in [IO.File]::ReadLines($transcript)) {
		$lineNumber++
		if ([string]::IsNullOrWhiteSpace($line)) { continue }
		$lineCount++
		try { $record = $line | ConvertFrom-Json -Depth 100 }
		catch { Complete-Measurement 1 'error' 'transcript.malformed-line' "Transcript line $lineNumber is not valid JSON." }
		# Sidechain lines are a subagent's own context and never entered the main session.
		if ((Get-Field $record 'isSidechain') -eq $true) { continue }
		$recordType = [string](Get-Field $record 'type')
		# A background subagent's handoff reaches main in one of two shapes, neither of which is a tool_result
		# even though both entered main's context the same way: a `queued_command` attachment when the handoff
		# was still queued as the turn ended, and otherwise a system-authored `user` record whose string content
		# is a task-notification block (branch below). Both name the dispatching tool_use id in a
		# `<tool-use-id>` tag, so keying each by that id ranks it with a resolved tool name instead of
		# `unknown`. A queued command without the tag is human-typed, and `queue-operation` records repeat a
		# handoff already counted here; both stay uncounted.
		if ($recordType -ceq 'attachment') {
			$attachment = Get-Field $record 'attachment'
			$prompt = Get-Field $attachment 'prompt'
			if ([string](Get-Field $attachment 'type') -ceq 'queued_command' -and $prompt -is [string]) {
				$queuedToolUseId = Get-ToolUseIdTag $prompt
				if (-not [string]::IsNullOrEmpty($queuedToolUseId)) {
					$measured.Add([pscustomobject]@{ ToolUseId = $queuedToolUseId; Chars = $prompt.Length })
				}
			}
			continue
		}
		if ($recordType -cne 'assistant' -and $recordType -cne 'user') { continue }
		$content = Get-Field (Get-Field $record 'message') 'content'
		if ($content -is [string] -and [string](Get-Field (Get-Field $record 'origin') 'kind') -ceq 'task-notification') {
			$measured.Add([pscustomobject]@{ ToolUseId = (Get-ToolUseIdTag $content); Chars = (Measure-ResultChars $content) })
			continue
		}
		if ($content -isnot [object[]]) { continue }
		foreach ($element in $content) {
			$elementType = [string](Get-Field $element 'type')
			if ($recordType -ceq 'assistant' -and $elementType -ceq 'tool_use') {
				$toolUseId = [string](Get-Field $element 'id')
				if (-not [string]::IsNullOrEmpty($toolUseId)) {
					$toolUses[$toolUseId] = [pscustomobject]@{ Name = [string](Get-Field $element 'name'); InputSummary = (Get-InputSummary (Get-Field $element 'input')) }
				}
			}
			elseif ($recordType -ceq 'user' -and $elementType -ceq 'tool_result') {
				$measured.Add([pscustomobject]@{ ToolUseId = [string](Get-Field $element 'tool_use_id'); Chars = (Measure-ResultChars (Get-Field $element 'content')) })
			}
		}
	}

	$totalChars = 0
	foreach ($entry in $measured) { $totalChars += $entry.Chars }
	$ranked = @($measured | Sort-Object -Property @{ Expression = 'Chars'; Descending = $true }, @{ Expression = 'ToolUseId'; Descending = $false })
	$breaches = @($ranked | Where-Object { $_.Chars -ge $ThresholdChars })
	$overThresholdCount = $breaches.Count
	# Every breach must reach the reviewer, so breaches fill the rows first, up to the hard cap; -Top only
	# governs how many below-threshold rows come along as context.
	$reported = @($breaches | Select-Object -First $script:BreachRowCap)
	if ($reported.Count -lt $Top) {
		$reported += @($ranked | Where-Object { $_.Chars -lt $ThresholdChars } | Select-Object -First ($Top - $reported.Count))
	}
	$topResults = [Collections.Generic.List[object]]::new()
	$rank = 0
	foreach ($entry in $reported) {
		$rank++
		$use = if ($toolUses.ContainsKey($entry.ToolUseId)) { $toolUses[$entry.ToolUseId] } else { $null }
		$topResults.Add([ordered]@{
			rank = $rank
			toolName = $(if ($null -ne $use) { $use.Name } else { 'unknown' })
			chars = $entry.Chars
			overThreshold = ($entry.Chars -ge $ThresholdChars)
			toolUseId = $entry.ToolUseId
			inputSummary = $(if ($null -ne $use) { $use.InputSummary } else { '' })
		})
	}

	# totalChars is telemetry only: the verdict turns on per-result breaches, because a total with no
	# dominant emitter names nothing a reviewer could act on.
	$envelope = [ordered]@{
		schemaVersion = 'broken-engine-context-efficiency/v1'
		sessionId = $sessionLabel
		lineCount = $lineCount
		toolResultCount = $measured.Count
		totalChars = $totalChars
		thresholds = [ordered]@{ perResultChars = $ThresholdChars }
		verdict = $(if ($overThresholdCount -gt 0) { 'needs-review' } else { 'pass' })
		overThresholdCount = $overThresholdCount
		breachRowsTruncated = ($overThresholdCount -gt $script:BreachRowCap)
		topResults = @($topResults)
	}
	[Console]::Out.Write(($envelope | ConvertTo-Json -Depth 20 -Compress))
	exit 0
}
catch { Complete-Measurement 1 'error' 'measure.failed' $_.Exception.Message }
