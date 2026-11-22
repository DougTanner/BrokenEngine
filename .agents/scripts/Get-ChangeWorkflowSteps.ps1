# Prints the dispatches main owes in the Change Workflow Steps 4-8 (Implement and propagate through Verify the
# acceptance table) once the implementation slices return. This script owns that dispatch table: which row the tier,
# the landing state, the runner assignment, and the changed files trigger, its role, scope, and order. The
# changed-file triggers themselves are owned by SessionChangeInventory.psm1 Get-RoutingTrigger, which this script
# calls in-process via Get-SessionChangeInventory.
#
# Parameters:
#   -RepositoryRoot, -Baseline - passed to the inventory unchanged.
#   -Tier                      - the change's risk tier.
#   -Landing                   - the stage lands in this session (never the inventory's own -Landing mode).
#   -HarnessChecks             - the execution card's Roles field names the runner.
#   -Head                      - inventory head commit in place of -IncludeUntracked.
#
# Output: plain text on stdout, one heading line per step that has rows, each followed by its rows, nothing else:
#   - <role> <skill>[ <mode>]: <scope>[; after <rows of the same step it waits for>]
# Steps run in heading order; within a step, rows run in parallel except that a row waits for its after clause.
#
# Exit codes:
#   0 - prints the rows
#   2 - the inventory failed or did not pass; one stderr line names its code and message
#   1 - unexpected error; one stderr line

[CmdletBinding()]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory)][string] $Baseline,
	[Parameter(Mandatory)][ValidateSet(1, 2, 3)][int] $Tier,
	[switch] $Landing,
	[switch] $HarnessChecks,
	[string] $Head
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'SessionChangeInventory.psm1') -Force

$script:Utf8 = [Text.UTF8Encoding]::new($false)

function Write-Stderr([string] $Message) {
	$stderr = [Console]::OpenStandardError()
	$bytes = $script:Utf8.GetBytes("Get-ChangeWorkflowSteps: $($Message.Trim() -replace '\s*\r?\n\s*', ' ')`n")
	$stderr.Write($bytes, 0, $bytes.Length)
	$stderr.Flush()
}

try {
	$inventoryParams = @{ RepositoryRoot = $RepositoryRoot; Baseline = $Baseline }
	if ([string]::IsNullOrWhiteSpace($Head)) { $inventoryParams.IncludeUntracked = $true }
	else { $inventoryParams.Head = $Head }
	$document = Get-SessionChangeInventory @inventoryParams
	if ($null -eq $document -or $document.status -cne 'pass') {
		$reason = if ($null -ne $document) { "$($document.code): $($document.message)" } else { 'no result returned' }
		Write-Stderr "the session change inventory did not pass: $reason"
		exit 2
	}

	$t = $document.triggers
	$combined = $Tier -eq 1 -and $t.coherenceReview -and -not $t.repoCodeReview -and -not $t.glslReview
	$carried = @(
		if ($t.validateSkill) { '/external-skill-creator validate mode' }
		if ($t.progressiveDisclosureReview) { '/progressive-disclosure-review' }
		if (-not $Landing) { '/verify-acceptance (read-only apart from the /coherence-review ## Handoff Temp/ file)' }
	)
	$coherenceScope = 'changed non-C++ artifacts'
	if ($combined -and $carried.Count -gt 0) { $coherenceScope += "; also carries: $($carried -join ', ')" }
	$repoReviewScope = 'changed C++'
	if ($t.overEngineeringReview) { $repoReviewScope += "; its implementation handoff is the validator's, or the finder's when the validator is skipped" }
	$compileScope = 'one dispatch per Build required handoff that names a target, none when no handoff names one'
	if ($HarnessChecks) { $compileScope += '; brief states the /agent-harness trigger of /compile ## Inputs' }
	$staticTail = if ([string]::IsNullOrWhiteSpace($Head)) { '-IncludeUntracked' } else { "-Head $Head" }
	$runnerScope = 'every /agent-harness check on the execution card, including those marked before the first edit, on the edited tree, ' +
		"cited in the form /agent-harness ## Inputs defines with no step sequence of the brief's own; main reads no check"
	if ($Landing) { $runnerScope += '; before /finalize-changes' }
	$vcxprojScope = (@($t.vcxprojCandidates) | ForEach-Object { "$($_.path) ($($_.reason))" }) -join ', '
	$afterValidator = [ordered]@{ '/over-engineering-review validator' = '/over-engineering-review validator, or the finder when the validator is skipped' }

	# The dispatch table, in output order. After maps each same-step predecessor's Name to its printed text; '*' is
	# every other row of the step. A predecessor whose row is absent is left out of the after clause.
	$implement = 'Implement and propagate'
	$preReview = 'Run targeted pre-review checks'
	$review = 'Review and resolve correctness'
	$cleanup = 'Apply the triggered cleanup'
	$acceptance = 'Verify the acceptance table'
	$table = @(
		@{ Step = $implement; When = $t.updateAffectedCode; Role = 'implementer'; Name = '/update-affected-code'; Mode = ''; Scope = "the slices' changed C++ and GLSL"; After = @{} }
		@{ Step = $preReview; When = $true; Role = 'main'; Name = ''; Mode = ''; After = @{}
			Scope = "pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1 -RepositoryRoot '$RepositoryRoot' -Baseline $Baseline $staticTail" }
		@{ Step = $preReview; When = $t.codeStyleReview; Role = 'mechanic'; Name = '/code-style-review'; Mode = ''; Scope = 'changed C++'; After = @{} }
		@{ Step = $preReview; When = $true; Role = 'builder'; Name = '/compile'; Mode = ''; Scope = $compileScope; After = @{ '/code-style-review' = '/code-style-review' } }
		@{ Step = $review; When = $t.overEngineeringReview; Role = 'fresh reviewer'; Name = '/over-engineering-review finder'; Mode = ''; Scope = 'changed cpp-class files'; After = @{} }
		@{ Step = $review; When = $t.overEngineeringReview; Role = 'fresh implementer'; Name = '/over-engineering-review validator'; Mode = ''
			Scope = "the finder's removal candidates; skipped when it returns none"; After = @{ '/over-engineering-review finder' = '/over-engineering-review finder' } }
		@{ Step = $review; When = $t.repoCodeReview; Role = 'fresh reviewer'; Name = '/repo-code-review'; Mode = ''; Scope = $repoReviewScope; After = $afterValidator }
		@{ Step = $review; When = $t.commentReview; Role = 'mechanic'; Name = '/comment-review'; Mode = ''; Scope = 'changed C++ and GLSL'; After = $afterValidator }
		@{ Step = $review; When = $t.glslReview; Role = 'fresh reviewer'; Name = '/glsl-review'; Mode = ''; Scope = 'changed shaders'; After = @{} }
		@{ Step = $review; When = $t.coherenceReview; Role = 'fresh reviewer'; Name = '/coherence-review'; Mode = $(if ($combined) { 'combined mode' } else { 'coherence mode' })
			Scope = $coherenceScope; After = @{} }
		@{ Step = $review; When = $Tier -eq 3; Role = 'reviewer'; Name = '/adversarial-review'; Mode = ''; Scope = 'every changed artifact type'; After = '*' }
		@{ Step = $cleanup; When = $t.updateVcxproj; Role = 'mechanic'; Name = '/update-vcxproj'; Mode = ''; Scope = $vcxprojScope; After = @{} }
		@{ Step = $cleanup; When = $t.validateSkill -and -not $combined; Role = 'fresh reviewer'; Name = '/external-skill-creator'; Mode = 'validate mode'
			Scope = 'changed .agents/skills/*/ packages'; After = @{} }
		@{ Step = $cleanup; When = $t.updateClaudeDocs; Role = 'implementer'; Name = '/update-claude-docs'; Mode = ''; Scope = 'changed C++ and GLSL'; After = @{} }
		@{ Step = $cleanup; When = $t.progressiveDisclosureReview -and -not $combined; Role = 'fresh reviewer'; Name = '/progressive-disclosure-review'; Mode = ''
			Scope = 'changed instruction docs'; After = @{ '/update-claude-docs' = '/update-claude-docs' } }
		@{ Step = $acceptance; When = [bool] $HarnessChecks; Role = 'runner'; Name = '/agent-harness'; Mode = ''; Scope = $runnerScope; After = @{} }
		@{ Step = $acceptance; When = -not $Landing -and -not $combined; Role = 'fresh read-only reviewer'; Name = '/verify-acceptance'; Mode = ''
			Scope = "this stage's acceptance table"; After = @{ '/agent-harness' = '/agent-harness' } }
	)

	$rows = @($table | Where-Object { $_.When })
	$lines = [Collections.Generic.List[string]]::new()
	$step = ''
	foreach ($row in $rows) {
		if ($row.Step -cne $step) {
			$step = $row.Step
			$lines.Add($step)
		}
		$stepNames = @($rows | Where-Object { $_.Step -ceq $row.Step } | ForEach-Object { $_.Name })
		$waits = @(if ($row.After -ceq '*') {
			$stepNames | Where-Object { $_ -cne $row.Name }
		} else {
			$row.After.Keys | Where-Object { $stepNames -ccontains $_ } | ForEach-Object { $row.After[$_] }
		})
		$label = (@($row.Role, $row.Name, $row.Mode) | Where-Object { $_ }) -join ' '
		$line = "- ${label}: $($row.Scope)"
		if ($waits.Count -gt 0) { $line += "; after $($waits -join ', ')" }
		$lines.Add($line)
	}

	# Raw bytes, the way Write-SessionWorkflowContext.ps1 writes its part, so the text reaches main unchanged.
	$stdout = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes(($lines -join "`n") + "`n")
	$stdout.Write($bytes, 0, $bytes.Length)
	$stdout.Flush()
	exit 0
}
catch {
	Write-Stderr "$($_.Exception.Message)"
	exit 1
}
