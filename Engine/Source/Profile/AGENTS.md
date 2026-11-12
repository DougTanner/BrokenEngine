# Profile - Runtime Performance Telemetry

Engine CPU/GPU profiling, boot timing, overlays, and profile dumps. `ProfileManager`, reached through `gpProfileManager`, is the only profile manager; the game's counters and timers extend its engine counters and timers in the same contiguous index space.

## Index and Lifetime Contracts

- Game CPU counter and timer enums start at the matching engine count. Name tables preserve enum order and exact extent, except that a game with no counters keeps one unread placeholder entry; compile-time checks enforce both contracts.
- The manager holds engine and game rows together in one CPU counter array and one CPU timer array, allocated in its constructor at the game counts.
- Recording code compiles out through `if constexpr (kbProfiling)`. Shared CPU accessors and text formatters remain available to the main-thread Server Monitor snapshot capture even when profiling is disabled.

## CPU Timing

- Per-thread timer state is mutex-protected; worker threads may contribute, and text formatting takes the same lock. Cross-thread scopes require the explicit cross-thread mode.
- Allocation counts are process-wide samples over a timer's wall-clock window, not thread-local counts.
- Normal CPU timers latch into smoothing rings once per render frame. Timers that complete outside that cadence request latch-at-stop. A main-loop path that skips rendering while timers keep accumulating must discard those samples before rendering resumes, as the deferred-swapchain resume does, so the first rendered frame reports only its own work.
- Server raw timer samples are diagnostic-only and latch after all active-cell workers join, but only an accepted normal one-tick update at timescale 1/1 advances the sample. Paused, burst, recording/replay, and no-dispatch paths discard pending raw values, so those updates never advance the sample.

## Slow-operation Diagnostics

Client slow-operation warnings run only under `kbProfilingFrameSpike`, on
the main thread. `ProfileManager` owns the rendered-interval history and cached
expected interval across graphics recreation; `RecordRenderedFrameInterval`
owns the warmup, and the history declaration owns the window size.
`ScopedSlowOperation` captures that interval before timing its operation and
owns the warning multiplier, so the operation cannot raise its own warning
boundary. Without a warmed baseline it does no timing or warning. Client focus
notifications clear diagnostic histories, cancel active warnings, and restart
warmup after discarding each diagnostic's next interval; unfocused intervals
never enter the histories. Hosts without focus notifications remain eligible.
These warnings measure elapsed wall time, not a CPU/GPU cause, and remain
independent of aggregated CPU timers.

`ProfileManager` also owns the per-call reconciliation snapshot and prior normal
tick-cost history. `GameBase::ClientUpdate` clears the snapshot before each
reconciliation; the dispatcher publishes worker diagnostics after joining them.
Executed coord-ticks include failed replay and fallback attempts, unlike CRC
validation counts. `FrameTickPhaseTimings` stores steady-clock nanoseconds per
coord before the merge. The warning prints milliseconds for intervals and
microseconds for per-coord-tick averages. When a positive dispatcher interval
was measured, subtract it from the outer interval to get remaining
reconciliation wall time. `RunFrameTick`
and phase totals sum elapsed wall time across workers and can exceed dispatcher
time. Lazy build timings are included in cell setup. The normal comparison uses
earlier eligible calls with executions, no recalculation, and no desync.
`ScopedSlowOperation` adds the current sample after comparison, regardless of
whether it warns. This history follows the same focus and warmup eligibility as
the slow-operation diagnostics.

## GPU Timing

- GPU queries and ImGui output are client-only. Query reads never stall; `VK_NOT_READY` retains the previous smoothed result, and devices without timestamp support leave the pool unused.
- Each command buffer resets one contiguous timer range. Insert a timer into the range recorded by that command buffer and keep the enum and name table aligned.
- Timestamp endpoints use bottom-of-pipe ordering so adjacent regions measure completed preceding work.

`Graphics` registers each framebuffer's submission coverage with
`GpuSubmitted`; `GpuRead` consumes it once after that framebuffer's fence wait,
before reuse. Coverage belongs to the source submission, not the next frame's
world visibility. Unavailable results remain absent from raw diagnostics and
never advance their histories or the shadow sample; `LogTimers` only prints
cached timers and never consumes queries.

Under `kbProfilingFrameSpike`, GPU anomaly comparisons use fresh raw results,
the source submission's expected interval, and prior eligible per-region
history. Focus transitions clear GPU diagnostic histories and cached context
and invalidate pending submissions' diagnostic eligibility; query-pool teardown
clears submission coverage and GPU diagnostic state.
Measured totals add only non-overlapping roots; child regions overlap them.
They exclude uninstrumented uploads and acquisition, queue waiting, and
presentation waiting. Graphics slow-operation warnings attach the latest
eligible completed sample as older context with its submission identity and
age, never as same-frame CPU attribution.

## Presentation and Dumps

- `TickVisibilityCadence` is the single synchronized visibility decision for CPU timers, CPU counters, and GPU rows. UI views hide sustained zero rows; raw agent queries include them.
- Engine formatters own shared CPU text and every overlay screen: CPU, GPU, and
  Network, which compile under `kbProfileScreens`; ModelViewer compiles neither
  screen file. The memory text on the CPU and GPU screens reads the grid state
  stored on `GameBase` and the Network screen reads client session state, both
  through `game::gpGame`, so `ProfileScreens.cpp` and `ProfileNetworkScreen.cpp`
  are deliberate engine consumers of the game header. The Network screen reuses
  the FPS-header slot, which is free because the FPS header renders only on the
  CPU and GPU screens. Server formatting must remain server-safe and
  allocation-disciplined.
- CPU and GPU memory panels share client-process samples, refreshed on screen
  selection and every two seconds while visible. Private commit measures private
  committed backing; working set measures resident pages, including shared pages.
  These views overlap with pack payload and VMA accounting and must not be added
  into a combined total. Pack payload leaves other owners unmeasured; VMA block
  slack is the difference between VMA blocks and VMA allocations only.
- The ImPlot trend graphs beside the Network screen render only while that
  screen is selected and a client exists. The Network formatter itself samples
  most of their series, so the graphs only accumulate history while that screen
  is up; the clock series and the reconciliation rates instead arrive from the
  client session runtime and the game reconciler. Those callers submit every
  sample, including zero, so the one-second windows age old activity out instead
  of leaving stale nonzero rates on screen. `NetworkGraphs.cpp` reads
  `common::Smoothed` ring internals directly and relies on `miNext == miCount`
  until the ring is full.
- When network simulation is enabled, Network-screen metrics append `"!"` when out of tolerance: RTT and loss compare against `GetNetworkSimulationConfiguration`, the reconciliation rates against `GetNetworkSimulationBounds`, rollback against a hard-coded cap. Fast-forward (`miTimeMultiply > 1`) bypasses only the loss comparison and shows `BYPASS`; the other warning indicators stay live.
- When `kbProfilingDump` is enabled, CSV sampling continues with the overlay hidden and without GPU timestamp support. Its diagnostic-log lifetime spans graphics recreation and ends with manager destruction.

## See Also

- Memory (`../Memory/AGENTS.md`) - Allocation tracking
- Game Profile (the game project's own `Source/Profile/AGENTS.md`)
