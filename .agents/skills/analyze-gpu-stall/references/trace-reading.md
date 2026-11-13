# Reading a GPU Stall Trace

How to turn a DxgKrnl trace into a stall verdict. Capture lives in
[`capture.md`](capture.md); the steps that use this file live in
[`worker.md`](worker.md).

## Process identity

A non-elevated trace names every process `Unknown` and keeps only its ID. Take
the engine client's ID from the harness launch or the user's process, and the
compositor's from the `dwm.exe` row of `<etl>.processes.csv`; that file's
`Services` column names a `svchost.exe` by the services it hosts. A process
that started and exited inside the trace is in neither snapshot; name it
`unidentified <pid>`. An elevated trace carries the names itself.

A row's process ID is that of the thread running when the event was logged. A
DxgKrnl DPC or completion row, such as `DmaPacket/win:Info` and
`DmaPacket/win:Stop`, therefore carries whichever process the interrupt found
running, not the owner of the context it reports, so per-process counts,
including the timeline's `others` column, can name a process that issued no GPU
work. Name a process as busy on the GPU only from its own
`QueuePacket/win:Start` rows.

## Clock mapping

Trace timestamps are microseconds from the trace origin, on the
`QueryPerformanceCounter` clock (xperf's default clock type). Each
`VSyncDPC` row carries `FrameQPCTime`, so
`origin = FrameQPCTime - timestamp * frequency / 1,000,000`;
`Export-TraceWindow.ps1` and `Get-VsyncTimeline.ps1` print it as
`traceOriginQpc`. An engine QPC value maps to
`(qpc - origin) * 1,000,000 / frequency` trace microseconds. MSVC
`std::chrono::steady_clock` wraps `QueryPerformanceCounter`, so a steady-clock
count in nanoseconds is `qpc * 1,000,000,000 / frequency`. For a supplied trace
with only a wall-clock time, the instrumentation's clock line pairs QPC with
system time.

Dump a window around a logged QPC value, passing the capture's `startQpc` as
`-OriginQpc` for a circular trace whose first second may be gone:

```powershell
pwsh -NoProfile -File .agents/skills/analyze-gpu-stall/scripts/Export-TraceWindow.ps1 -EtlPath '<etl>' -OutPath 'Temp/analyze-gpu-stall/<capture>-<stall>.txt' -CenterQpc <qpc> [-OriginQpc <startQpc>]
pwsh -NoProfile -File .agents/skills/analyze-gpu-stall/scripts/Export-TraceWindow.ps1 -EtlPath '<etl>' -OutPath '<txt>' -FromUs <trace us> -ToUs <trace us>
```

The output's `centerUs` is the logged moment in trace time. Take the control
window from ordinary frames of the same trace.

## Reading a window

```powershell
pwsh -NoProfile -File .agents/skills/analyze-gpu-stall/scripts/Get-VsyncTimeline.ps1 -DumpPath '<txt>' -EnginePid <pid> -DwmPid <pid> [-From <us> -To <us>]
pwsh -NoProfile -File .agents/skills/analyze-gpu-stall/scripts/Get-TraceEvents.ps1 -DumpPath '<txt>' -From <us> -To <us> -Mode count
pwsh -NoProfile -File .agents/skills/analyze-gpu-stall/scripts/Get-TraceEvents.ps1 -DumpPath '<txt>' -From <us> -To <us> -Include '<regex>'
```

The timeline prints one row per vsync interval, with offsets after its vsync,
and flags `LONG` (a long interval), `NOCOMPOSE` (the compositor retrieved no
present token of any process), and `NOTOKEN` (no engine token retrieved). Rows
near the window start miss tokens presented before it. Its script header
defines each column. Use `Get-TraceEvents.ps1` count mode to
find who was busy in a flagged interval and list mode to read that activity in
order. Its default `-Exclude` hides the `VSyncSmoothened*` compositor wakes;
to read them, pass an `-Exclude` without `VSyncSmoothened`, or take the
compositor's first event per interval from the timeline's `dwmFirst` column.

## Event meanings

| Event | Meaning |
| --- | --- |
| `VSyncDPC` | A vsync; `FrameNumber` counts refreshes. |
| `DWMVsyncSignal` | The compositor's vsync signal. Ordinary rows carry matching counts; a mismatched row also occurs without any stall, so it supports a verdict and never decides one. |
| `VSyncSmoothened*` by the compositor | The compositor thread woke for that vsync. |
| `Present` | A process's present call. |
| `PresentHistory*/win:Start` | A presenting process hands a present token to the compositor. |
| `PresentHistory/win:Stop` by the compositor | The compositor retrieved that token for its next composition; the frame is ready by then. |
| `QueuePacket/win:Start` | A process queued GPU work. |
| `DmaPacket` start and stop | GPU execution of a software-scheduled context. A hardware-scheduled context logs none, so the engine's GPU completion is read from its token's retrieval. |
| `WaitForSynchronizationObjectFromCpu`, `UnwaitCpuWaiter` | A CPU wait on a GPU fence, and the kernel releasing it. |
| `MMIOFlip*` | A flip to scanout. |
| `*/win:DC_Start` bursts | A state rundown, such as a new DxgKrnl consumer enumerating every device, allocation, and fence. |
| `UnknownEvent/Crimson` by the engine | Instrumentation markers; the dumper does not decode their strings, so tell them apart by thread and order. |

## Normal-frame pattern

Measure it from the control window, never assume it. For a composed FIFO
client it reads: one engine present per vsync, shortly after it; one engine
token retrieved per interval, ahead of the next vsync; the acquisition fence
released shortly after a vsync, by the compositor's pass. A fence wait close to
one refresh minus the frame's CPU work is the ordinary throttle, not a stall. A
directly flipped client shows flips instead of compositor tokens; confirm that
shape in its own control window before using the table. Take the control window
from the same presentation path as the stall, composed or directly flipped.

## Decision table

| Signature in the stall window, against the control | Conclusion | Fix direction |
| --- | --- | --- |
| Engine present on cadence, its token retrieved only after the vsync it targeted, compositor composing every interval | Late GPU completion of the engine's own work | Reduce that frame's GPU cost; read the engine GPU timers for the frames before the stall |
| Engine present lands after its usual offset while the GPU and compositor sat idle before it | Late CPU submit or present | Find the CPU phase that ran long; an elevated trace tells descheduling from engine work |
| Fence released on time (`UnwaitCpuWaiter` or compositor pass) but the engine's wait ends much later | Late wake of the engine thread | Elevated trace required to prove; then CPU contention or thread priority |
| Engine token retrieved ahead of the vsync, compositor wakes but retrieves no token (`NOCOMPOSE`), next pass one refresh later | Compositor skipped a refresh | No engine defect; name what ran in the skipped interval (often another process's burst) |
| The compositor's `FlipMultiPlaneOverlay` plane layout changes, its flips of the client give way to kernel-issued `MMIOFlipMultiPlaneOverlay` flips (or back), and the engine's acquisition fence for the image held across the switch is released only by a flip on the new path | Presentation-path transition (composed to directly flipped, or back) | Environment cause, seen on a focus change; no engine defect |
| Another process's queue packets or GPU execution fill the interval the engine or compositor needed | Another process occupying the GPU | Environment cause; record the process and its activity |
| No `LONG` or `NOCOMPOSE` interval, one engine token per vsync, the long wait equals the measured ordinary wait | Ordinary FIFO throttle | No engine defect; the detector threshold or the expectation is wrong |

A stall matching no row stays unclassified, with its timeline cited, and a
`Residuals` row names what evidence would settle it.
