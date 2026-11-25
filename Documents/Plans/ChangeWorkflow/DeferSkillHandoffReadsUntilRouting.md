<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-24T20:58:49.338Z","dependsOn":["Documents/Plans/ChangeWorkflow/NextPlanBoundedSkillReads.md"]} -->
# Fix: `.agents/references/change-workflow.md` — main reads skill Handoff sections at dispatch time instead of waiting until routing a returned result

## Context

- 2026-11-23 (`agy/0dd9242c-5f71-4111-812c-71056a74cb82`): read
  `.agents/skills/plan-simplicity-review/SKILL.md` lines 54-106 before
  dispatch, pulling `## Handoff` into main before the reviewer ran; expected
  main to read `## Inputs` for dispatch and consult `## Handoff` only when
  routing a returned result.
- 2026-11-24 (`agy/b5903b2e-e0a6-4bea-8ed4-c96dcf52d739`): read
  `.agents/skills/plan-alternatives/SKILL.md` lines 49-192 (8,389 chars) before
  the researcher returned; expected main to use the Step 2 dispatch recipe and
  open `## Handoff` and `### User presentation` only after a candidate result
  returns.
- 2026-11-24 (`agy/ba36634f-b7cf-4fd2-ab31-229b3859e1d0`): read
  `.agents/skills/over-engineering-review/SKILL.md` lines 39-52 and 53-94 at
  finder dispatch time; expected main to read finder `## Inputs` for dispatch
  and defer `## Handoff` and validator instructions until the finder returns a
  removal candidate.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a
ref whose tree actually contains this Plan:
- Client: agy
- Conversation session ID: 1a2c314a-4495-47d9-b38b-43b3488ee2d3
- Worktree/branch UUID: ba36634f-b7cf-4fd2-ab31-229b3859e1d0
- Session branch: agy/ba36634f-b7cf-4fd2-ab31-229b3859e1d0
- Worktree: .agy\worktrees\BrokenEngine\ba36634f-b7cf-4fd2-ab31-229b3859e1d0
- Landing ref: agy/ba36634f-b7cf-4fd2-ab31-229b3859e1d0
  The review ref the invoker passes to /next-plan-review has two cases. When
  the `Observed in an earlier session` line is present, it is that line's
  landed commit hash, with no lookup. When that line is absent, the recording
  session is the observing session, and worktree reuse moves its branch past
  its landing, so the invoker never reviews the ref itself: it resolves the ref
  to the commit that added this Plan with
  `git log --follow --diff-filter=A -n 1 --format=%H <ref> -- <this plan path>`
  and passes that commit. When the recorded ref is gone or that command prints
  nothing (a later session moved this Plan), it runs the same command with
  `<ref>` omitted. A periodic Plan-history squash can make either command
  return an unrelated aggregate commit, and a landing that also deletes a
  similar file can make it return another session's commit, so review the
  result only when the commit is attributable to the observing session recorded
  above alone (its diff limited to that session's files); never review an
  aggregate or multi-session squash commit.
- Run the review before /cleanup-worktrees removes the worktree recorded above:
  Codex discovery requires a registered worktree, and Claude or agy review
  requires the conversation session ID above.

## Design

First root-cause the friction from the current tree and this Plan's
`## Context`. Only when the transcript is genuinely needed, in a new session
run `/next-plan-review <review ref>` in bounded friction mode for the single
recorded friction in this Plan's title ("main reads skill Handoff sections at
dispatch time instead of waiting until routing a returned result"), with the
review ref the provenance block above defines, supplying the recorded client
and conversation session ID. Then make the smallest fix inside the
`## In scope` boundary below so main reads only the cited dispatch recipe or
dispatch inputs (such as the Step 2 dispatch recipe, `## Task brief`, or a
skill's first-pass `## Inputs`) when building a brief and defers reading a
skill's `## Handoff` section (and follow-on presentation or validator-only
instructions) until routing a returned result or receiving a finder candidate.
If root-causing shows the fix lies outside that boundary, surface it for
re-planning instead of expanding scope.

## Critical files

- `.agents/references/change-workflow.md`
- `.agents/references/subagent-reporting.md`

## In scope

- Root-cause investigation as `## Design` states.
- The smallest resulting fix, confined to `.agents/references/change-workflow.md`
  (`### Subagents`, `#### Step 2 — Prepare and explore alternatives`, and
  `#### Step 4 — Implement and propagate`) and
  `.agents/references/subagent-reporting.md` (`## Task brief`).

## Out of scope

- The landed change the session produced.
- Unrelated skills or scripts; any transcript path or transcript text in the
  repo.

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior — workflow dispatch and handoff-read
timing guidance); escalate if the fix reaches build/bootstrap coordination.
Never embed transcript paths or home paths.

## Acceptance criteria

- The recorded symptom no longer reproduces under the documented invocation:
  before launching workers, main reads only the Step 2 dispatch recipe and
  `## Task brief` for `/plan-alternatives`, `## Inputs` and `## Task brief` for
  `/plan-simplicity-review`, and finder `## Inputs` and `## Task brief` for
  `/over-engineering-review`, deferring `## Handoff` and `### User presentation`
  until routing a returned result and validator-only instructions until the
  finder returns a candidate.
- The static-checks runner, invoked as `.agents/references/change-workflow.md`
  `#### Step 5 — Run targeted pre-review checks` documents it, reports every
  row the change triggers passing.
