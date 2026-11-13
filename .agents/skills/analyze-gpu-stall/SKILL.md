---
name: analyze-gpu-stall
description: >-
  Capture and analyze a Broken Engine client frame stall with Microsoft GPUView
  tracing (Windows Performance Toolkit xperf, DxgKrnl ETW), separating late
  engine GPU work, late CPU submit or present, late wake, a skipped compositor
  refresh, another process on the GPU, and ordinary FIFO throttle. Use for a
  frame-pacing stall, a late swapchain image or fence, or a suspected GPU,
  compositor, or driver scheduling stall the engine's own GPU logging cannot
  explain, or when the user supplies a GPUView or xperf ETL of one.
allowed-tools: [Read, Write, Edit, Grep, Glob, PowerShell]
---

# Analyze a GPU Frame Stall

## Purpose

One `implementer` captures a client frame stall in a GPU trace, or reads a
supplied one, and names what delayed presentation, keeping proven causes apart
from signature matches. The fix stays in the Change Workflow.

## When to use

- A frame-pacing stall, a late swapchain image or acquisition fence, or a
  suspected GPU, compositor (DWM), or driver scheduling stall that the engine's
  GPU timers and slow-operation warnings cannot explain.
- The user supplies a GPUView `Merged.etl` or another DxgKrnl ETL of a client
  stall.
- `/external-diagnose-bug` owns the root cause. A sampled CPU capture goes to
  `/analyze-diagsession` instead.

## Inputs

Dispatch one `implementer` with the shared task-brief fields from
[`../../references/subagent-reporting.md`](../../references/subagent-reporting.md)
`## Task brief`. The brief's Scope names the game project, the stall symptom
(log line or measured duration), the actions that reproduce it, and the route:

- `agent` — an agent-launched client under `/agent-harness`; the brief carries
  the latest `/compile` result for the Profile client and server, requested
  with the agent-harness scenario trigger in `/compile` `## Inputs`.
- `focused` — a client the user plays with focus while the worker traces; main
  first presents `### Focused session request` below and dispatches once the
  user is ready. The same `/compile` result is required.
- `supplied` — an ETL the user handed back, with its client log and the noted
  stall time.

A continuation after a build or a user capture carries the
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md)
`## Continuation capsule`.

## Handoff

Return the shared handoff form in
[`../../references/subagent-handoff.md`](../../references/subagent-handoff.md),
extended with:

- `Capture` — one row per trace: route, ETL path, client log path, display mode
  and refresh rate, client focus, and whether the compositor composed or
  flipped the client.
- `Verdict` — one row per stall: the conclusion from
  [`references/trace-reading.md`](references/trace-reading.md)
  `## Decision table`, `proven` or `signature-matched`, and the deciding
  observation as a path plus selector; or `unclassified`, citing its timeline.
- `Fix direction` — one row per verdict: an engine change and where, no engine
  change, or an environment cause.
- `Instrumentation` — `none added`, `removed` with the marker searched, or the
  files still carrying it pending a build.

A verdict is `proven` only when that stall lies inside a trace and its deciding
events were read in the stall window; a stall known only from the log is at
most `signature-matched`. Each shared `Residuals` row names an unproven branch,
a missing environment, or `user capture request: focused | elevated`, on which
main presents the matching request below; use `none` when absent.

`Status` is `PASS` when every captured stall is `proven` or
`signature-matched`, and `NEEDS_ACTION` when any is `unclassified`, with a
`Residuals` row naming what data would classify it, including a user capture
request. It is `BLOCKED` only when no stall was captured within the run budget,
or when the capture could not be set up or read, such as temporary
instrumentation needing a build first (shared `Build required` names the
targets and `Changed files` the instrumented files). `Findings` is `none`.

### Focused session request

Main fills the angle-bracket values and presents this before a `focused`
dispatch:

```text
I will start the Profile server and client for you with a client log file, then
run a GPU trace that stops by itself about a second after the client logs a
stall, or after <minutes> minutes. When the client window appears, click into it
so it has focus and keep it focused, and reproduce the stall: <reproduction
actions>. Tell me when you are ready.
```

### Elevated capture request

Main presents this when a `Residuals` row asks for an elevated capture:

```text
To see CPU scheduling around the stall I need one trace that only an
Administrator prompt can take:
1. Open Command Prompt as Administrator and change to a folder with several GB
   free, for example: cd /d %USERPROFILE%\Desktop
2. Start the trace:
   "%ProgramFiles(x86)%\Windows Kits\10\Windows Performance Toolkit\gpuview\log.cmd"
3. Switch to the game window, reproduce the stall, and note the local time.
4. In the same Administrator window, run the same command again to stop. It
   writes Merged.etl into that folder (GPUView.exe beside log.cmd opens it).
5. Tell me the full Merged.etl path and the time you noted.
```

## References

- [`references/worker.md`](references/worker.md) — private: read it only if you
  are the session executing this skill. The capture, analysis, and cleanup
  steps and rules.
- [`references/trace-reading.md`](references/trace-reading.md) — clock mapping,
  event meanings, the normal-frame pattern, and the decision table a `Verdict`
  cites.
