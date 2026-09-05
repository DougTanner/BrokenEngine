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

Report each criterion `PASS`, `FAIL`, or `BLOCKED` with exact command/query/scene/UI/screenshot/log evidence. Treat setup limitations as blocked checks.

A process-check finding — from launch, a poll, a transport-failure check, or release entry — is evidence like any other: `FAIL` the criterion that was live when it was observed, citing the finding's role, report path, exception headline, exit code, and retained evidence path. A finding observed outside any criterion, such as at release entry, fails the run and is reported as a residual with the same values. A criterion whose endpoint crashed before it could be exercised is `BLOCKED` on that crash, not silently retried. Do not diagnose or edit a failure in this role; return reproducing commands and evidence to the main agent for the `/resolve-findings` decision and affected-check retest.

Captures stay on disk. The `screenshot` result `{path, width, height}` is the evidence — cite it by path. Loading an image into context is a deliberate act for a check that genuinely needs pixels, and the report names which check and why.

If a required command, parameter, result field, query, or input primitive is missing, return that criterion `BLOCKED`. Name the missing capability and the narrowest harness extension that would expose it. The main agent decides whether the authorized change includes that extension or whether user authority/criterion revision is required. Never fake state with pixel guessing or log scraping, create an out-of-scope runtime edit, waive the gate with a follow-up plan, or silently skip the criterion.

End delegated process verification with:

```text
Files changed: none
Functions/regions touched: none
Residuals:
- <failed criterion, crash finding (role, report path, headline, evidence path), blocked prerequisite/missing capability, or none>
```

Return the complete report inline. A failed or blocked in-scope criterion remains incomplete until the capability/environment is supplied or the user explicitly revises acceptance.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The harness steps and rules the
  dispatched worker runs.
