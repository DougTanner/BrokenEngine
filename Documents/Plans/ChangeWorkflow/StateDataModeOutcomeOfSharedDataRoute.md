<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-06T19:20:39.544Z","dependsOn":[]} -->
# Fix: /agent-harness — the `shared-data.changed` route omits that a rebuild can switch to Local data

## Context

Observed symptom: `.agents/skills/agent-harness/scripts/Invoke-HarnessClaim.ps1`
exited 2 with `shared-data.changed`, and main routed `/compile` as
`.agents/skills/agent-harness/references/worker.md:192` (step 7) and
`.agents/skills/agent-harness/SKILL.md:117` direct. Neither says a rebuild can
resolve Local data, or whether that clears the refusal. The builder reported the
refusal not cleared, yet both later claims exited 0. Main dispatched on that
unresolved question, and a CPU timing criterion, labelled `Cost` on its execution
card, ran on islands different from its baseline. The gap could not be attributed
until a diagnosis dispatch separated code from islands.

The clearing path is visible in source. `/compile` resolves the data mode per
build (`.agents/skills/compile/references/runtime-data-mode.md`
`## Mode selection`), and `Compare-AgentSharedDataRecord` returns nothing when
the recorded data directory is under the worktree root
(`.agents/scripts/AgentScriptCommon.psm1:271`). So a rebuild clears the refusal
either by rewriting a Shared listing or by resolving Local, and the second leaves
the session on different data. Which `dataBuildModeReason` this session's rebuild
reported is not in the recorded evidence.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 4ba81e08-3493-4492-84a8-df446e573f41
- Worktree/branch UUID: aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Session branch: claude/aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Worktree: .claude\worktrees\BrokenEngine\aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
- Landing ref: claude/aa8d6b01-5a38-4ae9-a02f-fbbcb7611124
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/StateDataModeOutcomeOfSharedDataRoute.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <landing ref>` in bounded friction mode, supplying the
recorded client and the recorded conversation session ID. Then make the smallest
fix inside the `## In scope` boundary below. If root-causing shows the fix lies
outside that boundary, surface it for re-planning instead of expanding scope.

The author recommends stating, in the `SKILL.md` `## Handoff` route main reads,
that the routed `/compile` may resolve Local data, that re-entering the claim is
the test of whether the refusal cleared, and that a data mode differing from a
measured baseline's means taking that baseline again. Step 7 of `worker.md` then
needs at most a citation of that route. The rule against a worker switching data
itself stays as written, because the build makes that switch, not the worker.

## Critical files

- `.agents/skills/agent-harness/SKILL.md` — `## Handoff`
- `.agents/skills/agent-harness/references/worker.md` — step 7
  `shared-data.changed`

## In scope

- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to the `shared-data.changed` route
  sentences of `SKILL.md` `## Handoff` and `worker.md` step 7

## Out of scope

- The landed change the session produced
- `Compare-AgentSharedDataRecord`, `Invoke-HarnessClaim.ps1`, and the data-mode
  decision itself
- Why Shared and Local islands differed in the observing session
- The build configuration a CPU timing scenario requests
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 1: the highest trigger is documentation with no public signature or
invariant exposure, since route wording changes and the claim's refusal codes and
the data-mode rule do not. Determinism, wire, serialization, replay, affinity,
threading, allocation, shader, and build surfaces are not exposed. The change
needs no live verification, and the supplied evidence records no harness setup.
Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation: the
  route states that a rebuild can resolve Local data, how to tell whether the
  refusal cleared, and what a data-mode change does to a measured baseline
- The route's statements match `Compare-AgentSharedDataRecord` and
  `.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
