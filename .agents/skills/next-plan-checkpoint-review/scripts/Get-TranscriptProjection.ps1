# Prints one row per tool call, per tool result, per assistant text element, per string-content record, per matched
# read/brief pair, and per unmet brief requirement of a Claude Code or native `agy` JSONL transcript, given as a path,
# for the checkpoint isolation lens: a record with several of those yields several rows, and a record with none yields
# none. The rows are:
# `<line> use <tool> <input summary capped at 160 chars>`
# `<line> match <read path> read-at <read line>` — a delegation record's brief lists a path an earlier `Read`/`read_file`
#   record or allowlisted read-only shell command opened; `<line>` is the delegation record's line and `<read line>` the
#   reading record's line. A delegation record is a `tool_use` or `PLANNER_RESPONSE` tool call whose string input field
#   starts with a `Role:` line and carries a `Governing paths:` line; that line's value and a `Scope:` line's value are
#   the brief paths, compared case- and separator-insensitively against the read path.
# `<line> brief-missing <input field> <field label>` — a delegation-shaped input field fails a brief requirement: its
#   first non-blank line is not a `Role:` line, or it carries no `Governing paths:` line. A field is delegation-shaped
#   when at least two `.agents/references/subagent-reporting.md` `## Task brief` field labels each start a line of it.
#   One row per unmet requirement; such a field yields no `match` rows. Any string input field qualifies, including one
#   carrying quoted or embedded brief text, such as a `Write`/`Edit` payload; open the record to dismiss such a row.
# `<line> result <tool_use_id> len <chars>`
# `<line> result <tool_use_id> len <chars> error` — only when `is_error` / `IsError` is true.
# `<line> assistant-text len <chars> <text>` — original text length; whitespace collapsed and payload capped at 160 chars.
# `<line> <record type> text len <chars>[ delivery=<tool_use_id>]` — a string-content record: a Claude
#   task-notification/queued-attachment record, or an `agy` `USER_INPUT`/`SYSTEM_MESSAGE` record (`delivery=` when
#   `DeliveryToolUseId` is set).
# `<line>` is the 1-based transcript line; on Claude only `assistant`, `user`, and `attachment` records print, and
# sidechain records and blank lines print nothing; on `agy`, `USER_INPUT`, `SYSTEM_MESSAGE`, `PLANNER_RESPONSE`, and
# `GENERIC` records print.

[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $TranscriptPath
)

$ErrorActionPreference = 'Stop'
# No Set-StrictMode: transcript records omit fields freely, and a missing field must read as null here, which
# strict mode would turn into an error.

$sharedScripts = Join-Path $PSScriptRoot '..\..\..\scripts'
if (-not (Test-Path -LiteralPath (Join-Path $sharedScripts 'AgentTranscriptCommon.psm1'))) {
	$sharedScripts = Join-Path $PSScriptRoot '..\..\..\..\.agents\scripts'
}
Import-Module (Join-Path $sharedScripts 'AgentTranscriptCommon.psm1') -Force

# A shell command counts as a read only from a fixed allowlist of print commands, with no token carrying `<`
# or `>` so a redirection or heredoc write never qualifies, and `sed` only in its `-n` form without `-i`. The
# read path is the first path-like token; when that token is not the file, as for a `sed` address containing
# `/`, it yields a path no brief lists and so no row.
function Get-AllowedShellReadPath([string] $Command) {
	if ([string]::IsNullOrWhiteSpace($Command)) { return $null }
	$tokens = @($Command -split '\s+' | Where-Object { $_ -ne '' })
	if ($tokens.Count -eq 0) { return $null }
	$verb = ([string] $tokens[0]).ToLowerInvariant()
	if ((($verb -in @('cat', 'head', 'tail', 'get-content')) -or
		($verb -eq 'sed' -and $tokens.Count -gt 1 -and $tokens[1] -eq '-n' -and $tokens -notcontains '-i')) -and -not ($tokens -match '[<>]')) {
		$pathToken = $tokens | Where-Object { $_ -match '[/\\]' } | Select-Object -First 1
		if ($pathToken) { return $pathToken.Trim('"', "'").TrimEnd('.', ')', '"', "'") }
	}
	return $null
}

$script:BriefLabels = @('Role:', 'Skill:', 'Objective:', 'Required sections:', 'Scope:', 'Exclusions:', 'Fixed decisions:', 'Governing paths:', 'Baseline:', 'Acceptance:', 'Prohibitions:', 'Return:')

function Write-ToolCallProjection([int] $LineNumber, [string] $ToolName, $InputObject, [Collections.Generic.List[object]] $Reads) {
	$summarySource = if ($null -ne $InputObject) { $InputObject } else { @{} }
	$summary = ($summarySource | ConvertTo-Json -Compress -Depth 100) -replace '\s+', ' '
	'{0} use {1} {2}' -f $LineNumber, $ToolName, $summary.Substring(0, [Math]::Min(160, $summary.Length))

	$readPath = $null
	if ($ToolName -ceq 'Read') {
		$candidate = Get-AgentTranscriptField $InputObject 'file_path'
		if ($candidate -is [string] -and -not [string]::IsNullOrWhiteSpace($candidate)) { $readPath = $candidate }
	}
	elseif ($ToolName -ceq 'read_file') {
		$candidate = Get-AgentTranscriptField $InputObject 'Path'
		if ($candidate -is [string] -and -not [string]::IsNullOrWhiteSpace($candidate)) { $readPath = $candidate }
	}
	elseif ($ToolName -ceq 'Bash' -or $ToolName -ceq 'PowerShell') {
		$command = Get-AgentTranscriptField $InputObject 'command'
		if ($command -is [string]) { $readPath = Get-AllowedShellReadPath $command }
	}
	elseif ($ToolName -ceq 'run_command') {
		$command = Get-AgentTranscriptField $InputObject 'CommandLine'
		if ($command -is [string]) { $readPath = Get-AllowedShellReadPath $command }
	}
	if ($readPath) {
		$Reads.Add(@{ Line = $LineNumber; Path = $readPath; Key = ($readPath -replace '\\', '/').ToLowerInvariant() })
	}

	if ($InputObject -isnot [Management.Automation.PSCustomObject]) { return }
	# Brief paths come from the unescaped field value, never the serialized JSON above, so a token is delimited by
	# real whitespace, a real backtick, or the `,`/`;` a brief separates listed paths with, and loses the
	# sentence punctuation a brief may end it with; prose in those fields yields no token.
	$briefMissing = [Collections.Generic.List[string]]::new()
	$briefKeys = @(foreach ($field in $InputObject.PSObject.Properties) {
		if ($field.Value -isnot [string]) { continue }
		$fieldLines = $field.Value -split '\r?\n'
		$firstText = $fieldLines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1
		$hasRole = $firstText -like 'Role:*'
		$pathLines = $fieldLines | Where-Object { $_ -like 'Governing paths:*' -or $_ -like 'Scope:*' }
		$hasGoverningPaths = [bool] ($pathLines | Where-Object { $_ -like 'Governing paths:*' })
		if (-not ($hasRole -and $hasGoverningPaths)) {
			# Two distinct brief labels separate a malformed brief from prose that merely carries one such line.
			$labelHits = @($script:BriefLabels | Where-Object { $label = $_; @($fieldLines | Where-Object { $_ -like "$label*" }).Count -gt 0 })
			if ($labelHits.Count -ge 2) {
				if (-not $hasRole) { $briefMissing.Add(('{0} brief-missing {1} Role:' -f $LineNumber, $field.Name)) }
				if (-not $hasGoverningPaths) { $briefMissing.Add(('{0} brief-missing {1} Governing paths:' -f $LineNumber, $field.Name)) }
			}
			continue
		}
		foreach ($token in (($pathLines -join ' ') -split '[\s`,;]+')) {
			$token = $token.TrimEnd('.', ')', '"', "'")
			if ($token -match '[/\\]') { ($token -replace '\\', '/').ToLowerInvariant() }
		}
	})
	foreach ($row in $briefMissing) { $row }
	foreach ($read in $Reads) {
		foreach ($briefKey in $briefKeys) {
			if ($read.Key -eq $briefKey -or $read.Key.EndsWith('/' + $briefKey)) {
				'{0} match {1} read-at {2}' -f $LineNumber, $read.Path, $read.Line
				break
			}
		}
	}
}

# Each entry is the line and printable path of one `Read`/`read_file` record or allowlisted read-only shell command, plus
# that path folded to lower case with `\` as `/` for comparison. Reads always precede the brief that lists them, so one
# streaming pass suffices.
$reads = [Collections.Generic.List[object]]::new()

try {
	$resolved = Resolve-AgentAgyTranscriptPath -TranscriptPath $TranscriptPath
	switch ($resolved.Status) {
		'passthrough' { $transcript = $resolved.Path }
		'safe' { $transcript = $resolved.Path }
		'missing' { throw 'Transcript file does not exist.' }
		'unsafe' { throw 'Transcript path is an unsafe reparse point.' }
		default { throw 'Transcript path could not be safely read.' }
	}
	if (-not (Test-Path -LiteralPath $transcript -PathType Leaf)) { throw 'Transcript file does not exist.' }

	if (Test-AgentAgyTranscript $transcript) {
		foreach ($record in (Read-AgentAgyTranscript $transcript)) {
			if ($record.Type -ceq 'USER_INPUT' -or $record.Type -ceq 'SYSTEM_MESSAGE') {
				$row = '{0} {1} text len {2}' -f $record.LineNumber, $record.Type, $record.Content.Length
				if ($record.Type -ceq 'SYSTEM_MESSAGE' -and -not [string]::IsNullOrEmpty($record.DeliveryToolUseId)) {
					$row += " delivery=$($record.DeliveryToolUseId)"
				}
				$row
			}
			elseif ($record.Type -ceq 'PLANNER_RESPONSE') {
				foreach ($tc in $record.ToolCalls) {
					Write-ToolCallProjection $record.LineNumber $tc.Name $tc.Args $reads
				}
				if (-not [string]::IsNullOrEmpty($record.Content)) {
					$text = $record.Content
					$summary = $text -replace '\s+', ' '
					'{0} assistant-text len {1} {2}' -f $record.LineNumber, $text.Length, $summary.Substring(0, [Math]::Min(160, $summary.Length))
				}
			}
			elseif ($record.Type -ceq 'GENERIC') {
				$toolUseId = if ($null -ne $record.PairedToolCall) { $record.PairedToolCall.ToolUseId } else { 'unknown' }
				$result = '{0} result {1} len {2}' -f $record.LineNumber, $toolUseId, $record.Content.Length
				if ($record.IsError) { $result += ' error' }
				$result
			}
		}
	}
	else {
		$n = 0
		foreach ($line in [IO.File]::ReadLines($transcript)) {
			$n++
			$record = $line | ConvertFrom-Json
			# Only these types carry the conversation; others, such as request copies, repeat it or carry none.
			if ($record.isSidechain -eq $true -or $record.type -notin @('assistant', 'user', 'attachment')) { continue }
			$content = $record.message.content
			if ($record.type -eq 'attachment') { $content = $record.attachment.prompt }
			if ($content -is [string]) { '{0} {1} text len {2}' -f $n, $record.type, $content.Length; continue }
			foreach ($element in $content) {
				if ($element.type -eq 'tool_use') {
					Write-ToolCallProjection $n ([string] $element.name) $element.input $reads
				}
				elseif ($element.type -eq 'tool_result') {
					$result = '{0} result {1} len {2}' -f $n, $element.tool_use_id, ($element.content | ConvertTo-Json -Compress -Depth 100).Length
					if ($element.is_error -eq $true) { $result += ' error' }
					$result
				}
				elseif ($record.type -eq 'assistant' -and $element.type -eq 'text') {
					$text = [string] $element.text
					$summary = $text -replace '\s+', ' '
					'{0} assistant-text len {1} {2}' -f $n, $text.Length, $summary.Substring(0, [Math]::Min(160, $summary.Length))
				}
			}
		}
	}
}
catch {
	Write-Error $_
	exit 1
}
