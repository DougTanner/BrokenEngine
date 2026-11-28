<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T21:30:00.766Z","dependsOn":[]} -->
# Give mapped and rejected follow-up proposals a completion route

## Context

`.agents/skills/create-follow-up-plans/SKILL.md` `## Handoff` lets a
proposal end in one of three ways: a created Plan, an updated existing Plan, or
an outcome with no Plan write (a duplicate mapping, or a rejection reported as
a residual). The worker steps in
`.agents/skills/create-follow-up-plans/references/worker.md` `## Steps` give
only the first two a path to done:

- Step 1 (worker.md:25-28) maps a proposal to an existing Plan that owns the
  same root cause and implementation boundary, and extends that Plan only when
  the proven unmet acceptance criterion requires it. `## Rules` (worker.md:144)
  rejects each proposal class `SKILL.md` `## When to use` excludes.
- Step 2 `### 2. Draft and classify` runs for every surviving proposal and
  directs creating the file with `.agents/scripts/New-PlanFile.ps1`
  (worker.md:70-74), and its done condition is "the Plan body exists"
  (worker.md:81). Step 3 is where the worker picks a case, so step 2 directs a
  new Plan file before the route is chosen, including for a proposal that
  step 3 then routes to an existing-Plan edit.
- Step 3 `### 3. Apply exactly one case` (worker.md:84-90) has three rows, all
  of which write Plan bytes: New Plan, Existing Plan prose only, Existing Plan
  dependency change. Its done condition (worker.md:110) requires "exactly one
  row's route was taken for each proposal". A mapped duplicate that needs no
  extension and a rejected proposal match no row, so a worker following the
  steps literally either cannot finish or writes a Plan the skill's own
  outcomes forbid.

The defect predates the change that last edited this file, which only shortened
one step 2 sentence at worker.md:52. It was found by an
`/external-skill-creator` validate-mode review of the `create-follow-up-plans`
package.

## Design

Recommended edit, kept inside steps 1, 2, and 3:

1. End a mapped duplicate that needs no extension, and a rejected proposal
   (including a capability addition `## Rules` reports for main-agent
   routing), at step 1: its done condition records the mapping or the
   rejection reason and, for a tooling-friction proposal, that its incident
   rows were deleted as the `WorkflowIncidents.md` header directs for a mapped
   or rejected key; such a proposal skips steps 2 and 3. The author recommends
   this over adding no-write rows to the step 3 table, because those outcomes
   need no drafting and the table's columns describe Plan bytes.
2. Open step 2 with one sentence naming what each route takes from it: a new
   Plan takes every paragraph; an existing-Plan prose edit skips the area and
   filename choice and drafts only the sections its extension changes, under
   the same content rules, re-deriving the Plan's tier; a dependency change
   drafts only the `dependsOn` entries and the reciprocal `## Coordination`
   prose step 3's row allows, and records no tier trigger. Reword the
   dependency sentences so a new Plan passes its dependencies through
   `-DependsOn` and an existing Plan's dependency change writes them into the
   marker by step 3's row, keeping the `## Unresolved decisions` and
   completing-Plan exclusions for both. Move the `New-PlanFile.ps1` invocation
   block and its parameter paragraph into step 3's New Plan route. Reword the
   done condition so each route's draft meets it, requiring the tier trigger
   only on a route that records one. The tooling-friction body sentence stays
   unchanged: `## Rules` rejects a re-observed tooling-friction duplicate, so
   such a proposal reaches step 2 only as a new Plan.
3. Scope step 3's done condition to the proposals that reach it, keeping its
   incident-row deletion for a tooling-friction proposal that reaches step 3.

Change Workflow tier: Tier 1 — documentation: skill instruction prose in one
worker file. No C++, shader, build, `.pack`, determinism/CRC, wire, replay,
trust-boundary, or live-verification exposure.

## Critical files

- `.agents/skills/create-follow-up-plans/references/worker.md`

## In scope

- `## Steps` `### 1. Prove and consolidate proposals`: the duplicate paragraph
  and the done condition.
- `## Steps` `### 2. Draft and classify`: a new opening sentence naming what
  each route takes, the dependency sentences of the tier-and-dependency
  paragraph, the `New-PlanFile.ps1` invocation block, its parameter
  paragraph, and the done condition.
- `## Steps` `### 3. Apply exactly one case`: the New Plan row's script
  reference and the done condition.

## Out of scope

- `.agents/skills/create-follow-up-plans/SKILL.md`, whose `## Handoff` already
  names the three outcomes.
- `.agents/references/new-plan-file.md` and `.agents/scripts/New-PlanFile.ps1`.
- The tooling-friction template, step 2's tooling-friction body sentence, and
  every `## Rules` bullet: the rejection rule's "stating why" is met by step
  1's done condition recording the reason.

## Acceptance criteria

- Every proposal outcome `SKILL.md` `## Handoff` names — a created Plan, an
  updated existing Plan, a duplicate mapping, a rejection — has a step whose
  done condition it can meet, and each of those done conditions requires a
  tooling-friction proposal's incident rows deleted.
- `New-PlanFile.ps1` and `-DependsOn` are used only on the new-Plan route, and
  an existing Plan's dependency change follows step 2 without them.
- Every step 2 instruction is one an existing-Plan prose edit or dependency
  change can follow, or the opening sentence excludes it for that route.
- `Validate-Skill.ps1` returns `VALID` for `create-follow-up-plans`.
- The static-checks runner passes for the changed file.

## Notes

The `/external-skill-creator` validate-mode review is the validation route for
the changed package.
