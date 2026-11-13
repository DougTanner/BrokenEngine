# Analyze a GPU Frame Stall Worker

The capture, analysis, and cleanup steps and rules. Triggers, inputs, the
handoff, and the user requests live in [`../SKILL.md`](../SKILL.md). Capture
mechanics live in [`capture.md`](capture.md); trace reading in
[`trace-reading.md`](trace-reading.md).

## Steps

### Capture

1. Take the route from the brief and confirm `xperf` resolves with
   `Get-Command xperf`. Done when the route is fixed, or a missing Windows
   Performance Toolkit is a `BLOCKED` residual.
2. For the `agent` route, when the built Profile client lacks the detector of
   [`capture.md`](capture.md) `## Temporary instrumentation`, add it and return
   `BLOCKED` with the Profile client under `Build required`. Done when the
   detector is built in or that return is made.
3. For the `agent` route, run the harness and the trace as
   [`capture.md`](capture.md) `## Agent route` directs while driving the
   brief's reproduction actions. Done when the harness is released and a trace
   holds a caught stall, or the run budget ended.
4. For the `focused` route, launch the client and server and trace as
   [`capture.md`](capture.md) `## Focused route` directs. Done when a trace
   holds a caught stall, or the trace ended without one and both processes are
   left running.
5. For the `supplied` route, record the ETL, the client log, and the noted
   stall time. Done when each is recorded or named missing in a residual.

### Read

6. Name the engine client and compositor process IDs by
   [`trace-reading.md`](trace-reading.md) `## Process identity`. Done when both
   are named from evidence.
7. Export one window around each caught stall and one control window of
   ordinary frames by [`trace-reading.md`](trace-reading.md)
   `## Clock mapping`. Done when each dump exists and the stall's mapped trace
   time lies inside its window.
8. Read every window by [`trace-reading.md`](trace-reading.md)
   `## Reading a window`, and write each stall's and control's rows to
   `Temp/analyze-gpu-stall/<capture>-<stall>.md` under one `##` heading per
   window. Done when every stall and control has its rows on file.
9. Classify each stall by [`trace-reading.md`](trace-reading.md)
   `## Decision table`, marking it `proven`, `signature-matched`, or
   `unclassified`. Done when
   every stall has a verdict, or a verdict needing CPU scheduling is a
   `user capture request: elevated` residual.

### Clean up and return

10. Unless the brief keeps the detector for a further capture, remove every
    temporary block by the rule [`capture.md`](capture.md)
    `## Temporary instrumentation` cites, then search the markers. Done when
    the search is empty or the kept files are named under `Instrumentation`.
11. Delete each capture file the handoff does not cite by its exact path, and
    confirm `xperf -loggers` lists no session this run started. Done when only
    cited captures remain and no session of this run is running.
12. Return the [`../SKILL.md`](../SKILL.md) Handoff shape. Done when it is
    returned.

## Rules

- Never elevate, install software, change system settings, or start a kernel
  session; hand elevated work to the user through the `Residuals` request.
- Never stop or alter another process, service, or program; record what it did
  on the GPU instead.
- Read traces only through the bundled scripts and bounded dumps; never load an
  ETL or a whole dump into context.
- Treat a stall that coincides with a trace session starting or stopping as
  possibly trace-induced, and say so in the verdict's evidence.
- Conclude only what the trace shows.
