<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T17:50:18.050Z","dependsOn":[]} -->
# Fix: the two collection skills restate a `Runtime acceptance requests` row form their owner no longer requires

## Context
`.agents/skills/add-collection/SKILL.md` `## Handoff` (line 41) and
`.agents/skills/add-collection-member/SKILL.md` `## Handoff` (line 37) each tell
the enclosing worker to name "one runtime-observable criterion per
`Runtime acceptance requests` row". Neither skill owns that field: both fold
their results into the enclosing `/implement-plan` handoff, and
`.agents/skills/implement-plan/SKILL.md` `## Handoff` (line 60) owns the field's
form. That owning declaration now reads: for each criterion that lives in a plan
file the handoff already cites, one citation row giving that path plus its `##`
selector for the covered set, then one row per such criterion whose setup,
action, observation, or required evidence the plan does not already state; only a
criterion with no cited home takes a row giving all four parts.

So the two sibling sentences state a per-row rule the owner no longer requires,
and a worker following them literally would emit a full criterion row where the
owner prescribes a citation row plus partial rows. The restatement is also a
progressive-disclosure duplication: the row form belongs to the owning
declaration, and the siblings should point at it rather than carry a second copy
that drifts whenever the owner is amended — which is exactly what happened here.

## Design
The author's recommendation is a documentation-only fix in the two sibling
`## Handoff` sentences: replace the "one runtime-observable criterion per
`Runtime acceptance requests` row" clause with a deferral to the form
`/implement-plan` `## Handoff` declares, while leaving each sentence's other
obligations intact — the `Build required` targets, the skill-specific invariants
under `Reviewer focus areas`, and the `Residuals` items. Each sibling already
links `/implement-plan`, so the deferral can reuse that link and needs no new
reference. Keep the two sentences parallel with each other, as they are today.

Whether the deferral names the field and the owning section explicitly or only
points at the owner's `## Handoff` is a wording choice for the fix session, as is
whether the clause keeps naming the field at all.

## Critical files
- `.agents/skills/add-collection/SKILL.md` — `## Handoff`, the sentence at line 41
- `.agents/skills/add-collection-member/SKILL.md` — `## Handoff`, the sentence at
  line 37

## In scope
- The `Runtime acceptance requests` clause of the `## Handoff` sentence in
  `.agents/skills/add-collection/SKILL.md`
- The `Runtime acceptance requests` clause of the `## Handoff` sentence in
  `.agents/skills/add-collection-member/SKILL.md`

## Out of scope
- `.agents/skills/implement-plan/SKILL.md`, whose amended owning declaration this
  Plan defers to rather than changes
- The rest of each sibling's `## Handoff` sentence: `Build required`,
  `Reviewer focus areas`, and `Residuals`
- Both skills' `references/worker.md` and every other section of the two skills
- Any other skill that mentions runtime acceptance criteria

## Risk tier and invariants
Tier 1 (mechanical: skill documentation prose, no public signature or invariant
exposure). The two sibling sentences stay parallel, and no fact the owning
declaration holds is copied back into either sibling.

## Acceptance criteria
- Neither sibling `## Handoff` states a per-row form for
  `Runtime acceptance requests`; each defers to `/implement-plan` `## Handoff`
- Each sibling still names its `Build required` targets, its skill-specific
  `Reviewer focus areas` invariants, and its `Residuals` items
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing
