---
name: plan-simplicity-review
description: >-
  Review an implementation plan that adds new code or modifies
  non-documentation behavior (root AGENTS.md Step 2 owns the exact trigger) for
  YAGNI/KISS violations before implementation: ultra-rare edge-case handling,
  speculative hardening, problems a plainly simpler mechanism or a deferred
  ASSERT would solve, and bandaid fixes that suppress a symptom instead of
  removing its root cause. Runs at every tier inside one delegated `reviewer`,
  in parallel with /plan-audit where that runs; skip a plan for which Step 2's
  trigger does not fire. Findings only, with no edits, harness work, user
  interview, or further delegation.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Plan Simplicity Review

Judge one question: are the plan's changes the simplest complete way to serve
the repository? The plan's own problem statement carries no authority here —
this pass is a repository-level safeguard against over-engineered plans, so
question whether the stated problem is worth solving at all before judging how
it is solved. Most executable Plans are written by agents on their own
decision; authoring, tracking, claiming, or selecting a Plan never means the
user approved its objective, so the objective is reviewable like any other part
of the plan, and recommending that the whole plan be rejected is an in-remit
outcome when no amount of simplification makes it worth implementing. The
plan's internal design decisions and its rejected alternatives are equally
non-authoritative, per the authority-order directive in root `AGENTS.md`. Use
this skill at every tier, for every implementation plan with a
step that adds new code per Step 2's trigger in root `AGENTS.md` — including an
addition inside an existing file — or that modifies non-documentation behavior
per that same trigger, which owns where skill prose ends and skill behavior
begins, tracked executable Plans and session-prepared plans alike; a plan for
which that trigger does not fire skips it. Correctness, traceability, and
citation verification belong to `/plan-audit`, running in parallel on the same snapshot where that review runs —
leave them there; post-implementation diff minimality belongs to
`/scope-review`. A clean pass is the expected common outcome, because this pass
must not become its own source of over-engineering.

## Inputs

- `/save-plan`: the final unsaved Plan body snapshot, supplied inline or by
  exact file path.
- Change Workflow preparation or a bounded final rerun: the manager supplies
  the final complete plan presented for implementation, including the
  execution card and exact refinements when they carry resolved work, inline
  or by exact file path; on a bounded final rerun, treat that supplied final
  plan as the review input.
- The manager's trigger evidence: which plan steps add code or modify
  non-documentation artifacts.
- On a bounded final rerun, the manager's trigger evidence and classification
  also identify which steps materially enlarge the solution.
- User intent and known residuals.

Classify occurrence and likelihood evidence explicitly:

- `observed` — a concrete incident, reproduction, log, diagnostic, capture, or
  output from executing a named current workflow that demonstrates the reviewed
  state or failure.
- `credible exposure` — a named current workflow, a concrete trigger or timing
  window, and a likelihood signal such as measured cadence, bounded timing,
  multiplicity, or an analogous incident. An analogous incident counts only
  when it shares the relevant failure mechanism, trigger or window, and
  affected consumer.
- `hypothetical` — static code or specification reachability, or a possible
  consequence without the evidence above. Code reachability alone never counts
  as occurrence evidence.

Load the supplied snapshot bytes once and retain that
immutable snapshot for the whole review; ignore later edits. If an input above
is missing, return `BLOCKED` naming the exact missing input.

## Execution Context

Run in the delegated execution context of
`../../references/subagent-reporting.md`, dispatched once — in Claude Code that
dispatch goes through `/codex-review`, while a Codex caller is already the Sol
mapping and dispatches the `reviewer` directly —
in parallel with `/plan-audit` on the same plan snapshot where `/plan-audit`
runs (Tier 2+) and standalone where it does not (Tier 1, and the save-time
dispatch from `/save-plan`); inline review is prohibited. If the mandatory
reviewer is unavailable, the manager reports a blocker.

## Review

Apply questions 1 and 2 to every step that adds code or modifies
non-documentation behavior; apply questions 3-5, 8, and 9 to every step that
adds code or materially enlarges the solution; apply question 6 to every step
that changes a non-documentation artifact; apply question 7 to a plan whose
steps change product, user interface, tooling, or public interface behavior.

1. Reachability and evidence. Classify the reviewed state or failure as
   `observed`, `credible exposure`, or `hypothetical` using the evidence rules
   above. Report it when the condition is ultra-rare, self-healing, or already
   answered by an existing ordered fallback — idempotent re-run, lease expiry,
   claim healing, Git state. Canonical examples: hardening a repository script
   against power loss mid-execution; solving stale export-artifact files with
   new tracking machinery when cleaning the export directory on final release
   builds would do.
2. Problem worth solving. State who is hurt and when in every finding, tie that
   harm to the occurrence or likelihood evidence, and compare the proposed
   prevention's implementation, maintenance, runtime, and integration cost with
   the simpler alternative's cost and with accepting or deferring the problem's
   likelihood, impact, and recovery cost. Use qualitative bands when exact
   measurements are unavailable; never invent precision. A merely hypothetical
   cost is not itself a problem. When the problem remains `hypothetical`, and
   no stronger `simplify` or whole-plan `plan-not-worth-executing` finding
   applies, emit `speculative-hardening` with `user-judgment` regardless of
   prevention cost or severity. Retain the cost comparison and present options
   with a recommendation; severity alone is neither occurrence evidence nor
   grounds for a clean `PASS`.
3. Simpler mechanism. For each applicable step, name the plainly simpler
   alternative when one exists: reuse of an existing mechanism, a narrower
   change, deleting the requirement, or fixing at the origin.
4. ASSERT-and-defer floor. Weigh the minimal fallback every time — an ASSERT or
   clear failure at the point the rare condition would manifest, dealt with
   later if it ever occurs. Honor the repository's no-useless-ASSERT rule (root
   `AGENTS.md`, Directives): prefer making the condition impossible or
   recovering gracefully over an ASSERT that adds nothing.
5. Configuration and extension surface. Report options, hooks, and formats with
   no current consumer.
6. Bandaid versus root cause. For any step that fixes, guards, works around, or
   compensates for a defect or misbehavior, establish whether the change removes
   the cause or only suppresses the symptom — re-tuning a value, adding a
   compensating offset, catching and ignoring a failure, or special-casing one
   call site of a shared bug. Evaluate a choice the plan itself declares decided
   against the root-cause alternative it rejected. Every bandaid finding names
   the suspected root cause with repository evidence (`path:line`) and sketches
   the base-level durable fix as its alternative.
7. Actual consumer and observable benefit. Name who consumes the result and the
   benefit they can observe once this plan lands. Judge only what the plan
   already proposes: never ask for an extra feature or a wider boundary to
   supply the missing benefit. When no step reaches a consumer, that absence is
   the finding.
8. Requested signal or pass-through. For a step that adds or forwards a signal —
   a field, status, flag, event, or parameter threaded between layers — name the
   owner that produces it and the consumer that reads it, and establish whether
   the signal already exists at either boundary. Require evidence of the
   consumer before the plumbing is added, and keep every required layer, trust,
   client/server, and repository boundary intact; a simpler alternative never
   means crossing one. Question 5 already covers an option, hook, or format with
   no current consumer — report such a case once, there.
9. Representation fit. For coupled flags, repeated variants, or a family of
   branches, ask whether a repository-native shape — an enum, `common::Flags`,
   an SOA column, or a lookup — replaces them. Support that swap only when the
   plan's evidence shows it deletes concrete branches or rules, and report a
   proposed registry, state machine, or similar indirection that deletes none.

Report a question 7-9 finding under an existing class — `speculative-hardening`
for machinery built ahead of its consumer, otherwise `overbuilt-mechanism` —
unless the whole-plan rule below applies.

When the whole plan's value does not justify any implementation — not one
overbuilt step, but the objective itself — report exactly one whole-plan finding
of class `plan-not-worth-executing` recommending rejection, cite the plan's goal
or title line, and drop the per-step findings that recommendation subsumes.
Rejecting a tracked Plan always requires explicit user authorization
(`Documents/Plans/AGENTS.md`, `plan reject --user-authorized-rejection`), so
give that finding the `user-judgment` disposition with its options and a
recommendation; the manager routes it, and this reviewer still never interviews
the user.

Give every finding one disposition. Use `simplify` when the evidence supports a
concrete simpler replacement. Use `user-judgment` when two or more viable
answers remain and the trade-off is genuinely the user's — accepting a rare
failure versus paying for the machinery that prevents it, or keeping a symptom
patch versus paying for a much larger root-cause fix — and present the
options, their costs, and a recommendation in plain language, so the manager can
put it to the user (Tier 2) or feed `/external-grill-plan` (Tier 3) without
rework. The manager owns all judgment; this reviewer never interviews the user.

Do not edit any repository file, run `/agent-harness`, interview the user, or
spawn another agent.

## Output

For each finding:

> `PSR-F-###` — `plan-path:line` (for an inline plan, the line in the
> supplied plan text) — class:
> rare-edge-case | speculative-hardening | overbuilt-mechanism |
> assert-and-defer | bandaid-fix | plan-not-worth-executing — concrete problem
> — evidence: `<stable locator: repository-path:line, plan-path:line,
> supplied artifact selector, or named command/output;
> "none supplied" only when Q1 classifies the case as hypothetical because
> occurrence evidence is missing>` — occurrence/likelihood: `<Q1
> classification and supporting evidence>` —
> simpler alternative(s): `<applicable Q3-Q9 result; for bandaid-fix include
> Q6's root-cause fix sketch, and for plan-not-worth-executing state what
> replaces the plan (nothing or a much smaller change)>` — cost comparison:
> `<Q2 comparison>` — disposition: `<Q2
> disposition: simplify | user-judgment (options + recommendation)>`

A `plan-not-worth-executing` finding cites the plan's goal or title line and is
always the only finding in the report.

If clean, state `PASS — plan changes are minimally scoped.` Return:

```text
Steps reviewed: <added-code and modified-artifact steps>
Findings: <entries or none>
```

Follow those extension fields with the shared handoff lines
(`../../references/subagent-reporting.md`, `## Handoffs`); this findings-only
review never changes a file and never requires a build.

Use `NEEDS_ACTION` for findings and `BLOCKED` only when a required input is
missing.
