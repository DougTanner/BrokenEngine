# Prepare Change Worker

The preparation steps and judgment rules the dispatched `planner` runs.
Triggers, inputs, and the handoff form live in [`../SKILL.md`](../SKILL.md).
When `mode: claimed-plan` is set, also read and apply
[`claimed-plan.md`](claimed-plan.md).

## Steps

1. Read the source [`../SKILL.md`](../SKILL.md) `## Inputs` names,
   `### Risk tiers` in [risk-tiers.md](../../../references/risk-tiers.md), and
   every named repository region. Done when each has been read.
2. Gather the tier evidence: for each risk-tier trigger the change could touch,
   cite the `repository-path:line` that proves it present or absent, and
   classify at the highest applicable tier. Done when every such trigger carries
   its citation and the tier follows from them.
3. For Tier-2+ ordinary preparation or any `mode: claimed-plan` run, draft the
   plan or snapshot to a worktree-readable file (`Temp/` for claims) with exact
   `## In scope` and `## Out of scope` headings concrete to region level. Done
   when written, or an ordinary Tier-1 classify-only run skips steps 3 to 5.
4. Draft the execution card into the same file, headed and filled as
   [`../../plan-audit/SKILL.md`](../../plan-audit/SKILL.md) `## Inputs`
   requires, on the field template in
   [`../../../references/execution-card.md`](../../../references/execution-card.md)
   `## Template`, with each acceptance check inside the tier's evidence ceiling
   in
   [`../../verify-acceptance/references/worker.md`](../../verify-acceptance/references/worker.md)
   `## Tier evidence ceiling`. Done when every template field is filled under
   that heading as the template states.
5. Confirm the drafted file's structure with one run from the worktree root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Invoke-PlanCitationsCheck.ps1 <drafted plan path>
   ```

   Write in whatever `headings.inScopePresent`, `headings.outOfScopePresent`, or
   `card.missingFields` reports as absent, then re-run; treat other records
   (including `citations.items` with `pathExists` or `lineExists` false) as
   advisory leads, not defects to clear and rerun. Done when both heading values
   are true and `card.missingFields` is empty.

## Rules

- Preparation only: draft the plan and the card, and never implement any part of
  the change or edit a tracked file.
- Every plan statement and card field rests on evidence read in this session;
  cite the `repository-path:line` for anything a reader would otherwise take on
  trust. For a guard, constant, or ordering the plan changes whose reason the
  code does not state, that evidence includes the commit that introduced it
  (`git log -S '<exact code text>' -- <file>`) and its message, or
  `no recorded reason` when the history is silent.
- Leave a decision the request does not settle unresolved and return it for
  main, per the `## Directives` trivial-choices bullet in root
  [AGENTS.md](../../../../AGENTS.md); which decisions go to the user is in
  [`change-workflow.md`](../../../references/change-workflow.md)
  `### Resolving Ambiguity`.
- Scope the plan to the smallest complete change, per the minimum-sufficient
  change directive in root [AGENTS.md](../../../../AGENTS.md).
- Apply the Change Workflow Verify the acceptance table step's Leftovers rule in
  [`change-workflow.md`](../../../references/change-workflow.md) to every
  leftover the preparation proves, such as the same defect at a sibling site:
  whenever a plan or snapshot is drafted, record each small one on the execution
  card as approved scope, named in `### What does this plan do?`, so the Plan
  review step reviews it, and on ordinary preparation also add it to
  `## In scope`; every other one, and every small one when no plan is drafted,
  returns under `Residuals`.
- Delegation conduct is in
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md);
  never route work to another worker.
