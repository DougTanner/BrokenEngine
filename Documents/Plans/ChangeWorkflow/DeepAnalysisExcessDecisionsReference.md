<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-10T22:55:12.760Z","dependsOn":[]} -->
# Replace the restated excessDecisions interpretation in external-deep-analysis with a reference

## Context
The root `AGENTS.md` progressive-disclosure directive requires each fact —
including a genuinely new term's definition — to live once at its owning layer
and be referenced elsewhere.

`.agents/skills/code-quality-metrics/references/MetricContract.md:61-69` owns the
`excessDecisions` metric: its definition, its `MetricValue` shape, and the
interpretation bar at lines 65-69 — "Excess decisions are net scope evidence:
unchanged means only no net decision removal, never redistribution without
source-diff evidence", plus what a target decrease does and does not prove.
Line 53 of the same file owns the related fact that excess decisions never enter
outlier or Phase-0 hint buckets.

`.agents/skills/external-deep-analysis/SKILL.md:66-68` is step 7 of its Phase 0:
"Treat `excessDecisions` as net scope evidence: unchanged means only no net
decision removal, never redistribution without source-diff evidence. Done when
every `excessDecisions` reading is judged that way." The first sentence restates
verbatim in meaning the interpretation `MetricContract.md` owns, so the fact
lives in two layers at once and the two copies can drift.

This was reported as a residual by the Tier-1 combined `/coherence-review` of the
session executing
`Documents/Plans/ChangeWorkflow/CodeQualityMetricsSkillSkeleton.md`. The
originating step was Verify the acceptance table; the unmet criterion was "a
repository-wide grep for the `excessDecisions` interpretation shows it once",
which passed at package scope and failed at repository scope only because of this
line. The finding is pre-existing and outside that Plan's approved boundary: its
`## Out of scope` excludes "Any other skill package", and the user approved that
scope, so the duplication was deliberately left standing rather than fixed there.

## Design
A documentation-only edit to one step of one skill file.

Recommended: rewrite `.agents/skills/external-deep-analysis/SKILL.md` step 7 so
the action still fires but the interpretation is cited rather than restated —
direct the reader to judge every `excessDecisions` reading by the interpretation
bar in `.agents/skills/code-quality-metrics/references/MetricContract.md`, and
keep the step's existing Done condition ("Done when every `excessDecisions`
reading is judged that way") unchanged so the step remains checkable. The
rationale is that `external-deep-analysis` is a consumer of the metric contract,
not its owner, and it already invokes the owning package's script in step 4; a
reference keeps the single owner and removes the drift risk without weakening the
instruction.

An author of the fix session may instead judge that the reference must name the
owning heading or line span rather than the file alone; that choice is theirs.
What is not open is leaving the interpretation restated in both places.

## Critical files
- `.agents/skills/external-deep-analysis/SKILL.md` — Phase 0 step 7, the only
  region to change
- `.agents/skills/code-quality-metrics/references/MetricContract.md` — the owning
  layer for the `excessDecisions` interpretation, read only

## In scope
- Step 7 of `## Phase 0: Scoped Metric Evidence` in
  `.agents/skills/external-deep-analysis/SKILL.md`, currently at lines 66-68

## Out of scope
- Every other step, phase, and section of
  `.agents/skills/external-deep-analysis/SKILL.md`, including steps 4-6 and 8-11
- `.agents/skills/external-deep-analysis/references/**`
- `.agents/skills/code-quality-metrics/**`, including any edit to
  `.agents/skills/code-quality-metrics/references/MetricContract.md` itself and
  to that package's `SKILL.md` and `references/` worker file
- Any change to what `excessDecisions` means, how it is computed, or how a Phase-0
  reading is acted on
- Renumbering the Phase 0 steps or moving step 7 to another phase

## Risk tier and invariants
Expected Tier 1. The trigger from `.agents/references/risk-tiers.md` is
"documentation": the change rewrites instruction prose in one skill file and
alters no script, no metric definition, and no runtime behavior.

Invariants: the step keeps its ordinal and its existing Done condition; the
action the current step requires — that every `excessDecisions` reading is judged
by the net-scope-evidence bar — still fires after the edit; the interpretation
itself continues to live in `MetricContract.md` unchanged; the reference resolves
to a file that exists.

## Acceptance criteria
- A repository-wide grep for the net-scope-evidence interpretation wording finds
  it only in `.agents/skills/code-quality-metrics/references/MetricContract.md`
- `.agents/skills/external-deep-analysis/SKILL.md` step 7 still names the
  judgment the reader must make and retains its Done condition
- `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` reports the
  `validate-skill` and `markdown-links` rows passing for
  `.agents/skills/external-deep-analysis/`

## Notes
No dependency edge is recorded. The reference target,
`.agents/skills/code-quality-metrics/references/MetricContract.md`, is named in
the `## Out of scope` of
`Documents/Plans/ChangeWorkflow/CodeQualityMetricsSkillSkeleton.md`, so that
Plan's execution cannot move or reword the interpretation this Plan points at,
and the two changes touch disjoint files.
