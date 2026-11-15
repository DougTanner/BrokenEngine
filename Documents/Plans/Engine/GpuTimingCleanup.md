<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:08:25.023Z","dependsOn":[]} -->
# Zero unwritten GPU timer rows and extract the GPU stall timeline

## Context

Two problems in `Engine/Source/Profile/ProfileManager.cpp`, both verified at
commit `213d2577`.

### Unwritten GPU timer rows keep their last sample

Logged as a refactor candidate on 2026-11-05: `ProfileManager::GpuRead` treats
a query that is still in flight and a query its recording never writes the same
way, so a row that stops being recorded keeps its last sample forever, and the
inactive water foam recording keeps three empty timestamp pairs only to work
around that.

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

### ProfileManager.cpp is over its size threshold

`Engine/Source/Profile/ProfileManager.cpp` measures 12,513 `bt-token-v1`
against the 10,000 `.cpp` reduction threshold `/reduce-file` owns
(`.agents/scripts/Measure-Tokens.ps1`, commit `213d2577`). Its counterpart
`Engine/Source/Profile/ProfileManager.h` measures 4,610 against the 5,000 `.h`
threshold. Logged as a size observation on 2026-11-13 naming the GPU stall
timeline block (`LogGraphicsStall`, `LogStallTimeline`) as the cohesive split.

Evidence for the boundary, all in `ProfileManager.cpp` at that commit:

- `ProfileManager::LogGraphicsStall` (lines 607-707) and
  `ProfileManager::LogStallTimeline` (lines 709-879) are 11,920 bytes, about
  2,980 `bt-token-v1`, the largest single responsibility in the file.
- The state they own is four `ProfileManager` members used for nothing else:
  `mGpuTimeline`, `miPendingStallSequence`, `mpcPendingStallOperation`,
  `mPendingStallStart`, plus the nested `GpuFrameTimeline` struct and
  `kiGpuTimelineCapacity` (`ProfileManager.h:516-538`), and the file-local
  `kpeGpuTimelineRoots` and `GpuTimelineTick` (`ProfileManager.cpp:25-36`).
- Every other touch of that state is a short ring write or clear: `Destroy`
  (141-142), `GpuSubmitted` (400-410), `GpuRead` (429-437, 451-460, 501-504),
  `GpuFenceObserved` (564-577), `GpuImageWait` (589-603), `SetDiagnosticFocus`
  (1048-1049). No file outside `ProfileManager.cpp` and `ProfileManager.h`
  names any of these symbols.
- The block runs on the client main thread only, under `kbProfilingFrameSpike`
  (true only in the Profile configuration, `EngineSwitches.h:51`), and only
  writes log lines.

Two constraints rule out the simpler shapes:

- `ProfileManager` is declared with `class`, so `Documents/C++StyleGuide.txt`
  rule 67 forbids defining its members in a second `.cpp`. Moving the two
  member functions as they are, which is how the logged row reads, is not
  allowed; the block has to become its own type.
- The only rule-67-exempt code in the file, the free function
  `LogReconciliationDiagnostics` plus the `ScopedBootTimer`,
  `ScopedCpuProfile`, and `ScopedSlowOperation` members (lines 1193-1398), is
  8,179 bytes, about 2,045 `bt-token-v1`. Moving all of it leaves the file near
  10,470, still above the threshold, so it is not a qualifying reduction.

## Design

The row fix runs first, then the extraction. The row fix's statement in the
`GpuRead` `VK_NOT_READY` branch stays in `ProfileManager::GpuRead`; it is not
part of the moved code.

### 1. Confirm the Vulkan claim

Before editing, confirm through `/verify-external-claims` the one Vulkan fact
the row fix rests on and the repository cannot prove: once the fence of a
queue submission is signaled, every timestamp query that submission wrote with
`vkCmdWriteTimestamp` is available to `vkGetQueryPoolResults`, and a query
reset by `vkCmdResetQueryPool` and not written afterward is unavailable and
returns `VK_NOT_READY` when neither `VK_QUERY_RESULT_WAIT_BIT` nor
`VK_QUERY_RESULT_PARTIAL_BIT` is set. If the claim is disproved, stop and
return the Plan for re-planning instead of choosing another mechanism.

### 2. Zero unwritten rows in GpuRead

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

### 3. Remove the foam workaround

`RecordWaterFoam` returns at once when `WaterFoamActive()` is false, and the
two `if (bActive)` wrappers and the `bActive` local go away, so an inactive
recording writes no foam timestamp and no foam debug label. Recommended over
keeping the wrappers because the early return is the smaller function.

### 4. Extract GpuStallTimeline

Recommended: extract the stall timeline into one new class,
`engine::GpuStallTimeline`, owned by value by `ProfileManager`. This is
`/reduce-file` reduction 2 (a cohesive stateful responsibility becomes a
class); it is the smallest boundary that brings the file below its threshold.
No log text, condition, or ordering changes.

Files:

- `Engine/Source/Profile/ProfileManager.h` — declares `GpuStallTimeline` in the
  existing `#if defined(BT_CLIENT)` block that forward-declares
  `AcquisitionDiagnostics`, before `class ProfileManager`. Recommended over a
  new header because the comment at `ProfileManager.h:3-5` requires this header
  to stay free of engine includes, and `ProfileManager` holds the object by
  value, which needs the complete type.
- `Engine/Source/Profile/GpuStallTimeline.cpp` — new, defines every
  `GpuStallTimeline` member. It includes `Pch.h`, carries a whole-file
  `#if defined(BT_CLIENT)` guard, and opens `namespace engine`, as
  `NetworkGraphs.cpp` does.
- `Engine/Source/Profile/ProfileManager.cpp` — keeps thin forwarding members.

Types hoisted in `ProfileManager.h`: move the protected nested structs
`ProfileManager::GpuSubmission` and `ProfileManager::GpuDiagnosticSample` to
namespace scope in that same `BT_CLIENT` block, unchanged in fields, so the new
class can name them. Turn `ProfileManager::LogGpuSample(const
GpuDiagnosticSample&, std::string_view)` into the struct member
`void GpuDiagnosticSample::Log(std::string_view pcContext) const`; its body
(lines 527-554) reads no `ProfileManager` state. It and
`GpuDiagnosticSample::RootMicroseconds` stay defined in `ProfileManager.cpp`
(a `struct` is exempt from rule 67), next to `kfGpuAnomalyMultiplier`, which
stays there.

`GpuStallTimeline` private state, moved from `ProfileManager` with names
unchanged: the `GpuFrameTimeline` struct, `kiGpuTimelineCapacity`,
`mGpuTimeline`, `miPendingStallSequence`, `mpcPendingStallOperation`,
`mPendingStallStart`, and their existing comments. `kpeGpuTimelineRoots` and
`GpuTimelineTick` move to `GpuStallTimeline.cpp` as file-local items.

Public members, each taking over the named existing code:

| Member | Body taken from | Called from |
|---|---|---|
| `void Clear()` | `mGpuTimeline.fill({})` and `miPendingStallSequence = 0` | `Destroy`, `SetDiagnosticFocus` |
| `void Submitted(int64_t iSequence, int64_t iFramebuffer, bool bWorld, bool bAcquireBarriers, std::chrono::steady_clock::time_point submitTime)` | the ring write at 402-409 | `GpuSubmitted` |
| `void RecordTicks(int64_t iSequence, GpuTimers eGpuTimer, uint64_t uiStartTick, uint64_t uiEndTick)` | the slot match at 432-436 and the root lookup and write at 453-459 | `GpuRead`, once per read timer |
| `void FenceObserved(int64_t iSequence)` | 565-577 | `GpuFenceObserved` |
| `void ImageWait(int64_t iNewestSequence, std::chrono::steady_clock::time_point waitStart, std::chrono::steady_clock::time_point waitEnd)` | 589-603 | `GpuImageWait` |
| `void LogGraphicsStall(const char* pcOperation, const AcquisitionDiagnostics* pAcquisitionDiagnostics, std::chrono::steady_clock::time_point stallStart, int64_t iNewestSequence, const GpuDiagnosticSample& rLatestSample)` | 611-705 | `ProfileManager::LogGraphicsStall` |
| `void Retired(const GpuDiagnosticSample& rSample, bool bAnomaly)` | the pending-sequence test at 501 and the whole `LogStallTimeline` body, 713-877 | `GpuRead` |

Inside the moved bodies the only substitutions are `miGpuSubmissionSequence`
to the `iNewestSequence` parameter, `mLatestGpuSample` to `rLatestSample`, and
`LogGpuSample(rSample, "Stalled submission")` to
`rSample.Log("Stalled submission")`.

`ProfileManager` after the move:

- Gains the member `GpuStallTimeline mGpuStallTimeline;` in place of the four
  moved members; loses the `LogStallTimeline` and `LogGpuSample` declarations.
- Its public interface is unchanged: `GpuSubmitted`, `GpuRead`,
  `GpuFenceObserved`, `GpuImageWait`, `LogGraphicsStall`, `Destroy`, and
  `SetDiagnosticFocus` keep their signatures, so `Graphics.cpp`,
  `SwapchainManager.cpp`, `Main.cpp`, and `ScopedSlowOperation` do not change.
- Each of those members keeps every `if constexpr (kbProfiling)` and
  `if constexpr (kbProfilingFrameSpike)` test and every `mVkQueryPool` test it
  has today, then forwards. `GpuFenceObserved` still reads the sequence from
  `mGpuSubmissions.at(iFramebuffer)`. `GpuStallTimeline` members add no switch
  test of their own.
- `GpuRead` calls `sample.Log("GPU anomaly")` where it called `LogGpuSample`.

Extraction order:

1. Hoist the two structs and convert `LogGpuSample`; the tree builds.
2. Add `GpuStallTimeline`, move the bodies, replace the members and call
   sites.
3. Add `GpuStallTimeline.cpp` to the projects through `/update-vcxproj`. It is
   a client-only file: expected in `BrokenEngineSandbox.vcxproj` and in
   `ModelViewer.vcxproj`, which compiles `ProfileManager.cpp` as a client, and
   not in `BrokenEngineSandboxServer.vcxproj`.

Expected sizes:

- `ProfileManager.cpp`: about 9,150 `bt-token-v1` (from 12,513).
- `GpuStallTimeline.cpp`: about 3,500.
- `ProfileManager.h`: about 4,810 (from 4,610); the class declaration adds
  seven member declarations and nothing else moves in or out.

If the header would measure above 5,000 after the move, stop and report
rather than trimming unrelated declarations: the by-value design is then no
longer a qualifying reduction and the ownership choice needs re-planning.

### 5. Documents

Update the statements the change makes false:

- `Engine/Source/Profile/AGENTS.md` `## GPU Timing`:
  - The first bullet's "`VK_NOT_READY` retains the previous smoothed result"
    becomes the new rule and its reason (the read follows the framebuffer's
    fence wait, so an unavailable query is one the submission did not write,
    and its row reads zero).
  - The file named beside `fQuarterIntervalMilliseconds` changes from
    `ProfileManager.cpp` to `GpuStallTimeline.cpp`. `/update-claude-docs` owns
    any further wording.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### CommandBufferManager`: the
  foam bullet drops "while retaining paired total, Compute, and History
  timestamps so empty results replace previous samples"; the sentences on what
  Compute and History include stay.

## Critical files

- `Engine/Source/Profile/ProfileManager.cpp`
- `Engine/Source/Profile/ProfileManager.h`
- `Engine/Source/Profile/GpuStallTimeline.cpp` (new)
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`
- `Engine/Source/Profile/AGENTS.md`
- `Engine/Source/Graphics/Managers/AGENTS.md`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `.vcxproj.filters`
- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` and
  `.vcxproj.filters`

## In scope

- `ProfileManager.cpp`:
  - `ProfileManager::GpuRead`: the `VK_NOT_READY` branch (the zeroing
    statement).
  - `kpeGpuTimelineRoots` and `GpuTimelineTick`; the timeline statements in
    `Destroy`, `GpuSubmitted`, `GpuRead`, `GpuFenceObserved`, `GpuImageWait`,
    and `SetDiagnosticFocus`; `LogGraphicsStall`, `LogStallTimeline`, and
    `LogGpuSample` whole.
- `ProfileManager.h`: the nested `GpuSubmission`, `GpuDiagnosticSample`, and
  `GpuFrameTimeline` structs, `kiGpuTimelineCapacity`, the four timeline
  members, the `LogGpuSample` and `LogStallTimeline` declarations, and the new
  `GpuStallTimeline` declaration and member.
- `GpuStallTimeline.cpp`: the new file.
- Project and filter membership for the new file.
- `CommandBufferRecordMain::RecordWaterFoam`: the inactive early return and
  the removal of `bActive` and its two wrappers.
- `Engine/Source/Profile/AGENTS.md` `## GPU Timing`: the first bullet and the
  one file name beside `fQuarterIntervalMilliseconds`.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### CommandBufferManager`,
  foam bullet.

## Out of scope

- Any change to what the stall timeline logs, when, or in what order, and any
  change to a threshold, capacity, or condition.
- Any `ProfileManager` function, member, or call from
  `CommandBufferManager::RecordCommandBuffers` added for the row fix; the row
  fix is the one `VK_NOT_READY` statement.
- Every other `ProfileManager` responsibility: CPU timers, boot timers, raw
  server timers, query-pool setup, `GpuStart`, `GpuStop`, `ResetQueryPools`,
  the anomaly comparison, `mGpuSubmissions`, `mGpuDiagnosticHistories`, and
  `mGpuShadowSample` in `GpuRead`, `DumpTimers`, and the rendered-interval
  history.
- The timer enum and the name table.
- The rows before `kGpuTimerUiRender` on a world-hidden frame: `GpuRead` does
  not read them for a UI-only submission, so they keep their last world
  sample. That is the submission-coverage rule, not this defect.
- `ScopedBootTimer`, `ScopedCpuProfile`, `ScopedSlowOperation`, and
  `LogReconciliationDiagnostics`: they stay in `ProfileManager.cpp`.
- A new header, a change to `Engine.h`, and any change to a caller of the
  `ProfileManager` GPU members outside `Engine/Source/Profile/`.
- `ProfileScreens.cpp`, `ProfileNetworkScreen.cpp`, `NetworkGraphs.cpp`.
- Every other timestamp pair in the Global, Main, and ImGui recordings,
  including the conditional ones in `RecordImageRenderPass`.
- Foam activation, images, pipelines, and the foam shaders.

## Risk and invariants

Change Workflow Tier 2 — scoped behavior. Triggers: it changes what the client
GPU profiling rows report, and it restructures one subsystem's (Profile)
runtime diagnostic code behind a new class and adds namespace-scope types to a
header `Engine.h` aggregates, which is more than Tier 1's "no public signature
exposure" allows. No Tier 3 surface: the code is client-only, main-thread,
log- and profile-row-only, outside the simulation and the CRC, and touches no
wire format, serialization, save, replay, threading, or trust boundary.

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

- `pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path 'Engine/Source/Profile/ProfileManager.cpp','Engine/Source/Profile/GpuStallTimeline.cpp','Engine/Source/Profile/ProfileManager.h' -Json"`
  reports each `.cpp` at or below 10,000 and the header at or below 5,000.
- Every `ProfileManager` member is defined in `ProfileManager.cpp` and every
  `GpuStallTimeline` member in `GpuStallTimeline.cpp` (rule 67).
- The moved statements differ from commit `213d2577` only by the three
  substitutions `## Design` step 4 names and by indentation; every `LOG` format
  string in the block is byte-identical. Exception: the one statement step 2
  adds to the `GpuRead` `VK_NOT_READY` branch (zeroing
  `mpGpuTimers[eGpuTimer].smoothedMicroseconds`) is excluded from this
  comparison.
- The BrokenEngineSandbox client and ModelViewer build in the Profile
  configuration, the one where `kbProfilingFrameSpike` is true, and the
  BrokenEngineSandbox server builds; `/compile` owns the commands.
- The static-checks runner reports no project-membership failure for the new
  file.
- One live `/agent-harness` run of the Profile client reading `query_profile`
  `gpuTimers` settles the row fix; the supplied evidence records no
  reproducing setup, so the executing session builds one covering these
  states:
  - With Graphics Water Foam off, the Water Foam, Compute, and History rows
    report `currentUs` 0, and turning foam off after it ran drops those rows
    to 0 instead of leaving their last active sample.
  - With `gDebugTexture` on, the Terrain, Water, and Billboards rows report
    `currentUs` 0 instead of their last normal-mode sample.
  - With the map overlay covering the world, the HDR Resolve row reports
    `currentUs` 0.
  - With foam on and the world visible, every row that reported a nonzero
    sample before the change still does.

The stall timeline prints only when a graphics operation overruns its expected
interval in a Profile client; the evidence behind this Plan records no
`/agent-harness` setup that forces one, so the diff and the builds are the
decisive checks for the extraction.

## Coordination

- `Documents/Plans/Engine/IslandsUpdateOrderingAndEmission.md` may move the
  `Graphics::WaitFramebuffer` calls this Plan cites by line
  (`Graphics.cpp:213-216`, `244-247`); it keeps the fence wait ahead of
  `GpuRead` on the same framebuffer, the order this Plan relies on. Whichever
  lands second re-derives its regions by symbol from the landed file.
- Another logged candidate proposes a behavior change near
  `ProfileManager.cpp:378` (`GpuStop`). It is separate work in a function this
  Plan does not touch; whichever lands second rebases over the other.
