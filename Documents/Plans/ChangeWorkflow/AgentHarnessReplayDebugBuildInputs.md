<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T19:39:42.030Z","dependsOn":[]} -->
# Fix: /agent-harness — Inputs never says replay determinism needs a Debug (kbDebugInput) build

## Context
Observed symptom: a replay determinism check in the run of
`Documents/Plans/Engine/StyleGuideScannerRuleSweepCommonDataPackerTools.md`
was dispatched against the Release server, and `replay_record` exited 2 with
error "replay requires kbDebugInput build" (thrown in
`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:58`,
also :109 and :155). `Projects/BrokenEngineSandbox/Source/Pch.h:67` sets
`kbDebugInput = false` in the `BT_RELEASE` block.
`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md`
`### Replay determinism acceptance` states the requirement ("Run replay
acceptance only on a `kbDebugInput` build"), but
`.agents/skills/agent-harness/SKILL.md` names replay determinism as a use and
its `## Inputs` asks only for executables "for the requested configuration",
so the dispatcher requested a Release build. Cost: a wasted harness run, a
Debug rebuild, and a second harness dispatch.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 529e7113-822c-42a1-ae08-17fe68b76d80
- Worktree/branch UUID: 485a0f59-4094-4ff4-8816-96f7e08e2de2
- Session branch: claude/485a0f59-4094-4ff4-8816-96f7e08e2de2
- Worktree: .claude/worktrees/BrokenEngine/485a0f59-4094-4ff4-8816-96f7e08e2de2
- Landing ref: claude/485a0f59-4094-4ff4-8816-96f7e08e2de2 (the observing
  session records and lands this Plan itself).
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/AgentHarnessReplayDebugBuildInputs.md`,
  but a periodic Plan-history squash can make it return an unrelated aggregate
  commit, so review its result only when the commit is attributable to one
  session alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Claude review requires the exact conversation session ID above.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review claude/485a0f59-4094-4ff4-8816-96f7e08e2de2` in bounded
friction mode, supplying client `claude` and the recorded conversation session
ID. Then make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author recommends one sentence in `## Inputs` stating that a replay
determinism scenario needs a `kbDebugInput` (Debug) build of the server, with
a pointer to `replay.md` `### Replay determinism acceptance`, because
`## Inputs` is what the dispatcher reads when it requests the `/compile` build.

## Critical files
- `.agents/skills/agent-harness/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/agent-harness/SKILL.md`
  `## Inputs`

## Out of scope
- The landed change the session produced
- `replay.md`, `Pch.h`, `ServerSimulationFixtures.cpp`, and which builds enable
  `kbDebugInput`
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Tier 2 (scoped tool behavior); escalate if the fix reaches build/bootstrap
coordination. The replay requirement stays owned by `replay.md`. Never embed
transcript paths or home paths.

## Acceptance criteria
- A dispatcher reading only `/agent-harness` `## Inputs` requests a
  `kbDebugInput` server build for a replay determinism scenario
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
