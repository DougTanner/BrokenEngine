# Prints one row per tool call, per tool result, per assistant text element, per string-content record, per matched
# read/brief pair, per unmet brief requirement, and per Codex bridge invocation/stub/delivery of a Claude Code or
# native `agy` JSONL transcript, given as a path, plus one trailing `checkpoint-span` row for the checkpoint review
# (and, when `-IncludeContextEnvelope` is set, one trailing `context-envelope` row measuring eligible non-sidechain
# results in the same pass): a record with several of those yields several rows, and a record with none yields none. The rows
# are:
# `<line> use <tool> <input summary capped at 160 chars>`
# `<line> match <read path> read-at <read line>` — a delegation record's brief lists a path an earlier `Read`/`read_file`
#   record or allowlisted read-only shell command opened; `<line>` is the delegation record's line and `<read line>` the
#   reading record's line. A delegation record is a `tool_use` or `PLANNER_RESPONSE` tool call whose string input field
#   starts with a `Role:` line and carries a `Governing paths:` line; `Governing paths:` and `Scope:` values (including
#   continuation lines up to the next column-0 1-3 word key-colon header) are the brief paths, compared case- and
#   separator-insensitively against the read path (resolving relative paths against the transcript's recorded worktree
#   root when available). Three exemptions suppress `match` rows: (1) a single bounded `Read`/`read_file` call per path
#   in its checkpoint span whose requested line count is `<= 60` (`$script:SmallReadMaxLines`) and whose paired
#   successful text result length is `<= 4,000` chars (`$script:SmallReadMaxChars`), (2) a resolved worktree public
#   subagent `SKILL.md` directly under `.agents/skills/<skill>/SKILL.md` or `.claude/skills/<skill>/SKILL.md` when
#   `.agents/skills/<skill>/references/worker.md` exists, and (3) a single `Read`/`read_file` call per canonical path in
#   its checkpoint span whose paired successful non-empty text result is `<= 20,000` chars (`$script:WorkflowRefMaxChars`)
#   for a resolved worktree main-owned `.agents/references/<doc>` in `$script:MainOwnedReferenceDocs` or a main-session
#   skill reference under `.agents/skills/<skill>/references/<file>.md` or `.claude/skills/<skill>/references/<file>.md`
#   when `.agents/skills/<skill>/SKILL.md` exists and `.agents/skills/<skill>/references/worker.md` does not
#   (`.agents/skills/` and `.claude/skills/` spellings of the same skill reference share one canonical peer identity).
#   Non-exempt paths (including `references/worker.md`, references under skills with `references/worker.md`, shared
#   non-skill references without an owning `SKILL.md`, main-session `SKILL.md` files without `references/worker.md`,
#   worker-only `.agents/references/` docs, `change-workflow.md`, Plans, `Temp/` files, source files, and
#   out-of-worktree files) still emit `match` rows outside exemption (1), as do over-cap reads, allowlisted shell reads,
#   failed/unpaired/non-text/missing results, checkpoint-dispatch reads, and repeat reads of the same path (or canonical
#   peer) in the same checkpoint span. Eligible `match` rows are emitted with their original `<line>` and `<read line>`
#   immediately before `checkpoint-span`, while all ordinary rows stream unchanged and `checkpoint-span` and
#   `context-envelope` formats stay unchanged.
# `<line> brief-missing <input field> <field label>` — a delegation-shaped input field fails a brief requirement: its
#   first non-blank line is not a `Role:` line, or it carries no `Governing paths:` line. A field is delegation-shaped
#   when at least two `.agents/references/subagent-reporting.md` `## Task brief` field labels each start a line of it.
#   One row per unmet requirement; such a field yields no `match` rows. Any string input field qualifies, including one
#   carrying quoted or embedded brief text, such as a `Write`/`Edit` payload; open the record to dismiss such a row.
# `<line> bridge-launch <tool_use_id> skill=<skill>[ mode=<mode>][ noretry][ prompt-write=<writeLine>]` — an unclipped
#   `Invoke-Codex.ps1` launch call (`-Skill`, ignoring `-CheckModels`); `prompt-write=` names the latest matching
#   `Write`/`write_file` line for `-PromptFile` (restricted after any prior distinct checkpoint group's final dispatch
#   for `next-plan-checkpoint-review`).
# `<line> bridge-wait <tool_use_id> runId=<runId>` — an unclipped `Invoke-Codex.ps1 -Wait <runId>` call.
# `<line> bridge-stub <tool_use_id>` — a background launch stub (`agy` `GENERIC` with `LaunchedTaskId`, or non-error
#   Claude `tool_result` matching a background-task stub, when no outer bridge receipt is present) for a tracked bridge call.
# `<line> bridge-delivery <tool_use_id> status=<status> runId=<runId> skill=<skill|unknown>[ mode=<mode>][ noretry] findings=<yes|no>`
#   — a delivery for a tracked bridge call carrying an outer compact single-line `broken-engine-codex-run/v2` JSON receipt
#   (Claude `tool_result`, Claude `task-notification` `user` or `queued_command` `attachment` with `<tool-use-id>`,
#   `agy` `GENERIC`, or `agy` `SYSTEM_MESSAGE`); `findings=yes` only when `status=completed` and `--- findings ---`
#   appears after the receipt line. Pre-v2 outer receipts are ignored.
# `<line> result <tool_use_id> len <chars>`
# `<line> result <tool_use_id> len <chars> error` — only when `is_error` / `IsError` is true.
# `<line> assistant-text len <chars> <text>` — original text length; whitespace collapsed and payload capped at 160 chars.
# `<line> <record type> text len <chars>[ delivery=<tool_use_id>]` — a string-content record: a Claude
#   task-notification/queued-attachment record, or an `agy` `USER_INPUT`/`SYSTEM_MESSAGE` record (`delivery=` when
#   `DeliveryToolUseId` is set).
# `checkpoint-span start=<startLine> end=<endLine> exclude=<handoffLine|none> exclude-id=<toolUseId|none>` — emitted
#   once at end of stream; gives the 1-based inclusive line span `[start, end]` for the current
#   `/next-plan-checkpoint-review` run (`start=0 end=0` for an empty transcript; empty span when `start > end`), plus
#   the prior checkpoint's completed-handoff line (`exclude`, one record line excluded for friction and isolation) and
#   dispatching `tool_use_id` (`exclude-id`, excluded from context-efficiency `topResults`). Recognizes bridged, native
#   (`Agent`/`invoke_subagent`), and trailing desktop (`Write`/`write_file`) checkpoints.
# `context-envelope <compact single-line JSON>` — emitted once after `checkpoint-span` only when
#   `-IncludeContextEnvelope` is set (`-Top` 1..25 default 10, `-ThresholdChars` >= 1 default 20000); carries
#   `broken-engine-context-efficiency/v1` (non-sidechain Claude `user` content-array `tool_result` entries,
#   `assistant`/`user` `task-notification` strings, `queued_command` attachment strings with non-empty `<tool-use-id>`,
#   plus `agy` paired `GENERIC` and delivery `SYSTEM_MESSAGE` records ranked by text chars, up to 25 breach rows and
#   filled to `-Top`; projected array-valued attachment `tool_result` rows and unpaired `agy` `GENERIC` results are
#   excluded) or `broken-engine-context-efficiency-error/v1` (`code: "measure.failed"`) with exit `0` when
#   post-`checkpoint-span` envelope assembly fails.
# `<line>` is the 1-based transcript line; on Claude only `assistant`, `user`, and `attachment` records print, and
# sidechain records and blank lines print nothing; on `agy`, `USER_INPUT`, `SYSTEM_MESSAGE`, `PLANNER_RESPONSE`, and
# `GENERIC` records print. Missing, unsafe, unreadable, malformed JSON, or non-object JSON inputs fail closed in both
# modes with exit `1`, no `checkpoint-span`, and fixed safe single-line stderr.

[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $TranscriptPath,
	[switch] $IncludeContextEnvelope,
	[ValidateRange(1, 25)][int] $Top = 10,
	[ValidateRange(1, [int]::MaxValue)][int] $ThresholdChars = 20000
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
$script:BriefHeaderPattern = '^[A-Za-z][A-Za-z0-9_-]*(?: [A-Za-z0-9_-]+){0,2}:(?:\s|$)'
$script:ClaudeBridgeStubPattern = '(?i)\b(?:running in (?:the )?background|background (?:task|command|shell)|async(?:hronous)?\s+(?:task|agent|command)|with task id:)'
$script:ClaudeNativeAgentStubPattern = '(?i)\A\s*(?:async(?:hronous)?\s+agent\s+launched\s+successfully\.?\s*\z|(?:(?:async(?:hronous)?|background)\s+agent\s+(?:launched|started)|agent\s+(?:launched|started)\s+(?:asynchronously|in\s+(?:the\s+)?background)|(?:launched|started)\s+(?:(?:an?\s+)?(?:async(?:hronous)?|background)\s+agent|asynchronously|in\s+(?:the\s+)?background))[^\r\n]*(?:\r?\n[ \t]*)?(?:\bagentId:|\bwith\s+(?:task\s+)?id:))'
$script:BridgeReceiptOrSeparatorPattern = '(?m)(?:^|content=|<result>)[ \t]*(?:(?<sep>--- findings ---)|(?<json>\{"schemaVersion":"broken-engine-codex-run/[^\r\n]*?))(?:[ \t]*</result>)?[ \t]*\r?$'
$script:ValidReceiptStatuses = @('completed', 'malformed', 'failed', 'running')
# Cap breach rows so the trailing context-envelope stays bounded even when a session breaches constantly.
$script:BreachRowCap = 25
# Small-read floor: a single bounded Read/read_file call per path in a checkpoint span at or below both limits emits no `match` row.
$script:SmallReadMaxLines = 60
$script:SmallReadMaxChars = 4000
# Small main-readable workflow-reference floor and allowlisted main-owned `.agents/references/` docs.
$script:WorkflowRefMaxChars = 20000
$script:MainOwnedReferenceDocs = @('authority-order.md', 'codex-bridge.md', 'execution-card.md', 'subagent-handoff.md', 'subagent-reporting.md')
# Repository skills root and per-skill cache for the worktree public subagent `SKILL.md` and main-session skill reference exemptions.
$script:SkillsRoot = Join-Path $PSScriptRoot '..\..'
$script:PublicSubagentSkillCache = @{}
# Input fields that name the emitting invocation without leaking result content.
$script:ContextSummaryFields = @('command', 'CommandLine', 'file_path', 'Path', 'path', 'pattern', 'url', 'query', 'prompt', 'Prompt', 'description', 'Role', 'TypeName', 'Recipient', 'TaskId', 'Action')

function Stop-ProjectionFailure([string] $Message) {
	[Console]::Error.WriteLine($Message)
	exit 1
}

function Get-ToolUseIdTag([string] $Text) {
	$match = [regex]::Match($Text, '<tool-use-id>([^<]*)</tool-use-id>')
	if ($match.Success) { return $match.Groups[1].Value }
	return ''
}

function Measure-ContextResultChars($Content) {
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

function Measure-ReadResultTextChars($Content) {
	if ($Content -is [string]) { return $Content.Length }
	if ($Content -is [object[]] -and $Content.Length -gt 0) {
		$total = 0
		foreach ($element in $Content) {
			if ([string](Get-AgentTranscriptField $element 'type') -cne 'text') { return $null }
			$text = Get-AgentTranscriptField $element 'text'
			if ($text -isnot [string]) { return $null }
			$total += $text.Length
		}
		return $total
	}
	return $null
}

function Get-PositiveIntegralNumber($InputObject, [string] $Name) {
	$raw = Get-AgentTranscriptField $InputObject $Name
	if ($null -eq $raw) { return $null }
	if ($raw -is [int] -or $raw -is [long] -or $raw -is [double] -or $raw -is [decimal]) {
		$d = [double] $raw
		if ([double]::IsFinite($d) -and $d -ge 1 -and $d -le 9007199254740991 -and [Math]::Truncate($d) -eq $d) {
			return $d
		}
	}
	return [double] -1
}

function Get-ContextInputSummary($ToolInput) {
	$text = ''
	foreach ($field in $script:ContextSummaryFields) {
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

function ConvertTo-NormalizedPathKey([string] $Path) {
	if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
	$normalized = ($Path.Trim() -replace '\\', '/').ToLowerInvariant()
	while ($normalized.StartsWith('./')) { $normalized = $normalized.Substring(2) }
	return $normalized
}

function ConvertTo-CanonicalPathKey([string] $Path, [bool] $AllowGitBashDrive = $false) {
	if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
	$trimmed = $Path.Trim()
	$prefix = ''
	$remainder = ''
	if ($AllowGitBashDrive -and $trimmed -match '^/([A-Za-z])/(.*)$') {
		$prefix = $Matches[1].ToLowerInvariant() + ':/'
		$remainder = ($Matches[2] -replace '\\', '/').ToLowerInvariant()
	}
	else {
		$normalized = ($trimmed -replace '\\', '/').ToLowerInvariant()
		$remainder = $normalized
		if ($normalized -match '^(//(?![.]{1,2}(?:/|$))[^/]+/(?![.]{1,2}(?:/|$))[^/]+)(?:/(.*))?$') {
			$prefix = $Matches[1]
			$remainder = if ($null -ne $Matches[2]) { $Matches[2] } else { '' }
		}
		elseif ($normalized -match '^([a-z]:)/(.*)$') {
			$prefix = $Matches[1] + '/'
			$remainder = $Matches[2]
		}
		elseif ($normalized.StartsWith('/')) {
			$prefix = '/'
			$remainder = $normalized.TrimStart('/')
		}
	}

	$segments = [Collections.Generic.List[string]]::new()
	foreach ($segment in ($remainder -split '/')) {
		if ([string]::IsNullOrEmpty($segment) -or $segment -ceq '.') { continue }
		if ($segment -ceq '..') {
			if ($segments.Count -gt 0 -and $segments[$segments.Count - 1] -cne '..') {
				$segments.RemoveAt($segments.Count - 1)
			}
			elseif ($prefix -ceq '') {
				$segments.Add('..')
			}
			continue
		}
		$segments.Add($segment)
	}

	if ($prefix -ceq '') {
		if ($segments.Count -eq 0) { return $null }
		return ($segments -join '/')
	}
	if ($segments.Count -eq 0) { return $prefix }
	if ($prefix.EndsWith('/')) { return $prefix + ($segments -join '/') }
	return $prefix + '/' + ($segments -join '/')
}

function Resolve-NormalizedPathKey([string] $Key, [string] $RootKey) {
	if ([string]::IsNullOrEmpty($Key)) { return $null }
	if ($Key -match '^[a-z]:/' -or $Key.StartsWith('//')) { return $Key }
	if ($Key.StartsWith('/')) {
		if (-not [string]::IsNullOrEmpty($RootKey) -and $RootKey -match '^([a-z]:)/') {
			return $Matches[1] + $Key
		}
		return $Key
	}
	if (-not [string]::IsNullOrEmpty($RootKey)) {
		$joined = if ($RootKey.EndsWith('/')) { $RootKey + $Key } else { $RootKey + '/' + $Key }
		if ($Key -ceq '..' -or $Key.StartsWith('../')) {
			return ConvertTo-CanonicalPathKey $joined $false
		}
		return $joined
	}
	return $Key
}

function Test-PathKeyMatch([string] $LeftKey, [string] $RightKey, [string] $RootKey) {
	if ([string]::IsNullOrEmpty($LeftKey) -or [string]::IsNullOrEmpty($RightKey)) { return $false }
	$left = Resolve-NormalizedPathKey $LeftKey $RootKey
	$right = Resolve-NormalizedPathKey $RightKey $RootKey
	if ([string]::IsNullOrEmpty($left) -or [string]::IsNullOrEmpty($right)) { return $false }
	$leftAbs = $left -match '^(?:[a-z]:/|/)'
	$rightAbs = $right -match '^(?:[a-z]:/|/)'
	if ($leftAbs -eq $rightAbs) { return $left -ceq $right }
	$abs = if ($leftAbs) { $left } else { $right }
	$rel = if ($leftAbs) { $right } else { $left }
	return $rel.Contains('/') -and $abs.EndsWith('/' + $rel)
}

function Get-SpanIndex([int] $Line, $Checkpoints) {
	for ($i = 0; $i -lt $Checkpoints.Count; $i++) {
		if ($Line -lt $Checkpoints[$i].FirstLine) { return $i }
		if ($Line -le $Checkpoints[$i].FinalDispatchLine) { return -1 }
	}
	return $Checkpoints.Count
}

function Test-NormalizedPathMatch([string] $Left, [string] $Right) {
	if ([string]::IsNullOrEmpty($Left) -or [string]::IsNullOrEmpty($Right)) { return $false }
	return $Left -ceq $Right -or $Left.EndsWith('/' + $Right) -or $Right.EndsWith('/' + $Left)
}

function Get-WorktreeRelativePathKey([string] $Key, [string] $RootKey) {
	if ([string]::IsNullOrEmpty($Key)) { return $null }
	$resolvedKey = Resolve-NormalizedPathKey $Key $RootKey
	if ([string]::IsNullOrEmpty($resolvedKey)) { return $null }
	if (-not [string]::IsNullOrEmpty($RootKey)) {
		$rootPrefix = if ($RootKey.EndsWith('/')) { $RootKey } else { $RootKey + '/' }
		if (-not $resolvedKey.StartsWith($rootPrefix, [StringComparison]::Ordinal)) { return $null }
		return $resolvedKey.Substring($rootPrefix.Length)
	}
	return $resolvedKey
}

function Test-SkillHasWorkerRef([string] $SkillName) {
	if (-not $script:PublicSubagentSkillCache.ContainsKey($SkillName)) {
		$script:PublicSubagentSkillCache[$SkillName] = Test-Path -LiteralPath (Join-Path $script:SkillsRoot "$SkillName\references\worker.md") -PathType Leaf
	}
	return $script:PublicSubagentSkillCache[$SkillName]
}

function Test-PublicSubagentSkillPath([string] $Key, [string] $RootKey) {
	$relKey = Get-WorktreeRelativePathKey $Key $RootKey
	if ([string]::IsNullOrEmpty($relKey)) { return $false }
	if ($relKey -cnotmatch '^\.(?:agents|claude)/skills/([^/]+)/skill\.md$') { return $false }
	return (Test-SkillHasWorkerRef $Matches[1])
}

function Get-MainWorkflowRefKey([string] $Key, [string] $RootKey) {
	$relKey = Get-WorktreeRelativePathKey $Key $RootKey
	if ([string]::IsNullOrEmpty($relKey)) { return $null }
	if ($relKey -cmatch '^\.agents/references/([^/]+\.md)$') {
		if ($script:MainOwnedReferenceDocs -ccontains $Matches[1]) { return $relKey }
		return $null
	}
	if ($relKey -cmatch '^\.(?:agents|claude)/skills/([^/]+)/references/([^/]+\.md)$') {
		$skillName = $Matches[1]
		$refFile = $Matches[2]
		if ($refFile -cne 'worker.md' -and
			(Test-Path -LiteralPath (Join-Path $script:SkillsRoot "$skillName\SKILL.md") -PathType Leaf) -and
			-not (Test-SkillHasWorkerRef $skillName)) {
			return '.agents/skills/' + $skillName + '/references/' + $refFile
		}
	}
	return $null
}

function Test-CheckpointReviewBrief([string] $Text) {
	if ([string]::IsNullOrWhiteSpace($Text)) { return $false }
	$lines = $Text -split '\r?\n'
	$firstText = $lines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1
	if ($null -eq $firstText -or -not ($firstText.TrimStart() -like 'Role:*')) { return $false }
	foreach ($line in $lines) {
		if ($line -match '^\s*Skill:\s*/?next-plan-checkpoint-review\s*$') { return $true }
	}
	return $false
}

function Push-CheckpointGroup([hashtable] $State, [hashtable] $Group) {
	$State.PreviousCheckpoint = $State.CurrentCheckpoint
	$State.CurrentCheckpoint = $Group
	$State.Checkpoints.Add($Group)
}

function Push-PendingDesktopCheckpoint([hashtable] $State) {
	if ($null -eq $State.PendingPromptWrite) { return }
	$pending = $State.PendingPromptWrite
	$State.PendingPromptWrite = $null
	Push-CheckpointGroup $State @{
		Kind = 'desktop'
		FirstLine = $pending.FirstLine
		FinalDispatchLine = $pending.LastLine
		FinalToolUseId = $null
		LaunchOutFile = $null
		CompletedHandoffLine = $null
	}
}

function Split-ShellCommands([string] $Command, [string] $Shell = 'powershell') {
	$isBash = $Shell -ceq 'bash'
	$escapeChar = if ($isBash) { '\' } else { '`' }
	$commands = [Collections.Generic.List[object]]::new()
	$current = [Collections.Generic.List[string]]::new()
	$i = 0
	$len = $Command.Length
	while ($i -lt $len) {
		$ch = $Command[$i]
		if ($ch -eq ' ' -or $ch -eq "`t") { $i++; continue }
		if ($ch -eq $escapeChar -and ($i + 1) -lt $len -and ($Command[$i + 1] -eq "`r" -or $Command[$i + 1] -eq "`n")) {
			$i++
			if ($Command[$i] -eq "`r" -and ($i + 1) -lt $len -and $Command[$i + 1] -eq "`n") { $i += 2 }
			else { $i++ }
			continue
		}
		if ($ch -eq '#') {
			while ($i -lt $len -and $Command[$i] -ne "`r" -and $Command[$i] -ne "`n") { $i++ }
			continue
		}
		if ($ch -eq ';' -or $ch -eq '|' -or $ch -eq "`r" -or $ch -eq "`n" -or $ch -eq ')') {
			if ($current.Count -gt 0) {
				$commands.Add($current)
				$current = [Collections.Generic.List[string]]::new()
			}
			$i++
			continue
		}
		if ($ch -eq '(' -and $current.Count -eq 0) {
			$i++
			continue
		}
		if ($ch -eq '&') {
			if (($i + 1) -lt $len -and $Command[$i + 1] -eq '&') {
				if ($current.Count -gt 0) {
					$commands.Add($current)
					$current = [Collections.Generic.List[string]]::new()
				}
				$i += 2
				continue
			}
			if ($current.Count -eq 0) {
				$current.Add('&')
				$i++
				continue
			}
			$commands.Add($current)
			$current = [Collections.Generic.List[string]]::new()
			$i++
			continue
		}
		$sb = [Text.StringBuilder]::new()
		while ($i -lt $len) {
			$c = $Command[$i]
			if ($c -eq ' ' -or $c -eq "`t" -or $c -eq ';' -or $c -eq '|' -or $c -eq "`r" -or $c -eq "`n" -or $c -eq ')') { break }
			if ($c -eq '&' -and -not ($sb.Length -gt 0 -and $sb[$sb.Length - 1] -eq '>')) { break }
			if ($c -eq '>' -and $sb.Length -gt 0 -and $sb.ToString() -notmatch '^[0-9*]+$') { break }
			if ($c -eq $escapeChar -and ($i + 1) -lt $len) {
				$next = $Command[$i + 1]
				if ($next -eq "`r" -or $next -eq "`n") {
					if (-not $isBash) { break }
					$i++
					if ($Command[$i] -eq "`r" -and ($i + 1) -lt $len -and $Command[$i + 1] -eq "`n") { $i += 2 }
					else { $i++ }
					continue
				}
				[void] $sb.Append($next)
				$i += 2
				continue
			}
			if ($c -eq "'") {
				$i++
				while ($i -lt $len) {
					if ($Command[$i] -eq "'") {
						if (-not $isBash -and ($i + 1) -lt $len -and $Command[$i + 1] -eq "'") {
							[void] $sb.Append("'")
							$i += 2
							continue
						}
						$i++
						break
					}
					[void] $sb.Append($Command[$i])
					$i++
				}
				continue
			}
			if ($c -eq '"') {
				$i++
				while ($i -lt $len) {
					if ($Command[$i] -eq '"') {
						if (-not $isBash -and ($i + 1) -lt $len -and $Command[$i + 1] -eq '"') {
							[void] $sb.Append('"')
							$i += 2
							continue
						}
						$i++
						break
					}
					if ($isBash) {
						if ($Command[$i] -eq '\' -and ($i + 1) -lt $len) {
							$next = $Command[$i + 1]
							if ($next -eq "`r" -or $next -eq "`n") {
								$i++
								if ($Command[$i] -eq "`r" -and ($i + 1) -lt $len -and $Command[$i + 1] -eq "`n") { $i += 2 }
								else { $i++ }
								continue
							}
							if ($next -eq '"' -or $next -eq '\' -or $next -eq '$' -or $next -eq '`') {
								[void] $sb.Append($next)
								$i += 2
								continue
							}
						}
					}
					elseif ($Command[$i] -eq '`' -and ($i + 1) -lt $len) {
						[void] $sb.Append($Command[$i + 1])
						$i += 2
						continue
					}
					[void] $sb.Append($Command[$i])
					$i++
				}
				continue
			}
			[void] $sb.Append($c)
			$i++
		}
		$current.Add($sb.ToString())
	}
	if ($current.Count -gt 0) { $commands.Add($current) }
	return , $commands
}

function Parse-BridgeArguments([string[]] $Tokens) {
	$skill = $null
	$mode = $null
	$promptFile = $null
	$outFile = $null
	$noRetry = $false
	$waitRunId = $null
	$isCheckModels = $false
	for ($idx = 0; $idx -lt $Tokens.Count; $idx++) {
		$token = $Tokens[$idx]
		if ($token -notmatch '^-([A-Za-z]+)(?::(.*))?$') { continue }
		$flag = $Matches[1].ToLowerInvariant()
		$hasInline = $token.Contains(':')
		$inlineVal = if ($hasInline) { $Matches[2] } else { $null }
		switch ($flag) {
			'checkmodels' {
				if (-not $hasInline -or $inlineVal -match '^(?:\$?true|1)$') { $isCheckModels = $true }
			}
			'noretry' {
				if (-not $hasInline -or $inlineVal -match '^(?:\$?true|1)$') { $noRetry = $true }
				else { $noRetry = $false }
			}
			'skill' {
				if ($hasInline -and $inlineVal -ne '') { $skill = $inlineVal }
				elseif (($idx + 1) -lt $Tokens.Count) { $idx++; $skill = $Tokens[$idx] }
			}
			'mode' {
				if ($hasInline -and $inlineVal -ne '') { $mode = $inlineVal }
				elseif (($idx + 1) -lt $Tokens.Count) { $idx++; $mode = $Tokens[$idx] }
			}
			'promptfile' {
				if ($hasInline -and $inlineVal -ne '') { $promptFile = $inlineVal }
				elseif (($idx + 1) -lt $Tokens.Count) { $idx++; $promptFile = $Tokens[$idx] }
			}
			'outfile' {
				if ($hasInline -and $inlineVal -ne '') { $outFile = $inlineVal }
				elseif (($idx + 1) -lt $Tokens.Count) { $idx++; $outFile = $Tokens[$idx] }
			}
			'wait' {
				if ($hasInline -and $inlineVal -ne '') { $waitRunId = $inlineVal }
				elseif (($idx + 1) -lt $Tokens.Count) { $idx++; $waitRunId = $Tokens[$idx] }
			}
		}
	}
	if ($isCheckModels) { return $null }
	if (-not [string]::IsNullOrWhiteSpace($skill)) {
		return @{
			Kind = 'launch'
			Skill = $skill
			Mode = $mode
			PromptFile = $promptFile
			OutFile = $outFile
			NoRetry = $noRetry
		}
	}
	if (-not [string]::IsNullOrWhiteSpace($waitRunId)) {
		return @{
			Kind = 'wait'
			RunId = $waitRunId
		}
	}
	return $null
}

function Parse-BridgeCommand([string] $Command, [int] $Depth = 0, [string] $Shell = 'powershell') {
	if ($Depth -gt 3 -or [string]::IsNullOrWhiteSpace($Command) -or $Command -notmatch 'Invoke-Codex\.ps1') { return $null }
	foreach ($tokens in (Split-ShellCommands $Command $Shell)) {
		$cmdIdx = 0
		while ($cmdIdx -lt $tokens.Count -and $tokens[$cmdIdx] -match '^[A-Za-z_][A-Za-z0-9_]*=') { $cmdIdx++ }
		if ($cmdIdx -lt $tokens.Count -and $tokens[$cmdIdx] -ceq 'env') {
			$cmdIdx++
			while ($cmdIdx -lt $tokens.Count) {
				if ($tokens[$cmdIdx] -match '^[A-Za-z_][A-Za-z0-9_]*=') { $cmdIdx++; continue }
				if ($tokens[$cmdIdx] -ceq '-u' -and ($cmdIdx + 1) -lt $tokens.Count) { $cmdIdx += 2; continue }
				if ($tokens[$cmdIdx].StartsWith('-')) { $cmdIdx++; continue }
				break
			}
		}
		if (($cmdIdx + 1) -lt $tokens.Count -and $tokens[$cmdIdx].StartsWith('$') -and $tokens[$cmdIdx + 1] -ceq '=') {
			$cmdIdx += 2
		}
		if ($cmdIdx -lt $tokens.Count -and ($tokens[$cmdIdx] -ceq '&' -or $tokens[$cmdIdx] -ceq '.')) {
			$cmdIdx++
		}
		if ($cmdIdx -ge $tokens.Count) { continue }

		$head = $tokens[$cmdIdx]
		if ($head -match '(?i)(?:^|[\\/])Invoke-Codex\.ps1$') {
			$argTokens = if (($cmdIdx + 1) -lt $tokens.Count) { [string[]] $tokens.GetRange($cmdIdx + 1, $tokens.Count - $cmdIdx - 1).ToArray() } else { [string[]] @() }
			$parsed = Parse-BridgeArguments $argTokens
			if ($null -ne $parsed) { return $parsed }
			continue
		}
		if ($head -match '(?i)(?:^|[\\/])(?:pwsh|powershell)(?:\.exe)?$') {
			for ($j = $cmdIdx + 1; $j -lt $tokens.Count; $j++) {
				$arg = $tokens[$j]
				if ($arg -match '(?i)^-(?:File|f)(?::(.*))?$') {
					$scriptPath = if ($arg.Contains(':')) { $Matches[1] } elseif (($j + 1) -lt $tokens.Count) { $j++; $tokens[$j] } else { $null }
					if ($scriptPath -match '(?i)(?:^|[\\/])Invoke-Codex\.ps1$') {
						$argTokens = if (($j + 1) -lt $tokens.Count) { [string[]] $tokens.GetRange($j + 1, $tokens.Count - $j - 1).ToArray() } else { [string[]] @() }
						$parsed = Parse-BridgeArguments $argTokens
						if ($null -ne $parsed) { return $parsed }
					}
					break
				}
				if ($arg -match '(?i)^-(?:Command|c)(?::(.*))?$') {
					$inlineCmd = if ($arg.Contains(':')) { $Matches[1] } else { $null }
					$restCount = $tokens.Count - $j - 1
					if ([string]::IsNullOrEmpty($inlineCmd) -and $restCount -eq 1) {
						$parsed = Parse-BridgeCommand $tokens[$j + 1] ($Depth + 1) 'powershell'
					}
					elseif ([string]::IsNullOrEmpty($inlineCmd) -and $restCount -gt 1 -and $tokens[$j + 1] -match '(?i)(?:^|[\\/])Invoke-Codex\.ps1$') {
						$argTokens = [string[]] $tokens.GetRange($j + 2, $restCount - 1).ToArray()
						$parsed = Parse-BridgeArguments $argTokens
					}
					else {
						$parts = [Collections.Generic.List[string]]::new()
						if (-not [string]::IsNullOrEmpty($inlineCmd)) { $parts.Add($inlineCmd) }
						for ($k = $j + 1; $k -lt $tokens.Count; $k++) { $parts.Add($tokens[$k]) }
						$parsed = Parse-BridgeCommand ($parts -join ' ') ($Depth + 1) 'powershell'
					}
					if ($null -ne $parsed) { return $parsed }
					break
				}
				if ($arg -match '(?i)^-(?:ExecutionPolicy|ep|InputFormat|if|OutputFormat|of|WindowStyle|w|WorkingDirectory|wd|ConfigurationName|CustomPipeName|SettingsFile)$') {
					$j++
					continue
				}
				if ($arg.StartsWith('-')) { continue }
				if ($arg -match '(?i)(?:^|[\\/])Invoke-Codex\.ps1$') {
					$argTokens = if (($j + 1) -lt $tokens.Count) { [string[]] $tokens.GetRange($j + 1, $tokens.Count - $j - 1).ToArray() } else { [string[]] @() }
					$parsed = Parse-BridgeArguments $argTokens
				}
				else {
					$rest = [string[]] $tokens.GetRange($j, $tokens.Count - $j).ToArray()
					$parsed = Parse-BridgeCommand ($rest -join ' ') ($Depth + 1) 'powershell'
				}
				if ($null -ne $parsed) { return $parsed }
				break
			}
		}
	}
	return $null
}

function Get-ClaudeResultPlainText($Content) {
	if ($Content -is [string]) { return $Content }
	if ($Content -is [object[]]) {
		$parts = [Collections.Generic.List[string]]::new()
		foreach ($item in $Content) {
			if ([string](Get-AgentTranscriptField $item 'type') -cne 'text') { continue }
			$text = Get-AgentTranscriptField $item 'text'
			if ($text -is [string]) { $parts.Add($text) }
		}
		return ($parts -join "`n")
	}
	return ''
}

function Get-ClaudeToolUseResultStubStatus($Record, [int] $ToolResultCount) {
	if ($ToolResultCount -ne 1) { return $null }
	$toolUseResult = Get-AgentTranscriptField $Record 'toolUseResult'
	if ($null -eq $toolUseResult) { return $null }
	if ((Get-AgentTranscriptField $toolUseResult 'isAsync') -eq $true) { return $true }
	$status = [string](Get-AgentTranscriptField $toolUseResult 'status')
	if ($status -ceq 'async_launched') { return $true }
	if (-not [string]::IsNullOrWhiteSpace([string](Get-AgentTranscriptField $toolUseResult 'backgroundTaskId'))) { return $true }
	if ($status -ceq 'completed') { return $false }
	return $null
}

function Test-ClaudeBridgeLaunchStub($Record, [int] $ToolResultCount, [string] $PlainText) {
	$metaStub = Get-ClaudeToolUseResultStubStatus $Record $ToolResultCount
	if ($null -ne $metaStub) { return $metaStub }
	return $PlainText -match $script:ClaudeBridgeStubPattern
}

function Test-ClaudeNativeAgentLaunchStub($Record, [int] $ToolResultCount, [string] $PlainText) {
	$metaStub = Get-ClaudeToolUseResultStubStatus $Record $ToolResultCount
	if ($null -ne $metaStub) { return $metaStub }
	return $PlainText -match $script:ClaudeNativeAgentStubPattern
}

function Find-BridgeOuterReceipt([string] $Text) {
	if ([string]::IsNullOrEmpty($Text) -or (-not $Text.Contains('broken-engine-codex-run/') -and -not $Text.Contains('--- findings ---'))) {
		return @{ Kind = 'none'; Receipt = $null }
	}
	$match = [regex]::Match($Text, $script:BridgeReceiptOrSeparatorPattern)
	if (-not $match.Success) {
		return @{ Kind = 'none'; Receipt = $null }
	}
	if ($match.Groups['sep'].Success) {
		return @{ Kind = 'incompatible'; Receipt = $null }
	}
	$jsonText = $match.Groups['json'].Value
	try {
		$obj = ConvertFrom-Json -InputObject $jsonText -Depth 16 -ErrorAction Stop
	}
	catch {
		return @{ Kind = 'incompatible'; Receipt = $null }
	}
	if ($obj -isnot [Management.Automation.PSCustomObject]) {
		return @{ Kind = 'incompatible'; Receipt = $null }
	}
	$schemaVersion = Get-AgentTranscriptField $obj 'schemaVersion'
	if ($schemaVersion -isnot [string] -or $schemaVersion -cne 'broken-engine-codex-run/v2') {
		return @{ Kind = 'incompatible'; Receipt = $null }
	}
	$status = Get-AgentTranscriptField $obj 'status'
	$runId = Get-AgentTranscriptField $obj 'runId'
	if ($status -isnot [string] -or $script:ValidReceiptStatuses -cnotcontains $status -or
		$runId -isnot [string] -or [string]::IsNullOrWhiteSpace($runId)) {
		return @{ Kind = 'incompatible'; Receipt = $null }
	}
	$skillRaw = Get-AgentTranscriptField $obj 'skill'
	$skill = if ($skillRaw -is [string] -and -not [string]::IsNullOrWhiteSpace($skillRaw)) { $skillRaw } else { 'unknown' }
	$modeRaw = Get-AgentTranscriptField $obj 'mode'
	$mode = if ($modeRaw -is [string] -and -not [string]::IsNullOrWhiteSpace($modeRaw)) { $modeRaw } else { $null }
	$noRetry = (Get-AgentTranscriptField $obj 'noRetry') -eq $true
	$outFileRaw = Get-AgentTranscriptField $obj 'outFile'
	$outFile = if ($outFileRaw -is [string] -and -not [string]::IsNullOrWhiteSpace($outFileRaw)) { $outFileRaw } else { $null }
	$afterReceipt = $Text.Substring($match.Index + $match.Length)
	$hasFindings = ($status -ceq 'completed') -and ($afterReceipt -cmatch '(?m)^--- findings ---\r?$')
	return @{
		Kind = 'v2'
		Receipt = @{
			Status = $status
			RunId = $runId
			Skill = $skill
			Mode = $mode
			NoRetry = $noRetry
			OutFile = $outFile
			HasFindings = $hasFindings
		}
	}
}

function Write-BridgeDeliveryProjection([int] $LineNumber, [string] $ToolUseId, [hashtable] $Receipt, [hashtable] $State) {
	$findingsTag = if ($Receipt.HasFindings) { 'yes' } else { 'no' }
	$row = '{0} bridge-delivery {1} status={2} runId={3} skill={4}' -f $LineNumber, $ToolUseId, $Receipt.Status, $Receipt.RunId, $Receipt.Skill
	if ($null -ne $Receipt.Mode) { $row += " mode=$($Receipt.Mode)" }
	if ($Receipt.NoRetry) { $row += ' noretry' }
	$row += " findings=$findingsTag"
	$row

	if ($Receipt.Skill -cne 'next-plan-checkpoint-review') { return }
	$waitLine = $State.BridgeCallByToolUseId[$ToolUseId]
	if ($null -ne $waitLine) {
		$group = if ($null -ne $State.CurrentCheckpoint -and $waitLine -ge $State.CurrentCheckpoint.FirstLine) {
			$State.CurrentCheckpoint
		}
		else {
			$State.PreviousCheckpoint
		}
		$receiptOutKey = ConvertTo-NormalizedPathKey $Receipt.OutFile
		if ($null -ne $group -and
			$group.Kind -ceq 'bridged' -and
			(Test-NormalizedPathMatch $group.LaunchOutFile $receiptOutKey) -and
			$group.FirstLine -lt $waitLine -and
			$waitLine -ge $group.FinalDispatchLine) {
			$group.FinalDispatchLine = $waitLine
			$group.FinalToolUseId = $ToolUseId
			$group.CompletedHandoffLine = if ($Receipt.Status -ceq 'completed' -and $Receipt.HasFindings) { $LineNumber } else { $null }
		}
	}
	else {
		foreach ($candidate in @($State.CurrentCheckpoint, $State.PreviousCheckpoint)) {
			if ($null -ne $candidate -and $candidate.Kind -ceq 'bridged' -and $candidate.FinalToolUseId -ceq $ToolUseId) {
				if ($Receipt.Status -ceq 'completed' -and $Receipt.HasFindings) {
					$candidate.CompletedHandoffLine = $LineNumber
				}
				break
			}
		}
	}
}

function Get-NativeCheckpointGroup([string] $ToolUseId, [hashtable] $State) {
	foreach ($candidate in @($State.CurrentCheckpoint, $State.PreviousCheckpoint)) {
		if ($null -ne $candidate -and $candidate.Kind -ceq 'native' -and $candidate.FinalToolUseId -ceq $ToolUseId) {
			return $candidate
		}
	}
	return $null
}

function Set-NativeCheckpointHandoff([int] $LineNumber, [string] $ToolUseId, [hashtable] $State, [bool] $OnlyWhenUnset) {
	$group = Get-NativeCheckpointGroup $ToolUseId $State
	if ($null -ne $group -and (-not $OnlyWhenUnset -or $null -eq $group.CompletedHandoffLine)) {
		$group.CompletedHandoffLine = $LineNumber
	}
}

function Write-ToolCallProjection([int] $LineNumber, [string] $ToolName, [string] $ToolUseId, $InputObject, [Collections.Generic.List[object]] $Reads, [hashtable] $State) {
	$summarySource = if ($null -ne $InputObject) { $InputObject } else { @{} }
	$summary = ($summarySource | ConvertTo-Json -Compress -Depth 100) -replace '\s+', ' '
	'{0} use {1} {2}' -f $LineNumber, $ToolName, $summary.Substring(0, [Math]::Min(160, $summary.Length))

	$readPath = $null
	$isBoundedSmallLines = $false
	$shellCommand = $null
	$shellKind = $null
	if ($ToolName -ceq 'Read') {
		$candidate = Get-AgentTranscriptField $InputObject 'file_path'
		if ($candidate -is [string] -and -not [string]::IsNullOrWhiteSpace($candidate)) {
			$readPath = $candidate
			$limit = Get-PositiveIntegralNumber $InputObject 'limit'
			$offset = Get-PositiveIntegralNumber $InputObject 'offset'
			$pages = Get-AgentTranscriptField $InputObject 'pages'
			$isBoundedSmallLines = ($null -eq $pages -and $null -ne $limit -and $limit -ge 1 -and $limit -le $script:SmallReadMaxLines -and ($null -eq $offset -or $offset -ge 1))
		}
	}
	elseif ($ToolName -ceq 'read_file') {
		$candidate = Get-AgentTranscriptField $InputObject 'Path'
		if ($candidate -is [string] -and -not [string]::IsNullOrWhiteSpace($candidate)) {
			$readPath = $candidate
			$startLine = Get-PositiveIntegralNumber $InputObject 'StartLine'
			$endLine = Get-PositiveIntegralNumber $InputObject 'EndLine'
			$byteOffset = Get-AgentTranscriptField $InputObject 'ByteOffset'
			$hasByteOffset = $true
			if ($null -eq $byteOffset) {
				$hasByteOffset = $false
			}
			elseif ($byteOffset -is [int] -or $byteOffset -is [long] -or $byteOffset -is [double] -or $byteOffset -is [decimal]) {
				if ([double] $byteOffset -eq 0) { $hasByteOffset = $false }
			}
			$effectiveStart = if ($null -eq $startLine) { [double] 1 } else { $startLine }
			$isBoundedSmallLines = (-not $hasByteOffset -and $null -ne $endLine -and $endLine -ge 1 -and $effectiveStart -ge 1 -and $endLine -ge $effectiveStart -and (($endLine - $effectiveStart + 1) -le $script:SmallReadMaxLines))
		}
	}
	elseif ($ToolName -ceq 'Bash' -or $ToolName -ceq 'PowerShell') {
		$command = Get-AgentTranscriptField $InputObject 'command'
		if ($command -is [string]) {
			$shellCommand = $command
			$shellKind = if ($ToolName -ceq 'Bash') { 'bash' } else { 'powershell' }
			$readPath = Get-AllowedShellReadPath $command
		}
	}
	elseif ($ToolName -ceq 'run_command') {
		$command = Get-AgentTranscriptField $InputObject 'CommandLine'
		if ($command -is [string]) {
			$shellCommand = $command
			$shellKind = 'powershell'
			$readPath = Get-AllowedShellReadPath $command
		}
	}
	elseif ($ToolName -ceq 'Write' -or $ToolName -ceq 'write_file') {
		$pathField = if ($ToolName -ceq 'Write') { 'file_path' } else { 'Path' }
		$contentField = if ($ToolName -ceq 'Write') { 'content' } else { 'Content' }
		$writePath = Get-AgentTranscriptField $InputObject $pathField
		$writeContent = Get-AgentTranscriptField $InputObject $contentField
		if ($writePath -is [string] -and -not [string]::IsNullOrWhiteSpace($writePath)) {
			$normPath = ConvertTo-NormalizedPathKey $writePath
			if (-not [string]::IsNullOrEmpty($normPath)) {
				$State.LastWriteByPath[$normPath] = $LineNumber
				if ($writeContent -is [string] -and (Test-CheckpointReviewBrief $writeContent)) {
					if ($null -ne $State.PendingPromptWrite -and (Test-NormalizedPathMatch $State.PendingPromptWrite.PathKey $normPath)) {
						$State.PendingPromptWrite.LastLine = $LineNumber
					}
					else {
						Push-PendingDesktopCheckpoint $State
						$State.PendingPromptWrite = @{
							FirstLine = $LineNumber
							LastLine = $LineNumber
							PathKey = $normPath
						}
					}
				}
			}
		}
	}
	if ($readPath) {
		$readRecord = @{
			Index = $Reads.Count
			Line = $LineNumber
			Path = $readPath
			Key = (ConvertTo-CanonicalPathKey $readPath -AllowGitBashDrive ($shellKind -ceq 'bash'))
			IsBoundedSmallLines = $isBoundedSmallLines
			ResultChars = $null
			SpanIndex = -1
			IsSmallReadExempt = $null
		}
		$Reads.Add($readRecord)
		if ($null -eq $shellKind -and -not [string]::IsNullOrEmpty($ToolUseId)) {
			$State.ReadByToolUseId[$ToolUseId] = $readRecord
		}
	}

	if ($null -ne $shellCommand) {
		$bridgeCmd = Parse-BridgeCommand $shellCommand 0 $shellKind
		if ($null -ne $bridgeCmd) {
			$idLabel = if (-not [string]::IsNullOrEmpty($ToolUseId)) { $ToolUseId } else { 'unknown' }
			if ($bridgeCmd.Kind -ceq 'launch') {
				if (-not [string]::IsNullOrEmpty($ToolUseId)) { $State.BridgeCallByToolUseId[$ToolUseId] = $null }
				$promptFileKey = ConvertTo-NormalizedPathKey $bridgeCmd.PromptFile
				$isCheckpointLaunch = $bridgeCmd.Skill -ceq 'next-plan-checkpoint-review'
				$matchedPendingFirstLine = $null
				if ($isCheckpointLaunch -and $null -ne $State.PendingPromptWrite) {
					if (Test-NormalizedPathMatch $State.PendingPromptWrite.PathKey $promptFileKey) {
						$matchedPendingFirstLine = $State.PendingPromptWrite.FirstLine
						$State.PendingPromptWrite = $null
					}
					else {
						Push-PendingDesktopCheckpoint $State
					}
				}
				$isCheckpointAttach = $isCheckpointLaunch -and $bridgeCmd.NoRetry -and $null -ne $State.CurrentCheckpoint -and $State.CurrentCheckpoint.Kind -ceq 'bridged'
				$minPromptWriteLine = 0
				if ($isCheckpointLaunch) {
					$priorDistinct = if ($isCheckpointAttach) { $State.PreviousCheckpoint } else { $State.CurrentCheckpoint }
					if ($null -ne $priorDistinct) { $minPromptWriteLine = $priorDistinct.FinalDispatchLine }
				}
				$promptWriteLine = $null
				if (-not [string]::IsNullOrEmpty($promptFileKey)) {
					foreach ($entry in $State.LastWriteByPath.GetEnumerator()) {
						if ((Test-NormalizedPathMatch $entry.Key $promptFileKey) -and $entry.Value -gt $minPromptWriteLine) {
							if ($null -eq $promptWriteLine -or $entry.Value -gt $promptWriteLine) { $promptWriteLine = $entry.Value }
						}
					}
				}
				$launchRow = '{0} bridge-launch {1} skill={2}' -f $LineNumber, $idLabel, $bridgeCmd.Skill
				if (-not [string]::IsNullOrWhiteSpace($bridgeCmd.Mode)) { $launchRow += " mode=$($bridgeCmd.Mode)" }
				if ($bridgeCmd.NoRetry) { $launchRow += ' noretry' }
				if ($null -ne $promptWriteLine) { $launchRow += " prompt-write=$promptWriteLine" }
				$launchRow

				if ($isCheckpointLaunch) {
					$launchOutKey = ConvertTo-NormalizedPathKey $bridgeCmd.OutFile
					if ($isCheckpointAttach) {
						$State.CurrentCheckpoint.FinalDispatchLine = $LineNumber
						$State.CurrentCheckpoint.FinalToolUseId = $ToolUseId
						$State.CurrentCheckpoint.LaunchOutFile = $launchOutKey
						$State.CurrentCheckpoint.CompletedHandoffLine = $null
					}
					else {
						$firstLine = if ($null -ne $matchedPendingFirstLine) {
							$matchedPendingFirstLine
						}
						elseif ($null -ne $promptWriteLine) {
							$promptWriteLine
						}
						else {
							$LineNumber
						}
						Push-CheckpointGroup $State @{
							Kind = 'bridged'
							FirstLine = $firstLine
							FinalDispatchLine = $LineNumber
							FinalToolUseId = $ToolUseId
							LaunchOutFile = $launchOutKey
							CompletedHandoffLine = $null
						}
					}
				}
			}
			elseif ($bridgeCmd.Kind -ceq 'wait') {
				if (-not [string]::IsNullOrEmpty($ToolUseId)) { $State.BridgeCallByToolUseId[$ToolUseId] = $LineNumber }
				'{0} bridge-wait {1} runId={2}' -f $LineNumber, $idLabel, $bridgeCmd.RunId
			}
		}
	}

	if ($InputObject -isnot [Management.Automation.PSCustomObject]) { return }
	$isNativeAgentTool = $ToolName -ceq 'Agent' -or $ToolName -ceq 'invoke_subagent'
	$isNativeCheckpoint = $false
	# Brief paths come from the unescaped field value, never the serialized JSON above, so a token is delimited by
	# real whitespace, a real backtick, or the `,`/`;` a brief separates listed paths with, and loses the
	# sentence punctuation a brief may end it with; prose in those fields yields no token.
	$briefMissing = [Collections.Generic.List[string]]::new()
	$briefKeys = @(foreach ($field in $InputObject.PSObject.Properties) {
		if ($field.Value -isnot [string]) { continue }
		if ($isNativeAgentTool -and -not $isNativeCheckpoint -and (Test-CheckpointReviewBrief $field.Value)) {
			$isNativeCheckpoint = $true
		}
		$fieldLines = $field.Value -split '\r?\n'
		$firstText = $fieldLines | Where-Object { $_.Trim() -ne '' } | Select-Object -First 1
		$hasRole = $firstText -like 'Role:*'
		$pathLines = [Collections.Generic.List[string]]::new()
		$inPathBlock = $false
		$hasGoverningPaths = $false
		foreach ($fieldLine in $fieldLines) {
			if ($fieldLine -like 'Governing paths:*' -or $fieldLine -like 'Scope:*') {
				$inPathBlock = $true
				$pathLines.Add($fieldLine)
				if ($fieldLine -like 'Governing paths:*') { $hasGoverningPaths = $true }
			}
			elseif ($inPathBlock) {
				if ($fieldLine -match $script:BriefHeaderPattern) {
					$inPathBlock = $false
				}
				else {
					$pathLines.Add($fieldLine)
				}
			}
		}
		if (-not ($hasRole -and $hasGoverningPaths)) {
			# Two distinct brief labels separate a malformed brief from prose that merely carries one such line.
			$labelHits = @($script:BriefLabels | Where-Object { $label = $_; @($fieldLines | Where-Object { $_ -like "$label*" }).Count -gt 0 })
			if ($labelHits.Count -ge 2) {
				if (-not $hasRole) { $briefMissing.Add(('{0} brief-missing {1} Role:' -f $LineNumber, $field.Name)) }
				if (-not $hasGoverningPaths) { $briefMissing.Add(('{0} brief-missing {1} Governing paths:' -f $LineNumber, $field.Name)) }
			}
			continue
		}
		if ([string]::IsNullOrEmpty($State.WorktreeRoot)) {
			foreach ($fieldLine in $fieldLines) {
				if ($fieldLine -match '^\s*Baseline:(?<pre>.*?[\s,(|;])(?:worktree|(?:absolute\s+)?repo(?:sitory)?\s+toplevel)(?<sep>[:=])\s*(?<val>.+)$') {
					$pre = $Matches['pre']
					$sep = $Matches['sep']
					$val = $Matches['val'].Trim()
					if ($val -match '^(["''`])([^\r\n]+?)\1(?=\s*(?:[,|;).\s]|$))') {
						$rawRoot = $Matches[2]
					}
					else {
						$fieldSplitPattern = if ($sep -ceq '=') {
							'[,|;]\s*[A-Za-z][A-Za-z0-9 _-]*(?::(?![\\/])|=)'
						}
						else {
							'[,|;]\s*[A-Za-z][A-Za-z0-9 _-]*:(?![\\/])'
						}
						$parts = $val -split $fieldSplitPattern, 2
						$rawRoot = $parts[0].Trim()
						$openParens = ($pre -replace '[^(]', '').Length - ($pre -replace '[^)]', '').Length
						if ($openParens -gt 0) {
							if ($parts.Count -eq 1) {
								$rawRoot = $rawRoot -replace '\)\s*\.?\s*$', ''
							}
							elseif ($rawRoot.EndsWith(')')) {
								$tailCloses = ($parts[1] -replace '[^)]', '').Length - ($parts[1] -replace '[^(]', '').Length
								if ($tailCloses -lt $openParens) {
									$rawRoot = $rawRoot.Substring(0, $rawRoot.Length - 1)
								}
							}
						}
					}
					$wtKey = ConvertTo-CanonicalPathKey $rawRoot
					if (-not [string]::IsNullOrEmpty($wtKey) -and $wtKey -match '^(?:[a-z]:/|/)') {
						$State.WorktreeRoot = $wtKey
						break
					}
				}
			}
		}
		foreach ($token in (($pathLines -join ' ') -split '[\s`,;]+')) {
			$token = $token.TrimEnd('.', ')', '"', "'")
			if ($token -match '[/\\]') {
				$normToken = ConvertTo-CanonicalPathKey $token
				if (-not [string]::IsNullOrEmpty($normToken)) { $normToken }
			}
		}
	})
	if ($isNativeCheckpoint) {
		Push-PendingDesktopCheckpoint $State
		Push-CheckpointGroup $State @{
			Kind = 'native'
			FirstLine = $LineNumber
			FinalDispatchLine = $LineNumber
			FinalToolUseId = $ToolUseId
			LaunchOutFile = $null
			CompletedHandoffLine = $null
		}
	}
	foreach ($row in $briefMissing) { $row }
	if ($briefKeys.Count -gt 0) {
		foreach ($read in $Reads) {
			foreach ($briefKey in $briefKeys) {
				if (Test-PathKeyMatch $read.Key $briefKey $State.WorktreeRoot) {
					$State.CandidateMatches.Add(@{ BriefLine = $LineNumber; Read = $read })
					break
				}
			}
		}
	}
}

# Each entry tracks one `Read`/`read_file` record or allowlisted read-only shell command, its normalized path key,
# bounded-line flag, paired text result length, checkpoint span index, and cached small-read exemption verdict.
$reads = [Collections.Generic.List[object]]::new()
$state = @{
	LastWriteByPath = @{}
	BridgeCallByToolUseId = @{}
	ReadByToolUseId = @{}
	WorktreeRoot = $null
	Checkpoints = [Collections.Generic.List[object]]::new()
	CandidateMatches = [Collections.Generic.List[object]]::new()
	PreviousCheckpoint = $null
	CurrentCheckpoint = $null
	PendingPromptWrite = $null
}
$totalLines = 0
$sessionLabel = ''
$toolUses = $null
$measured = $null
if ($IncludeContextEnvelope) {
	$toolUses = @{}
	$measured = [Collections.Generic.List[object]]::new()
}

try {
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
		'missing' { Stop-ProjectionFailure 'Transcript file does not exist.' }
		'unsafe' { Stop-ProjectionFailure 'Transcript path is an unsafe reparse point.' }
		default { Stop-ProjectionFailure 'Transcript path could not be safely read.' }
	}
	if (-not (Test-Path -LiteralPath $transcript -PathType Leaf)) { Stop-ProjectionFailure 'Transcript file does not exist.' }

	if (Test-AgentAgyTranscript $transcript) {
		Read-AgentAgyTranscript $transcript | ForEach-Object {
			$record = $_
			$totalLines = $record.LineNumber
			if ($record.Type -ceq 'USER_INPUT' -or $record.Type -ceq 'SYSTEM_MESSAGE') {
				$row = '{0} {1} text len {2}' -f $record.LineNumber, $record.Type, $record.Content.Length
				if ($record.Type -ceq 'SYSTEM_MESSAGE' -and -not [string]::IsNullOrEmpty($record.DeliveryToolUseId)) {
					$row += " delivery=$($record.DeliveryToolUseId)"
				}
				$row
				if ($record.Type -ceq 'SYSTEM_MESSAGE' -and -not [string]::IsNullOrEmpty($record.DeliveryToolUseId)) {
					if ($IncludeContextEnvelope) {
						$measured.Add([pscustomobject]@{ ToolUseId = [string] $record.DeliveryToolUseId; Chars = $record.Content.Length })
					}
					if ($state.BridgeCallByToolUseId.ContainsKey($record.DeliveryToolUseId)) {
						$receiptLookup = Find-BridgeOuterReceipt $record.Content
						if ($receiptLookup.Kind -ceq 'v2') {
							Write-BridgeDeliveryProjection $record.LineNumber $record.DeliveryToolUseId $receiptLookup.Receipt $state
						}
					}
					elseif (-not $record.IsIdleNotice) {
						Set-NativeCheckpointHandoff $record.LineNumber $record.DeliveryToolUseId $state $false
					}
				}
			}
			elseif ($record.Type -ceq 'PLANNER_RESPONSE') {
				foreach ($tc in $record.ToolCalls) {
					Write-ToolCallProjection $record.LineNumber $tc.Name ([string] $tc.ToolUseId) $tc.Args $reads $state
					if ($IncludeContextEnvelope -and -not [string]::IsNullOrEmpty($tc.ToolUseId)) {
						$toolUses[$tc.ToolUseId] = @{ Name = $tc.Name; InputSummary = (Get-ContextInputSummary $tc.Args) }
					}
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
				if ($IncludeContextEnvelope -and $null -ne $record.PairedToolCall) {
					$measured.Add([pscustomobject]@{ ToolUseId = [string] $record.PairedToolCall.ToolUseId; Chars = $record.Content.Length })
				}
				if ($null -ne $record.PairedToolCall -and -not [string]::IsNullOrEmpty($record.PairedToolCall.ToolUseId)) {
					$pairedId = $record.PairedToolCall.ToolUseId
					if ($state.ReadByToolUseId.ContainsKey($pairedId)) {
						$state.ReadByToolUseId[$pairedId].ResultChars = if (-not $record.IsError -and $record.Content.Length -gt 0) {
							$record.Content.Length
						}
						else {
							$null
						}
					}
					if ($state.BridgeCallByToolUseId.ContainsKey($pairedId)) {
						$receiptLookup = Find-BridgeOuterReceipt $record.Content
						if ($receiptLookup.Kind -ceq 'v2') {
							Write-BridgeDeliveryProjection $record.LineNumber $pairedId $receiptLookup.Receipt $state
						}
						elseif ($receiptLookup.Kind -ceq 'none' -and $null -ne $record.PairedToolCall.LaunchedTaskId) {
							'{0} bridge-stub {1}' -f $record.LineNumber, $pairedId
						}
					}
					elseif (-not $record.IsError -and $null -eq $record.PairedToolCall.LaunchedSubagentId) {
						Set-NativeCheckpointHandoff $record.LineNumber $pairedId $state $true
					}
				}
			}
		}
	}
	else {
		$n = 0
		foreach ($line in [IO.File]::ReadLines($transcript)) {
			$n++
			if ([string]::IsNullOrWhiteSpace($line)) { continue }
			try {
				$record = ConvertFrom-Json -InputObject $line -NoEnumerate -ErrorAction Stop
			}
			catch {
				Stop-ProjectionFailure "Transcript line $n is not valid JSON."
			}
			if ($record -isnot [Management.Automation.PSCustomObject]) {
				Stop-ProjectionFailure "Transcript line $n is not a JSON object."
			}
			# Only these types carry the conversation; others, such as request copies, repeat it or carry none.
			if ($record.isSidechain -eq $true -or $record.type -notin @('assistant', 'user', 'attachment')) { continue }
			if ([string]::IsNullOrEmpty($state.WorktreeRoot)) {
				$cwd = Get-AgentTranscriptField $record 'cwd'
				if ($cwd -is [string] -and -not [string]::IsNullOrWhiteSpace($cwd)) {
					$cwdKey = ConvertTo-CanonicalPathKey $cwd
					if (-not [string]::IsNullOrEmpty($cwdKey) -and $cwdKey -match '^(?:[a-z]:/|/)') {
						$state.WorktreeRoot = $cwdKey
					}
				}
			}
			$content = $record.message.content
			if ($record.type -eq 'attachment') { $content = $record.attachment.prompt }
			if ($content -is [string]) {
				'{0} {1} text len {2}' -f $n, $record.type, $content.Length
				$isValidStringDelivery = $false
				if ($record.type -ceq 'user') {
					$isValidStringDelivery = [string](Get-AgentTranscriptField (Get-AgentTranscriptField $record 'origin') 'kind') -ceq 'task-notification'
				}
				elseif ($record.type -ceq 'attachment') {
					$isValidStringDelivery = [string](Get-AgentTranscriptField (Get-AgentTranscriptField $record 'attachment') 'type') -ceq 'queued_command'
				}
				if ($IncludeContextEnvelope) {
					if ($record.type -ceq 'attachment') {
						if ($isValidStringDelivery) {
							$queuedToolUseId = Get-ToolUseIdTag $content
							if (-not [string]::IsNullOrEmpty($queuedToolUseId)) {
								$measured.Add([pscustomobject]@{ ToolUseId = $queuedToolUseId; Chars = $content.Length })
							}
						}
					}
					elseif (($record.type -ceq 'assistant' -or $record.type -ceq 'user') -and
						[string](Get-AgentTranscriptField (Get-AgentTranscriptField $record 'origin') 'kind') -ceq 'task-notification') {
						$measured.Add([pscustomobject]@{ ToolUseId = (Get-ToolUseIdTag $content); Chars = (Measure-ContextResultChars $content) })
					}
				}
				if ($isValidStringDelivery -and $content -cmatch '<tool-use-id>([^<]+)</tool-use-id>') {
					$deliveryToolUseId = $Matches[1]
					if ($state.BridgeCallByToolUseId.ContainsKey($deliveryToolUseId)) {
						$receiptLookup = Find-BridgeOuterReceipt $content
						if ($receiptLookup.Kind -ceq 'v2') {
							Write-BridgeDeliveryProjection $n $deliveryToolUseId $receiptLookup.Receipt $state
						}
					}
					else {
						Set-NativeCheckpointHandoff $n $deliveryToolUseId $state $false
					}
				}
				continue
			}
			$toolResultCount = @($content | Where-Object { $_.type -eq 'tool_result' }).Count
			foreach ($element in $content) {
				if ($element.type -eq 'tool_use') {
					Write-ToolCallProjection $n ([string] $element.name) ([string] $element.id) $element.input $reads $state
					if ($IncludeContextEnvelope -and $record.type -ceq 'assistant' -and $content -is [object[]] -and [string](Get-AgentTranscriptField $element 'type') -ceq 'tool_use') {
						$toolUseId = [string](Get-AgentTranscriptField $element 'id')
						if (-not [string]::IsNullOrEmpty($toolUseId)) {
							$toolUses[$toolUseId] = @{ Name = [string](Get-AgentTranscriptField $element 'name'); InputSummary = (Get-ContextInputSummary (Get-AgentTranscriptField $element 'input')) }
						}
					}
				}
				elseif ($element.type -eq 'tool_result') {
					$result = '{0} result {1} len {2}' -f $n, $element.tool_use_id, ($element.content | ConvertTo-Json -Compress -Depth 100).Length
					if ($element.is_error -eq $true) { $result += ' error' }
					$result
					if ($IncludeContextEnvelope -and $record.type -ceq 'user' -and $content -is [object[]] -and [string](Get-AgentTranscriptField $element 'type') -ceq 'tool_result') {
						$measured.Add([pscustomobject]@{
							ToolUseId = [string](Get-AgentTranscriptField $element 'tool_use_id')
							Chars = (Measure-ContextResultChars (Get-AgentTranscriptField $element 'content'))
						})
					}
					$resultToolUseId = [string] $element.tool_use_id
					if (-not [string]::IsNullOrEmpty($resultToolUseId)) {
						if ($state.ReadByToolUseId.ContainsKey($resultToolUseId)) {
							$state.ReadByToolUseId[$resultToolUseId].ResultChars = if ($element.is_error -eq $true) {
								$null
							}
							else {
								Measure-ReadResultTextChars (Get-AgentTranscriptField $element 'content')
							}
						}
						$plainText = Get-ClaudeResultPlainText $element.content
						if ($state.BridgeCallByToolUseId.ContainsKey($resultToolUseId)) {
							$receiptLookup = Find-BridgeOuterReceipt $plainText
							if ($receiptLookup.Kind -ceq 'v2') {
								Write-BridgeDeliveryProjection $n $resultToolUseId $receiptLookup.Receipt $state
							}
							elseif ($receiptLookup.Kind -ceq 'none' -and $element.is_error -ne $true -and (Test-ClaudeBridgeLaunchStub $record $toolResultCount $plainText)) {
								'{0} bridge-stub {1}' -f $n, $resultToolUseId
							}
						}
						elseif ($element.is_error -ne $true) {
							$nativeGroup = Get-NativeCheckpointGroup $resultToolUseId $state
							if ($null -ne $nativeGroup -and $null -eq $nativeGroup.CompletedHandoffLine -and -not (Test-ClaudeNativeAgentLaunchStub $record $toolResultCount $plainText)) {
								$nativeGroup.CompletedHandoffLine = $n
							}
						}
					}
				}
				elseif ($record.type -eq 'assistant' -and $element.type -eq 'text') {
					$text = [string] $element.text
					$summary = $text -replace '\s+', ' '
					'{0} assistant-text len {1} {2}' -f $n, $text.Length, $summary.Substring(0, [Math]::Min(160, $summary.Length))
				}
			}
		}
		$totalLines = $n
	}

	Push-PendingDesktopCheckpoint $state
	if ($state.CandidateMatches.Count -gt 0) {
		foreach ($read in $reads) {
			$read.SpanIndex = Get-SpanIndex $read.Line $state.Checkpoints
		}
		foreach ($cand in $state.CandidateMatches) {
			$read = $cand.Read
			if (Test-PublicSubagentSkillPath $read.Key $state.WorktreeRoot) { continue }
			if ($null -eq $read.IsSmallReadExempt) {
				$isSmallBounded = ($read.IsBoundedSmallLines -and
					$null -ne $read.ResultChars -and
					$read.ResultChars -le $script:SmallReadMaxChars)
				$workflowRefKey = Get-MainWorkflowRefKey $read.Key $state.WorktreeRoot
				$isSmallWorkflowRef = ($null -ne $workflowRefKey -and
					$null -ne $read.ResultChars -and
					$read.ResultChars -gt 0 -and
					$read.ResultChars -le $script:WorkflowRefMaxChars)
				if ($read.SpanIndex -ge 0 -and ($isSmallBounded -or $isSmallWorkflowRef)) {
					$hasPeer = $false
					foreach ($other in $reads) {
						if ($other.SpanIndex -eq $read.SpanIndex -and
							$other.Index -ne $read.Index -and
							((Test-PathKeyMatch $read.Key $other.Key $state.WorktreeRoot) -or
							($null -ne $workflowRefKey -and $workflowRefKey -ceq (Get-MainWorkflowRefKey $other.Key $state.WorktreeRoot)))) {
							$hasPeer = $true
							break
						}
					}
					$read.IsSmallReadExempt = -not $hasPeer
				}
				else {
					$read.IsSmallReadExempt = $false
				}
			}
			if (-not $read.IsSmallReadExempt) {
				'{0} match {1} read-at {2}' -f $cand.BriefLine, $read.Path, $read.Line
			}
		}
	}
	if ($null -ne $state.CurrentCheckpoint -and $null -ne $state.PreviousCheckpoint) {
		$startLine = $state.PreviousCheckpoint.FinalDispatchLine + 1
		$endLine = $state.CurrentCheckpoint.FirstLine - 1
		$hasExclude = $null -ne $state.PreviousCheckpoint.CompletedHandoffLine
		$excludeLine = if ($hasExclude) { [string] $state.PreviousCheckpoint.CompletedHandoffLine } else { 'none' }
		$excludeId = if ($hasExclude -and -not [string]::IsNullOrEmpty($state.PreviousCheckpoint.FinalToolUseId)) { $state.PreviousCheckpoint.FinalToolUseId } else { 'none' }
	}
	elseif ($null -ne $state.CurrentCheckpoint) {
		$startLine = 1
		$endLine = $state.CurrentCheckpoint.FirstLine - 1
		$excludeLine = 'none'
		$excludeId = 'none'
	}
	else {
		$startLine = if ($totalLines -gt 0) { 1 } else { 0 }
		$endLine = $totalLines
		$excludeLine = 'none'
		$excludeId = 'none'
	}
	'checkpoint-span start={0} end={1} exclude={2} exclude-id={3}' -f $startLine, $endLine, $excludeLine, $excludeId
}
catch {
	if ($null -ne $_.Exception -and $null -ne $_.Exception.Data -and $_.Exception.Data.Contains('IsMalformedTranscriptLine')) {
		Stop-ProjectionFailure $_.Exception.Message
	}
	Stop-ProjectionFailure 'Transcript projection failed.'
}

if ($IncludeContextEnvelope) {
	try {
		$ranked = @($measured | Sort-Object -Property @{ Expression = 'Chars'; Descending = $true }, @{ Expression = 'ToolUseId'; Descending = $false })
		$overThresholdCount = 0
		foreach ($entry in $ranked) {
			if ($entry.Chars -ge $ThresholdChars) { $overThresholdCount++ }
			else { break }
		}
		$reportCount = [Math]::Min($ranked.Count, [Math]::Max($Top, [Math]::Min($script:BreachRowCap, $overThresholdCount)))
		$topResults = [Collections.Generic.List[object]]::new()
		for ($i = 0; $i -lt $reportCount; $i++) {
			$entry = $ranked[$i]
			$use = if (-not [string]::IsNullOrEmpty($entry.ToolUseId) -and $toolUses.ContainsKey($entry.ToolUseId)) { $toolUses[$entry.ToolUseId] } else { $null }
			$topResults.Add([ordered]@{
				rank = ($i + 1)
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
		'context-envelope {0}' -f ($envelope | ConvertTo-Json -Depth 20 -Compress)
	}
	catch {
		'context-envelope {"schemaVersion":"broken-engine-context-efficiency-error/v1","status":"error","code":"measure.failed","message":"Context-efficiency measurement failed."}'
	}
}
