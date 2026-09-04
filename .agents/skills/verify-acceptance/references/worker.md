# Verify Acceptance Worker

Steps and rules for the dispatched reviewer. The public [`SKILL.md`](../SKILL.md)
owns the triggers, inputs, and handoff form.

## Steps

1. List the approved criteria and invariants exactly as approved, one row each,
   splitting a criterion that carries two separable claims. Done when every
   approved item has its own row and nothing is merged, reworded, or dropped.

2. Map each row to evidence that settles it on its own: a read of the changed
   bytes, a command's output, a script or validator result, or a harness
   observation. A check that settles the row only together with an assumption,
   or only alongside another row's check, is not decisive — replace it. Done
   when every row names a check, its expected result, and the result observed.

3. Where two or more checks cover the same criterion, each needs its own
   independent signal — a different command, file, or observation, so one
   failing silently cannot be hidden by the other. Done when every duplicated
   check cites a distinct source.

4. Keep every check inside the evidence ceiling the change's tier allows, and
   run only checks that change no tracked file. Done when no row's evidence
   exceeds that tier's ceiling.

5. Fill the `Criteria` section and set `Status` per the public `SKILL.md`
   handoff. Done when every row carries evidence and a verdict.

## Tier evidence ceiling

Tier 1 checks are static, schema, link, validator, and compilation of changed
C++; Tier 2 adds the smallest observable scenario; Tier 3 adds exposed invariant
or integration checks.

## Rules

- Read-only: never edit a file and never fix a failing criterion. An unsettled
  criterion is a `Findings` row for main to route.
- An assertion that the work was done — an implementer's claim, a handoff
  summary, a plan saying the change would do it — is not evidence. Name the
  observation instead.
- A criterion whose check has not been run yet fails with the missing check
  named; it is never a pass with a caveat.
- Evidence must come from the stage's current bytes. Re-read or re-run a check
  whose inputs changed after it first ran.
- Never produce, reuse, or update a landing acceptance table, per
  [`../SKILL.md`](../SKILL.md) `## When to use`.
- Report a proven leftover that no approved criterion covers under `Residuals`.
