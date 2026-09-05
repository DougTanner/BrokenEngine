---
name: agent-harness
description: >-
  Drive the Broken Engine client/server for automated verification. Use for
  runtime-observable acceptance criteria, replay determinism, or whenever the
  user or a plan's Verification section asks to launch, drive, query, or
  screenshot the game, or requests GPU frame capture or RenderDoc capture
  analysis.
allowed-tools: [PowerShell]
---

# Agent Interaction Harness

## Purpose

Drive the local headless server and rendered client through loopback,
length-prefixed JSON.

## When to use

- A runtime-observable acceptance criterion or a replay determinism check.
- The user or a plan's Verification section asks to launch, drive, query, or
  screenshot the game.
- A request for GPU frame capture or RenderDoc capture analysis.

## Inputs

Require the latest `/compile` result's `DataBuildMode`, `RunDataPacker=false`, and normalized `GameDataDirectory`.

## Handoff

Report each criterion `PASS`, `FAIL`, or `BLOCKED` with exact
command/query/scene/UI/screenshot/log evidence, and treat a setup limitation as
a blocked check. Give each observed criterion failure and process-check failure
a stable `HARNESS-F-###` ID and report it exactly once as a `Required` row in
the shared `Findings` field. Process-check findings can come from launch, a poll, a
transport-failure check, or release entry; record the role, report path,
exception headline, exit code, and retained evidence path in that finding.
Do not repeat those values in criterion rows, `Evidence`, or `Residuals`.
Criterion rows cite the finding ID instead. `FAIL` the criterion that was live
when a process-check failure was observed. A criterion whose endpoint crashed
before it could be exercised is `BLOCKED` and may cite that same process
finding; do not silently retry it. A process finding observed outside any
criterion, such as at release entry, still fails the run.
Do not diagnose or edit a failure in this role; return reproducing commands and
evidence to the main agent for the `/resolve-findings` decision and
affected-check retest.

Captures stay on disk. The `screenshot` result `{path, width, height}` is the evidence — cite it by path. Loading an image into context is a deliberate act for a check that genuinely needs pixels, and the report names which check and why.

If a required command, parameter, result field, query, or input primitive is missing, return that criterion `BLOCKED`. Name the missing capability and the narrowest harness extension that would expose it. The main agent decides whether the authorized change includes that extension or whether user authority/criterion revision is required. Never fake state with pixel guessing or log scraping, create an out-of-scope runtime edit, waive the gate with a follow-up plan, or silently skip the criterion.

Return the shared handoff form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
extended with these fields, one criterion row per acceptance criterion:

```text
Criterion results:
- <criterion ID> — PASS | FAIL | BLOCKED — <command/query/scene/UI and evidence selector, or HARNESS-F-### when that finding holds the evidence>
Findings: <HARNESS-F-### Required report-path:selector — observed criterion or process-check failure — evidence; or none>
Changed files: none
Build required: none
Residuals: <missing capability or environment, or none>
```

Any `FAIL` or out-of-criterion process finding makes the shared `Status`
`NEEDS_ACTION`, including a run that also has blocked criteria. Otherwise, any
blocked prerequisite, capability, or environment makes it `BLOCKED`; when
every criterion passes, it is `PASS`.
Return the complete report inline. A failed or blocked in-scope criterion
remains incomplete until the capability/environment is supplied or the user
explicitly revises acceptance.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The harness steps and rules the
  dispatched worker runs.
