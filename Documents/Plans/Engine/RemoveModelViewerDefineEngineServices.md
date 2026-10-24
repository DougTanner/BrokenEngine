<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T14:02:14.679Z","dependsOn":[]} -->
# Remove BT_MODEL_VIEWER, Phase 2: Engine Services

## Context

The engine must not know that the ModelViewer exists. `BT_CLIENT` and `BT_SERVER` stay, because both executables are
the engine; `BT_MODEL_VIEWER` names one application and goes. This is phase 2 of 3:

- Phase 1 (landed): `kbModelViewer` and `kbFramebufferClearColor` are gone and neither program's `Pch.h` gained a
  switch (both still include `EngineSwitches.h`); one image pass (`CommandBufferRecordMain::RecordImageRenderPass`)
  serves both programs; where the viewer differs, each program's `Game.h` supplies `game::kVkHdrClearColor` and
  `game::kfUiReferenceHeight`, and the viewer supplies `engine::RenderFrameMain` and `game::Camera::SunAngle`.
- Phase 2 (this plan): removes the gates around four engine services the viewer neither uses nor can link: the client
  crash-report upload, the audio streaming harness rig hooks in pack loading, the profile overlay screens, and the
  engine collections' graphics resources.
- Phase 3 (Documents/Plans/Engine/RemoveModelViewerDefineGameCoupling.md, depends on this plan): removes the gates around
  engine code that names the game (aggregation header, camera update, terrain elevation, island upload, UI screen
  ownership with the Tweaks save and load across Graphics recreation) and deletes the define.

User's words (recorded): "remove BT_MODEL_VIEWER and replace its changes with either variables passed in or more
fine-grained constexpr kb* feature switches"; "I'm ok with the viewer including more engine files, we don't have to cap
it at 69 files. I really don't want the engine aware that the model viewer exists, BT_CLIENT BT_SERVER makes sense
because they both ARE the engine."; "We don't need to fix this all in a single plan, we could do this in phases, and
work towards the goal." For each remaining gate the user chose, case by case, among the viewer supplying the fixed-name
piece the engine expects, the engine taking a game value as a parameter, and code that names the game moving into an
engine file only gameplay programs compile, "with the simplest most logical paths for the particular code taken on a
case-by-case basis". Sandbox client/server behavior, CRC/determinism, and pack format stay unchanged; ModelViewer
behavior stays unchanged.

Why these gates exist: each sits in an engine file the viewer compiles (ModelViewer.vcxproj:155-158, 178, 205) and
calls code defined only in files the viewer does not compile. MSVC resolves every external an object file references,
including references from functions nothing calls, so the call itself must leave the viewer's objects. Compiling the
callees into the viewer is not possible, because each chain reaches the game:

- Crash upload calls `Client::SendCrashReport` and `Client::UploadCrashReport` (Engine/Source/Network/Client/Client.cpp:342,
  352); Client.cpp needs `game::gpGame` (:821, :834) and a complete `game::Frame` (:394, :1039).
- Rig hooks call `AudioStreamingHarnessRig` statics (Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp:269, 470,
  520, 533, 541); that file calls `StreamingVoices::Play`/`Clear`, whose file calls `game::gpGame->GetNextMusicTrack()`
  (Engine/Source/Audio/StreamingVoices.cpp:55).
- Profile screens call `FormatFramesPerSecondHeader`, `FormatCpuScreen`, `FormatGpuScreen`
  (Engine/Source/Profile/ProfileScreens.cpp:269, 306, 350; game state at :192-202) and `FormatNetworkScreen`
  (Engine/Source/Profile/ProfileNetworkScreen.cpp:234; `game::gpGame` and `game::gpClientSession` at :53, :132, :144-165).
- `ForEachGraphicsResources(InterpolateTypes {})` instantiates the eight engine collections' `GraphicsResources`,
  defined in `*Render.cpp` files whose other members read engine collections of `game::FrameInterpolate`
  (for example Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp:36, 57), which the viewer's
  `game::FrameInterpolate` (ModelViewer/Source/Frame/Frame.h:12-16) does not hold.

History: every `BT_MODEL_VIEWER` gate was introduced in 601012a8d and 7d64500ec, both "Opus/Astra plans" (no recorded
reason).

Line numbers cite commit 7d64500ecb21b6d05acc859ab5f417067a8f0796. Phase 1 edits no region this plan names other than
ModelViewer/AGENTS.md `## Engine integration` (Design item 6) and adds no switch to either program's `Pch.h`, so
Design items 2 and 3 create each program's switch block; locate regions by content when lines have shifted.

## Design

Each item states the path chosen, why, and the sandbox and viewer equivalence.

1. Client crash-report upload: the code that needs the gameplay-only `Client` moves into a gameplay-only engine file.
   - Recommended over a switch because the gated region holds namespace-scope definitions (`if constexpr` cannot wrap
     them) and over viewer-supplied `Client` members because `Client` is a class (Documents/C++StyleGuide.txt rule 67).
   - New file Engine/Source/CrashReportUpload.cpp, client-only (whole-file `#if defined(BT_CLIENT)`), listed only in
     the sandbox client project. It holds Engine/Source/CrashReport.cpp:354-519 verbatim inside `namespace engine`:
     the sent-trailer constants, `sbPendingCrashReportDelivered`, `OnCrashReportPacketFreed`, `ReadCrashReport`,
     `AppendCrashReportSentTrailer`, `TryUploadCrashReport`, and `PollPendingCrashReport`. It includes `CrashReport.h`.
   - `ClientInstanceMutexName` (CrashReport.cpp:346-352) stays in CrashReport.cpp; it needs no `Client`. Its gate
     (:344, :521) becomes `#if defined(BT_CLIENT)` ... `#endif // BT_CLIENT`.
   - `ReportPath` and `PendingReportPath` (CrashReport.cpp:33-43) lose `static` and are declared in
     Engine/Source/CrashReport.h beside `ParseCrashReportHeader` (:20), unguarded, because the moved functions read
     them. Recommended: keep their names, since no other `engine::` name collides (git grep) and renaming would touch
     every caller in CrashReport.cpp.
   - Every caller already sits in a gameplay-only file (Engine/Source/Main.cpp:498, 513;
     Engine/Source/Agent/Commands/CrashReportHarnessRig.cpp:30; Engine/Source/Network/Client/ClientSessionRuntime.cpp:205),
     and the declarations in CrashReport.h:21-26 are unchanged. Sandbox: the same functions run with the same bodies
     and the same statics. Viewer: none of these functions was compiled before or after except
     `ClientInstanceMutexName`, which it never calls.
2. Audio streaming harness rig hooks: a new feature switch `kbAudioStreamingHarnessRig`.
   - Recommended because the hooks are statements inside `PackChunks` and `PackChunkLoader` member functions, and the
     viewer has no audio streaming for a rig to observe (it compiles no Engine/Source/Audio file), so the switch names a
     real difference. Pulling the audio chain into the viewer would need `StreamingVoices`, `StreamingVoice`, and
     `AudioManager`, which reach the game too, plus a change to the lazy playlist advance in `CheckTrackTransition`.
   - Projects/BrokenEngineSandbox/Source/Pch.h (shared by the sandbox client and server), in a new switch block after
     the `kbAutoConnect` block (:5-11): `inline constexpr bool kbAudioStreamingHarnessRig = true;` with a trailing
     comment stating it compiles the agent audio streaming harness rig's pack-loading hooks (Debug clients).
     ModelViewer/Source/Pch.h, in a new switch block after `#include "ExternalHeaders.h"` (:3): the same name
     `= false`. Not in EngineSwitches.h: the viewer `Pch.h` includes it, so a definition
     there would collide.
   - Include gates Engine/Source/File/PackChunks.cpp:8 and Engine/Source/File/PackChunkLoader.cpp:7 become
     `#if defined(BT_DEBUG)`, so the rig names stay declared for the discarded statements.
   - Every hook site keeps its configuration gate without the viewer term and wraps its unchanged statements in
     `if constexpr (kbAudioStreamingHarnessRig) { ... }`: PackChunks.cpp:650-652, 722-724, 729-731, 759-761, 886-889
     (`#if defined(BT_DEBUG)`), and PackChunkLoader.cpp:89-94, 193-195, 242-249, 301-307
     (`#if defined(BT_CLIENT) && defined(BT_DEBUG)`). Recommended to wrap the :242 stall check too, although it
     names only header-inline state, so the switch means one thing: with it off, pack loading consults no rig at all.
   - PackChunks.cpp:843-847 becomes: `ePartition` computed exactly as today, `bool bHeld = false;`, then
     `if constexpr (kbAudioStreamingHarnessRig) { Record(...); bHeld = HoldAudioRead(...); }`, so the :886-889 block
     can still read both.
   - Sandbox: the switch is `true` in every configuration, so each statement runs in the same order with the same
     arguments; `bHeld` receives the same value. Viewer: the discarded statements generate no code and name only
     declared entities, so nothing references the rig's definitions, as at baseline.
3. Profile overlay screens: a new feature switch `kbProfileScreens`.
   - Recommended because the gated region is a statement block inside `ProfileManager::UpdateProfileText`, every name
     in it is declared in the viewer (`FormatFramesPerSecondHeader` and the other formatters in
     Engine/Source/Profile/ProfileManager.h:494-500; `game::kCpuTimerFrameUpdate`, already read unconditionally at
     ProfileManager.cpp:14), and the viewer presents no profile screens. The formatters themselves name the game, so
     compiling them into the viewer is not an option.
   - Projects/BrokenEngineSandbox/Source/Pch.h, in the item 2 switch block after `kbAudioStreamingHarnessRig`:
     `inline constexpr bool kbProfileScreens = true;` with a trailing comment stating it compiles the CPU, GPU, and
     Network profile overlay screens. ModelViewer/Source/Pch.h, in its item 2 switch block: `= false`.
   - ProfileManager.cpp:617-647: `#if !defined(BT_MODEL_VIEWER)` ... `#endif` becomes
     `if constexpr (kbProfileScreens) { ... }` with the body unchanged (still inside the existing
     `#if defined(BT_CLIENT)` and `if constexpr (kbProfiling)`).
   - Sandbox: same statements. Viewer: discarded block, as at baseline; the viewer never toggles a screen anyway.
4. Engine-collection graphics resources: the engine call moves into a gameplay-only engine file, called by the game
   frame type the same way the engine interpolate update is.
   - Precedent: the sandbox `game::FrameInterpolate::Update` calls `FrameInterpolateBase::Update` first
     (Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:152), which iterates the engine `InterpolateTypes`
     (Engine/Source/Frame/FrameBase.cpp:199). Recommended because the program whose frame type derives from
     `FrameInterpolateBase` is the one that holds engine collections, so it is the one that creates their resources;
     the viewer's frame type holds none.
   - Engine/Source/Frame/FrameBase.h: `FrameInterpolateBase` gains `static void GraphicsResources();` after `Update`
     (:104), inside `#if defined(BT_CLIENT)`.
   - Engine/Source/Frame/FrameBase.cpp, after `FrameInterpolateBase::Update` (:188-200), inside
     `#if defined(BT_CLIENT)`: `void FrameInterpolateBase::GraphicsResources() { ForEachGraphicsResources(InterpolateTypes {}); }`.
     FrameBase.cpp is not a viewer item.
   - Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:131-134: `FrameInterpolate::GraphicsResources` calls
     `FrameInterpolateBase::GraphicsResources();` before its existing `ForEachGraphicsResources(GameInterpolateTypes {})`.
   - Engine/Source/Graphics/Managers/PipelineManager.cpp:164-166: delete; :167 `game::FrameInterpolate::GraphicsResources();`
     stays.
   - Sandbox: the same eight engine calls, then the same game calls, at the same point in the `PipelineManager`
     constructor. Viewer: its `game::FrameInterpolate::GraphicsResources` is unchanged, as at baseline.
5. Project membership via `/update-vcxproj`: add Engine/Source/CrashReportUpload.cpp to the sandbox client project and
   its filters (Engine filter beside CrashReport.cpp). Server and ModelViewer membership unchanged.
6. Documentation (`/update-claude-docs`; each would be wrong after the change):
   - Engine/Source/Profile/AGENTS.md:27-33: the overlay screens are compiled under `kbProfileScreens` rather than
     "in gameplay builds".
   - ModelViewer/AGENTS.md `## Engine integration` (as Phase 1 leaves it): the define no longer excludes crash upload,
     the rig hooks, profile screens, or collection graphics resources; the viewer `Pch.h` sets
     `kbAudioStreamingHarnessRig` and `kbProfileScreens` to `false`.
   - Engine/Source/AGENTS.md `## Hub Conventions` Switches bullet (:33): `EngineSwitches.h` holds the engine-read
     switches with one value for every program and the `keLogLevel*` compile floors, and each program's `Pch.h` (game
     and ModelViewer) includes it after `Log/LogTypes.h` and before `Common.h`; an engine-read switch whose value
     differs by program is defined instead in each program's `Pch.h`, before those includes, as `kbAutoConnect` is.
   - Unchanged and still true: Engine/Source/AGENTS.md `## Crash Reporting` (names functions, not files),
     Engine/Source/Agent/AGENTS.md:58-66, Engine/Source/Graphics/Managers/AGENTS.md:74.

## Critical files

- Engine/Source/CrashReport.cpp, Engine/Source/CrashReport.h, Engine/Source/CrashReportUpload.cpp (new).
- Engine/Source/File/PackChunks.cpp, Engine/Source/File/PackChunkLoader.cpp.
- Engine/Source/Profile/ProfileManager.cpp.
- Engine/Source/Frame/FrameBase.h, Engine/Source/Frame/FrameBase.cpp, Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp,
  Engine/Source/Graphics/Managers/PipelineManager.cpp.
- Projects/BrokenEngineSandbox/Source/Pch.h, ModelViewer/Source/Pch.h.
- Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj and .filters.

## Risk triggers and invariants

- Tier 3: the change spans independently owned subsystems (crash reporting, File/Agent harness rig, Profile, Frame and
  Graphics, the sandbox frame type, and project membership).
- Sandbox CRC, determinism, and pack format unchanged: no PostRender or simulation code changes; the rig hooks run on
  loader threads with unchanged statements and order.
- Sandbox text at each site equals its baseline sandbox text, moved or wrapped; viewer behavior equals its baseline
  `BT_MODEL_VIEWER` behavior.
- The viewer builds rely on statements discarded by `if constexpr` in a non-template function generating no reference
  to undefined entities and raising no unused-variable warning for `ePartition`/`bHeld`; the A2 Debug and Release
  viewer builds settle both.
- No callback or function pointer added to reach game code; no switch names the application.

## In scope

- Engine/Source/CrashReport.cpp:33, 39 (`static` removed), 344 and 521 (gate becomes `BT_CLIENT` only), 354-519 (moved
  out).
- Engine/Source/CrashReport.h beside :20 — `ReportPath` and `PendingReportPath` declarations.
- Engine/Source/CrashReportUpload.cpp (new) — the moved block.
- Engine/Source/File/PackChunks.cpp:8-10, 650-652, 722-724, 729-731, 759-761, 843-847, 886-889.
- Engine/Source/File/PackChunkLoader.cpp:7-9, 89-94, 193-195, 242-249, 301-307.
- Engine/Source/Profile/ProfileManager.cpp:617-647.
- Engine/Source/Frame/FrameBase.h:103-104 region — `GraphicsResources` declaration.
- Engine/Source/Frame/FrameBase.cpp after :200 — `FrameInterpolateBase::GraphicsResources` definition.
- Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:131-134.
- Engine/Source/Graphics/Managers/PipelineManager.cpp:164-166.
- Projects/BrokenEngineSandbox/Source/Pch.h after :11 and ModelViewer/Source/Pch.h after :3 — a new switch block
  holding the two switches, sandbox `true` and viewer `false`.
- Membership: sandbox client vcxproj and filters add CrashReportUpload.cpp.
- Documentation lines in Design item 6.

## Out of scope

- Phase 3 (Documents/Plans/Engine/RemoveModelViewerDefineGameCoupling.md) — these `BT_MODEL_VIEWER` sites remain after
  this plan:
  - Engine/Source/Engine.h:11-13, 91-102.
  - Engine/Source/Graphics/CameraBase.cpp:9-12, 17-67, 76-368, 380-523.
  - Engine/Source/Frame/IslandTerrain.cpp:7-9, 232-256, 324-403, 525-569.
  - Engine/Source/Graphics/Islands.cpp:192-399.
  - Engine/Source/Graphics/GraphicsUtils.cpp:123-136.
  - Engine/Source/Graphics/Graphics.cpp:465-473, 747-757 (Tweaks load and save around ImGui manager recreation; Phase 3
    because they exist only while the ImGui manager owns the Tweaks screen).
  - Engine/Source/Graphics/Managers/ImGuiManager.cpp:7-16, 20-22, 171-181, 491-518 and ImGuiManager.h:64-66, 104-113.
  - ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66 (both define entries).
- Phase 1 sites and changes.
- Compiling Audio, Client, ProfileScreens, ProfileNetworkScreen, or collection `*Render.cpp` files into the viewer.
- Any change to `StreamingVoices::CheckTrackTransition`, the rig's behavior, crash-report format or version, profile
  formatting, or collection resource creation.
- Shader lighting, pack format, extra viewer features, descriptor pool, lazy pool, and worktree-startup-build findings.
- New harness commands; ModelViewer harness support.
- Backward compatibility of any kind.

## Acceptance criteria

The execution card's Acceptance checks A1-A5 are the acceptance criteria.

## Execution card

Execution card:
### What does this plan do?
Second step of removing the ModelViewer-only compile flag from the engine. Four engine services the viewer neither uses
nor can link lose their viewer gates: the client crash-report upload moves into its own engine file that only the game
client compiles; pack loading's audio streaming harness rig hooks and the profile overlay screens get narrowly named
on/off switches; and the engine collections' graphics resources are created by the game's frame type the same way it
already runs the engine interpolate update.
### Why this is good for the codebase
The user does not want the engine to know the viewer exists. These gates go without adding any viewer code, each
replacement names what it controls, and what remains for Phase 3 is only the engine code that names the game.
- Goal: the Phase 2 gates in CrashReport.cpp, PackChunks.cpp, PackChunkLoader.cpp, ProfileManager.cpp:617-647, and PipelineManager.cpp are gone; remaining `BT_MODEL_VIEWER` sites are exactly the `## Out of scope` Phase 3 list; sandbox and viewer behavior unchanged; all six builds pass.
- Out of scope: Phase 3 sites (including the Graphics.cpp Tweaks load and save), compiling more game-reaching engine files into the viewer, behavior changes, other review findings, new harness commands (see `## Out of scope`).
- Tier trigger: Tier 3 — spans independently owned subsystems (crash reporting, File/Agent rig, Profile, Frame/Graphics, sandbox frame type, project membership).
- Interfaces and invariants: new client-only Engine/Source/CrashReportUpload.cpp (sandbox client only) holds the upload
  block verbatim; `engine::ReportPath()`/`PendingReportPath()` become declared non-static functions; new switches
  `kbAudioStreamingHarnessRig` and `kbProfileScreens`, `true` in the sandbox Pch.h and `false` in the viewer Pch.h;
  `FrameInterpolateBase::GraphicsResources()` (client) iterates the engine collections and the sandbox
  `FrameInterpolate::GraphicsResources` calls it first. Invariants: no PostRender/CRC change; sandbox statements and
  order at every site equal baseline; no callback to game code; no switch names the application.
- Acceptance checks:
  - A1 `git grep -n BT_MODEL_VIEWER -- Engine ModelViewer/Source ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` lists exactly the `## Out of scope` Phase 3 sites by file and gate (re-numbered as edits shift lines); a read at the session baseline commit shows the Phase 2 sites present.
  - A2 `/compile` ModelViewer Debug|x64 and Release|x64 and BrokenEngineSandbox client and server Debug|x64 and Release|x64 all succeed with warnings as errors (Release includes PREfast); the viewer builds settle the discarded-statement and unused-variable risk.
  - A3 Code read of the diff against the session baseline commit: CrashReportUpload.cpp body equals CrashReport.cpp:354-519; each switch site's statements equal baseline inside `if constexpr`; `bHeld` gets the same value in the sandbox; the sandbox calls engine then game `GraphicsResources` at the PipelineManager.cpp:167 point; `/update-vcxproj` reports client-only membership for the new file.
  - A4 Sandbox Debug client+server per `/agent-harness`, once before the first edit and once after: connect and spawn;
    `audio_resume`, then `audio_streaming_harness_rig` `clear_history`, `play` one music CRC from `Data/Audio.h`, and
    `inspect` after the stream starts: `records` contain `refill_queued`, `refill_loading`, and `refill_ready` phases in
    both runs; `key` `P` then `query_profile`: `cpuTimerText` is non-empty in both runs; `query_profile` GPU rows
    `    VisibleLights`, `    Billboards`, `    Hex Shields` are nonzero after spawn wherever they were nonzero in the
    baseline run; no desync or CRC-mismatch log lines; finally `crash_report_harness_rig` on the connected client and
    server `get_logs` with pattern `ClientCrashReport Stored` returns a line in both runs.
  - A5 ModelViewer runtime: after implementation the user launches the viewer and confirms models, background, UI, and sun drag look as before.
- Roles: implementer `/implement-plan` (single slice); `/update-affected-code`; `/update-vcxproj`; `/compile`; `/agent-harness` for A4; reviewers `/repo-code-review`, `/comment-review`, `/code-style-review`, `/over-engineering-review`, `/adversarial-review` (Tier 3); `/update-claude-docs`, then `/coherence-review` and `/progressive-disclosure-review` for the AGENTS.md edits; `/verify-acceptance`; user for A5; plan review `/plan-audit`, `/plan-simplicity-review`, `/external-grill-plan`.
