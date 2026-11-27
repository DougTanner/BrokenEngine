# Prints the dispatches main owes in the Change Workflow Steps 4-8 (Implement and propagate through Verify the
# acceptance table) once the implementation slices return, and drives their review rounds. This script owns that
# dispatch table: which row the tier, the landing state, the runner assignment, and the changed files trigger, its
# role, scope, and order. The changed-file triggers themselves are owned by SessionChangeInventory.psm1
# Get-RoutingTrigger, which this script calls in-process via Get-SessionChangeInventory; that module's
# Get-ReviewRoundState reads the rounds, and ReviewRounds.psm1 writes them.
#
# Review rounds. A round is one snapshot commit, the review rows printed for it, and, once main closes it, one close
# commit on that snapshot's tree with the snapshot as parent, so a printed Head stays reachable. A snapshot is HEAD's
# tree plus every path git status reports (both sides of a rename) staged in a temporary index, so the session branch,
# the real index, and the working tree are never written and a sparse-checkout path keeps HEAD's blob; its parent is
# the tip of refs/worktree/review-rounds when the chain reads, else HEAD. Every freeze snapshots the Coretide clone
# first (when it exists), chained under the clone's own refs/worktree/review-rounds (parent: that ref's tip when it
# resolves, else the clone's HEAD), then the engine, and advances each ref by compare-and-swap, never through a
# symbolic ref, from the value the round was planned from (the zero OID when absent), so a concurrent advance fails
# it. One chain per repository spans every round the worktree runs. Messages, one form each,
# store no scope, path, or printed row (lines after the subject follow one blank line):
#   snapshot: "Review round <n>"; "Round: <n>", "Baseline: <session baseline>",
#     "Phase: code|words|combined|acceptance|sweep", "Kind: ordinary|check", "Tier: <1|2|3>" (absent on a sweep round),
#     one "Owes: <key>" per key the round covers (each printed review key and, in a combined round, each check its row
#     carries), and, when the clone exists, "Coretide-Snapshot: <sha>" and "Coretide-Baseline: <its merge base with
#     upstream>". The Coretide snapshot's message is "Review round <n>" alone.
#   close: "Review round <n> closed"; "Closes: <n>", one "Result: <key> <severity>" per result (a combined round's
#     carried keys take its /coherence-review result), "Next: repeat|check|done".
#   A sweep round (Invoke-Sweep.ps1 -ReviewRound) is a snapshot with the sweep baseline, "Phase: sweep",
#   "Kind: ordinary", and no Tier or Owes line, closed at once with "Next: done" and no Result line, so it is never
#   open.
# Reading rule: walking first parents from the engine ref tip, a snapshot with no close child is open. The chain does
# not read when the ref is absent or symbolic, a walked commit is missing or its message does not parse, a round other
# than the tip is open, a close's parent is not the snapshot of the round it names, a close's tree differs from that
# snapshot's, its results miss an Owes key or hold another key than /adversarial-review, or its Next is not the one
# -CloseRound gives, a sweep round is not in its one form above, round numbers do not descend by one to 1, or a
# Coretide-Snapshot does not resolve in the clone.
# Every reader treats a chain that does not read as no reviewed
# round (a default run prints a first round; landing is refused until one closes), and it is never repaired.
#
# Review keys: code /repo-code-review, /glsl-review; words /comment-review, /progressive-disclosure-review; neutral
# /coherence-review, /external-skill-creator (validate mode), /adversarial-review (Tier 3 with any other key's
# trigger); acceptance /verify-acceptance. Each key k has a since point S_k: the snapshot of the last round whose close
# gave k a result below High, where a sweep round counts for every key; else the session baseline. D(S_k) runs from
# S_k to the new snapshot (to the working tree in a default run's preview) over the session paths, holding a session
# path that was not one at S_k even with S_k's bytes, and a proven comment-text-only C++ or GLSL edit owes no code key
# (SessionChangeInventory.psm1 -Since). k is owed when its Get-RoutingTrigger trigger fires on D(S_k) or its latest
# result is High or Critical (main's added /adversarial-review included), and, for every key but /verify-acceptance,
# its trigger (for /adversarial-review, any other key's, at any tier) also fires on the session diff, so a review with
# nothing of its file types left in the session change is never owed. With L the last round, in order:
#   1. L open: a default run reprints L's rows and its close row; -StartRound exits 2.
#   2. A code key owed: phase code, printing the owed code and neutral keys. No words key prints while a code key is
#      owed (words follow code); a withheld key keeps its S_k.
#   3. Else a words key owed: phase words, printing the owed words and neutral keys.
#   4. Else a neutral key owed: phase code.
#   5. Else acceptance, without -Landing, when /verify-acceptance is owed or its S_k is not L's snapshot.
#   6. Else the loop is done.
# Kind is check when L closed check, else ordinary. At Tier 1 a rule 2-4 round whose keys hold /coherence-review and
# none of /repo-code-review, /glsl-review, /comment-review is phase combined: one /coherence-review combined mode row,
# key /coherence-review, carrying the owed /external-skill-creator and /progressive-disclosure-review and, without
# -Landing, /verify-acceptance, all covered by its one result, beside an owed /adversarial-review. Every review row
# prints its first-pass scope: an owed review re-reads the whole session change for its file types; the rules decide
# only which keys are owed. Test-ReviewRoundCoverage refuses a landing candidate for which these rules, at the tier the
# last non-sweep round recorded, owe a round, so a tier raised after the last round takes effect there at the next
# -StartRound.
#
# Parameters:
#   Default and -StartRound: -RepositoryRoot, -Baseline (passed to the inventory), -Tier (the change's risk tier),
#     -Landing (the stage lands in this session; never the inventory's own -Landing mode), -HarnessChecks (latest
#     card-emitting planner handoff reports Harness checks: yes: card Roles names runner and at least one non-deferred
#     /agent-harness check remains on the card).
#     A default run writes no object or ref. With L open it prints L's rows; with a round owed it prints the prepare
#     rows, each When evaluated on D(B) (/update-affected-code only when L is absent or closed done, /update-claude-docs
#     only before a words round), then the -StartRound row after every printed row except /agent-harness, which it
#     also waits for when the round runs /verify-acceptance.
#     -StartRound freezes the round and prints its review rows and its close row, or, when the frozen diffs owe none,
#     the done output with no ref moved.
#   -CloseRound -RepositoryRoot -Head <the open round's snapshot> -Results '<key>=<severity>,...': one value per
#     printed key, the highest severity among the findings main accepted from that row after regrading, or none, from
#     a PASS or NEEDS_ACTION handoff of its review at that Head; /adversarial-review may be added at any tier and then
#     counts. Severity is none|Low|Medium|High|Critical. The round's highest decides for every accepted finding of the
#     round: Next is repeat on Critical or High, check on Medium or Low in an ordinary round, else done. It writes the
#     close commit and advances the ref from the snapshot to it by compare-and-swap.
#
# Output: plain text on stdout, a heading line before each run of one step's rows, each row one line:
#   - <role> <skill>[ <mode>]: <scope>[; after <active rows it waits for>][; Head <sha>[, Coretide head <sha>]]
# Review rows carry the Head clause and run in parallel, except /adversarial-review, which waits for the round's other
# review rows; the round's close row waits for every review row. A done loop prints the static-checks, /compile, and
# /agent-harness rows and "Review rounds: done at round <n>" ("Review rounds: none owed" with no round).
# -CloseRound prints "round <n> closed: <next> — <instruction>", plus the /external-diagnose-bug row when the round is
# the third consecutive one whose highest result is Critical or High; a default run before the next round repeats it.
# Headings group rows by workflow stage without adding an inter-step barrier; rows run in parallel across steps once
# every prerequisite in their after clause is satisfied, and absent predecessors are omitted.
#
# Exit codes:
#   0 - prints the rows
#   2 - an inventory did not pass, -StartRound found an open round, a -CloseRound refusal, or a failed
#       compare-and-swap; one stderr line names it
#   1 - unexpected error; one stderr line

[CmdletBinding(DefaultParameterSetName = 'Rows')]
param(
	[Parameter(Mandatory)][string] $RepositoryRoot,
	[Parameter(Mandatory, ParameterSetName = 'Rows')][Parameter(Mandatory, ParameterSetName = 'Start')][string] $Baseline,
	[Parameter(Mandatory, ParameterSetName = 'Rows')][Parameter(Mandatory, ParameterSetName = 'Start')][ValidateSet(1, 2, 3)][int] $Tier,
	[Parameter(ParameterSetName = 'Rows')][Parameter(ParameterSetName = 'Start')][switch] $Landing,
	[Parameter(ParameterSetName = 'Rows')][Parameter(ParameterSetName = 'Start')][switch] $HarnessChecks,
	[Parameter(Mandatory, ParameterSetName = 'Start')][switch] $StartRound,
	[Parameter(Mandatory, ParameterSetName = 'Close')][switch] $CloseRound,
	[Parameter(Mandatory, ParameterSetName = 'Close')][string] $Head,
	[Parameter(Mandatory, ParameterSetName = 'Close')][string] $Results
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'SessionChangeInventory.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'ReviewRounds.psm1') -Force

$script:Utf8 = [Text.UTF8Encoding]::new($false)

function Write-Stderr([string] $Message) {
	$stderr = [Console]::OpenStandardError()
	$bytes = $script:Utf8.GetBytes("Get-ChangeWorkflowSteps: $($Message.Trim() -replace '\s*\r?\n\s*', ' ')`n")
	$stderr.Write($bytes, 0, $bytes.Length)
	$stderr.Flush()
}

function Exit-Refused([string] $Message) {
	Write-Stderr $Message
	exit 2
}

# Raw bytes, the way Write-SessionWorkflowContext.ps1 writes its part, so the text reaches main unchanged.
function Exit-Printed([string[]] $Lines) {
	$stdout = [Console]::OpenStandardOutput()
	$bytes = $script:Utf8.GetBytes(($Lines -join "`n") + "`n")
	$stdout.Write($bytes, 0, $bytes.Length)
	$stdout.Flush()
	exit 0
}

$implement = 'Implement and propagate'
$preReview = 'Run targeted pre-review checks'
$review = 'Review and resolve correctness'
$cleanup = 'Apply the triggered cleanup'
$acceptance = 'Verify the acceptance table'
$afterLabels = @{
	'/over-engineering-review validator' = '/over-engineering-review validator, or the finder when the validator is skipped'
	'/external-skill-creator' = '/external-skill-creator validate mode'
}
# The review rows in print order: step, role, mode, and the scope a first pass from the session baseline prints.
$reviewRows = [ordered]@{
	'/repo-code-review' = @{ Step = $review; Role = 'fresh reviewer'; Mode = ''; First = 'changed C++' }
	'/comment-review' = @{ Step = $review; Role = 'mechanic'; Mode = ''; First = 'changed C++ and GLSL' }
	'/glsl-review' = @{ Step = $review; Role = 'fresh reviewer'; Mode = ''; First = 'changed shaders' }
	'/coherence-review' = @{ Step = $review; Role = 'fresh reviewer'; Mode = 'coherence mode'; First = 'changed non-C++ artifacts' }
	'/adversarial-review' = @{ Step = $review; Role = 'reviewer'; Mode = ''; First = 'every changed artifact type' }
	'/external-skill-creator' = @{ Step = $cleanup; Role = 'fresh reviewer'; Mode = 'validate mode'; First = 'changed .agents/skills/*/ packages' }
	'/progressive-disclosure-review' = @{ Step = $cleanup; Role = 'fresh reviewer'; Mode = ''; First = 'changed instruction docs' }
	'/verify-acceptance' = @{ Step = $acceptance; Role = 'fresh read-only reviewer'; Mode = ''; First = "this stage's acceptance table" }
}
$carriedLabels = @{
	'/external-skill-creator' = '/external-skill-creator validate mode'
	'/progressive-disclosure-review' = '/progressive-disclosure-review'
	'/verify-acceptance' = '/verify-acceptance (read-only apart from the /coherence-review ## Handoff Temp/ file)'
}

# Renders the active rows of a table in its order. Each row's dependency key is Name. After holds predecessor
# keys or selectors: 'review' expands to active Review and resolve correctness rows (falling back to
# Invoke-StaticChecks.ps1 and /compile when none matches), and '*' expands to every other active row; absent
# predecessors are omitted. A Final row (main's -StartRound or -CloseRound call) is never a 'review' predecessor.
# Tail is appended after the after clause.
function Format-Rows([object[]] $Table) {
	$rows = @($Table | Where-Object { $_.When })
	$activeKeys = [Collections.Generic.List[string]]::new()
	$activeKeySet = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
	$activeReviewKeys = [Collections.Generic.List[string]]::new()
	foreach ($row in $rows) {
		$key = $row.Name
		$activeKeys.Add($key)
		[void] $activeKeySet.Add($key)
		if ($row.Step -ceq $review -and -not $row.ContainsKey('Final')) { $activeReviewKeys.Add($key) }
	}
	$resolve = {
		param([string] $RowKey, [string[]] $Candidates)
		$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
		return @($Candidates | Where-Object { $_ -cne $RowKey -and $activeKeySet.Contains($_) -and $seen.Add($_) })
	}
	$lines = [Collections.Generic.List[string]]::new()
	$step = ''
	foreach ($row in $rows) {
		if ($row.Step -cne $step) {
			$step = $row.Step
			$lines.Add($step)
		}
		$rowKey = $row.Name
		$candidates = @(foreach ($dep in $row.After) {
			if ($dep -ceq '*') { $activeKeys }
			elseif ($dep -ceq 'review') { $activeReviewKeys }
			else { $dep }
		})
		$resolvedKeys = @(& $resolve $rowKey $candidates)
		if ($resolvedKeys.Count -eq 0 -and ($row.After -ccontains 'review')) {
			$resolvedKeys = @(& $resolve $rowKey @('Invoke-StaticChecks.ps1', '/compile'))
		}
		$waits = @($resolvedKeys | ForEach-Object {
			if ($afterLabels.ContainsKey($_)) { $afterLabels[$_] } else { $_ }
		})
		$label = (@($row.Role, $row.Name, $row.Mode) | Where-Object { $_ }) -join ' '
		$line = "- ${label}: $($row.Scope)"
		if ($waits.Count -gt 0) { $line += "; after $($waits -join ', ')" }
		if ($row.ContainsKey('Tail')) { $line += $row.Tail }
		$lines.Add($line)
	}
	return , $lines
}

# The review rows of a round and its close row, each review row naming the snapshot it reads.
function Get-RoundLines([object] $Plan, [string] $Snapshot, [string] $CoretideSnapshot) {
	$tail = "; Head $Snapshot"
	if (-not [string]::IsNullOrEmpty($CoretideSnapshot)) { $tail += ", Coretide head $CoretideSnapshot" }
	$printed = @($reviewRows.Keys | Where-Object { $Plan.Keys -ccontains $_ })
	$table = [Collections.Generic.List[object]]::new()
	foreach ($key in $printed) {
		$row = $reviewRows[$key]
		$scope = $row.First
		$mode = $row.Mode
		if ($key -ceq '/repo-code-review' -and $Plan.OverEngineering) { $scope += "; its implementation handoff is the validator's, or the finder's when the validator is skipped" }
		if ($key -ceq '/coherence-review' -and $Plan.Phase -ceq 'combined') {
			$mode = 'combined mode'
			if ($Plan.Carried.Count -gt 0) { $scope += "; also carries: $((@($Plan.Carried) | ForEach-Object { $carriedLabels[$_] }) -join ', ')" }
		}
		$after = if ($key -ceq '/adversarial-review') { [string[]] @($printed | Where-Object { $_ -cne $key }) } else { [string[]] @() }
		$table.Add(@{ Step = $row.Step; When = $true; Role = $row.Role; Name = $key; Mode = $mode; Scope = $scope; After = $after; Tail = $tail })
	}
	$template = (@($printed) | ForEach-Object { "$_=<severity>" }) -join ','
	$table.Add(@{ Step = $review; When = $true; Role = 'main'; Name = '-CloseRound'; Mode = ''; After = [string[]] $printed; Final = $true
		Scope = "pwsh -NoProfile -NonInteractive -File .agents/scripts/Get-ChangeWorkflowSteps.ps1 -RepositoryRoot '$RepositoryRoot' -CloseRound -Head $Snapshot -Results '$template'" })
	return Format-Rows ([object[]] $table.ToArray())
}

# The rows of a finished loop: the final static checks, builds, and harness checks, and the done line.
function Get-DoneLines([object] $State) {
	$lines = Format-Rows @(
		@{ Step = $preReview; When = $true; Role = 'main'; Name = 'Invoke-StaticChecks.ps1'; Mode = ''; Scope = '-IncludeUntracked'; After = @('/compile') }
		@{ Step = $preReview; When = $true; Role = 'builder'; Name = '/compile'; Mode = ''; Scope = $compileScope; After = @() }
		@{ Step = $acceptance; When = [bool] $HarnessChecks; Role = 'runner'; Name = '/agent-harness'; Mode = ''; Scope = $runnerScope; After = @('/compile') }
	)
	$lines.Add($(if ($null -ne $State.Last) { "Review rounds: done at round $($State.Last.Number)" } else { 'Review rounds: none owed' }))
	return , $lines
}

function Get-DiagnosisScope([int] $Number) {
	return "why rounds $($Number - 2) to $Number each had an accepted Critical or High; before the next /resolve-findings"
}

try {
	$root = [IO.Path]::GetFullPath($RepositoryRoot).TrimEnd('\', '/')

	if ($CloseRound) {
		$state = Get-ReviewRoundState -RepositoryRoot $root -ChainOnly
		if (-not $state.Readable -or -not $state.Open) { Exit-Refused 'no review round is open: the chain does not read or its tip round is closed.' }
		$round = $state.Last
		$headSha = (& git -C $root rev-parse --verify --quiet "$Head^{commit}" 2>$null)
		if ($LASTEXITCODE -ne 0 -or "$headSha".Trim() -cne $round.Snapshot) { Exit-Refused "-Head '$Head' is not round $($round.Number)'s snapshot $($round.Snapshot)." }
		$parsed = [ordered]@{}
		$printedKeys = @($round.Owes | Where-Object { $round.Carried -cnotcontains $_ })
		foreach ($item in ($Results -split ',')) {
			$pair = [regex]::Match($item.Trim(), '^([^=\s]+)=([^=\s]+)$')
			if (-not $pair.Success) { Exit-Refused "malformed -Results entry '$($item.Trim())'; use <key>=<severity>." }
			$key = $pair.Groups[1].Value
			$value = $pair.Groups[2].Value
			if ($value -cnotin @('none', 'Low', 'Medium', 'High', 'Critical')) { Exit-Refused "result '$value' for $key is outside none|Low|Medium|High|Critical." }
			if ($parsed.Contains($key)) { Exit-Refused "$key has more than one result." }
			if ($printedKeys -cnotcontains $key -and $key -cne '/adversarial-review') { Exit-Refused "round $($round.Number) printed no $key row." }
			$parsed[$key] = $value
		}
		$missing = @($printedKeys | Where-Object { -not $parsed.Contains($_) })
		if ($missing.Count -gt 0) { Exit-Refused "round $($round.Number) has no result for $($missing -join ', ')." }
		foreach ($key in $round.Carried) { $parsed[$key] = $parsed['/coherence-review'] }
		$rank = @{ none = 0; Low = 1; Medium = 2; High = 3; Critical = 4 }
		$ordered = @(@($round.Owes) + @($parsed.Keys | Where-Object { $round.Owes -cnotcontains $_ }) | ForEach-Object { [pscustomobject] @{ Key = $_; Severity = $parsed[$_] } })
		$highest = 'none'
		foreach ($result in $ordered) { if ($rank[$result.Severity] -gt $rank[$highest]) { $highest = $result.Severity } }
		$next = if ($highest -cin @('High', 'Critical')) { 'repeat' } elseif ($round.Kind -ceq 'ordinary' -and $highest -cin @('Medium', 'Low')) { 'check' } else { 'done' }
		$tree = (& git -C $root rev-parse --verify "$($round.Snapshot)^{tree}").Trim()
		if ($LASTEXITCODE -ne 0) { throw "could not read the tree of $($round.Snapshot)." }
		$close = New-ReviewRoundCommit $root $tree $round.Snapshot (Format-ReviewRoundCloseMessage $round.Number ([object[]] $ordered) $next)
		if (-not (Update-ReviewRoundRef $root $close $round.Snapshot)) { Exit-Refused 'refs/worktree/review-rounds moved during the close; rerun the step script.' }
		$instruction = switch ($next) {
			'repeat' { 'send every accepted finding of this round, all four levels, to /resolve-findings, then rerun the step script' }
			'check' { 'send every accepted finding to /resolve-findings once, then rerun the step script for the check pass' }
			default {
				if ($round.Kind -ceq 'check' -and $highest -ceq 'Medium') { 'file the accepted Medium findings through /create-follow-up-plans, not the Leftovers rule; accepted Low findings are neither fixed nor filed; then rerun the step script' }
				elseif ($round.Kind -ceq 'check' -and $highest -ceq 'Low') { 'accepted Low findings are neither fixed nor filed; then rerun the step script' }
				else { 'rerun the step script' }
			}
		}
		$lines = [Collections.Generic.List[string]]::new()
		$lines.Add("round $($round.Number) closed: $next — $instruction")
		if ((Get-ReviewRoundState -RepositoryRoot $root -ChainOnly).ConsecutiveHigh -eq 3) { $lines.Add("- implementer /external-diagnose-bug: $(Get-DiagnosisScope $round.Number)") }
		Exit-Printed ([string[]] $lines.ToArray())
	}

	$compileScope = 'one dispatch per Build required handoff that names a target, none when no handoff names one'
	if ($HarnessChecks) {
		$compileScope += '; brief states the /agent-harness trigger of /compile ## Inputs — reuse only a latest successful build handoff ' +
			"already covering the scenario's project, executables, and configuration per /agent-harness ## Inputs that still applies to " +
			'the current edited inputs and resolved Local vs Shared runtime-data mode ' +
			'(.agents/skills/compile/references/runtime-data-mode.md ## Mode selection, never a stale pre-edit build), ' +
			'dispatching a build when none exists even when no handoff names one'
	}
	$runnerScope = 'every non-deferred /agent-harness check on the execution card, including those marked before the first edit, on the edited tree, ' +
		"cited in the form /agent-harness ## Inputs defines with no step sequence of the brief's own; main reads no check"
	if ($Landing) { $runnerScope += '; before /finalize-changes' }

	if ($StartRound) {
		# One read of the ref: the round planned from it and the compare-and-swap from its Tip.
		$chain = Get-ReviewRoundState -RepositoryRoot $root -ChainOnly
		if ($chain.Open) { Exit-Refused "round $($chain.Last.Number) is open; close it with the -CloseRound row a default run prints." }
		$engineOld = $chain.Tip
		$parent = $null
		if ($chain.Readable) { $parent = $chain.Last.Close.Commit }
		else {
			$parent = "$(& git -C $root rev-parse --verify 'HEAD^{commit}')".Trim()
			if ($LASTEXITCODE -ne 0) { throw 'could not resolve HEAD.' }
		}
		$coretide = New-ReviewRoundCoretideSnapshot $root $chain.Number
		$tree = New-ReviewRoundTree $root
		# The diffs are taken against the frozen tree before its message, which names the owed keys, is final.
		$provisional = New-ReviewRoundCommit $root $tree $parent (Format-ReviewRoundSnapshotMessage $chain.Number $Baseline 'code' 'ordinary' $Tier @() $coretide)
		$state = Get-ReviewRoundState -RepositoryRoot $root -Chain $chain -Baseline $Baseline -Head $provisional -Tier $Tier -Landing:$Landing
		if ($null -ne $state.Failure) { Exit-Refused $state.Failure }
		if ($state.Plan.Done) { Exit-Printed ([string[]] (Get-DoneLines $state).ToArray()) }
		$plan = $state.Plan
		# The inventory's resolved baseline, the lowercase form the chain reader accepts.
		$snapshot = New-ReviewRoundCommit $root $tree $parent (Format-ReviewRoundSnapshotMessage $chain.Number $plan.DB.baselineSha $plan.Phase $plan.Kind $Tier ([string[]] (@($plan.Keys) + @($plan.Carried))) $coretide)
		if ($null -ne $coretide -and -not (Update-ReviewRoundRef $coretide.Root $coretide.Commit $coretide.Old)) { Exit-Refused "the Coretide clone's refs/worktree/review-rounds moved during the freeze; rerun -StartRound." }
		if (-not (Update-ReviewRoundRef $root $snapshot $engineOld)) { Exit-Refused 'refs/worktree/review-rounds moved during the freeze; rerun -StartRound.' }
		$coretideSnapshot = if ($null -ne $coretide) { $coretide.Commit } else { '' }
		Exit-Printed ([string[]] (Get-RoundLines $plan $snapshot $coretideSnapshot).ToArray())
	}

	$state = Get-ReviewRoundState -RepositoryRoot $root -Baseline $Baseline -Tier $Tier -Landing:$Landing
	if ($null -ne $state.Failure) { Exit-Refused $state.Failure }
	if ($state.Open) { Exit-Printed ([string[]] (Get-RoundLines $state.Plan $state.Last.Snapshot $state.Last.CoretideSnapshot).ToArray()) }
	if ($state.Plan.Done) { Exit-Printed ([string[]] (Get-DoneLines $state).ToArray()) }

	$plan = $state.Plan
	$t = $plan.DB.triggers
	$afterDone = $null -eq $state.Last -or $state.Last.Close.Next -ceq 'done'
	$oeScope = 'changed cpp-class files'
	if ($null -ne $state.Last) { $oeScope = "session-changed C++ in $((@($plan.DB.entries | Where-Object { $_.class -ceq 'cpp' } | ForEach-Object { $_.path })) -join ', ')" }
	$vcxprojScope = (@($t.vcxprojCandidates) | ForEach-Object { "$($_.path) ($($_.reason))" }) -join ', '
	$diagnosis = $state.ConsecutiveHigh -eq 3

	# The prepare rows in strict topological order (every declared edge points from an earlier row to a later row).
	$table = @(
		@{ Step = $implement; When = $t.updateAffectedCode -and $afterDone; Role = 'implementer'; Name = '/update-affected-code'; Mode = ''; Scope = "the slices' changed C++ and GLSL"; After = @() }
		@{ Step = $preReview; When = $t.updateVcxproj; Role = 'mechanic'; Name = '/update-vcxproj'; Mode = ''; Scope = $vcxprojScope; After = @('/update-affected-code') }
		@{ Step = $preReview; When = $t.codeStyleReview; Role = 'mechanic'; Name = '/code-style-review'; Mode = ''; Scope = 'changed C++'; After = @('/update-affected-code', '/update-vcxproj') }
		@{ Step = $preReview; When = $true; Role = 'main'; Name = 'Invoke-StaticChecks.ps1'; Mode = ''; Scope = '-IncludeUntracked'; After = @('/update-affected-code', '/code-style-review', '/update-vcxproj') }
		@{ Step = $preReview; When = $true; Role = 'builder'; Name = '/compile'; Mode = ''; Scope = $compileScope; After = @('/update-affected-code', '/code-style-review', '/update-vcxproj') }
		@{ Step = $review; When = $t.overEngineeringReview; Role = 'fresh reviewer'; Name = '/over-engineering-review finder'; Mode = ''; Scope = $oeScope; After = @('/compile') }
		@{ Step = $review; When = $t.overEngineeringReview; Role = 'fresh implementer'; Name = '/over-engineering-review validator'; Mode = ''
			Scope = "the finder's removal candidates; skipped when it returns none"; After = @('Invoke-StaticChecks.ps1', '/over-engineering-review finder') }
		@{ Step = $cleanup; When = $plan.Phase -ceq 'words' -and $plan.DW.triggers.updateClaudeDocs; Role = 'implementer'; Name = '/update-claude-docs'; Mode = ''; Scope = 'changed C++ and GLSL'; After = @('review') }
		@{ Step = $acceptance; When = [bool] $HarnessChecks; Role = 'runner'; Name = '/agent-harness'; Mode = ''; Scope = $runnerScope; After = @('/compile') }
	)
	$runsAcceptance = $plan.Phase -ceq 'acceptance' -or $plan.Carried -ccontains '/verify-acceptance'
	$waitKeys = @($table | Where-Object { $_.When -and ($_.Name -cne '/agent-harness' -or $runsAcceptance) } | ForEach-Object { $_.Name })
	if ($diagnosis) { $waitKeys += '/external-diagnose-bug' }
	$command = "pwsh -NoProfile -NonInteractive -File .agents/scripts/Get-ChangeWorkflowSteps.ps1 -RepositoryRoot '$RepositoryRoot' -Baseline $Baseline -Tier $Tier"
	if ($Landing) { $command += ' -Landing' }
	if ($HarnessChecks) { $command += ' -HarnessChecks' }
	if ($diagnosis) {
		$table += @{ Step = $review; When = $true; Role = 'implementer'; Name = '/external-diagnose-bug'; Mode = ''; After = @()
			Scope = Get-DiagnosisScope $state.Last.Number }
	}
	$table += @{ Step = $review; When = $true; Role = 'main'; Name = '-StartRound'; Mode = ''; Scope = "$command -StartRound"; After = [string[]] $waitKeys; Final = $true }
	Exit-Printed ([string[]] (Format-Rows $table).ToArray())
}
catch {
	Write-Stderr "$($_.Exception.Message)"
	exit 1
}
