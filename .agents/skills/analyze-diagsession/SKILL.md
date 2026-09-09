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

One `researcher` delivers a per-process hotspot report and evidence-backed plan
proposals. Plan execution remains in the Change Workflow.

## When to use

- The user supplies a Visual Studio `.diagsession` or extracted ETL CPU capture.
- The user explicitly asks to analyze such a capture.

## Inputs

Dispatch one `researcher` with the authoritative shared task-brief fields from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md).
The brief's Scope names the `.diagsession` or extracted ETL path and target
process.

## Handoff

The researcher returns the shared handoff form in
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md),
extended with:

- `Capture` — capture, target process, and module-proven configuration.
- `Top per-process shares` — one row per measured process hotspot.
- `Clustered causes` — one row per clustered cause, separating measured facts
  from source-attribution inferences and build overhead from algorithmic or
  data-movement cost.
- `Source context` — one row per hotspot cluster with file:line quotes,
  enclosing loop/frame phase, and container/comparator types.
- `Frame phase and CRC exposure` — one row per hotspot cluster.
- `Gain ceilings` — one row per actionable cluster with its measured share and
  expected gain ceiling.
- `Plan proposals` — one row per evidence-backed proposal or `none`.

Each shared `Residuals` row names missing source context or symbols; use `none`
when absent.

The full function bodies and call-site listings do not travel inline. The
researcher writes them to gitignored
`Temp/analyze-diagsession/<capture>-<hotspot cluster>.md` files, whose names use
a filename-safe form of each cluster label, under one `## <hotspot cluster>`
heading that keeps the label itself, and cites each path plus its `##` selector
under `Evidence`.

`Changed files` and `Build required` are `none` because this researcher run edits
no tracked file.

Main presents the profiling report, decides every proposal, routes accepted
residuals through `/create-follow-up-plans`, and owns any landing action.

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The researcher's extraction,
  measurement, clustering, source-attribution, and reporting steps and rules.
