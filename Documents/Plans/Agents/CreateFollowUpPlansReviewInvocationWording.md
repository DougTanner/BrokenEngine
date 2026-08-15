<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-15T20:57:24.512Z","dependsOn":[]} -->
# Fix: create-follow-up-plans — friction Plans tell a session to run the user-reserved `/next-plan-review`

## Context

During a `/next-plan` run that had claimed
`Documents/Plans/Agents/CompileSharedDataVersionDrift.md`, the session followed
that Plan's `## Design` instruction at `:60-64`, "In a new session, run
`/next-plan-review <landing ref>` supplying the recorded client and landing ref
only." The session attempted `Skill(next-plan-review)` and the host refused it:
"Skill next-plan-review cannot be used with Skill tool due to
disable-model-invocation... reserved for explicit user invocation." The session
had to stop and ask the user to type `/next-plan-review` themselves, costing a
user round-trip the Plan's authoring guidance did not anticipate.

The instruction comes from this skill, not from the individual Plan author:
`.agents/skills/create-follow-up-plans/SKILL.md:82-87` (the tooling-friction
body template's `## Design` block) and `:94-96` (its `## In scope` block) both
direct a future session to "run `/next-plan-review`", while
`.agents/skills/next-plan-review/SKILL.md:4` sets
`disable-model-invocation: true` with `user-invocable: true`, so no model may
invoke it. Every tooling-friction Plan already authored from this template
carries the same instruction, so the same stall recurs on each of them.

The claimed Plan's `## In scope` (`:81-89`) is confined to the compile skill,
`references/runtime-data-mode.md`, `Resolve-CompileContext.ps1`,
`New-DataOracleReceipt.ps1`, and `Test-DataOracleReceipt.ps1`;
`.agents/skills/create-follow-up-plans/SKILL.md` is outside it, so this is
tooling friction rather than an in-scope blocker of the active change.

Session provenance (machine-local; not reproducible after cleanup):
- Client: claude
- Conversation session ID: 4d8a4bb5-09ca-499b-a3e3-e54f68ee5a76
- Worktree/branch UUID: 846d5dd2-6b98-43b7-b41b-283ed5de1c11
- Session branch: claude/846d5dd2-6b98-43b7-b41b-283ed5de1c11
- Worktree: .claude\worktrees\BrokenEngine\846d5dd2-6b98-43b7-b41b-283ed5de1c11
- Landing ref: the session branch above, whose tip is the session's final
  commit and which survives exactly as long as the worktree recorded above.
  Fallback once that branch is gone:
  `git log --diff-filter=A --format=%H -- <this plan path>`, but a periodic
  Plan-history squash can make it return an unrelated aggregate commit, so
  review its result only when the commit is attributable to this session alone
  (its diff limited to this session's files); never review an aggregate or
  multi-session squash commit.
- Run the review before /cleanup-worktrees removes this worktree: Codex
  transcript discovery requires the producing worktree to remain registered,
  and Claude review requires the exact conversation session ID above.

## Design

The resolution is decided: fix the authoring guidance, not the invocation
policy. `/next-plan-review` stays deliberately user-reserved.

In a new session, present the exact command `/next-plan-review <landing ref>`
to the user and ask the user to run it, supplying the recorded client and the
recorded conversation session ID above; that review confirms the symptom from
the proven transcript. Then make the smallest fix inside the `## In scope`
boundary below: reword this skill's tooling-friction body template so a Plan
that depends on a `/next-plan-review` pass instructs the future session to
present the exact command to the user and request that the user run it, rather
than phrasing it as something the session itself runs. If root-causing shows
the fix lies outside that boundary, surface it for re-planning instead of
expanding scope.

## Critical files

- `.agents/skills/create-follow-up-plans/SKILL.md` — the tooling-friction body
  template's `## Design` block (`:82-87`) and `## In scope` block (`:94-96`),
  plus the `## Workflow` sentence deferring root cause to
  "the `/next-plan-review` session named in the Plan's `## Design`" (`:19`).

## In scope

- Symptom confirmation via `/next-plan-review`, presented to the user for the
  user to run with the recorded landing ref, client, and conversation session
  ID.
- The smallest resulting wording fix, confined to the template `## Design` and
  `## In scope` blocks and the `## Workflow` deferral sentence named above, so
  the authored instruction is "present the command and ask the user to run it"
  instead of an instruction the session cannot carry out.

## Out of scope

- `.agents/skills/next-plan-review/SKILL.md` and its invocation policy: its
  `disable-model-invocation: true` is deliberate and must not change.
- Rewriting the already-authored tooling-friction Plans under
  `Documents/Plans/Agents/` that carry the old wording.
- The landed change this session produced; unrelated skills and scripts; any
  transcript path or transcript text in the repository.

## Risk tier and invariants

Expected Tier 1 (documentation wording in one skill file, no public signature
or invariant exposure); escalate to Tier 2 if the fix turns out to require
changing skill routing or invocation behavior rather than authoring wording.
Preserve the template's existing byte-zero marker rules, provenance block, and
profile-relative `Worktree` locator, and never embed transcript paths or home
paths.

## Acceptance criteria

- A tooling-friction Plan authored from the updated template instructs the
  future session to present `/next-plan-review <landing ref>` to the user and
  request that the user run it, and no longer implies the session can invoke
  the skill itself.
- `.agents/skills/next-plan-review/SKILL.md` still sets
  `disable-model-invocation: true`.
- `/validate-skill` passes for `.agents/skills/create-follow-up-plans/SKILL.md`,
  and WorktreeCli `plan validate` exits `0` with `status:valid` and `code:ok`.

## Notes

This Plan is keyed to the concrete skill-plus-symptom pair
(`.agents/skills/create-follow-up-plans/SKILL.md` template text directing a
session to run `/next-plan-review`, and the host refusal "cannot be used with
Skill tool due to disable-model-invocation"). A later observation of that same
pair is a duplicate, not a new residual. This body records the instruction, the
refusal, the forced user round-trip, and provenance without embedding
transcript material.
