# Prints one row per tool call, per tool result, and per string-content record of a Claude Code JSONL transcript,
# given as an absolute path, for the checkpoint isolation lens: a record with several of those yields several rows,
# and a record with none yields none. The rows are:
# `<line> use <tool> <input summary capped at 160 chars>`
# `<line> result <tool_use_id> len <chars>`
# `<line> <record type> text len <chars>` — a string-content record: a task-notification handoff or an `attachment` record's prompt.
# `<line>` is the 1-based transcript line; sidechain records and blank lines print nothing.

[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $TranscriptPath
)

$ErrorActionPreference = 'Stop'
# No Set-StrictMode: transcript records omit fields freely, and a missing field must read as null here, which
# strict mode would turn into an error.

$n = 0
foreach ($line in [IO.File]::ReadLines($TranscriptPath)) {
	$n++
	$record = $line | ConvertFrom-Json
	if ($record.isSidechain -eq $true) { continue }
	$content = $record.message.content
	if ($record.type -eq 'attachment') { $content = $record.attachment.prompt }
	if ($content -is [string]) { '{0} {1} text len {2}' -f $n, $record.type, $content.Length; continue }
	foreach ($element in $content) {
		if ($element.type -eq 'tool_use') {
			$summary = ($element.input | ConvertTo-Json -Compress -Depth 100) -replace '\s+', ' '
			'{0} use {1} {2}' -f $n, $element.name, $summary.Substring(0, [Math]::Min(160, $summary.Length))
		}
		elseif ($element.type -eq 'tool_result') { '{0} result {1} len {2}' -f $n, $element.tool_use_id, ($element.content | ConvertTo-Json -Compress -Depth 100).Length }
	}
}
