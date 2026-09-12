<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T13:25:03.417Z","dependsOn":[]} -->
# Fix: .agents/references/subagent-reporting.md — no named wait mechanism for a background worker, so main takes each handoff twice

## Context
Observed during this session's `/next-plan` run checkpoint, by
`/next-plan-checkpoint-review`; the finding was accepted at the checkpoint and
classified fixable-defect in the emitter,
`.agents/references/subagent-reporting.md` `## Whether a worker is still
running, and interruption`.

- Symptom: main dispatched six background subagents during the run. No
  repository instruction names how main waits for a background worker, so main
  loaded the host's deferred `TaskOutput` tool ad hoc through `ToolSearch` and
  called it with `block: true` on each task id.
- Observed result: each completed worker's handoff entered main's context
  twice — once as the `TaskOutput` tool result, and again as the host's own
  task-notification attachment, which carries the identical handoff text.
- Cost and workaround: roughly 27k characters of duplicated handoff text across
  the six dispatches, and an ad hoc `ToolSearch` load of an undocumented host
  tool on every dispatch.
- Contradicted contract: `.agents/references/subagent-handoff.md:50-53`, which
  states that main consumes each dispatched worker's handoff once, from the
  host's own delivery of it.
- The emitting section, `.agents/references/subagent-reporting.md` `## Whether a
  worker is still running, and interruption`, covers wait timeouts, no-progress,
  and interruption but never names the mechanism main uses to wait for a
  background worker, which is why the ad hoc blocking call was invented.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: bbf2d7ab-d589-479c-9468-23604eeaba37
- Worktree/branch UUID: b8030bcd-d2d8-4ec5-b04d-61b77f5fe77e
- Session branch: claude/b8030bcd-d2d8-4ec5-b04d-61b77f5fe77e
- Worktree: .claude\worktrees\BrokenEngine\b8030bcd-d2d8-4ec5-b04d-61b77f5fe77e
- Landing ref: claude/b8030bcd-d2d8-4ec5-b04d-61b77f5fe77e — this Plan is
  recorded and lands with the change of the session that observed the friction,
  so that branch's tip is its final commit and it survives exactly as long as
  the worktree recorded above.
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above.

## Design
First root-cause the friction from the current tree and this Plan's
`## Context`; the citation above already names both sides of the conflict, so
the transcript should not be needed. Only when it genuinely is, in a new session
run `/next-plan-review claude/b8030bcd-d2d8-4ec5-b04d-61b77f5fe77e` in bounded
friction mode, supplying client `claude` and the recorded conversation session
ID.

The author's recommendation is to add to
`.agents/references/subagent-reporting.md` `## Whether a worker is still
running, and interruption` a short statement naming the wait mechanism main uses
on Claude for a background worker: end the turn, or do independent work, and let
the host's task-notification deliver the handoff — never a blocking host
result-fetch call such as `TaskOutput` with `block: true` — so the handoff
arrives exactly once, as `.agents/references/subagent-handoff.md` `## Handoffs`
already requires. The existing timeout, no-progress, and interruption prose then
reads as the exception route rather than the normal path, and the fixer should
reconcile the section's "wait times out" wording with the named mechanism so the
two do not describe different waits.

The recommended text should also state the Codex equivalent, but only if the
repository or client documentation already documents one; if no documented Codex
wait mechanism is found, say so explicitly in the section rather than inventing
one, and surface that absence as a residual.

Make the smallest fix inside the `## In scope` boundary below. If root-causing
shows the fix lies outside that boundary — for example that
`.agents/references/subagent-handoff.md` or a dispatching skill must change —
surface it for re-planning instead of expanding scope.

## Critical files
- `.agents/references/subagent-reporting.md` (`## Whether a worker is still
  running, and interruption`) — the authorized fix boundary
- `.agents/references/subagent-handoff.md` (`## Handoffs`) — read as the
  authoritative once-only consumption rule; changing it is out of scope

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `## Whether a worker is still
  running, and interruption` section of
  `.agents/references/subagent-reporting.md`: naming the background-worker wait
  mechanism, banning a blocking host result-fetch call for a handoff main will
  also receive by notification, stating the Codex equivalent only where already
  documented, and reconciling the section's existing wait/timeout wording with
  the named mechanism

## Out of scope
- `.agents/references/subagent-handoff.md` and the shared handoff form
- Any skill body, dispatching workflow, or role definition
- The landed change the recording session produced
- Any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 1 (mechanical): documentation of a shared reference, with no public
signature or invariant exposure. Escalate only if the fix reaches the shared
handoff form or a dispatching skill's contract. Never embed transcript paths or
home paths. No unit tests.

## Acceptance criteria
- `.agents/references/subagent-reporting.md` `## Whether a worker is still
  running, and interruption` names the wait mechanism main uses for a background
  worker on Claude and rules out a blocking host result-fetch of a handoff the
  host will also deliver, so the handoff enters main's context exactly once.
- The section's Codex statement either names a documented equivalent mechanism
  or records that none is documented.
- The section no longer contradicts `.agents/references/subagent-handoff.md`
  `## Handoffs` on once-only handoff consumption.
- `/progressive-disclosure-review` over the changed reference reports no new
  finding.
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing.

## Notes
Recorded as a tooling-friction follow-up from the `/next-plan` run checkpoint;
there was no unmet acceptance criterion — the cost is duplicated main-context
text and an ad hoc host-tool workaround.
