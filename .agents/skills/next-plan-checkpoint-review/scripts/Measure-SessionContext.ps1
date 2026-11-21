[CmdletBinding()]
param(
	[string] $SessionId,
	[string] $TranscriptPath,
	[ValidateRange(1, 25)][int] $Top = 10,
	[ValidateRange(1, [int]::MaxValue)][int] $ThresholdChars = 20000
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentTranscriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentTranscriptCommon.psm1') -Force

# The envelope stays small even when a session breaches the threshold constantly; past this many breach
# rows the envelope reports the truncation instead of growing.
$script:BreachRowCap = 25

$result = [ordered]@{ schemaVersion = 'broken-engine-context-efficiency-error/v1'; status = 'error'; code = 'internal.error'; message = 'Measurement did not run.' }

function Complete-Measurement([int] $ExitCode, [string] $Status, [string] $Code, [string] $Message) {
	$result.status = $Status
	$result.code = $Code
	# Every failure message is redacted the same way the envelope body is, because an exception message can
	# carry the transcript path and the envelope must never carry one.
	$result.message = Protect-AgentTranscriptPath $Message
	[Console]::Out.Write(($result | ConvertTo-Json -Depth 20 -Compress))
	exit $ExitCode
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
			if ([string](Get-AgentTranscriptField $element 'type') -cne 'text') { continue }
			$text = Get-AgentTranscriptField $element 'text'
			if ($text -is [string]) { $total += $text.Length }
		}
	}
	return $total
}

# The summary is the selector a reviewer needs to name the emitting invocation, never result content.
$script:SummaryFields = @('command', 'CommandLine', 'file_path', 'Path', 'path', 'pattern', 'url', 'query', 'prompt', 'Prompt', 'description', 'Role', 'TypeName', 'Recipient', 'TaskId', 'Action')
function Get-InputSummary($ToolInput) {
	$text = ''
	foreach ($field in $script:SummaryFields) {
		$value = Get-AgentTranscriptField $ToolInput $field
		if ($value -is [string] -and -not [string]::IsNullOrWhiteSpace($value)) { $text = $value; break }
	}
	if ($text -eq '' -and $ToolInput -is [Management.Automation.PSCustomObject]) {
		foreach ($property in $ToolInput.PSObject.Properties) {
			if ($property.Value -is [string] -and -not [string]::IsNullOrWhiteSpace($property.Value)) { $text = $property.Value; break }
		}
	}
	$text = Protect-AgentTranscriptPath (($text -replace '\s+', ' ').Trim())
	if ($text.Length -gt 160) { $text = $text.Substring(0, 157) + '...' }
	return $text
}

try {
	$hasSessionId = $PSBoundParameters.ContainsKey('SessionId')
	$hasTranscriptPath = $PSBoundParameters.ContainsKey('TranscriptPath')
	if ($hasSessionId -and $hasTranscriptPath) { Complete-Measurement 1 'error' 'input.conflicting-source' 'Pass either -SessionId or -TranscriptPath, not both.' }
	if ((-not $hasSessionId -and -not $hasTranscriptPath) -or
		($hasSessionId -and [string]::IsNullOrWhiteSpace($SessionId)) -or
		($hasTranscriptPath -and [string]::IsNullOrWhiteSpace($TranscriptPath))) {
		Complete-Measurement 1 'error' 'input.missing-source' 'Pass -SessionId or -TranscriptPath.'
	}

	if ($hasTranscriptPath) {
		$resolved = Resolve-AgentAgyTranscriptPath -TranscriptPath $TranscriptPath
		switch ($resolved.Status) {
			'passthrough' {
				$transcript = $resolved.Path
				$sessionLabel = [IO.Path]::GetFileNameWithoutExtension($transcript)
			}
			'safe' {
				$transcript = $resolved.Path
				$sessionLabel = if (-not [string]::IsNullOrWhiteSpace($resolved.SessionId)) { $resolved.SessionId } else { [IO.Path]::GetFileNameWithoutExtension($transcript) }
			}
			'missing' { Complete-Measurement 2 'blocked' 'transcript.not-found' 'The session transcript file does not exist.' }
			'unsafe' { Complete-Measurement 1 'error' 'transcript.unsafe-path' 'The session transcript path could not be safely read.' }
			default { Complete-Measurement 1 'error' 'transcript.read-failed' 'The session transcript path could not be safely read.' }
		}
	}
	else {
		# Claude stores a transcript at ~/.claude/projects/<cwd with `:`, `\`, `/`, and `.` replaced by `-`>/<session id>.jsonl,
		# so the script must run from the session worktree root the transcript belongs to.
		$projectDirectory = ((Get-Location).Path -replace '[:\\/.]', '-')
		$claudeTranscript = Join-Path (Join-Path (Join-Path ([Environment]::GetFolderPath('UserProfile')) '.claude/projects') $projectDirectory) "$SessionId.jsonl"
		if (Test-Path -LiteralPath $claudeTranscript -PathType Leaf) {
			$transcript = $claudeTranscript
			$sessionLabel = $SessionId
		}
		elseif ($SessionId -cnotmatch '\A[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\z') {
			Complete-Measurement 2 'blocked' 'transcript.not-found' 'The session transcript file does not exist.'
		}
		else {
			$resolved = Resolve-AgentAgyTranscriptPath -SessionId $SessionId
			switch ($resolved.Status) {
				'safe' {
					$transcript = $resolved.Path
					$sessionLabel = $SessionId
				}
				'missing' { Complete-Measurement 2 'blocked' 'transcript.not-found' 'The session transcript file does not exist.' }
				'unsafe' { Complete-Measurement 1 'error' 'transcript.unsafe-path' 'The session transcript path could not be safely read.' }
				default { Complete-Measurement 1 'error' 'transcript.read-failed' 'The session transcript path could not be safely read.' }
			}
		}
	}
	if (-not (Test-Path -LiteralPath $transcript -PathType Leaf)) { Complete-Measurement 2 'blocked' 'transcript.not-found' 'The session transcript file does not exist.' }

	$toolUses = @{}
	$measured = [Collections.Generic.List[object]]::new()
	$isAgy = $false
	try { $isAgy = Test-AgentAgyTranscript $transcript }
	catch {
		if ($_.Exception.Data.Contains('IsMalformedTranscriptLine')) {
			Complete-Measurement 1 'error' 'transcript.malformed-line' $_.Exception.Message
		}
		throw
	}

	if ($isAgy) {
		try {
			foreach ($record in (Read-AgentAgyTranscript $transcript)) {
				if ($record.Type -ceq 'PLANNER_RESPONSE') {
					foreach ($tc in $record.ToolCalls) {
						if (-not [string]::IsNullOrEmpty($tc.ToolUseId)) {
							$toolUses[$tc.ToolUseId] = [pscustomobject]@{ Name = $tc.Name; InputSummary = (Get-InputSummary $tc.Args) }
						}
					}
				}
				elseif ($record.Type -ceq 'GENERIC') {
					if ($null -ne $record.PairedToolCall) {
						$measured.Add([pscustomobject]@{ ToolUseId = $record.PairedToolCall.ToolUseId; Chars = $record.Content.Length })
					}
				}
				elseif ($record.Type -ceq 'SYSTEM_MESSAGE' -and -not [string]::IsNullOrEmpty($record.DeliveryToolUseId)) {
					$measured.Add([pscustomobject]@{ ToolUseId = $record.DeliveryToolUseId; Chars = $record.Content.Length })
				}
			}
		}
		catch {
			if ($_.Exception.Data.Contains('IsMalformedTranscriptLine')) {
				Complete-Measurement 1 'error' 'transcript.malformed-line' $_.Exception.Message
			}
			throw
		}
	}
	else {
		$lineNumber = 0
		foreach ($line in [IO.File]::ReadLines($transcript)) {
			$lineNumber++
			if ([string]::IsNullOrWhiteSpace($line)) { continue }
			try { $record = $line | ConvertFrom-Json -Depth 100 }
			catch { Complete-Measurement 1 'error' 'transcript.malformed-line' "Transcript line $lineNumber is not valid JSON." }
			# Sidechain lines are a subagent's own context and never entered the main session.
			if ((Get-AgentTranscriptField $record 'isSidechain') -eq $true) { continue }
			$recordType = [string](Get-AgentTranscriptField $record 'type')
			# A background subagent's handoff reaches main in one of two shapes, neither of which is a tool_result
			# even though both entered main's context the same way: a `queued_command` attachment when the handoff
			# was still queued as the turn ended, and otherwise a system-authored `user` record whose string content
			# is a task-notification block (branch below). Both name the dispatching tool_use id in a
			# `<tool-use-id>` tag, so keying each by that id ranks it with a resolved tool name instead of
			# `unknown`. A queued command without the tag is human-typed, and `queue-operation` records repeat a
			# handoff already counted here; both stay uncounted.
			if ($recordType -ceq 'attachment') {
				$attachment = Get-AgentTranscriptField $record 'attachment'
				$prompt = Get-AgentTranscriptField $attachment 'prompt'
				if ([string](Get-AgentTranscriptField $attachment 'type') -ceq 'queued_command' -and $prompt -is [string]) {
					$queuedToolUseId = Get-ToolUseIdTag $prompt
					if (-not [string]::IsNullOrEmpty($queuedToolUseId)) {
						$measured.Add([pscustomobject]@{ ToolUseId = $queuedToolUseId; Chars = $prompt.Length })
					}
				}
				continue
			}
			if ($recordType -cne 'assistant' -and $recordType -cne 'user') { continue }
			$content = Get-AgentTranscriptField (Get-AgentTranscriptField $record 'message') 'content'
			if ($content -is [string] -and [string](Get-AgentTranscriptField (Get-AgentTranscriptField $record 'origin') 'kind') -ceq 'task-notification') {
				$measured.Add([pscustomobject]@{ ToolUseId = (Get-ToolUseIdTag $content); Chars = (Measure-ResultChars $content) })
				continue
			}
			if ($content -isnot [object[]]) { continue }
			foreach ($element in $content) {
				$elementType = [string](Get-AgentTranscriptField $element 'type')
				if ($recordType -ceq 'assistant' -and $elementType -ceq 'tool_use') {
					$toolUseId = [string](Get-AgentTranscriptField $element 'id')
					if (-not [string]::IsNullOrEmpty($toolUseId)) {
						$toolUses[$toolUseId] = [pscustomobject]@{ Name = [string](Get-AgentTranscriptField $element 'name'); InputSummary = (Get-InputSummary (Get-AgentTranscriptField $element 'input')) }
					}
				}
				elseif ($recordType -ceq 'user' -and $elementType -ceq 'tool_result') {
					$measured.Add([pscustomobject]@{ ToolUseId = [string](Get-AgentTranscriptField $element 'tool_use_id'); Chars = (Measure-ResultChars (Get-AgentTranscriptField $element 'content')) })
				}
			}
		}
	}

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

	$envelope = [ordered]@{
		schemaVersion = 'broken-engine-context-efficiency/v1'
		sessionId = $sessionLabel
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
