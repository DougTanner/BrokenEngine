# GPU Stall Capture

How each route gets a stall into a trace. Steps live in [`worker.md`](worker.md);
reading the result lives in [`trace-reading.md`](trace-reading.md).

## Trace sessions

GPUView's `log.cmd` and `GPUView.exe` sit in the `gpuview` folder of the Windows
Performance Toolkit; `xperf` is on PATH. Two kinds of session exist:

- Non-elevated: a user-mode session with the DxgKrnl provider at `log.cmd`'s
  present-level selection starts and stops without elevation (observed for a
  non-elevated member of Performance Log Users). It records vsyncs, presents,
  compositor present tokens, GPU queue packets, and sync-object waits per
  process, but no CPU context switches, no process names, and no stacks, so it
  cannot show a thread descheduled or woken late.
- Elevated: `log.cmd` starts the NT Kernel Logger (process, thread, context
  switch, DPC, and interrupt events) plus the GPU providers and merges them
  into `Merged.etl`. An agent's kernel session fails with access denied; only
  the user can take this trace.

`Invoke-GpuTrace.ps1` runs the non-elevated session; its header describes the
session and the files it writes. It also prints the display adapters and modes,
a `startQpc` close to the trace origin, and the ETL path. Keep each foreground
call's `-MaxSeconds` under the host tool cap, and run `xperf -stop` with the
script's `-SessionName` default only after a killed call left that session
running.

```powershell
pwsh -NoProfile -File .agents/skills/analyze-gpu-stall/scripts/Invoke-GpuTrace.ps1 -EtlPath 'Temp/analyze-gpu-stall/<capture>.etl' -ClientLog '<client log>' -StallPattern '<regex>' -MaxSeconds 540 [-MarkerProvider '<guid>'] [-AgentHarness '<path>' -Owner '<owner token>']
```

## Stall detectors

A Profile client logs its own slow-operation warnings, lines matching
` took [0-9.]+ms > `, only once it has focus;
`Engine/Source/Profile/AGENTS.md` `## Slow-operation Diagnostics` owns that
gating, and an agent launch never takes focus. A focused route uses that
pattern. An agent route needs the temporary detector below.

## Temporary instrumentation

Add it, each edit between a `// TEMP START` line and a `// TEMP END` line, under
the temporary-edit rule of `.agents/references/change-workflow.md`
`#### Step 5 — Run targeted pre-review checks`, which also owns its removal, in
`SwapchainManager` (`AcquireNextImage`, `PresentToQueue`, `Present`):

- Read `QueryPerformanceCounter` at the present wake, before and after
  `vkQueuePresentKHR`, before and after the acquisition fence wait, and after
  `vkAcquireNextImageKHR`, into one record per acquisition with a sequence
  number.
- Keep a ring of recent records. When a fence wait exceeds about 1.5 refresh
  intervals, log one line per ring record before it (`before`), the stall
  (`STALL`, carrying the wait in microseconds), and a few after (`after`); the
  cause usually lies in the frames before the stall.
- Log a histogram of all fence waits periodically, so the ordinary per-frame
  wait is measured rather than assumed.
- Log once, at the first acquisition, a clock line pairing a QPC value with
  `GetSystemTimePreciseAsFileTime`, the QPC frequency, and the process and
  thread IDs.
- Optionally register an ETW provider under a freshly generated GUID and write
  one `EventWriteString` marker per point; pass that GUID as
  `-MarkerProvider`.
- Format log text without heap allocation on the main loop, under
  `Engine/Source/Memory/AGENTS.md`.

Use the `STALL` line text as `-StallPattern`.

## Agent route

Run the harness by the `/agent-harness` worker steps inside this task, under
that skill's run-as-evidence paragraph in its `## Handoff`: claim and launch
both roles with `-Configuration Profile` and an explicit client `-LogFile`, then
restore and set fullscreen as its `### Performance investigations` rule
requires. Start
`Invoke-GpuTrace.ps1` with `-AgentHarness` and `-Owner` so the claim stays
warm, drive the reproduction actions while it waits, and rerun it within the
run budget until a stall is caught. Release by the harness lifecycle steps.

An agent client is never focused, and the desktop compositor composes it rather
than flipping it directly. A focused, directly flipped client paces
differently, so a stall the agent route cannot catch, or one whose verdict
depends on composition, calls for the focused route.

## Focused route

Launch the server and client for the user by
`.agents/skills/agent-harness/references/launch.md` `## Normal launch`, under
its exception for the `/analyze-gpu-stall` focused route. The client logs
`WM_SETFOCUS` and `WM_KILLFOCUS` lines, and losing focus also turns off raw
input, so nothing can be reproduced. Before each trace, confirm the client log's
last focus line is `WM_SETFOCUS`; otherwise ask the user to click into the
client. Then run `Invoke-GpuTrace.ps1` with `-ClientLog`, no `-AgentHarness`,
and the slow-operation pattern joined with `|WM_KILLFOCUS`, while the user
plays, so a lost focus also stops the trace. Only a printed slow-operation line
is a caught stall; when a printed line is `WM_KILLFOCUS`, stop tracing and
report the lost focus. Leave both processes running.

## Elevated route

`log.cmd` keeps its on/off state in the Command Prompt's environment, so the
stopping run must use the window that started it, and it writes `Merged.etl`
into that window's current folder. Without an argument it captures at its
normal level; `light` captures less. The user's text is
`../SKILL.md` `### Elevated capture request`. A merged trace carries process
names and context switches; read it with the same scripts, adding the thread
rows `Get-TraceEvents.ps1` lists for the engine's present and main threads.
