---
name: next-plan-review
description: Review a landed change from its Git commit and proven parent/child session transcripts. Use when prioritized process improvements are wanted for plan/objective conformance, solution minimality/overengineering, review/testing coverage, main-session token efficiency, workflow friction, or landing speed, including when a tooling-friction Plan directs this session to run `/next-plan-review <landing ref>`.
user-invocable: true
argument-hint: "[commit-ish]"
allowed-tools: [Read, Grep, Glob, Agent, PowerShell]
shell: powershell
---

# Next Plan Review

## Purpose

Audit one completed landing read-only, producing an evidence-based,
priority-sorted improvement backlog; never retry the change, edit files, alter
Plan claims, or inspect unrelated sessions. The invoking parent runs this skill and executes [`references/worker.md`](references/worker.md).

## When to use

Run only in the invoking parent/manager context. Never route this skill
through `/codex-review` or another delegated `reviewer`; this skill dispatches
its required fresh reviewer itself, routed per the delegated-review routing
bullet in the root [AGENTS.md](../../../AGENTS.md).

### Bounded friction mode

This mode runs only when the invoker names bounded friction mode and the one
recorded friction to root-cause, the form the `/create-follow-up-plans`
tooling-friction template emits. Every other invocation, a landing review
included, runs the full audit in [`references/worker.md`](references/worker.md).

## Inputs

- The commit-ish argument, default `HEAD`.
- The optional exact session ID.
- The optional bounded-friction-mode designation with its one recorded friction.

## Handoff

The handoff enters the main session's context whole, so it carries decision
evidence as one-line rows, never the reasoning that produced it, and no row
restates another row's evidence — a later row refers to the earlier one; large
evidence is named by selector as the shared form requires. The whole handoff
stays under 16,000 characters, which leaves room for the host's result wrapper
under the 20,000-character per-result budget the `/next-plan` checkpoint
measures. Require the shared handoff form in
`../../references/subagent-reporting.md`, extended with:

```text
Status: PASS | BLOCKED
Changed files: none
Decisive checks: provenance; sessions read; sourced timeline; pauses; conformance, minimality, and process evidence
Timeline: one row per lifecycle event named in `references/concerns.md` that occurred, in time order, each `<time> | <event> | <citation>`
Root cause: up to seven rows, in time order, each `<event> | <evidence> | <session ID> <timestamp or record>`
Assessment: one row per assessed concern, `<concern> | <verdict> | <one-sentence basis> | <citation>`, ending with the alternative-explanation answer
Control-work evidence: one row per agent/session in the `references/report.md` control-work table's column order, totals only, plus the citation for each total
Model-routing evidence: one row per direct child/headless attempt in the `references/report.md` routing table's column order — attempts sharing route, configuration, proof source, and verdict may share one row that lists every attempt's citation — then one aggregate row
Build required: none
Executor: <own model id> <own effort>
Residuals: missing transcript or unverifiable fact, or none
```

## References

- [`references/worker.md`](references/worker.md) — the steps and rules the
  invoking parent executes.
