<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T19:41:11.171Z","dependsOn":[]} -->
# Fix: next-plan — step 4 names no card file for a preparation run that cites no scratch snapshot

## Context
`.agents/skills/next-plan/SKILL.md` step 4 instructs the preparation
`implementer` to write "the complete resolved Plan ... and the complete
execution card" into one gitignored `Temp/` file only conditionally: "When the
preparation handoff will cite a scratch snapshot as the Plan review reviews'
plan input rather than the claimed Plan path" (lines 87-98 at baseline
647ff083). The same step's Done condition (line 106 at baseline) is
unconditional: the run is done only when "the preparation handoff cites it as
one file path plus `##` selector", and the file's `## Handoff` section likewise
has the card cited under `Evidence` as a path plus `##` selector in every run.

So a preparation run that does not cite a scratch snapshot must still cite a
card file, and no step tells it which file that is or has it write one. The
claimed Plan is immutable (step 4, line 67 at baseline: "The Plan is immutable
... every contradiction returns to main as a card correction rather than an
edit") and a Plan file carries no execution card, so the claimed Plan path
cannot satisfy the Done condition either. The worker is left to invent a card
file path, and the Done condition's unconditional requirement cannot be
evaluated from the steps alone.

This is a pre-existing gap in the run steps, not a symptom of the change that
observed it: the change that fixed the `## Handoff` declared fields lists
"every other section of `.agents/skills/next-plan/SKILL.md`, including ... the
run steps" under its `## Out of scope`, and the conditional wording and the
unconditional Done condition both predate it.

## Design
First confirm from the current tree that the conditional write in step 4 and the
unconditional card citation in its Done condition and in `## Handoff` still
disagree; the line numbers above are from baseline 647ff083 and the `##
Handoff` section has since changed the card's carrier field to `Evidence`.
Then make the smallest fix inside the `## In scope` boundary below.

Recommended fix, because it removes the branch rather than documenting a second
route, and because the snapshot file is the only artifact in the step that
already exists to hold the card: make the resolved-Plan-plus-card snapshot
unconditional — delete the "When the preparation handoff will cite a scratch
snapshot ... rather than the claimed Plan path" condition so every preparation
run writes that one gitignored `Temp/` file and cites it, and drop the matching
conditional clause from the Done condition so the `Test-PlanCitations.ps1`
heading check applies to every run. That leaves one route, one cited file, and
one Done condition with nothing to branch on.

Authorship note for the fix session: the recommendation above is this Plan
author's, with the rationale stated. The alternative is to keep the condition
and add a sentence naming the file a non-snapshot run writes the card into and
cites; the fix session may take it, but must then state whether the
`Test-PlanCitations.ps1` check applies to that file, since that script checks
the copied `## In scope`/`## Out of scope` headings a non-snapshot card file
would not carry.

Whichever option is taken, the card's single carrier field stays as the current
`## Handoff` section defines it; this Plan does not reopen that choice.

## Critical files
- `.agents/skills/next-plan/SKILL.md`

## In scope
- The confirmation `## Design` states
- The smallest resulting fix, confined to step 4 of
  `.agents/skills/next-plan/SKILL.md` — the conditional snapshot-write
  paragraph and the step's Done condition — plus the `## Handoff` section's
  card-citation sentence only where the chosen option forces it

## Out of scope
- `.agents/skills/next-plan/SKILL.md` steps 1-3 and 5-7, its claim-disposition
  table, and the `### Execution card presentation/template` card content
- Which field carries the card path in the preparation handoff, and the
  declared extension-field list
- `.agents/references/subagent-handoff.md`, `.agents/references/subagent-reporting.md`,
  and `.agents/skills/plan-audit/scripts/Test-PlanCitations.ps1` behavior
- Unrelated skills and scripts; any transcript path or home path in the repo

## Risk tier and invariants
Expected Tier 2 (scoped tool behavior — one skill's run-step behavior);
escalate if the fix reaches the shared handoff form or the plan-audit script.
The snapshot file stays a gitignored `Temp/` file addressed by its path from the
worktree root, and the claimed Plan stays immutable — no option may have the
preparation worker edit the Plan to carry the card.

## Acceptance criteria
- Step 4's instructions and its Done condition agree for every preparation run:
  a run whose Done condition requires a cited card file is a run some step
  tells the worker to write
- No step has the preparation worker edit the claimed Plan or cite the claimed
  Plan path as the card file
- The static-checks runner, invoked as `.agents/references/static-checks.md`
  documents it, reports every row the change triggers passing

## Notes
Recorded from the coherence review of the change that fixed this skill's
`## Handoff` declared fields
(`Documents/Plans/ChangeWorkflow/NextPlanPreparationHandoffFieldCoverage.md`).
No `dependsOn` edge is recorded: that change lands first, and a Plan being
completed in the same run cannot carry a surviving edge.
