# Grill Plan Worker

The grill steps, the decision checklist, and the closing checks. Triggers,
inputs, the interview question format, and the handoff form live in
[`../SKILL.md`](../SKILL.md).

## Steps

1. Read the complete frozen plan text, audit result, finding decisions,
   execution card, applicable instructions, and every cited repository region
   needed to test its assumptions. Done when each of those has been read.
2. For a bug fix, rank three to five checkable causal hypotheses internally.
   Done when that ranked list exists, or the plan is not a bug fix.
3. Verify those hypotheses' predictions from code, logs, or supplied
   diagnostics. Done when each prediction is confirmed or refuted.
4. Ask about a cause only when multiple live causes would meaningfully change
   implementation or verification. Done when every cause asked about meets that
   bar.
5. Run the gate in [`library-gate.md`](library-gate.md) before other questions
   when the plan adds or rewrites a non-trivial subsystem, algorithm, or data
   structure. Done when that gate has run or the plan does not trigger it.
6. Search the repository until local evidence either resolves a candidate
   decision or proves that user judgment is required. Do not ask the user to
   rediscover code facts. Done when every candidate decision is evidence-resolved
   or proven to need user judgment.
7. Scan remaining decisions through the compact checklist in
   `### Decision Checklist` and map them as a dependency tree. Done when every
   remaining decision sits in that tree.
8. Return the current frontier — every decision whose prerequisites are already
   settled — as the round's `Interview questions`. Done when the whole frontier
   is in that handoff.
9. On resume with the round's answers, record each selected choice and the exact
   plan refinement it implies. Done when every answer carries a recorded choice
   and refinement.
10. Recompute the frontier and return the next round. Done when no decision with
    settled prerequisites remains unreturned.
11. Run the closing checks in `### Closing Checks`. Done when each of them is
    internally verified.
12. Return all refinements to the manager. Done when every refinement is in the
    handoff.

## Rules

- Load an exact-path plan once and treat those bytes as the frozen plan text.
  Never edit the plan, scheduler claims, code, or any other repository file.
  Audit findings are decision inputs, not permission to patch that text. If the
  supplied revision differs from the audited revision, the audit did not
  complete, or the plan is not Tier 3, stop and return the needed correction to
  the manager.
- A pending `locator` `/verify-external-claims` verdict is an unsettled
  prerequisite that blocks only the decisions depending on it; keep interviewing
  the rest of the frontier while it runs.
- The manager owns incorporation and approval validity. A design or library
  pivot additionally requires a fresh `/plan-audit` before this skill re-enters.
- Do not manufacture optional improvements, future extensibility, or routine
  confirmation questions. Stop when every meaningful branch is evidence-resolved,
  user-resolved, or returned as a named pivot. The plan's own declaration that a
  branch is decided resolves nothing; such a branch counts as evidence-resolved
  only when independent repository evidence settles it, and as user-resolved only
  through the required inputs in [`../SKILL.md`](../SKILL.md), per the
  authority-order directive in root `AGENTS.md`. If no meaningful decision
  survives, return a no-delta handoff without asking the user anything.

### Decision Checklist

Probe only surfaces the plan actually touches:

- Shape and ownership: unresolved alternatives; owning layer, translation
  unit, type, or phase; delete versus reserve; interface shape; reuse of an
  existing `common::` or vendored mechanism.
- Behavior and scale: hidden semantic changes, ordering/error/default
  behavior, unjustified constants or granularity, unbounded sparse-world scale,
  and kilometer-scale coordinates.
- Determinism and compatibility: client/server equivalence, floating-point
  or iteration order, CRC participation and `LogDifferences`, wire IDs,
  `kiVersion`/pack layout, save/replay compatibility, and interpolation versus
  snapping.
- Execution safety: Update/PostRender/Interpolate phase, dispatch ownership
  and shared writes, collection allocation/copy/spawn/transfer alignment,
  main-loop allocation tracking and workbuffer use.
- Integration: client/server guard scope, build/project/filter membership,
  PCH include placement, engine/game layer boundaries, consumers and mirrored
  sites, roles, and acceptance checks with independent signals for duplicates.

If the interface or system design itself is unresolved, do not improvise it.
Return a design pivot naming the exact questions for
`/external-design-interface`. Its result must be incorporated into a new plan
revision and pass a fresh `/plan-audit` before re-entry.

### Plan Context

The preparation `implementer` never runs a command that changes scheduler state;
the idempotent claim invocation belongs to main, per `/next-plan`.

Consult a referenced Plan only when the plan declares a dependency, shares
files or symbols with another Plan, or the closing checks reveal likely overlap.
Never read machine-local scheduler claims. `Documents/Features` is manual and
outside scheduler inventory.

### Closing Checks

Internally verify, without a routine final question:

- adjacent producers, consumers, shared headers, and mirrored client/server
  paths are covered;
- a deletion, existing mechanism, or verified library does not eliminate the
  proposed custom work;
- every exposed determinism, layout, protocol, replay, affinity, threading,
  trust-boundary, or allocation invariant is explicit;
- assumptions remain valid for unbounded counts, sparse-cell parallelism, and
  world-coordinate magnitude;
- roles and acceptance checks match the Tier-3 triggers.

Surface only a concrete meaningful decision established by current evidence. Feed
it through the same two-or-three-choice interaction contract.
