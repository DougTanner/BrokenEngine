---
name: plan-alternatives
description: >-
  Explore materially different ways to make a change before its plan reviews:
  dispatch blind, one-shot `researcher` workers on fixed axes, compare their
  candidates against the drafted approach, and ask the user only when a
  candidate is clearly better. Use at the Change Workflow Prepare and explore
  alternatives step when the change adds new code or modifies non-documentation
  behavior (/plan-simplicity-review `## When to use` defines both), and from a
  /next-plan claim once the Plan is verified. A new Collection, manager, or
  subsystem API routes to /external-design-interface instead, and a saved but
  unclaimed Plan waits for its claim.
allowed-tools: [Read, Grep, Glob, Agent, AskUserQuestion]
---

# Plan Alternatives

## Purpose

Answer one question before the Plan review step: is there a materially
different way to do this? Main dispatches one to three blind `researcher` workers
on fixed axes, compares their candidates against the drafted approach, and
reports that the original stands or puts a clearly better candidate to the user.

## When to use

- The Change Workflow Prepare and explore alternatives step, after
  `/prepare-change` drafts the plan at Tier 2+ or after main fixes the intended
  change at Tier 1, whenever the trigger in
  [`../plan-simplicity-review/SKILL.md`](../plan-simplicity-review/SKILL.md)
  `## When to use` fires — minus that skill's `/save-plan` save-time dispatch.
- From a `/next-plan` claim, after the preparation `implementer` verifies the
  Plan and before the Plan review reviewers.
- Not for a new Collection, manager, or subsystem API: `/external-design-interface`
  already produces three designs and replaces this skill for that case.
- Not from a saved but unclaimed Plan; such a Plan gets this step when it is
  claimed.

## Inputs

Main's dispatch recipe, including every field each researcher's brief carries,
is the root `AGENTS.md` Prepare and explore alternatives step; the brief itself
is the shared task-brief form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Task brief`.

## Handoff

Each researcher returns these extension fields, one row each and each row one
line, followed by the shared handoff lines with `Build required: none` and
`Status: PASS` for a returned candidate or a reported empty axis (neither is a
finding); `BLOCKED` only when the brief or a cited path cannot be read:

```text
Axis: <1 Reuse | 2 Remove the need | 3 Reshape>
Candidate: <name, or "no viable candidate on this axis", which sets every row below Mechanism to none>
Mechanism: <the concrete mechanism, or the reason no candidate exists>
Critical files: <3-5 repository paths>
Adds/Deletes: <what it adds; what it deletes>
Invariant surfaces: <surfaces it touches, or none>
Pays off when: <the condition that makes it the right call>
```

Supporting or quoted source text stays out of every row; a researcher that wants
to show its sourcing cites the existing repository path plus selector under
`Evidence` instead.

### Comparison

Main scores candidate zero and every returned candidate on the same criteria,
selects at most one candidate, and never merges candidates into a hybrid:

- objective fully met;
- cause removed versus symptom suppressed;
- net new code;
- invariant surfaces touched (a candidate that raises the tier is noted, not
  disqualified);
- reuse of an existing mechanism.

The verdict is two-way: a candidate is `clearly better` when it fully meets the
objective, wins at least two criteria, and loses none; otherwise it is not.

### User presentation

When no candidate is clearly better, the original stands and the turn's report
carries one line:

```text
Alternatives: original stands (<n> candidates, <axes>)
```

Otherwise, before the Plan review step, main presents every clearly better
candidate beside the drafted approach in plain language per root `AGENTS.md`
`### User Interaction` — the same criteria, what it adds and deletes, which
invariant surfaces it touches, and when it pays off — with a recommendation,
then asks verbatim:

> Which approach should this change use: the drafted plan as written, or one of
> the alternatives above?

The user picks one, and main takes it down the route that matches the change:

- Tier-2+ change that is not a claimed Plan: the choice goes back to
  `/prepare-change` as a fixed decision for a redraft, and the Plan review
  reviews run on the redraft.
- `/next-plan` claim: the claimed Plan file is immutable, so the choice goes
  back to the preparation `implementer` as a fixed decision for a redrafted
  execution card, which that worker writes into a resolved Plan snapshot under
  gitignored `Temp/` and cites in its handoff; the Plan review reviews then run
  on that snapshot. A "do nothing" choice is a user-authorized Plan rejection
  through the existing `plan reject --user-authorized-rejection` route.
- Tier 1: there is no plan file, so main adopts the chosen approach directly and
  describes it in the report.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The steps and rules the dispatched
  researcher follows.
- [`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
  — task-brief fields and the shared handoff form.
- [`../plan-simplicity-review/SKILL.md`](../plan-simplicity-review/SKILL.md) —
  the trigger this skill shares.
