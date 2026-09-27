<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T22:12:49.789Z","dependsOn":[]} -->
# Fix: /agent-harness — replay determinism dispatch accepts a Release build that cannot replay

## Context
An `/agent-harness` replay determinism dispatch was briefed with the
Release|x64 configuration. The Release server answered both `replay_record`
and `replay_play` with "replay requires kbDebugInput build", so the dispatch
returned BLOCKED after a full harness run (about 72k subagent tokens) and had
to be re-dispatched against a Debug build, where the check passed.

`.agents/skills/agent-harness/SKILL.md` `## Inputs` asks the dispatcher only for
the `/compile` result "for the requested configuration" and never states that a
replay determinism check needs a `kbDebugInput` build. The requirement exists
only below the dispatch boundary:
`Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md:7` ("Run replay
acceptance only on a `kbDebugInput` build"), and
`Projects/BrokenEngineSandbox/Source/Pch.h` sets `kbDebugInput` true only under
`BT_DEBUG` (false under `BT_PROFILE` and `BT_RELEASE`). A dispatcher reading
the skill's contract therefore has no signal to request a Debug build.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 0137e606-5a32-41c8-8a9d-9b9bbca965dd
- Worktree/branch UUID: 9c221577-c019-4af3-9f60-6e5484c428cc
- Session branch: claude/9c221577-c019-4af3-9f60-6e5484c428cc
- Worktree: .claude\worktrees\BrokenEngine\9c221577-c019-4af3-9f60-6e5484c428cc
- Landing ref: claude/9c221577-c019-4af3-9f60-6e5484c428cc
  Fallback once the recorded ref is gone:
  `git log --follow --diff-filter=A --format=%H -- Documents/Plans/ChangeWorkflow/AgentHarnessReplayBuildInput.md`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex transcript discovery requires the producing worktree to remain
  registered, and Claude review requires the exact conversation session ID
  above. OpenCode transcript review remains unsupported regardless of worktree
  retention.

## Design
First root-cause the friction from the current tree and this Plan's `## Context`.
Only when the transcript is genuinely needed, in a new session run
`/next-plan-review <review ref>` in bounded friction mode — the landing ref —
supplying the recorded client and the recorded conversation session ID. Then
make the smallest fix inside the `## In scope` boundary below. If
root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

The author's recommendation, on the current evidence: state in `## Inputs` that
a replay determinism check requires a `kbDebugInput` (Debug) build, referencing
`replay.md` as the owner of the rule rather than restating it, so the
dispatcher requests the right `/compile` configuration up front.

## Critical files
- `.agents/skills/agent-harness/SKILL.md`

## In scope
- Root-cause investigation as `## Design` states
- The smallest resulting fix, confined to `.agents/skills/agent-harness/SKILL.md`
  `## Inputs`

## Out of scope
- The landed change the session produced
- `kbDebugInput` values in `Projects/BrokenEngineSandbox/Source/Pch.h` and the
  server's replay gating
- The `Invoke-HarnessClaim.ps1` `GameDataDirectory` absolute-path check
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior: the dispatch contract of `/agent-harness`);
escalate if the fix reaches build/bootstrap coordination. Never embed
transcript paths or home paths.

## Acceptance criteria
- The recorded symptom no longer reproduces under the documented invocation: a
  dispatcher following `/agent-harness` `## Inputs` for a replay determinism
  check requests a `kbDebugInput` build
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing
