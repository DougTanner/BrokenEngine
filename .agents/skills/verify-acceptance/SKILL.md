---
name: verify-acceptance
description: >-
  Verify a completing stage's acceptance table: map every approved criterion and
  invariant to evidence that settles it on its own. Use at the Change Workflow
  Verify the acceptance table step when a stage completes without landing. Runs
  inside one fresh read-only delegated `reviewer`; findings only, never edits,
  and never the landing acceptance table.
allowed-tools: [Read, Grep, Glob, PowerShell]
---

# Verify Acceptance

## Purpose

The acceptance verdict for a completing stage: how many approved criteria and
invariants passed, a row for each that did not citing its evidence or the
missing check, and a status saying whether the approved checks have all closed.

## When to use

- The Change Workflow Verify the acceptance table step in root
  [AGENTS.md](../../../AGENTS.md), for a stage completing without landing.
- Not for a stage that lands in the same session: that table comes from the
  Verify and land step's landing table, which
  [`/finalize-changes`](../finalize-changes/SKILL.md) owns and builds itself
  from the prepared diff.
- Not as a separate dispatch where the Review and resolve correctness combined
  pass applies:
  [`/coherence-review`](../coherence-review/SKILL.md)'s combined pass is that
  reviewer and runs this skill's mapping as its acceptance component.

Run in the delegated execution context of
[`subagent-reporting.md`](../../references/subagent-reporting.md), in a context
that did not produce the work.

## Inputs

- The approved acceptance criteria and invariants, verbatim — from the execution
  card where one exists, otherwise from the approved request.
- The approved tier, which fixes the evidence ceiling.
- The changed files and regions the stage covers.
- The evidence locations: handoffs, command output, logs, `Temp/` files, or the
  repository paths a check reads.

Return `BLOCKED` naming the missing input when the criteria or the changed
bytes are unavailable.

## Handoff

Return the shared handoff form in
[`subagent-reporting.md`](../../references/subagent-reporting.md) `## Handoffs`,
with one declared extension section above `Findings`:

```text
Criteria: <passed>/<total> passed
<criterion or invariant> — <evidence citation> — FAIL
```

Only the approved criteria and invariants that did not pass follow the count
line, in the approved order; an unsettled item counts as not passed and its row
cites the check it lacks. The evidence cell cites where the settling observation
lives — a path plus selector, or the `Decisive checks` row that settles it — and
never restates it. `Status` is `PASS` only when the count line reports every
item passed; any listed row is `NEEDS_ACTION` with a matching `Findings` row.
`Changed files` and `Build required` are `none`, because this read-only review
changes no file.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The mapping steps and rules the
  dispatched reviewer runs.
