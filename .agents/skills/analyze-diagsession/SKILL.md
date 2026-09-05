---
name: analyze-diagsession
description: >-
  Analyze Visual Studio .diagsession and extracted ETL CPU captures for the
  Broken Engine client or server, reporting per-process hotspots and
  evidence-backed optimization proposals. Use when the user supplies such a
  capture or explicitly asks to analyze one.
allowed-tools: [Read, Bash, PowerShell, Grep, Glob, Agent]
---

# Analyze Visual Studio CPU Captures

## Purpose

Deliver a per-process hotspot report and evidence-backed plan proposals. Plan
execution remains in the Change Workflow.

## When to use

- The user supplies a Visual Studio `.diagsession` or extracted ETL CPU capture.
- The user explicitly asks to analyze such a capture.

## Inputs

Main runs this orchestration. The brief's Scope names the `.diagsession` or
extracted ETL path and target process, and supplies the authoritative shared
task-brief fields. Main dispatches each independent hotspot cluster to one
`locator`; a delegated worker never delegates.

## Handoff

Each locator returns the shared handoff form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
extended with these fields:

```text
Source context: <hotspot cluster; full function bodies, call sites, enclosing loop/frame phase, container/comparator types, and file:line quotes>
Changed files: none
Build required: none
Residuals: <missing source context or symbols, or none>
```

Main interprets the measurements, confirms source attribution, and performs the
existing follow-up routing. Its own result is a profiling report, not a handoff,
and states:

- capture, target process, and module-proven configuration;
- top per-process shares and clustered causes;
- measured facts versus source-attribution inferences;
- build overhead versus algorithmic/data-movement cost;
- confirmed frame phase and PostRender/CRC exposure;
- expected gain ceiling and actionable plan proposals.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The analysis steps and the role and
  routing rules.
