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

The brief's Scope names the `.diagsession` or extracted ETL path and the target
process.

## Handoff

Report first:

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
