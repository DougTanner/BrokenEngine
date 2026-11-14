---
name: plan-alternatives
description: >-
  Explore materially different ways to make a change before its plan reviews:
  dispatch blind, one-shot `researcher` workers on fixed axes, compare their
  candidates against the drafted approach, and present every returned
  candidate to the user. Use when the change adds new code or modifies
  non-documentation behavior (/plan-simplicity-review `## When to use`
  defines both), both at the Change Workflow Prepare and explore alternatives
  step and from a /next-plan claim once the Plan is verified. A new
  Collection, manager, or subsystem API routes to /external-design-interface
  instead, and a saved but unclaimed Plan waits for its claim.
allowed-tools: [Read, Grep, Glob, Agent, AskUserQuestion]
---

# Plan Alternatives

## Purpose

Answer one question before the Plan review step: is there a materially different
way to do this? Main dispatches one to three blind `researcher` workers on fixed
axes, compares their candidates against the drafted approach, and reports that
the original stands or puts every returned candidate, and any option main adds
itself, to the user, who picks at most one option or combination.

## When to use

- The Change Workflow Prepare and explore alternatives step, after
  `/prepare-change` drafts the plan at Tier 2+ or after main fixes the intended
  change at Tier 1, whenever the trigger in
  [`../plan-simplicity-review/SKILL.md`](../plan-simplicity-review/SKILL.md)
  `## When to use` fires — minus that skill's `/save-plan` save-time dispatch.
- From a `/next-plan` claim, after the preparation `implementer` verifies the
  Plan and before the Plan review reviewers, whenever that same trigger fires.
- Not for a new Collection, manager, or subsystem API:
  `/external-design-interface` already produces three designs and replaces this
  skill for that case.
- Not from a saved but unclaimed Plan; such a Plan gets this step when it is
  claimed.

## Inputs

Main's dispatch recipe, including every field each researcher's brief carries,
is the Change Workflow Prepare and explore alternatives step; the brief itself
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
Existing guarantees: <what the code already guarantees that it relies on, such as ordering, ownership, or failure handling; or none>
Self-serving machinery: <new state or code that exists only to make this mechanism work, not to meet the objective; or none>
Pays off when: <the condition that makes it the right call>
```

Supporting or quoted source text stays out of every row; a researcher that wants
to show its sourcing cites the existing repository path plus selector under
`Evidence` instead.

### Comparison

When researcher handoffs contradict each other on a repository fact a
comparison depends on, main dispatches one `locator` to settle that fact and
scores from its returned rows instead of reading source itself.

Main scores candidate zero from the rows of the preparation handoff's
`Candidate zero` field when a preparation handoff returned that field's rows,
and a proposed combination from the rows of the candidates it combines.

Main scores candidate zero and every returned candidate on the same criteria:

- objective fully met;
- cause removed rather than symptom suppressed;
- shape, read from `Mechanism` and `Adds/Deletes`: fixing a fact where it is
  created is simpler than carrying a transient key to reconcile it later;
- the code, state, and machinery the codebase is left with;
- runtime cost;
- invariants kept, read from `Invariant surfaces`: every invariant the change
  must hold still holds on each surface listed;
- `Existing guarantees`: relies on what the code already guarantees instead of
  rebuilding it;
- `Self-serving machinery`: adds none;
- deterministic: a script or code path that decides the same way on every run
  is better than one that leaves the decision to agent judgement or
  instruction text;
- codebase improved beyond the objective: removes duplication or simplifies
  the surrounding code.

A requirement that neither the user nor a repository rule states — exact
preservation of current behavior, numbers, or visuals; smoothness of a
debug-only control; and the like — is an assumption. Main names it as an
assumption beside the option it shapes and never counts breaking it against a
simpler option.

The scores inform main's recommendation and the pros and cons stated beside
each option — an objective not fully met, a rebuilt existing guarantee, or
self-serving machinery among the cons; they are not a tally, and they never
decide which candidates the user sees. An empty axis says nothing about whether
candidate zero is the simplest approach; whether the drafted plan itself is
over-built is the `/plan-simplicity-review` question.

### User presentation

IMPORTANT: this step exists to widen the change when wider is better — a
root-cause fix, a refactor that improves the codebase, or a better idea the
draft missed — or to narrow it when a simpler alternative meets the objective.
Scope limits what implementation may change once the user has picked an
approach.

When no researcher returned a candidate and neither main-added option below
applies, the original stands and the turn's report carries one line:

```text
Alternatives: original stands (<axes>)
```

Otherwise, before the Plan review step on every route, main presents every
returned candidate and every main-added option that applies beside the drafted
approach in plain language per `### User Interaction`, giving each option, the
drafted plan included:

- Unique advantages: what it offers that the other options do not, each named
  with a short label, such as DRY (reuses an existing mechanism instead of
  adding a second one), KISS/YAGNI (less code, state, or machinery), root-cause
  fix (the `### Comparison` cause criterion), or codebase-improving refactor
  (the `### Comparison` codebase criterion) — or `none`.
- Pros and cons: the `### Comparison` criteria stated as pros and cons,
  including what it adds and deletes and when it pays off, worded by the
  `### User Interaction` rule for comparing options.
- Constraints: each repository rule, hook, or limit that shapes the option,
  stated with the reason the rule exists — or `none`. Main takes each rule and
  its reason from the handoffs already in hand; for a reason they do not carry,
  it dispatches the `### Comparison` `locator`, and writes
  `reason not recorded` only when that returns none.

Main adds each of these options itself when its condition holds and no
researcher returned it, each presented with the same three entries:

- Change the rule: when the drafted approach exists only to work around a
  repository rule, hook, or limit, the option is to change that rule instead.
- Do nothing: when the problem is rare, cosmetic, or an edge case that neither
  crashes nor breaks the simulation, the option is to drop the change (on a
  `/next-plan` claim, the rejection route below) and document the behavior as
  acceptable at its owning `AGENTS.md` section or code comment; main recommends
  it when every other option adds machinery. Never offered for a reachable
  race
  ([`../../references/change-workflow.md`](../../references/change-workflow.md)
  `### Resolving Ambiguity`).

Main proposes one combination whenever parts of different options are
compatible and together serve the objective better than either alone, presented
as its own option with the same three entries. When the recommendation passes
over a wider option that would still improve the codebase, the recommendation
names that option as a follow-up Plan candidate; when the user asks for it,
main routes it through `/create-follow-up-plans`.

Main then gives a recommendation and asks verbatim:

> Which approach should this change use: the drafted plan as written, or one of
> the other options above?

The user picks at most one option or combination, and main takes it down the
route that matches the change:

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
  `## Task brief` — task-brief fields.
- [`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
  — the shared handoff form.
- [`../plan-simplicity-review/SKILL.md`](../plan-simplicity-review/SKILL.md) —
  the trigger this skill shares.
