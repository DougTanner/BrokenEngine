# Jev on the `/plan-simplicity-review` dispatch trigger

Open question: is the "does this plan add new code or change non-documentation
behavior" decision a place to measure Jev's agreement with main, given that a
wrong answer can only err in the expensive direction? Part of the series in
`JevDecisionModelWorkflowUses.md`. Not piloted.

## The decision today

The Change Workflow Plan review step dispatches `/plan-simplicity-review`
"when the plan adds new code or changes non-documentation behavior … when
unsure whether a plan triggers it, dispatch it"
(`.agents/references/change-workflow.md`, Plan review step). The skill defines
both terms: new code is "a new tracked file, function, class, system, script,
guard or recovery path, or configuration surface absent at the session
baseline" (`.agents/skills/plan-simplicity-review/SKILL.md:33-36`); a skill
edit is behavior when it changes frontmatter, invocation, routing, a bundled
script, a workflow step, a contract, a trigger, or a threshold, and
documentation when it only rewords (`:48-55`). Main decides from the plan text.

## The question to Jev

Two `noul`s over the plan file, asked together: "does the plan add new code as
defined" and "does the plan change non-documentation behavior as defined",
each with the skill's definition as the `true` criterion and its
documentation-only counterpart as the `false` criterion. Dispatch when either
probability exceeds a deliberately low threshold, so the "when unsure,
dispatch" default survives. A plan file sits well inside the 32k-token state
limit, so no enumeration is needed.

## What still needs a full model

The review itself. Jev decides only whether to dispatch; the `reviewer`
subagent reads the plan and writes the findings.

A second question the same review could consume once the trigger measurement
exists: the worker must classify each plan step's stated problem as
`observed`, `credible exposure`, or `hypothetical`
(`references/worker.md:36-38`), from the evidence the step cites. That is one
`choice` per step over three concrete criteria and would order the steps the
reviewer reads first; it is deferred until the trigger's agreement rate is
known because it feeds a finding rather than a dispatch.

## The asymmetry

A false positive costs one reviewer dispatch, which is what the current rule
already chooses on purpose. A false negative skips a review the workflow
requires. So the saving is capped at the dispatches main would have made under
uncertainty, and the error runs only in the expensive direction. That makes
this a poor place to save anything and a good place to measure: Jev's answer
can be recorded beside main's on every plan without changing any dispatch, and
the agreement rate becomes evidence for the higher-stakes candidates
(`JevRiskTierSurfaceFlags.md`, `JevFindingTriage.md`) that share the "read a
plan or finding, pick a label" shape.

## What would make this a Plan

Success is measurement, not replacement: over at least 30 consecutive plans,
Jev's dispatch answer at the chosen threshold agrees with main's on every plan
main dispatched, and the disagreements on plans main skipped are re-read by a
human to see who was right. Only after that record exists does replacing main's
decision become a question, and the answer may still be no.

## Decisions a Plan needs

1. Where the two answers are recorded so they can be compared later — the
   execution card is the natural place, since it already carries the tier and
   its triggers.
2. The threshold, chosen low, and whether the two `noul`s are ORed or reported
   separately.
3. Whether the check runs at all for Tier-1 changes, which have no plan file
   (main briefs from the request), and if so what the state is.
4. The shared decisions in `JevDecisionModelWorkflowUses.md`.
