---
name: prepare-change
description: >-
  Prepare a change before Plan review: gather risk-tier evidence and draft the
  plan or resolved claimed-Plan snapshot with its execution card. Use for
  Change Workflow Approve and classify / Prepare and explore alternatives
  `planner` preparation (omit `mode:`), and `/next-plan` step 4 claimed-Plan
  preparation (`mode: claimed-plan`). Preparation only; never implements.
allowed-tools: [Read, Write, Edit, Glob, Grep, PowerShell]
---

# Prepare Change

## Purpose

One `planner` gathers risk-tier evidence and drafts the plan or resolved
claimed-Plan snapshot with its execution card before Plan review; an ordinary
Tier-1 classify-only dispatch returns the evidence alone.

## When to use

- Change Workflow Approve and classify and Prepare and explore alternatives
  `planner` preparation — Tier 2+, or any tier needing repository evidence to
  classify (omit `mode:`).
- `/next-plan` step 4 claimed-Plan preparation, redraft, and difference
  preparation (`mode: claimed-plan`).
- Delegated execution context:
  [`subagent-reporting.md`](../../references/subagent-reporting.md)
  `## Delegated execution context`.

## Inputs

Supply [`subagent-reporting.md`](../../references/subagent-reporting.md)
`## Task brief` fields plus:

- `mode: claimed-plan` — sole mode override for `/next-plan` step 4; omit
  `mode:` for ordinary preparation;
- triggered Change Workflow step and role assignments, as main states them;
- session baseline;
- Plan-review write-in: cite readable review outputs in `Continuation capsule:`
  per `## Task brief` above.

Ordinary preparation (omitted `mode:`) adds:

- user intent, quoted where exact wording binds;
- request text, or non-claimed plan document.

`mode: claimed-plan` adds:

- ordinary claim: claimed Plan path plus section line ranges from a heading
  lookup (`AGENTS.md` `## Environment`), written from the Plan's own citations
  (`path:line-range`). Main reads only Plan sections it must state from (such as
  the risk tier for roles) and only `## Task brief` above, names cited
  handoff/reporting references by path only, leaves them and target source to
  the `planner`, and reads source only for a decision the Plan and handoff
  cannot settle;
- difference re-entry (per
  [`claim-exceptions.md`](../next-plan/references/claim-exceptions.md)
  `## Changes requested at landing` or `completion.plan-cited`): the original
  claimed Plan path as an identity field (for the filename-based citation rule,
  including after Plan deletion), the existing `Temp/` snapshot with its
  `## In scope` and `## Out of scope` as source, the requested difference, and
  for `completion.plan-cited` whether the refused exit was completion or
  `-Reject`;
- scope-changing write-in or redraft: add `Slices` to `Return:` so the handoff
  returns the updated slice partition;
- evidence bounds: the card's verification evidence carries no verbatim source
  text except an acceptance item proposing replacement text; bound handoff
  verification to `Verification` rows without enumerating unaffected results.

## Handoff

Extend [`subagent-handoff.md`](../../references/subagent-handoff.md) with these
fields in every mode:

```text
Classification: Tier 1 | Tier 2 | Tier 3 — trigger, with repository-path:line evidence
Candidate zero: <`Mechanism` through `Pays off when` rows per `.agents/skills/plan-alternatives/SKILL.md` `## Handoff`; `Mechanism` obeys the `/plan-alternatives` brief exclusions in `.agents/references/change-workflow.md` `#### Step 2 — Prepare and explore alternatives` and names neither the plan file nor its `Temp/` snapshot; `none` when no plan was prepared>
```

### Ordinary preparation (omitted `mode:`)

```text
Prepared plan: <worktree-readable path with verbatim `## In scope` and `## Out of scope`; `none` when no plan was prepared>
Execution card: <card heading in the prepared plan per `/plan-audit` `## Inputs`; `none` when no plan was prepared>
Unresolved decisions: <one row each, decision and options for main, or none>
```

When prepared, main passes that one file to
[`../plan-audit/SKILL.md`](../plan-audit/SKILL.md) (`## Inputs`), never
inlining plan or card text in the handoff.

### Claimed-Plan preparation (`mode: claimed-plan`)

- `Claim` — Plan path (or none) with resolved state when claimed.
- `Verification` — one row per unresolved decision, contradiction needing a
  user decision or override, problem found gone or empty realized change,
  scope widened beyond the Plan's `## In scope`, and dropped or weakened Plan
  acceptance criterion or invariant, as
  `<kind> — <Plan citation> — <result and the change it requires>`; then
  `Snapshot corrections: <count>` for all other verified statements requiring
  card or implementation changes (written in the snapshot's corrections
  section, cited under `Evidence`); then `Unaffected statements: <count>`.
  Quantitative feasibility estimates are not repeated here (governed below).
- `Slices` — sole source of the slice partition (card `Roles` carries roles,
  not the split). One row per disjoint slice: owned snapshot items by section
  and label or bullet (never text) and allowed file scope; one row when not
  disjoint; `none` for an empty realized change; with a stage split, at least
  one row per in-scope stage naming its stage. Accepted Plan review or grill
  refinements write into the snapshot; return updated `Slices` whenever the
  brief names `Slices` in `Return:`.

`Candidate zero` also returns `none` for an empty realized change and for
every preparation handoff after the `/plan-alternatives` pick.

A quantitative feasibility estimate returned as a `Findings` row must name the
check proving the measured trial preserved the source document's rule or item
inventory, or report the estimate as unvalidated (dispatch expectation, not a
`/next-plan` step 4 Done gate).

Cite the snapshot under `Evidence` with one `##` selector per section, and
report `Invoke-PlanCitationsCheck.ps1`'s `headings.inScopePresent`,
`headings.outOfScopePresent`, and `card.missingFields` values as one
`Decisive checks` row.

On an unconfirmed-root-cause stop before the snapshot (worker
investigation-portion rule), return `Status: BLOCKED`, `Evidence: none`, and
one short `Residuals` row stating root cause is unconfirmed and diagnosis is
next, citing no card and returning no extension field above (overriding them).

### Execution card

In `mode: claimed-plan`, cite the card
([`execution-card.md`](../../references/execution-card.md) `## Template`) under
`Evidence`, never inline; main presents its first two sections per
[`### Implementation approval`](../next-plan/SKILL.md#implementation-approval).

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The steps and rules the dispatched
  worker follows.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  `## Task brief` — task-brief fields.
- [`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
  — the shared handoff form.
- [`../plan-audit/SKILL.md`](../plan-audit/SKILL.md) — the plan-file and
  execution-card contract this preparation must satisfy.
