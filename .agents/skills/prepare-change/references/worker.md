# Prepare Change Worker

The preparation steps and judgment rules the dispatched `implementer` runs.
Triggers, inputs, and the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Read the request text, or the plan document this change comes from when it is
   not a claimed executable Plan, then `### Risk tiers` and `### Steps` in root
   [AGENTS.md](../../../../AGENTS.md), and every repository region the request
   names. Done when each of those has been read.
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
   on the field template
   [`../../next-plan/SKILL.md`](../../next-plan/SKILL.md) `## Handoff` owns, with
   each acceptance check inside the tier's evidence ceiling in
   [`../../verify-acceptance/references/worker.md`](../../verify-acceptance/references/worker.md)
   `## Tier evidence ceiling`. Done when every template field is filled under
   that heading.
5. Re-read the drafted file and confirm the two scope headings, the card
   heading, and each filled card field are present in it. Done when that
   read confirms all three.

## Rules

- Preparation only: draft the plan and the card, and never implement any part of
  the change or edit a tracked file.
- Every plan statement and card field rests on evidence read in this session;
  cite the `repository-path:line` for anything a reader would otherwise take on
  trust.
- Leave a decision the request does not settle unresolved and return it for
  main, per `### Resolving Ambiguity` in root
  [AGENTS.md](../../../../AGENTS.md); an architectural choice is always the
  user's.
- Scope the plan to the smallest complete change, per the minimum-sufficient
  change directive in root [AGENTS.md](../../../../AGENTS.md).
- Delegation conduct is in
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md);
  never route work to another worker.
