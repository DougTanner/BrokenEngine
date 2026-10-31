<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-30T18:12:19.999Z","dependsOn":[]} -->
# Fix: agent-harness claim step — Claude Code backgrounds the claim wait after 180 s

## Context
Observed symptom: on Claude Code, the `/agent-harness` claim step
(`.agents/skills/agent-harness/references/worker.md` step 5, lines 61-112)
cannot be run as written while another session holds the harness lock. The
claim command was the one step 5 documents:

`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1 -RepositoryRoot '<absolute adopted worktree>' -Session '<label>' -Project '<Project>'`

Step 5 requires a synchronous foreground call with the maximum available
timeout (worker.md:95-102). On Claude Code the host moved every foreground
call still running after about 180 s to the background, even with a
600000 ms timeout. Step 5's background rule (worker.md:103-112) then says to
stop the still-running background task and re-invoke the identical command in
the foreground. That re-invocation restarts the wait and is backgrounded again
at 180 s, so while the holder stays fresh the rule can repeat without end.

The claim script waits longer than that host limit: `Invoke-HarnessClaim.ps1`
sets `$WaitBudgetSeconds = 480` (line 38), polls every 5 s
(`$PollMilliseconds`, line 31), and judges a holder stale at 300 s
(`$StaleHeartbeatSeconds`, line 33). Its comment at lines 34-37 sizes the
budget only against Claude Code's 600000 ms tool cap.

Observed twice in one session, both by delegated `/agent-harness` workers
waiting on another worktree's harness lock holder with a fresh heartbeat:

1. A Normal-launch worker's first claim (PowerShell tool) and second claim
   (Bash tool) were both moved to the background while still waiting. It
   stopped the first and re-invoked it as step 5 says. Seeing that this would
   repeat, it did not stop the second; it waited for that background task
   in-turn with a Monitor and a bounded wait-and-read call, then used its
   exit-0 result. Step 5 does not cover that workaround.
2. A spot-check worker's first claim waited on holder `stage2-probe`, was moved
   to the background, and was stopped (no orphan left) and re-invoked in the
   foreground. The re-invocation passed after 24 attempts. The claim wait took
   about 5 minutes of wall-clock time against the brief's 8-minute run budget.
   worker.md:797-800 measures that budget from claim success, so the cost was
   wall-clock delay and the lost first wait, not a budget breach.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 8e484dd3-b595-4dd4-9d25-89cc4cda3c89
- Worktree/branch UUID: daaad2ee-028f-445a-b1ff-75c12e6af429
- Session branch: claude/daaad2ee-028f-445a-b1ff-75c12e6af429
- Worktree: .claude\worktrees\BrokenEngine\daaad2ee-028f-445a-b1ff-75c12e6af429
- Landing ref: claude/daaad2ee-028f-445a-b1ff-75c12e6af429
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/AgentHarnessClaimBackgrounded.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review claude/daaad2ee-028f-445a-b1ff-75c12e6af429` in bounded
friction mode, supplying client `claude` and conversation session ID
`8e484dd3-b595-4dd4-9d25-89cc4cda3c89`. Then make the smallest fix inside the
`## In scope` boundary below. If root-causing shows the fix lies outside that
boundary, surface it for re-planning instead of expanding scope.

The author sees at least these candidate directions and does not choose
between them; the executing session decides from its root cause:
- Keep each claim invocation under the host's background threshold, by a
  shorter `$WaitBudgetSeconds` that ends in `claim.wait-timeout` and a
  re-invoke. That budget would no longer outlast `$StaleHeartbeatSeconds`, so
  the comment at Invoke-HarnessClaim.ps1:34-37 and the stale-holder path change
  with it.
- Let step 5 accept a still-running backgrounded claim and wait for it in-turn
  until it exits, as the first worker did, instead of stopping and
  re-invoking it.

## Critical files
- `.agents/skills/agent-harness/references/worker.md` (step 5, lines 61-112;
  step 7 only where it routes `claim.wait-timeout`)
- `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1` (the wait
  constants at lines 31-38 and the wait loop around line 416)

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to worker.md step 5 (and step 7's
  `claim.wait-timeout` routing if the fix changes it) and the claim wait in
  `Invoke-HarnessClaim.ps1`

## Out of scope
- The landed change the session produced
- The harness lock record format, takeover steps (worker.md step 11 onward),
  launch scripts, and release recovery
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: one skill's claim step and its script).
Escalate to Tier 3 if the fix changes the harness lock protocol or how a
waiting session treats another session's claim, because that is coordination
that can block other sessions. The script must still never steal from a fresh
holder, and a stopped or abandoned claim invocation must not later take the
lock. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  on Claude Code, a worker following step 5 while another worktree holds the
  harness lock with a fresh heartbeat for longer than 180 s reaches a claim
  result without stopping and restarting the wait, and without a workaround
  step 5 does not document. The supplied evidence records no exact setup for
  the holder session beyond a second worktree holding the lock during its own
  `/agent-harness` run.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
