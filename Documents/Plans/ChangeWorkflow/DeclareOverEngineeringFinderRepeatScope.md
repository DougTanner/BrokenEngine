<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T03:02:09.408Z","dependsOn":[]} -->
# Declare the over-engineering finder's repeat scope

## Context

After the first review round, the step script prints the
`/over-engineering-review finder` row with the scope
`session-changed C++ in <paths>`: the `cpp`-class paths of the diff since the
last round's snapshot (`.agents/scripts/Get-ChangeWorkflowSteps.ps1`, the
`$oeScope` assignment, which reads `$plan.DB`, the inventory from the last
snapshot that `Get-ReviewRoundState` in
`.agents/scripts/SessionChangeInventory.psm1` builds). The finder worker
narrows its inventory to those paths with `-PathPrefix` when its `Scope` has
that form (`.agents/skills/over-engineering-review/references/worker.md`
step 1).

`.agents/skills/over-engineering-review/SKILL.md` `## Inputs` lists `Pass`,
`Baseline`, and validator-only inputs, but no `Scope`. A dispatcher following
those inputs can omit the printed path list, and the finder then scans the
whole session diff and proposes removals outside the repeat scope. The gap is
pre-existing: at the session baseline the worker already read a `rerun:`
`Scope` that `## Inputs` did not declare.

A related open question: every review row the step script prints re-reads the
whole session change for its file types (header, "Every review row prints its
first-pass scope"), while this finder row still narrows to the C++ changed
since the last round. The user has not decided whether to widen it.

## Unresolved decisions

The recommendation below is the author's; the executing session presents it to
the user at plan approval.

1. The finder's repeat scope:
   - (a) Keep the narrowing (recommended): the finder's candidates on code
     unchanged since the last round were already proposed or dropped once, and
     the validator re-proves each candidate anyway. Trade-off: a removal made
     possible only by a later change elsewhere in the session is missed.
   - (b) Widen it to the whole session's C++, matching the review rows. The
     step script prints `changed cpp-class files` for every round, and the
     `session-changed C++ in <paths>` form and the worker's `-PathPrefix` use
     are removed. Trade-off: one rule for every review-step row, at the cost
     of re-scanning unchanged code each round.

## Design

No tracked file changes before the user answers. After the answer:

1. Add `Scope` to `SKILL.md` `## Inputs` for the finder pass, stating that the
   brief copies the printed row's scope and naming each form the step script
   prints under the chosen option: under (a) both forms (`changed cpp-class
   files` for the first round, `session-changed C++ in <paths>` after it),
   under (b) the first-round form alone.
2. Under (b) only: change the `$oeScope` assignment to the first-round scope
   for every round and drop the `-PathPrefix` sub-bullet of worker step 1.

Change Workflow tier: Tier 1 under (a) — documentation of an existing input.
Under (b), Tier 2 — scoped tool behavior: one step-script row's scope. No
determinism, wire, or trust-boundary change; no C++, shader, build, `.pack`,
replay, or live-verification exposure.

## Critical files

- `.agents/skills/over-engineering-review/SKILL.md` (`## Inputs`)
- `.agents/skills/over-engineering-review/references/worker.md` (step 1)
- `.agents/scripts/Get-ChangeWorkflowSteps.ps1` (`$oeScope`)

## In scope

- Before the user answers: no tracked change.
- After the answer: the `## Inputs` `Scope` entry (Design 1), and under (b)
  only the `$oeScope` assignment and the worker step 1 `-PathPrefix`
  sub-bullet (Design 2).

## Out of scope

- The validator pass's inputs and scope.
- The scopes of every other step-script row.
- The since-point rules that decide whether the finder row prints.

## Acceptance criteria

- The executing session records the user's answer.
- `SKILL.md` `## Inputs` declares the finder's `Scope` in every form the step
  script prints for that row, and under (b) the step script prints one form.
- `Validate-Skill.ps1` returns `VALID` for `over-engineering-review`.
- The static-checks runner passes for the changed files.
