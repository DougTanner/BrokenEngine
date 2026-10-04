<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T23:08:23.504Z","dependsOn":[]} -->
# Split the client-only Network screen out of ProfileManagerBase

## Context

At baseline `81cf299f2bd76ea5c37aa181a24b24cfdcb78b13`, `engine::ProfileManagerBase` (`Engine/Source/Profile/ProfileManagerBase.h:333`) is one class whose member definitions span three `.cpp` files of different build affinity:

- `Engine/Source/Profile/ProfileManagerBase.cpp` — compiled by both `BrokenEngineSandbox.vcxproj` and `BrokenEngineSandboxServer.vcxproj`; defines every other member.
- `Engine/Source/Profile/ProfileNetworkScreen.cpp` — client-only (client vcxproj only, whole-file `#if defined(BT_CLIENT)`); defines `FormatNetworkScreen`, `FormatNetworkTransport`, `FormatNetworkPeerMetrics`, `FormatNetworkTraffic`, `FormatNetworkSynchronization`, `FormatNetworkPrediction`, `FormatNetworkClock`, `FormatNetworkReconciliation`, `SetClockCorrection`, plus the file-local `AppendBytes`.
- `Engine/Source/Profile/NetworkGraphs.cpp` — client-only, whole-file `BT_CLIENT` wrap; defines `RenderImPlotGraphs` plus file-local `SmoothedGetter` and `PlotSmoothed`.

Every member those two client-only files define, and every data member they touch, belongs to one cohesive unit — the Network screen: the private formatters and the `BT_CLIENT` state at `ProfileManagerBase.h:482-508` (`mSmoothedRoundTripTime`, `mSmoothedJitter`, `mSmoothedClockOffset`, `mSmoothedClockTarget`, `mSmoothedClockError`, `mSmoothedRollback`, `mSmoothedBuffer`, `mSmoothedReceived`, and the five `InTheLastSecond` reconciliation rates), plus the public `RenderImPlotGraphs`, `SetClockCorrection` (`ProfileManagerBase.h:378-383`). No member defined in `ProfileManagerBase.cpp` reads that state; the only coupling is the one call `FormatNetworkScreen(rWorkbuffer)` in `ProfileManagerBase::UpdateProfileText` (`ProfileManagerBase.cpp:774`), and `RenderImPlotGraphs` reading `meProfileScreen` (`NetworkGraphs.cpp:35`).

External callers of the Network-screen members:

- `Engine/Source/Graphics/Managers/ImGuiManager.cpp:519` — `gpProfileManager->RenderImPlotGraphs()`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp:709` — `gpProfileManager->SetClockCorrection(...)`
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1122` — direct reads of `gpProfileManager->mSmoothedClockOffset.mSmoothedValue`, `mSmoothedClockTarget.mSmoothedValue`, and `mSmoothedClockError.mSmoothedValue`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp:123` — direct `.Set(...)` calls on the five reconciliation-rate members of `gpProfileManager`

`ProfileNetworkScreen.cpp` includes `Game.h` and reads `game::gpGame` and `game::gpClientSession`; `Engine/Source/Profile/AGENTS.md` `## Presentation and Dumps` records that as a deliberate engine consumer of the game header. `ProfileManagerBase.cpp` already includes `Game.h` for its own reasons; this change adds no game-header include to any shared file.

Gap: a class is split between a shared `.cpp` and client-only `.cpp` files, and its member definitions are spread over three `.cpp` files.

## Design

Recommended boundary, from the cohesion above: extract the Network screen into a new client-only class `engine::ProfileNetworkScreen`, named after its existing file and the established "Network screen" term, owned by `ProfileManagerBase` as a public `BT_CLIENT` data member. `ProfileManagerBase` then keeps all of its member definitions in the shared `ProfileManagerBase.cpp`, and `ProfileNetworkScreen` keeps all of its member definitions in the client-only `ProfileNetworkScreen.cpp`.

Ordered, buildable steps:

1. In `ProfileManagerBase.h`, declare `class ProfileNetworkScreen` inside an `#if defined(BT_CLIENT)` block placed before `class ProfileManagerBase` (the header stays engine-include-free; every type the class needs — `common::Workbuffer`, `common::Smoothed`, `common::InTheLastSecond`, `ENetPeer` — is already named by the current declaration). Public: `FormatNetworkScreen`, `RenderImPlotGraphs`, `SetClockCorrection`, the three smoothed clock members, and the five reconciliation-rate members. Private: the seven remaining `FormatNetwork*` helpers and the remaining Network-screen data members. Move all data members with their existing names, types, access, and default initialization. Keeping the existing member names avoids renaming churn at the call sites.
2. In `ProfileManagerBase`, delete those declarations (the public `RenderImPlotGraphs`/`SetClockCorrection` declarations and the whole `BT_CLIENT` Network-screen state and formatter section at `ProfileManagerBase.h:482-508`; drop the then-empty `private:` label) and add a public `ProfileNetworkScreen mNetworkScreen;` under the existing public `BT_CLIENT` block beside `mGpuShadowSample`, following that public-data precedent.
3. In `ProfileManagerBase::UpdateProfileText`, replace `FormatNetworkScreen(rWorkbuffer)` with `mNetworkScreen.FormatNetworkScreen(rWorkbuffer)`.
4. Merge `NetworkGraphs.cpp` into `ProfileNetworkScreen.cpp`: move `SmoothedGetter` and `PlotSmoothed` as file-local `static` functions and `RenderImPlotGraphs` as a `ProfileNetworkScreen` member; add `#include "Network/Client/Client.h"` to the merged file's include block in the order `.editorconfig` sorts it. Requalify every moved definition from `ProfileManagerBase::` to `ProfileNetworkScreen::`. `RenderImPlotGraphs` no longer has `meProfileScreen` as a member, so the recommendation is to read `gpProfileManager->meProfileScreen` at the same early-out (`ProfileNetworkScreen.cpp` and `ProfileScreens.cpp` already reach the manager and other managers through engine globals); the guard order and every other statement stay unchanged. Delete `NetworkGraphs.cpp`.
5. Route the four external sites through `gpProfileManager->mNetworkScreen`, preserving the same method arguments, clock-value reads, and five reconciliation-rate submissions.
6. Route file membership through `/update-vcxproj`: remove `NetworkGraphs.cpp` from `BrokenEngineSandbox.vcxproj` and `BrokenEngineSandbox.vcxproj.filters`; `ProfileNetworkScreen.cpp` stays client-only; no new header file is added.
7. Update `Engine/Source/Profile/AGENTS.md` `## Presentation and Dumps` through `/update-claude-docs`: the ImPlot-graph and `common::Smoothed` ring facts now name `ProfileNetworkScreen` / `ProfileNetworkScreen.cpp` instead of `NetworkGraphs.cpp`.

Behavior is unchanged: same statements, same state, same call order, same per-sample submission from `ClientSessionRuntime` and `ClientReconciler`. `ProfileNetworkScreen` holds only value members, so its construction and destruction as a member are equivalent to the current in-class members.

Change Workflow tier: **Tier 2**. Highest trigger: public engine API reshaped (four `ProfileManagerBase` public members move to a new class) with call-site edits across Graphics, Network/Client, Agent, and the game reconciler; one subsystem's (Profile) code, behavior-preserving. No determinism/CRC exposure — the Network screen and its smoothed series are client-only visuals outside PostRender state and the CRC. No wire, serialization, `.pack`/`kiVersion`, replay, threading, allocation-discipline, or shader exposure. Affinity exposure: the new class is client-only and must be invisible to the server build (header block under `BT_CLIENT`; the `.cpp` keeps its whole-file wrap per `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`).

## Critical files

- `Engine/Source/Profile/ProfileManagerBase.h`
- `Engine/Source/Profile/ProfileManagerBase.cpp`
- `Engine/Source/Profile/ProfileNetworkScreen.cpp`
- `Engine/Source/Profile/NetworkGraphs.cpp` (deleted)
- `Engine/Source/Graphics/Managers/ImGuiManager.cpp`
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp`
- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `.vcxproj.filters`
- `Engine/Source/Profile/AGENTS.md`

## In scope

- `ProfileManagerBase.h`: the new `BT_CLIENT` `class ProfileNetworkScreen` declaration; removal of the moved declarations from `ProfileManagerBase`; the new `mNetworkScreen` member.
- `ProfileManagerBase.cpp`: the one `FormatNetworkScreen` call in `ProfileManagerBase::UpdateProfileText`.
- `ProfileNetworkScreen.cpp`: requalification of every member definition to `ProfileNetworkScreen::`; the merged-in `RenderImPlotGraphs`, `SmoothedGetter`, `PlotSmoothed`, and the `Network/Client/Client.h` include; the `meProfileScreen` read in `RenderImPlotGraphs`.
- Deletion of `NetworkGraphs.cpp`.
- The single call expression at each of `ImGuiManager.cpp:519`, `ClientSessionRuntime.cpp:709`, `AgentCommandsClientGeneric.cpp:1122`, `ClientReconciler.cpp:123`.
- `NetworkGraphs.cpp` removal from the client vcxproj and filters.
- `Engine/Source/Profile/AGENTS.md` `## Presentation and Dumps` file/class naming.

## Out of scope

- Any change to Network-screen output text, thresholds, smoothing, sampling cadence, or graph layout.
- Any other split of `ProfileManagerBase` (GPU timers, raw server timers, dumps, boot timers stay as they are in the shared `ProfileManagerBase.cpp` under their existing guards).
- `ProfileScreens.cpp` free functions, the game `ProfileManager` class, and `ScopedBootTimer`/`ScopedCpuProfile`.
- Adding a game-header include to any shared engine file, a new header file, a new global pointer, or forwarding wrappers on `ProfileManagerBase`.
- The `ProfileManagerBase::Create` query-pool reset, owned by `Documents/Plans/Engine/VulkanHostQueryResetInitialization.md` (disjoint region; no ordering needed).

## Acceptance criteria

- `NetworkGraphs.cpp` no longer exists; no `ProfileManagerBase::` definition remains outside `ProfileManagerBase.cpp`, and every `ProfileNetworkScreen::` definition is in `ProfileNetworkScreen.cpp`.
- `/update-vcxproj` validation passes for both projects.
- `/compile` builds the client and the server cleanly; the server build sees no `ProfileNetworkScreen` symbol.
- Live check through `/agent-harness`: with a connected client and the profile overlay on the Network screen, a screenshot shows the same sections (Transport, Sync, Prediction, Clock, Reconciliation) and the Network Graphs window; the agent command that reads clock correction (`AgentCommandsClientGeneric.cpp:1122`) still returns values.

## Notes

- Origin: the requested rule that a `class` keeps all member definitions in one `.cpp`, and that no class is split between client-only and shared files.
- Size context: the merged Profile class files measure ~9,749 bt-token-v1 at baseline; the split moves the Network-screen declaration and both client-only files' definitions into one class and one `.cpp` without reducing total size.
