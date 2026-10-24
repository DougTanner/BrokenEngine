# Claude Code SubagentStop hook, registered in .claude/settings.json. Sends an
# implementer or reviewer subagent back when its final message lacks the shared
# handoff form of .agents/references/subagent-handoff.md ## Handoffs: a line
# that is exactly Status: PASS, Status: NEEDS_ACTION, or Status: BLOCKED; a line
# starting with each of Findings:, Changed files:, Decisive checks:,
# Build required:, Evidence:, and Residuals:; and none of those field lines
# after the Residuals: line. Every other agent type is allowed, and so is a stop
# the input marks as a repeat (stop_hook_active), so a subagent is sent back at
# most once. The character cap is not checked, because skills declare different
# caps.
# Exit codes:
#   0 - allowed, including input that cannot be parsed and any unexpected
#       script error
#   2 - blocked; one stderr line names the missing or misplaced field

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$script:Utf8 = [Text.UTF8Encoding]::new($false)
$script:CheckedAgentTypes = @('implementer', 'reviewer')
$script:FieldNames = @('Findings', 'Changed files', 'Decisive checks', 'Build required', 'Evidence', 'Residuals')

function Write-Stderr([string] $Message) {
	$stderr = [Console]::OpenStandardError()
	$bytes = $script:Utf8.GetBytes($Message)
	$stderr.Write($bytes, 0, $bytes.Length)
	$stderr.Flush()
}

function Get-HandoffProblem([string[]] $Lines) {
	if (@($Lines | Where-Object { $_.TrimEnd() -cmatch '^Status: (PASS|NEEDS_ACTION|BLOCKED)$' }).Count -eq 0) {
		return 'has no line that is exactly Status: PASS, Status: NEEDS_ACTION, or Status: BLOCKED'
	}
	foreach ($field in $script:FieldNames) {
		if (@($Lines | Where-Object { $_.StartsWith("${field}:", [StringComparison]::Ordinal) }).Count -eq 0) {
			return "has no $field`: field line"
		}
	}
	$residualsIndex = 0
	while (-not $Lines[$residualsIndex].StartsWith('Residuals:', [StringComparison]::Ordinal)) { $residualsIndex++ }
	for ($j = $residualsIndex + 1; $j -lt $Lines.Count; $j++) {
		foreach ($field in $script:FieldNames) {
			if ($Lines[$j].StartsWith("${field}:", [StringComparison]::Ordinal)) { return "has its $field`: field line after Residuals:, which stays last" }
		}
	}
	return $null
}

try {
	$reader = [IO.StreamReader]::new([Console]::OpenStandardInput(), $script:Utf8)
	$hook = $reader.ReadToEnd() | ConvertFrom-Json
	if ($hook.stop_hook_active -or $script:CheckedAgentTypes -cnotcontains $hook.agent_type) { exit 0 }
	$problem = Get-HandoffProblem ([string]$hook.last_assistant_message -split "`r?`n")
	if ($problem) {
		Write-Stderr "Invoke-SubagentStopHook: your final message $problem; end with the shared handoff form in .agents/references/subagent-handoff.md ## Handoffs as your final answer.`n"
		exit 2
	}
}
catch {
	# Fail open: a hook bug must never trap a subagent; the prose rule still applies.
}
exit 0
