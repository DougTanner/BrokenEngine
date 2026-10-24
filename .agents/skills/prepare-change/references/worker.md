# Prepare Change Worker

The preparation steps and judgment rules the dispatched `implementer` runs.
Triggers, inputs, and the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Read the request text or plan document [`../SKILL.md`](../SKILL.md)
   `## Inputs` names, then `### Risk tiers` in
   [risk-tiers.md](../../../references/risk-tiers.md), and every
   repository region the request names. Done when each of those has been read.
2. Gather the tier evidence: for each risk-tier trigger the change could touch,
   cite the `repository-path:line` that proves it present or absent, and
   classify at the highest applicable tier. Done when every such trigger carries
   its citation and the tier follows from them.
3. For a Tier-2+ plan preparation, draft the implementation plan into one file
   readable from the worktree root (a gitignored `Temp/` path is fine), carrying
   `## In scope` and `## Out of scope` verbatim, each concrete enough to test a
   diff region against. Done when that file exists with both headings filled to
   region level, or the dispatch was made only to classify the work and steps 3
   to 5 are skipped.
4. Draft the execution card into the same file, headed and filled as
   [`../../plan-audit/SKILL.md`](../../plan-audit/SKILL.md) `## Inputs` requires,
   on the field template in
   [`../../../references/execution-card.md`](../../../references/execution-card.md)
   `## Template`, with
   each acceptance check inside the tier's evidence ceiling in
   [`../../verify-acceptance/references/worker.md`](../../verify-acceptance/references/worker.md)
   `## Tier evidence ceiling`. Done when every template field is filled under
   that heading.
5. Confirm the drafted file's structure with one run from the worktree root:

   ```powershell
   pwsh -NoProfile -File .agents/skills/plan-audit/scripts/Invoke-PlanCitationsCheck.ps1 <drafted plan path>
   ```

   Write in whatever `headings.inScopePresent`, `headings.outOfScopePresent`, or
   `card.missingFields` reports as absent, then re-run. Done when both heading
   values are true and `card.missingFields` is empty.

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
  when a plan is drafted, each small one goes into its `## In scope`, named in
  the card's `### What does this plan do?`, so the Plan review step reviews it;
  every other one, and every small one when no plan is drafted, returns under
  `Residuals`.
- Delegation conduct is in
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md);
  never route work to another worker.
