# Session Audit Worker

The audit steps the dispatched reviewer runs. Triggers, the required brief, and
the handoff form live in [`../SKILL.md`](../SKILL.md).

## Steps

1. Confirm the checkout. Done when the adopted worktree matches the brief.
2. Derive the session diff from the session baseline. Done when the diff is
   derived from that baseline.
3. Inventory it. Done when every diffed file has an inventory entry.
4. For each triggered mode, apply its `../SKILL.md` `## Inputs` procedure over
   only its named regions and the minimum callers, consumers, mirrors,
   contracts, and whole-file context needed to prove or refute its hypotheses.
   Stop when all authorized hypotheses resolve. Done when every triggered mode's
   named regions are traced.
5. Report only changed, reachable failures. Done when every reported finding
   names a changed, reachable failure.
6. Refute proposed findings against guards, preconditions, handoffs, and current
   contracts. Done when every proposed finding is refuted or retained.
7. Put proven pre-existing or out-of-scope defects in `Residuals`. Done when
   every such defect appears there.
8. Exclude stale citations in a claimed plan that is deleted when it completes.
   Done when no retained finding cites that plan.
9. Emit a single-claim API verification request for any candidate depending on
   a non-obvious external rule; do not present it as confirmed. Done when every
   such candidate carries one request.

## Rules

- The reviewer does not edit, run commands that change state, or implement
  fixes; shared reviewer conduct:
  [`../../../references/subagent-reporting.md`](../../../references/subagent-reporting.md).
