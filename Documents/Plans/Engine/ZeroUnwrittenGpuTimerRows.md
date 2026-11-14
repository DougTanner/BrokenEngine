<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:08:25.023Z","dependsOn":[]} -->
# Zero the GPU timer rows a submission never wrote

## Context

Logged as a refactor candidate on 2026-11-05: `ProfileManager::GpuRead` treats
a query that is still in flight and a query its recording never writes the same
way, so a row that stops being recorded keeps its last sample forever, and the
inactive water foam recording keeps three empty timestamp pairs only to work
around that.

Verified at commit `213d2577`:

- `ProfileManager::GpuRead` (`Engine/Source/Profile/ProfileManager.cpp:414`)
  reads each timer's two queries with `vkGetQueryPoolResults` and no wait or
  partial flag (line 444). On `VK_NOT_READY` it does `continue` (lines
  445-448), which leaves `mpGpuTimers[eGpuTimer].smoothedMicroseconds` and
  `sample.microseconds[i]` untouched.
- Every recording resets its whole timer range first
  (`ProfileManager::ResetQueryPools`, called at
  `CommandBufferRecordGlobal.cpp:30`, `CommandBufferRecordMain.cpp:32`,
  `ImGuiManager.cpp:699`), so a timer the recording then does not write is
  unavailable on every read and its row never changes again.
- Both `GpuRead` callers run it straight after `Graphics::WaitFramebuffer` on
  the same framebuffer (`Engine/Source/Graphics/Graphics.cpp:213-216` and
  `244-247`), and `WaitFramebuffer` blocks until that framebuffer's fence is
  signaled (`Graphics.cpp:118-131`). The ImGui submission that signals the
  fence is the last of the frame. So the submission `GpuRead` consumes has
  finished, and "still in flight" is not a state `GpuRead` can observe; the
  one remaining source of `VK_NOT_READY` is a query the consumed submission
  did not write.
- Rows that go stale today:
  - Debug texture mode (`gDebugTexture`, a command-buffer-tier recreation,
    `Graphics.cpp:598`): `CommandBufferRecordMain::RecordImageRenderPass`
    skips the `if (!bDebugTextureMode)` block
    (`CommandBufferRecordMain.cpp:470-562`), so Objects, Terrain, Water, Hex
    Shields, Transparent Objects, both particle render rows, Visible Lights,
    and Billboards keep their last normal-mode sample.
  - A world-hidden frame: `ImGuiManager` writes `kGpuTimerHdrResolve` only
    when `bAfterMain` (`ImGuiManager.cpp:720-729`), and `GpuRead` starts such
    a submission at `kGpuTimerUiRender`, so HDR Resolve keeps its last world
    sample while the map overlay is up. This row changes every frame, not at
    re-recording.
- The workaround: `CommandBufferRecordMain::RecordWaterFoam`
  (`CommandBufferRecordMain.cpp:335-422`) writes the `kGpuTimerWaterFoam`,
  `kGpuTimerWaterFoamCompute`, and `kGpuTimerWaterFoamHistory` start and stop
  timestamps around two `if (bActive)` blocks, so an inactive recording still
  produces near-zero results. `Engine/Source/Graphics/Managers/AGENTS.md`
  `### CommandBufferManager` documents this as "retaining paired total,
  Compute, and History timestamps so empty results replace previous samples".

The logged row proposed clearing the `mpGpuTimers` rows when command buffers
are re-recorded. That mechanism does not hold at this commit, for two reasons
found while verifying it:

- A command-buffer-tier recreation does not call `ProfileManager::Destroy`
  (`Graphics.cpp:762-800` calls it only at the swapchain tier and above), so
  the query pool and each framebuffer's pending `mGpuSubmissions` entry
  survive the re-record. The first `GpuRead` per framebuffer after the
  re-record consumes the old submission, whose queries are still available,
  and writes the old sample back into the cleared row.
- It leaves HDR Resolve stale, because that row does not change at
  re-recording.

## Design

Recommended: fix the read, not the recordings. In `GpuRead`, on
`VK_NOT_READY`, assign zero to `mpGpuTimers[eGpuTimer].smoothedMicroseconds`
before the `continue`, and leave `sample.microseconds[i]` unset. Rationale: it
is one statement at the single place the two cases were confused, it needs no
new function, member, or caller, and it covers every unwritten row (inactive
foam, debug texture mode, HDR Resolve on a world-hidden frame) without each
recording having to cooperate. The row then reads zero, which is what the
foam's empty pairs produce today, and the profile screens already hide rows
that stay at zero (`TickVisibilityCadence`).

Leaving `sample.microseconds[i]` unset keeps the rule in
`Engine/Source/Profile/AGENTS.md` `## GPU Timing` that unavailable results
stay absent from raw diagnostics and never advance their histories or the
shadow sample.

Then remove the workaround: `RecordWaterFoam` returns at once when
`WaterFoamActive()` is false, and the two `if (bActive)` wrappers and the
`bActive` local go away, so an inactive recording writes no foam timestamp and
no foam debug label. Recommended over keeping the wrappers because the early
return is the smaller function.

Before editing, the executing session confirms through
`/verify-external-claims` the one Vulkan fact the design rests on and the
repository cannot prove: once the fence of a queue submission is signaled,
every timestamp query that submission wrote with `vkCmdWriteTimestamp` is
available to `vkGetQueryPoolResults`, and a query reset by
`vkCmdResetQueryPool` and not written afterward is unavailable and returns
`VK_NOT_READY` when neither `VK_QUERY_RESULT_WAIT_BIT` nor
`VK_QUERY_RESULT_PARTIAL_BIT` is set. If the claim is disproved, stop and
return the Plan for re-planning instead of choosing another mechanism.

Update the two documents whose statements the change makes false:

- `Engine/Source/Profile/AGENTS.md` `## GPU Timing`: the first bullet's
  "`VK_NOT_READY` retains the previous smoothed result" becomes the new rule
  and its reason (the read follows the framebuffer's fence wait, so an
  unavailable query is one the submission did not write, and its row reads
  zero).
- `Engine/Source/Graphics/Managers/AGENTS.md` `### CommandBufferManager`: the
  foam bullet drops "while retaining paired total, Compute, and History
  timestamps so empty results replace previous samples"; the sentences on what
  Compute and History include stay.

## Critical files

- `Engine/Source/Profile/ProfileManager.cpp`
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`
- `Engine/Source/Profile/AGENTS.md`
- `Engine/Source/Graphics/Managers/AGENTS.md`

## In scope

- `ProfileManager::GpuRead`: the `VK_NOT_READY` branch only.
- `CommandBufferRecordMain::RecordWaterFoam`: the inactive early return and
  the removal of `bActive` and its two wrappers.
- `Engine/Source/Profile/AGENTS.md` `## GPU Timing`, first bullet.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### CommandBufferManager`,
  foam bullet.

## Out of scope

- Any new `ProfileManager` function, member, or call from
  `CommandBufferManager::RecordCommandBuffers`.
- `mGpuSubmissions`, `mGpuDiagnosticHistories`, `mGpuShadowSample`, the
  anomaly comparison, and the stall timeline.
- The rows before `kGpuTimerUiRender` on a world-hidden frame: `GpuRead` does
  not read them for a UI-only submission, so they keep their last world
  sample. That is the submission-coverage rule, not this defect.
- `ResetQueryPools`, `GpuStart`, `GpuStop`, the timer enum, and the name
  table.
- Every other timestamp pair in the Global, Main, and ImGui recordings,
  including the conditional ones in `RecordImageRenderPass`.
- Foam activation, images, pipelines, and the foam shaders.

## Risk and invariants

Change Workflow Tier 2 — scoped behavior: it changes what the client GPU
profiling rows report, with no determinism, wire, serialization, threading, or
trust-boundary exposure.

- Client-only, compiled under `kbProfiling`; nothing here enters the CRC.
- Threading is unchanged: `GpuRead`, `UpdateProfileText`, the profile screens,
  and the agent `query_profile` handler all touch `mpGpuTimers` on the main
  thread, and command buffers are recorded on the main thread inside
  `Graphics::Create` after `vkDeviceWaitIdle`.
- Query reads still never stall: no wait flag is added.
- The Main recording still resets the foam query range every submission, so
  the unwritten foam queries are unavailable on every read.
- An inactive foam recording still omits every foam transition, dispatch,
  copy, and mip blit.

## Acceptance criteria

Settled by a live `/agent-harness` run of the Profile client reading
`query_profile` `gpuTimers`; the supplied evidence records no reproducing
setup, so the executing session builds one from the three states below.

- With Graphics Water Foam off, the Water Foam, Compute, and History rows
  report `currentUs` 0, and turning foam off after it ran drops those rows to
  0 instead of leaving their last active sample.
- With `gDebugTexture` on, the Terrain, Water, and Billboards rows report
  `currentUs` 0 instead of their last normal-mode sample.
- With the map overlay covering the world, the HDR Resolve row reports
  `currentUs` 0.
- With foam on and the world visible, every row that reported a nonzero
  sample before the change still does.

## Notes

- `Documents/Plans/Engine/ExtractGpuStallTimeline.md` also edits `GpuRead`,
  on other lines (the timeline statements at 429-437, 451-460, and 501-504).
  Neither Plan needs the other; whichever lands second rebases over a
  neighboring edit in the same function.
