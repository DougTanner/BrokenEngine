<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T12:05:42.904Z","dependsOn":[]} -->
# Fix: /external-grill-plan — grill questions are not self-contained on first presentation

## Context

Observed symptom, from a `/next-plan-review` of an earlier landed session: the
`/external-grill-plan` question turn at `17:17:02Z` was followed by a 1 hour
49 minute user pause and then a `/what` request at `19:06:45Z`. The rewritten,
plain-language explanation produced both user decisions within four minutes, so
the delay came from the first presentation not being answerable on its own.

Current behavior, read from the working tree:

- `.agents/skills/external-grill-plan/SKILL.md` `## Decision Interaction`
  (`:76-97`) requires repository evidence, why the choice changes the plan, two
  or three mutually exclusive recommended-first choices, the exact refinement
  each choice produces, and a fixed numbered prose shape.
- It never requires the plain, non-expert language the root `AGENTS.md`
  `### User Interaction` section demands, never requires stating what the answer
  changes or blocks, and never states that the full context must be visible as
  rendered message text before a structured-choice tool call — which that same
  root section requires, because text emitted before such a call may never be
  displayed to the user.
- `AskUserQuestion` is among the skill's allowed tools (`:8`), so the
  structured-choice route is the expected one.

The misbehaving surface is `/external-grill-plan`'s question step, outside the
`## In scope` boundary of the Plan the observing session had claimed, so this is
tooling friction rather than an in-scope acceptance failure of the change that
session landed.

Verify every cited line number against the working tree before editing — the
numbers above may have moved since.

Session provenance (machine-local; not reproducible after cleanup). The Client
through Worktree fields name the session that observed the friction — the
session `/next-plan-review` must reach — while the `Landing ref` line names a ref
whose tree actually contains this Plan:
- Client: claude
- Conversation session ID: 1b583079-2807-42e9-930f-2394560b2edc
- Worktree/branch UUID: 9210ba0b-3bb1-4251-80be-0a74eef865cc
- Session branch: claude/9210ba0b-3bb1-4251-80be-0a74eef865cc
- Worktree: .claude\worktrees\BrokenEngine\9210ba0b-3bb1-4251-80be-0a74eef865cc
- Observed in an earlier session: the six fields above are the observing
  session's, not the recording session's. That session's landed commit is
  `44b1259d50eaf4582bae5e25d5e35386abc3619b` ("Accept landing-candidate sessions
  in plan list and rename quarantined to excluded"), a single-session landing
  commit whose tree contains that session's work.
- Landing ref: `claude/a9c4f4cc-6cd6-4bf1-8511-9702c6308d1f`, the recording
  session's branch, whose tip is that session's final commit. Fallback once that
  ref is gone: `git log --diff-filter=A --format=%H -- <this plan path>`, but a
  periodic Plan-history squash can make it return an unrelated aggregate commit,
  so review its result only when the commit is attributable to one session alone
  (its diff limited to that session's files); never review an aggregate or
  multi-session squash commit.

## Design

The `/next-plan-review` that produced this residual has already run and proved
the symptom above, so no further transcript review is required before
implementing. The provenance block is retained only so an implementer who needs
more context can reach the observing session while it is still available.

Amend `.agents/skills/external-grill-plan/SKILL.md` `## Decision Interaction`
only, applying the root `AGENTS.md` `### User Interaction` rule at the step that
asks:

1. Add to the per-question requirement list that each question states, in plain
   non-expert language, what the answer changes or blocks, and that the question
   is answerable from the current message alone, without the user recalling
   earlier turns or reading the plan or source.
2. State that when the host's structured choice UI is used, the round's full
   context is presented first as ordinary message text and the tool call carries
   only the choices, because text emitted before such a call may never be
   displayed.

Keep the existing evidence, two-or-three-choice, recommended-first, exact-
refinement, and numbered-prose requirements unchanged, and do not restate the
root rule beyond the sentences this step needs.

## Critical files

- `.agents/skills/external-grill-plan/SKILL.md` — the `## Decision Interaction`
  requirement list and structured-UI paragraph (`:76-97`)
- root `AGENTS.md` — the `### User Interaction` rule, read as the governing
  source; never edited by this Plan

## In scope

- The `## Decision Interaction` requirement list and structured-UI paragraph in
  `.agents/skills/external-grill-plan/SKILL.md`: plain-language phrasing, what
  the answer changes or blocks, self-containment, and context presented as
  visible message text before a structured-choice tool call

## Out of scope

- Root `AGENTS.md` and every other governing document
- `/external-grill-plan`'s rounds and frontier computation, the library gate,
  `locator` dispatch and external-claim verification, the exact-refinement
  format, and the pivot and no-delta handoffs
- Every other skill that asks the user, including `/next-plan`,
  `/finalize-changes`, and `/external-design-interface`
- The landed change the observing session produced
- Unrelated skills/scripts; any transcript path or transcript text in the repo

## Risk tier and invariants

Expected Tier 2 (scoped tool behavior, documentation only); escalate if the fix
reaches root `AGENTS.md` or another skill's interaction contract. Invariants to
preserve: exactly two or three mutually exclusive choices, recommended first,
each naming the exact refinement it produces; no substitution of an open-ended
prompt; and no user question that the skill's evidence rules do not already
justify.

## Acceptance criteria

- A reader of `## Decision Interaction` can tell that each question must be
  answerable from the current message alone, in plain non-expert language, and
  must say what the answer changes or blocks
- The section states that a structured-choice tool call is preceded by the
  round's full context as visible message text
- The existing choice, ordering, and refinement requirements are unchanged
- `/validate-skill` passes for the changed `SKILL.md`; WorktreeCli
  `plan validate` exits `0` with `status: valid` and `code: ok`

## Notes

This Plan is keyed to the (skill, symptom) pair: `/external-grill-plan`'s
question step presents decisions that the user cannot answer from the message
alone. A later observation of the same pair is a duplicate, not a new residual.
