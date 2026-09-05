# Resolve Findings Worker

Fix steps and rules for the worker dispatched with
[`../SKILL.md`](../SKILL.md), which owns the purpose, the required assignment,
and the handoff form.

## Steps

1. Confirm a checkable root cause for each assigned item before editing it. Done
   when every item has a named cause, or is left unchanged because its cause is
   uncertain or out of scope.
2. Apply the smallest change that restores approved behavior for each item with
   a confirmed cause. Done when the edit is complete within the assigned files
   and functions.
3. Check the sites the edit affects. Done when each one is consistent or
   recorded as an out-of-scope candidate for the manager.
4. Return build and runtime verification to the manager instead of treating it
   as done here. Done when each required build or runtime check is named in the
   handoff.
5. Audit the completed edit against the assignment, session baseline, and
   smallest plausible regression. Done when the result is reported under
   `Self-audit resolved`.

## Rules

- The manager owns finding decisions and scope changes, and dispatches builds,
  independent verification, and all fresh-context review to their assigned
  roles.
- For C++ fixes, the conventions to apply are in
  [`../../../references/cpp-conventions.md`](../../../references/cpp-conventions.md).
- Resolve conflicting sources by the authority order in root `AGENTS.md`
  `### Diagnosis Discipline`, naming the contradiction and the controlling
  source.
- Do not establish disputed external API, language, specification, or library
  behavior from memory: emit one single-claim request per
  `/verify-external-claims` (`../../verify-external-claims/SKILL.md`,
  `## Inputs`). A pending verdict keeps the item unresolved and
  the handoff `NEEDS_ACTION`.
