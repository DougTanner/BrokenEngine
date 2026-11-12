<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-12T00:26:46.968Z","dependsOn":[]} -->
# Investigate acquisition-fence stalls with GPUView

## Context

The generic GPU logging change exposed an unresolved frame-pacing symptom,
separate from its completed diagnostic acceptance criteria. The user requested
this follow-up to investigate with Microsoft GPUView after landing that change.
The manual Profile run recorded tick 23840, acquisition invocation 942,
graphics frame 941, incoming framebuffer 1, returned image 2, and VkResult 0.
Its acquisition scope took 16,215,000 ns: the prior acquisition fence wait took
16,177,300 ns, vkAcquireNextImageKHR took 35,800 ns, and other work 1,900 ns.
The frame interval was 17.2 ms against an 8.3 ms expected interval.

`Engine/Source/Graphics/Managers/SwapchainManager.cpp:531`
(`SwapchainManager::AcquireNextImage`) waits for the previous acquisition fence
before resetting the next fence and calling vkAcquireNextImageKHR. These
same-invocation measurements localize the host delay, not its underlying cause.
The older completed GPU sample was submission 938, framebuffer 1, age 41.532 ms:
roots totaled 7,317 us (Global 259, Main 2,088, Image 4,955, UI 15).
Water's 3,176 us was a child region, not an additional root. That older sample
showed no GPU anomaly and cannot causally explain invocation 942. A separate
16.6 ms interval at tick 23651 also remains unexplained. The run ended normally.

The observed device was AMD Radeon RX 9070 XT with FIFO presentation. Neither
FIFO nor the older sample proves a cause. Exact manual actions and timestamps
were not recorded. Automated three-fleet/six-map scenarios at 4K/120 fps did not
reproduce the stall. The original log contains an absolute machine path, so its
relevant facts are preserved here instead of copying it into the Plan.

## Design

The author recommends a bounded GPUView investigation because application
queries cannot distinguish queued work, display pacing, and CPU scheduling.
First use `/external-diagnose-bug` to confirm root cause from a new correlated
capture. Check Windows Performance Toolkit/GPUView availability and capture
permissions; record any concrete feasibility blocker without guessing a cause.
Use Microsoft's current [GPUView guidance][gpuview] for a short ETL capture
covering CPU scheduling, GPU command queues, presentation, and vsync events.

Use `/compile` for Profile builds and `/agent-harness` for engine launch,
queries, and logs, with an explicit `--log-file`. Preserve the user's normal
settings and save. Prefer a short manual reproduction; record exact actions,
window/focus state, display refresh, present mode, and capture start/end times.
Establish a verified clock mapping and process/thread identity between the
acquisition log and ETL before attributing events. If existing timestamps are
insufficient, propose only minimal correlation instrumentation for review.
Compare the matching fence-wait interval against queued/active GPU work,
presentation/vsync, and CPU scheduling. Include a nearby normal interval.

After proving an engine defect, make the smallest fix within the acquisition
and presentation region below, with no performance fix selected in advance.
A cause or fix outside that region requires re-planning. If evidence instead
shows expected external pacing or remains inconclusive, record the supported
conclusion and return it for disposition; do not invent a fix or declare the
symptom resolved. Retain trace identity, timing ranges, and analysis evidence
in a durable findings record accessible to the completing session's reviewers.

[gpuview]: https://learn.microsoft.com/en-us/windows-hardware/drivers/display/using-gpuview

## Critical files

- `Engine/Source/Graphics/Managers/SwapchainManager.cpp`:
  `AcquireNextImage`, `PresentToQueue`, and `Present`.
- `Engine/Source/Graphics/Graphics.cpp`: `PresentAcquire`, read for call order.
- `Engine/Source/Profile/ProfileManager.cpp`: `GpuRead`, read for sample age.
- The corresponding Graphics/Managers and Profile AGENTS.md constraints.

## In scope

- Root-cause investigation of the recorded acquisition-fence stall using
  correlated GPUView evidence and a nearby non-stalling control interval.
- Minimal reviewed correlation instrumentation at `AcquireNextImage` only if
  needed to identify the same invocation in the trace.
- An evidence-supported fix limited to `AcquireNextImage`, `PresentToQueue`,
  and `Present`, after diagnosis and review of the concrete proposed change.

## Out of scope

- Generic profiler redesign, new tracing infrastructure, automated workload
  systems, shader optimization, or attributing the delay to the older sample.
- Changing the user's saved settings/save, simulation, server, wire formats,
  serialization, replay behavior, or unrelated frame-interval anomalies.
- Predetermined present-mode, fence, queue, or frame-in-flight policy changes.

## Acceptance criteria

- A reproducible capture recipe identifies the manual setup/actions, executable,
  settings, log, ETL, clock mapping, and exact matching acquisition invocation.
- Evidence distinguishes host wait from queued/active GPU execution, display
  pacing, and CPU scheduling; uncertainty is explicit and causal claims use
  matching timestamps rather than older GPU timings or FIFO alone.
- Any proposed fix follows the demonstrated cause and remains within scope.
  After a fix, the recorded symptom no longer reproduces under the same setup
  and bounded repeated captures; report duration and recurrence counts.
- A changed runtime passes the required Profile builds and `/agent-harness`
  checks, including normal presentation and swapchain recreation. Preserve
  synchronization correctness, delayed-sample attribution, and user data.

## Notes

Future execution is Tier 3 if it changes acquisition/presentation synchronization:
that exposes threading and synchronization invariants. Capture-only diagnosis
introduces no runtime change; classify any instrumentation or fix before edits.
Keep all work client-side and outside deterministic PostRender/CRC, save/replay,
wire, and pack formats. No unit tests. No dependencies or Coordination constraints
with existing Plans; lighting profiler Plans address different outcomes.
