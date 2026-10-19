# Tier 3 Preparation

This reference owns only the Tier-3 additions to the Change Workflow in
[change-workflow.md](../../../references/change-workflow.md) and to
[`/next-plan`](../SKILL.md); Tier 3 adds no approval gate.

Read this reference only after `/next-plan` has claimed and classified a plan as
Tier 3. It prepares a high-risk change without making its administration the
main task.

## Execution card

On the card template in
[`execution-card.md`](../../../references/execution-card.md) `## Template`,
the Tier-3 card's trigger must be concrete and each acceptance check names its
expected observation.

Never edit the claimed plan file during execution; carry meaningfully stale
source-citation corrections in the execution card instead. Search for
an affected mirrored pattern only when the intended change alters a signature,
identity, semantics, layout, guard scope, or named invariant.

## Plan review

Main returns the accepted findings of the Plan review step's reviewers to the
preparation `implementer`. That worker performs every repository read and
search `/external-grill-plan` requires
and returns an immutable decision brief in the shape that skill's `## Handoff`
defines; it never interviews, chooses, or delegates.
Main routes external-claim requests to a `locator` through
`/verify-external-claims`, then dispatches a fresh preparation `implementer`
per round whose continuation capsule carries that skill's
[`## Inputs`](../../external-grill-plan/SKILL.md#inputs) for the round, and
receives the next brief. Main alone interviews the user and decides, recording
the exact questions, answers, decisions, and refinements; it performs no
repository read or search work, and the idempotent claim invocation `/next-plan`
assigns to main is the one scheduler command it still runs. The final handoff
preserves that record and the worker updates the card. A decision-complete plan
yields a PASS audit and a no-question brief; do not manufacture findings or
interview questions to justify the review.

## Implementation and stop rule

Implementation, its checks, and the fix/retest limit follow the Change Workflow
steps at Tier 3, which already include `/adversarial-review`; this route adds
nothing to them.
