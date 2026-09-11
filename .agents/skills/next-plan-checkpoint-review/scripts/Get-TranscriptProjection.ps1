# Prints one row per tool call, per tool result, per main-assistant text element, per string-content record, and per
# matched read/brief pair of a Claude Code JSONL transcript, given as an absolute path, for the checkpoint isolation
# lens: a record with several of those yields several rows, and a record with none yields none. The rows are:
# `<line> use <tool> <input summary capped at 160 chars>`
# `<line> match <read path> read-at <read line>` — a delegation record's brief lists a path an earlier `Read` record or
#   allowlisted read-only shell command opened; `<line>` is the delegation record's line and `<read line>` the reading
#   record's line. A delegation record is a `tool_use` whose string input field starts with a `Role:` line and carries a
#   `Governing paths:` line; that line's value and a `Scope:` line's value are the brief paths, compared case- and
#   separator-insensitively against the read path.
# `<line> result <tool_use_id> len <chars>`
# `<line> result <tool_use_id> len <chars> error` — only when `is_error` is true.
# `<line> assistant-text len <chars> <text>` — original text length; whitespace collapsed and payload capped at 160 chars.
# `<line> <record type> text len <chars>` — a string-content record: a task-notification handoff or an `attachment` record's prompt.
# `<line>` is the 1-based transcript line; sidechain records and blank lines print nothing.

[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $TranscriptPath
)

$ErrorActionPreference = 'Stop'
# No Set-StrictMode: transcript records omit fields freely, and a missing field must read as null here, which
# strict mode would turn into an error.

# Each entry is the line and printable path of one `Read` record or allowlisted read-only shell command, plus that path
# folded to lower case with `\` as `/` for comparison. Reads always precede the brief that lists them, so one streaming
# pass suffices.
$reads = [Collections.Generic.List[object]]::new()

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
			$readPath = $null
			if ($element.name -eq 'Read' -and $element.input.file_path) {
				$readPath = [string] $element.input.file_path
			}
			# A shell command counts as a read only from a fixed allowlist of print commands, with no token carrying `<`
			# or `>` so a redirection or heredoc write never qualifies, and `sed` only in its `-n` form without `-i`. The
			# read path is the first path-like token; when that token is not the file, as for a `sed` address containing
			# `/`, it yields a path no brief lists and so no row.
			elseif (($element.name -eq 'Bash' -or $element.name -eq 'PowerShell') -and $element.input.command) {
				$tokens = @(([string] $element.input.command) -split '\s+' | Where-Object { $_ -ne '' })
				$verb = ([string] $tokens[0]).ToLowerInvariant()
				if ((($verb -in @('cat', 'head', 'tail', 'get-content')) -or
					($verb -eq 'sed' -and $tokens[1] -eq '-n' -and $tokens -notcontains '-i')) -and -not ($tokens -match '[<>]')) {
					$pathToken = $tokens | Where-Object { $_ -match '[/\\]' } | Select-Object -First 1
					if ($pathToken) { $readPath = $pathToken.Trim('"', "'").TrimEnd('.', ')', '"', "'") }
				}
			}
			if ($readPath) {
				$reads.Add(@{ Line = $n; Path = $readPath; Key = ($readPath -replace '\\', '/').ToLowerInvariant() })
			}
			# Brief paths come from the unescaped field value, never the serialized JSON above, so a token is delimited by
			# real whitespace, a real backtick, or the `,`/`;` a brief separates listed paths with, and loses the
			# sentence punctuation a brief may end it with; prose in those fields yields no token.
			$briefKeys = foreach ($field in $element.input.PSObject.Properties) {
				if ($field.Value -isnot [string]) { continue }
				$fieldLines = $field.Value -split '\r?\n'
				$firstText = $fieldLines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1
				if ($firstText -notlike 'Role:*') { continue }
				$pathLines = $fieldLines | Where-Object { $_ -like 'Governing paths:*' -or $_ -like 'Scope:*' }
				if (-not ($pathLines | Where-Object { $_ -like 'Governing paths:*' })) { continue }
				foreach ($token in (($pathLines -join ' ') -split '[\s`,;]+')) {
					$token = $token.TrimEnd('.', ')', '"', "'")
					if ($token -match '[/\\]') { ($token -replace '\\', '/').ToLowerInvariant() }
				}
			}
			foreach ($read in $reads) {
				foreach ($briefKey in $briefKeys) {
					if ($read.Key -eq $briefKey -or $read.Key.EndsWith('/' + $briefKey)) {
						'{0} match {1} read-at {2}' -f $n, $read.Path, $read.Line
						break
					}
				}
			}
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
