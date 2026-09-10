<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T21:12:20.367Z","dependsOn":[]} -->
# Conform the code-quality-metrics SKILL.md to the skill skeleton

## Context
An independent `/external-skill-creator` validate pass over the
`.agents/skills/code-quality-metrics/` package found two skeleton
non-conformances in its `SKILL.md`. Both are pre-existing and were outside the
approved scope of the session that observed them, whose Plan
`Documents/Plans/ChangeWorkflow/LandingCodeQualityHistoryRemoval.md` excludes
`/code-quality-metrics` as an on-request advisory skill from its
`## Out of scope`.

The package is a subagent skill: `.agents/skills/code-quality-metrics/references/worker.md`
exists, which by `.agents/references/skill-skeleton.md` `## Consumption shapes`
puts `Purpose`, `When to use`, `Inputs`, `Handoff`, and `References` in
`SKILL.md` and `Steps` and `Rules` in the worker file.

1. `.agents/skills/code-quality-metrics/SKILL.md:32-40` — the `## Handoff`
   section declares no extension field, fixed shared value, or narrowed row
   form. Its second half instead carries interpretation judgment about
   `excessDecisions` (net scope evidence, what a target decrease does and does
   not prove, and that it is not an outlier or Phase-0 hint metric).
   `.agents/references/skill-skeleton.md` `## Section order` item 5 reserves
   `Handoff` for extension fields, fixed shared values, and narrowed row forms,
   and item 6 assigns judgment no step owns to `Rules`, which for a subagent
   skill lives in `references/worker.md` per `## Section placement`.
2. `.agents/skills/code-quality-metrics/SKILL.md:1-47` — there is no `## Inputs`
   section, so the fields a dispatcher must supply — the mode
   (`Snapshot`, `Compare`, `BootstrapIdentity`, or the history `Contract`/
   `Generate`), the target path and scope kind (`Exact`, `Directory`,
   `Recursive`) for `Snapshot`, the targets file and full-SHA baseline for
   `Compare`, and the repository root every mode takes — exist only in the
   private `.agents/skills/code-quality-metrics/references/worker.md`. That
   breaks `.agents/references/skill-skeleton.md` `## Section placement`:
   "`SKILL.md` on its own must suffice for main to dispatch the worker".

## Design
Documentation-only edits inside the one package, in the order the skeleton's
section order gives.

Recommended for finding 1: move the `excessDecisions` interpretation sentences
verbatim in meaning into `references/worker.md` `## Rules`, and leave in
`## Handoff` only the reporting contract main acts on — report the result as
advisory evidence, name scope, coverage omissions, suppression reasons, and
comparison cohort before interpreting a delta, and never turn a metric into a
landing gate, person score, or automatic refactor instruction. The rationale is
that main dispatches and reads the returned evidence, while the worker is the
reader that applies the interpretation bar when producing it. An author of the
fix session may instead judge that one of those sentences is text main must
present verbatim and so stays inline; that judgment is theirs, but the default
recommendation is the move.

Recommended for finding 2: add an `## Inputs` section between `## When to use`
and `## Handoff` naming the task-brief fields above, referencing
`.agents/references/subagent-reporting.md` `## Task brief` for the brief form
rather than restating it, and citing the worker's per-mode invocation sections
for the exact parameter spellings rather than duplicating the command lines.
The section states which fields the dispatcher supplies; the worker keeps how
each is passed to the script.

Note for the fix session: `.agents/skills/code-quality-metrics/SKILL.md:36` is
cited by `Documents/Plans/ChangeWorkflow/LandingCodeQualityHistoryRemoval.md`
as the location of the no-gate rule. Keep that rule in `## Handoff` so the
citation stays true; only the `excessDecisions` interpretation moves.

## Critical files
- `.agents/skills/code-quality-metrics/SKILL.md` — the `## Handoff` section and
  the missing `## Inputs` section
- `.agents/skills/code-quality-metrics/references/worker.md` — the `## Rules`
  section that receives the moved interpretation
- `.agents/references/skill-skeleton.md` — the conformance standard, read only

## In scope
- In `.agents/skills/code-quality-metrics/SKILL.md`: the `## Handoff` section's
  `excessDecisions` interpretation text, and a new `## Inputs` section placed
  after `## When to use`
- In `.agents/skills/code-quality-metrics/references/worker.md`: the `## Rules`
  section, receiving the moved interpretation text

## Out of scope
- Every other section of `.agents/skills/code-quality-metrics/SKILL.md`,
  including `Purpose`, `When to use`, `References`, and the frontmatter
- The worker's `## Steps` and the per-mode invocation sections
- `.agents/skills/code-quality-metrics/scripts/**` and
  `.agents/skills/code-quality-metrics/references/MetricContract.md`,
  `Remediation.md`, `HistoryContract.md`
- The landing-time history mechanism owned by
  `Documents/Plans/ChangeWorkflow/LandingCodeQualityHistoryRemoval.md`
- Any change to what the metrics mean, how they are computed, or whether a
  metric may gate anything
- Any other skill package

## Risk tier and invariants
Expected Tier 1. The trigger from `.agents/references/risk-tiers.md` is
"documentation": the change moves and adds instruction prose within one skill
package and alters no script, no metric definition, and no runtime behavior.
Invariants: every action the current body requires survives exactly once across
`SKILL.md` and `references/worker.md`; the no-gate rule stays in `## Handoff`
so the citation from the landing Plan remains true; section order stays the
skeleton's order with no section reordered.

## Acceptance criteria
- `.agents/skills/code-quality-metrics/SKILL.md` has an `## Inputs` section
  naming every field a dispatcher must supply, and a `## Handoff` section
  holding only extension fields, fixed shared values, or narrowed row forms
- The `excessDecisions` interpretation appears exactly once in the package
- An `/external-skill-creator` validate pass over the package reports no finding
  against `.agents/references/skill-skeleton.md` `## Section order` item 5 or
  `## Section placement`
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `validate-skill` and `markdown-links` rows passing for the package
