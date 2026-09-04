---
name: external-grill-plan
description: >-
  Review an immutable, plan-audited Tier-3 implementation plan and interview
  the user about decisions that still require judgment. Use after /plan-audit
  for every Tier-3 change; return exact refinements or a decision-complete
  no-delta result. Do not use for Tier-1 or Tier-2 work.
allowed-tools: [Read, Grep, Glob, Agent, PowerShell, AskUserQuestion]
---

# Grill Plan

## Purpose

Resolve the meaningful decisions that remain after repository exploration and a
fresh `/plan-audit`. The main session owns every user interaction.

## When to use

- After `/plan-audit` for every Tier-3 change.
- Never for Tier-1 or Tier-2 work.

## Inputs

Require all of the following before starting:

- an immutable Tier-3 plan supplied inline or by exact repository path;
- evidence that `/plan-audit` reviewed this exact plan revision, plus the
  manager's decision on every finding;
- the text of every Plan the plan references, carried in the preparation
  `implementer`'s brief;
- user intent, applicable repository instructions, session baseline when one
  exists, and any changes the user approved after the plan;
- the draft execution card, carrying every field of the card template in
  [`../next-plan/SKILL.md`](../next-plan/SKILL.md) `## Handoff`.
- Roles: execution splits by role — the preparation `implementer` performs every
  repository read and search this skill requires and returns immutable decision
  briefs; the main session interviews the user, decides, and dispatches the
  `locator` for external-claim requests. Delegates may locate evidence or verify
  a single checkable external claim, but never interview the user or choose for
  them. Role contract: `../next-plan/references/tier3-workflow.md`.

## Handoff

Return the block below inline. Interview questions carry their full wording in
the block, because the manager runs the interview from that text and must not
read a file mid-question. The decision-and-refinement rows, the execution-card
rows, the execution card itself, the refined implementation plan, reproduction
recipes, and any other bulk evidence go to one file under `Temp/`, returned as
that path plus a per-section selector per
`../../references/subagent-reporting.md`, never as inline text. `Plan delta` is
relative to the frozen plan text; any behavior, scope, architecture,
acceptance, or verification change is meaningful.

```text
Plan: <exact path or inline plan title>
Plan-audit evidence: <audited plan revision and result>
Plan delta: none | not meaningful | meaningful
Interview questions:
- <full question in the `### Decision Interaction` shape, or none>
External claim verdicts:
- <stable claim ID> — VERIFIED | REFUTED | UNRESOLVED — <direct implication>
Decision detail file: <Temp/ path> — <decision-rows selector> — <execution-card-rows selector>
Required next step: none | incorporate refinements | incorporate library integration pivot and run fresh /plan-audit | run /external-design-interface, incorporate its design pivot, and run fresh /plan-audit
Files changed: none
Functions/regions touched: none
Residuals:
- <unresolved decision or none>
```

That file keeps one row per decision
(`<decision ID> — <selected choice> — <section and exact refinement>`) and one
row per execution-card decision
(`<Tier-3 trigger/role/criterion -> decisive check -> expected result; independent signal when duplicated>`).

Questions, answers, exact refinements, pivots, and claim verdicts remain live
handoff data, inline or in that file. Never collapse them into a prose summary.

### Decision Interaction

Lead with the first concrete decision, not a plan summary. For each question:

- state the repository evidence and why the choice changes the plan;
- offer exactly two or three meaningful, mutually exclusive choices;
- put the recommended choice first and explain its tradeoff;
- make every choice name the exact refinement it would produce;
- say what the answer changes or blocks, in plain, non-expert language;
- keep the question answerable from the current message alone, without the user
  recalling earlier turns or reading the plan or the source.

Use the host's structured choice UI when available. Make the structured-choice
tool call only after the round's full context is already visible to the user as
rendered message text, and let that call carry only the choices; text emitted in
the same turn before such a call may never be displayed, so it does not count.
Otherwise ask in prose but preserve the same two or three choices, in the same
recommended-first order, with the same tradeoffs and refinements. Do not replace
them with an open-ended prompt. In prose, give every question in the round one
fixed numbered shape so the user can answer by number:

```text
Q<n> — <decision title>: <evidence, why the choice changes the plan, and the
two or three choices>
-> Recommended: <choice> — <tradeoff>
```

Interview in rounds: each round asks the whole frontier — every decision whose
prerequisites are already settled — in one interaction, splitting across
consecutive interactions only when the host UI caps questions per call. After
each round's answers, recompute the frontier and ask the next round, until no
decision with settled prerequisites remains unasked. A decision whose answer
depends on another decision still open in this round belongs to a later round.

An exact refinement names the affected plan section and the concrete decision,
invariant, affected sites, or criterion/check text to add, replace, or remove.
Do not return summaries such as "clarify threading" or "use option A."

## References

- [`references/worker.md`](references/worker.md) — the grill steps, the decision
  checklist, and the closing checks.
