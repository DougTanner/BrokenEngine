<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-26T14:01:14.616Z","dependsOn":[]} -->
# Over-engineering validator reports declarations its removals leave unread

## Context

`.agents/skills/over-engineering-review/references/worker.md` step 7 applies
each proven candidate's form and rereads the edited file; no step checks what
the removal left without a reader or writer. `### Drop reasons` also drops an
edit to "a declaration whose users lie outside" the session-changed regions,
so a field whose last reader the validator removed is never reported.

Observed instance (squash `81c17f92c`): the validator removed the dead
`rTrailLayout.uiColor = static_cast<uint32_t>(rType.iColor);` store from
`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp` (finder
candidate OE-2). That orphaned `SmokeTrailsType::iColor`
(`Engine/Source/Frame/Collections/SmokeTrails/SmokeTrails.h:15`), a file the
session did not change. The current tree has no reader or writer of
`SmokeTrailsType::iColor` (`git grep -n iColor` over `Engine/` and
`Projects/BrokenEngineSandbox/` finds only the declaration for this type). The
later repo-code-review and adversarial passes raised only the
`Documents/Features/Graphics/SmokeColorBySource.md` doc claim, which was fixed
to describe the unset field.

## Design

After step 7's removals, the validator `git grep`s each field, member, or
declaration whose last reader or writer a removal deleted, and reports every
one left with no reader as a validator design suggestion in the
`../SKILL.md` `## Handoff` `Design suggestions` rows, even when it lies outside
the session-changed regions. It never deletes such a declaration itself: the
existing drop reason for code outside the regions stays, so main decides the
follow-up. The author recommends a new sub-bullet or sentence in step 7 rather
than a new step, keeping the validator's step count and the handoff form
unchanged.

## Critical files

- `.agents/skills/over-engineering-review/references/worker.md`

## In scope

- `.agents/skills/over-engineering-review/references/worker.md` `## Steps`
  step 7: the orphaned-declaration search and its report as a design
  suggestion.
- `.agents/skills/over-engineering-review/SKILL.md` `## Handoff` validator
  `Design suggestions` description only if its wording excludes this source.

## Out of scope

- Deleting `SmokeTrailsType::iColor`; that field is tracked by
  `Documents/Features/Graphics/SmokeColorBySource.md`.
- Re-running the over-engineering review after fix rounds (a separate gap).
- The finder steps 1-3 and `### Drop reasons`.

## Acceptance criteria

- Step 7 names the search for declarations whose last reader or writer a
  removal deleted and requires each to appear as a design suggestion.

## Notes

- Change Workflow tier: Tier 2 (scoped tool behavior: one review skill).
- Run `/external-skill-creator` validation after the skill package change.
