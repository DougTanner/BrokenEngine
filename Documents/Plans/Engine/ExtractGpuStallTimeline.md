<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:06:55.184Z","dependsOn":[]} -->
# Extract the GPU stall timeline out of ProfileManager.cpp

## Context

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

Recommended: extract the stall timeline into one new class,
`engine::GpuStallTimeline`, owned by value by `ProfileManager`. This is
`/reduce-file` reduction 2 (a cohesive stateful responsibility becomes a
class); it is the smallest boundary that brings the file below its threshold.
No log text, condition, or ordering changes.

### Files

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

### Types hoisted in ProfileManager.h

Move the protected nested structs `ProfileManager::GpuSubmission` and
`ProfileManager::GpuDiagnosticSample` to namespace scope in that same
`BT_CLIENT` block, unchanged in fields, so the new class can name them.
Turn `ProfileManager::LogGpuSample(const GpuDiagnosticSample&,
std::string_view)` into the struct member
`void GpuDiagnosticSample::Log(std::string_view pcContext) const`; its body
(lines 527-554) reads no `ProfileManager` state. It and
`GpuDiagnosticSample::RootMicroseconds` stay defined in `ProfileManager.cpp`
(a `struct` is exempt from rule 67), next to `kfGpuAnomalyMultiplier`, which
stays there.

### GpuStallTimeline

Private state, moved from `ProfileManager` with names unchanged: the
`GpuFrameTimeline` struct, `kiGpuTimelineCapacity`, `mGpuTimeline`,
`miPendingStallSequence`, `mpcPendingStallOperation`, `mPendingStallStart`,
and their existing comments. `kpeGpuTimelineRoots` and `GpuTimelineTick` move
to `GpuStallTimeline.cpp` as file-local items.

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

### ProfileManager after the move

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

### Order

1. Hoist the two structs and convert `LogGpuSample`; the tree builds.
2. Add `GpuStallTimeline`, move the bodies, replace the members and call
   sites.
3. Add `GpuStallTimeline.cpp` to the projects through `/update-vcxproj`. It is
   a client-only file: expected in `BrokenEngineSandbox.vcxproj` and in
   `ModelViewer.vcxproj`, which compiles `ProfileManager.cpp` as a client, and
   not in `BrokenEngineSandboxServer.vcxproj`.
4. In `Engine/Source/Profile/AGENTS.md` `## GPU Timing`, change the file named
   beside `fQuarterIntervalMilliseconds` from `ProfileManager.cpp` to
   `GpuStallTimeline.cpp`. `/update-claude-docs` owns any further wording.

### Expected sizes

- `ProfileManager.cpp`: about 9,150 `bt-token-v1` (from 12,513).
- `GpuStallTimeline.cpp`: about 3,500.
- `ProfileManager.h`: about 4,810 (from 4,610); the class declaration adds
  seven member declarations and nothing else moves in or out.

## Critical files

- `Engine/Source/Profile/ProfileManager.cpp`
- `Engine/Source/Profile/ProfileManager.h`
- `Engine/Source/Profile/GpuStallTimeline.cpp` (new)
- `Engine/Source/Profile/AGENTS.md`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`
  and `.vcxproj.filters`
- `Projects/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` and
  `.vcxproj.filters`

## In scope

- `ProfileManager.cpp`: `kpeGpuTimelineRoots` and `GpuTimelineTick`; the
  timeline statements in `Destroy`, `GpuSubmitted`, `GpuRead`,
  `GpuFenceObserved`, `GpuImageWait`, and `SetDiagnosticFocus`;
  `LogGraphicsStall`, `LogStallTimeline`, and `LogGpuSample` whole.
- `ProfileManager.h`: the nested `GpuSubmission`, `GpuDiagnosticSample`, and
  `GpuFrameTimeline` structs, `kiGpuTimelineCapacity`, the four timeline
  members, the `LogGpuSample` and `LogStallTimeline` declarations, and the new
  `GpuStallTimeline` declaration and member.
- `GpuStallTimeline.cpp`: the new file.
- Project and filter membership for the new file.
- The one file name in `Engine/Source/Profile/AGENTS.md` `## GPU Timing`.

## Out of scope

- Any change to what is logged, when, or in what order, and any change to a
  threshold, capacity, or condition.
- Every other `ProfileManager` responsibility: CPU timers, boot timers, raw
  server timers, query-pool setup, `GpuStart`, `GpuStop`, `ResetQueryPools`,
  the anomaly comparison and `mGpuDiagnosticHistories` in `GpuRead`,
  `DumpTimers`, and the rendered-interval history.
- `ScopedBootTimer`, `ScopedCpuProfile`, `ScopedSlowOperation`, and
  `LogReconciliationDiagnostics`: they stay in `ProfileManager.cpp`.
- A new header, a change to `Engine.h`, and any caller outside
  `Engine/Source/Profile/`.
- `ProfileScreens.cpp`, `ProfileNetworkScreen.cpp`, `NetworkGraphs.cpp`.

## Risk tier

Tier 2 — scoped behavior. Trigger: it restructures one subsystem's (Profile)
runtime diagnostic code behind a new class and adds namespace-scope types to a
header `Engine.h` aggregates, which is more than Tier 1's "no public signature
exposure" allows. No Tier 3 surface: the code is client-only, main-thread,
log-only, outside the simulation and the CRC, and touches no wire format,
serialization, save, replay, or trust boundary.

## Acceptance criteria

- `pwsh -NoProfile -Command "& '.agents/scripts/Measure-Tokens.ps1' -Path 'Engine/Source/Profile/ProfileManager.cpp','Engine/Source/Profile/GpuStallTimeline.cpp','Engine/Source/Profile/ProfileManager.h' -Json"`
  reports each `.cpp` at or below 10,000 and the header at or below 5,000.
- Every `ProfileManager` member is defined in `ProfileManager.cpp` and every
  `GpuStallTimeline` member in `GpuStallTimeline.cpp` (rule 67).
- The moved statements differ from commit `213d2577` only by the three
  substitutions `## Design` names and by indentation; every `LOG` format
  string in the block is byte-identical.
- The BrokenEngineSandbox client and ModelViewer build in the Profile
  configuration, the one where `kbProfilingFrameSpike` is true, and the
  BrokenEngineSandbox server builds; `/compile` owns the commands.
- The static-checks runner reports no project-membership failure for the new
  file.

The stall timeline prints only when a graphics operation overruns its expected
interval in a Profile client; the evidence behind this Plan records no
`/agent-harness` setup that forces one, so the diff and the builds are the
decisive checks.

## Notes

- If the header would measure above 5,000 after the move, stop and report
  rather than trimming unrelated declarations: the by-value design is then no
  longer a qualifying reduction and the ownership choice needs re-planning.
- Another logged candidate proposes a behavior change near
  `ProfileManager.cpp:378` (`GpuStop`). It is separate work in a function this
  Plan does not touch; whichever lands second rebases over the other.
