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

Each locator returns this domain extension followed by the complete shared
handoff from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md):

```text
Source context: <hotspot cluster; full function bodies, call sites, enclosing loop/frame phase, container/comparator types, and file:line quotes>
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: none
Decisive checks: <source searches and reads>
Build required: none
Evidence: <file:line and verbatim quote rows, or none>
Executor: <own model id> <own effort>, each unknown when unreadable
Residuals: <missing source context or symbols, or none>
```

Main interprets the measurements, confirms source attribution, and performs the
existing follow-up routing. Its final result starts with this domain extension:

```text
Profiling report:
- Capture/configuration: <capture, target process, and module-proven configuration>
- Shares/causes: <top per-process shares and clustered causes>
- Attribution: <measured facts versus source-attribution inferences>
- Cost classification: <build overhead versus algorithmic/data-movement cost>
- State exposure: <confirmed frame phase and PostRender/CRC exposure>
- Proposals: <expected gain ceiling and actionable plan proposals>
```

Follow that extension with one complete shared handoff:

```text
Status: PASS | NEEDS_ACTION | BLOCKED
Findings: <none>
Changed files: <follow-up Plan paths created through /create-follow-up-plans, or none>
Decisive checks: <capture extraction, share computation, PDB/configuration, and source-attribution checks>
Build required: <exact target/configuration/platform when verification needs a build, or none>
Evidence: <capture/report/analysis path plus selector, or none>
Executor: <main model id> <main effort>, each unknown when unreadable
Residuals: <unresolved attribution, missing capture/tool/symbols, or none>
```

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The analysis steps and the role and
  routing rules.
