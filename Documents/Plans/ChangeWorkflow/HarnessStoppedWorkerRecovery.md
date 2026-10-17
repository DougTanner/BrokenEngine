<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-17T21:01:38.045Z","dependsOn":[]} -->
# Fix: agent-harness SKILL.md — no recovery for a manager-stopped harness worker

## Context
Observed symptom: in one `/next-plan` run the main session stopped a running
`/agent-harness` worker (dispatched as "Re-record A0 on unedited main") with
the host's task-stop tool before the worker reached its release step. The
worker's Debug client and server stayed running and the harness lock stayed
held under the stopped worker's owner token. `.agents/skills/agent-harness/SKILL.md`
documents no recovery for this case: release is only the worker's own step 44
in `.agents/skills/agent-harness/references/worker.md`
(`Invoke-HarnessRelease.ps1 -AgentHarness … -Owner '<owner token>' -ServerPid … -ClientPid … -ProcessCheckStatePath …`),
which needs the owner token, PIDs, and process-check state path only the
stopped worker held. The stale-claim takeover in `worker.md` steps 11-15 lets
a later claimer proceed only after the holder's heartbeat is five minutes old,
and SKILL.md does not tell the dispatching session about it.

Workaround it forced: main probed with `tasklist | grep -i brokenengine` and a
`Temp` listing, found the server and client still running, and improvised a
release-only `/agent-harness` dispatch. That worker read the lock holder with
`AgentHarness lock status --key default`, recovered the owner token from the
stopped worker's own scratch setup script, and ran `Invoke-HarnessRelease.ps1`
with it (exit 0, lock released); by then both processes had already exited, so
the release entry check reported them as unexpected exits and the dispatch
returned `NEEDS_ACTION`.

Recorded at the run checkpoint of the `/next-plan` run that claimed
`Documents/Plans/Engine/SharedSessionProtocol.md`; that Plan's `## In scope`
names no agent-harness file.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: bbf5aa94-d769-4ae5-81b2-ad085e0500cc
- Worktree/branch UUID: 3616ee35-6344-4e8a-96dc-0bdbbbc50103
- Session branch: claude/3616ee35-6344-4e8a-96dc-0bdbbbc50103
- Worktree: .claude\worktrees\BrokenEngine\3616ee35-6344-4e8a-96dc-0bdbbbc50103
- Landing ref: claude/3616ee35-6344-4e8a-96dc-0bdbbbc50103, the session branch
  above, whose tip is that session's final commit and which survives exactly as
  long as the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/HarnessStoppedWorkerRecovery.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

The author recommends first checking whether the existing release path already
suffices once documented: a dispatcher-facing SKILL.md rule saying that after
stopping a harness worker, the next step is a release-only `/agent-harness`
dispatch that reads the holder from the lock and releases through
`Invoke-HarnessRelease.ps1`. Rationale: the release script already handles
absent PIDs. Whether the release-only path needs a way to get the owner token
and PIDs without scraping the stopped worker's scratch files — for example
from the lock record — is the decision the fix session makes from that check;
a script change is in scope only if the documented path cannot work without it.

## Critical files
- `.agents/skills/agent-harness/SKILL.md`
- `.agents/skills/agent-harness/references/worker.md` (step 44-45 release, steps 11-15 stale takeover)
- `.agents/skills/agent-harness/scripts/Invoke-HarnessRelease.ps1`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the files named above: a
  dispatcher-facing recovery rule in SKILL.md, the matching release-only entry
  in `worker.md`, and, only if the documented path cannot work without it, the
  inputs `Invoke-HarnessRelease.ps1` accepts

## Out of scope
- The landed change the session produced
- The stale-heartbeat threshold and takeover steps in `worker.md` and
  `Invoke-HarnessClaim.ps1`; the AgentHarness C++ lock implementation
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2: the trigger is one subsystem's tool behavior (agent-harness
release recovery); a documentation-only fix is Tier 1. Escalate to Tier 3 if
the fix reaches the AgentHarness lock in `Tools/AgentHarness` or other
build/bootstrap coordination. A recovery must never release or quit a fresh
foreign owner's lock or processes. Never embed transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation:
  after a harness worker is stopped mid-run, following SKILL.md alone releases
  its lock and leaves neither of its processes running, without reading the
  stopped worker's scratch files
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing

## Notes
- `Documents/Plans/Engine/AgentHarnessIntoEngine.md` repoints links at
  `.agents/skills/agent-harness/SKILL.md:27,42`; it is not a duplicate (a
  documentation move, not recovery), and an insertion below those lines keeps
  its line citations valid.
