<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T14:06:53.740Z","dependsOn":[]} -->
# Remove BT_MODEL_VIEWER, Phase 3: Game Coupling

## Context

The engine must not know that the ModelViewer exists. `BT_CLIENT` and `BT_SERVER` stay, because both executables are
the engine; `BT_MODEL_VIEWER` names one application and goes. This is phase 3 of 3:

- Phase 1 (landed): one image pass serves both programs; each program's `Game.h` supplies `game::kVkHdrClearColor`
  and `game::kfUiReferenceHeight`, and the viewer's `engine::RenderFrameMain` and `game::Camera::SunAngle` replace the
  gates where the viewer renders or behaves differently.
- Phase 2 (landed): the client crash-report upload is in Engine/Source/Network/Client/ClientCrashReport.cpp, which only
  the sandbox client compiles; the audio streaming harness rig's pack-loading hooks compile under
  `kbAudioStreamingHarnessRig` and the profile overlay screens under `kbProfileScreens`, both switches set in each
  program's `Pch.h`; and the sandbox `FrameInterpolate::GraphicsResources` creates the engine-collection graphics
  resources.
- Phase 3 (this plan): removes every remaining gate, each around engine code that names the game, and deletes the
  define. Afterwards no file outside Documents/Plans mentions `BT_MODEL_VIEWER`.

User's words (recorded): "remove BT_MODEL_VIEWER and replace its changes with either variables passed in or more
fine-grained constexpr kb* feature switches"; "I'm ok with the viewer including more engine files, we don't have to cap
it at 69 files. I really don't want the engine aware that the model viewer exists, BT_CLIENT BT_SERVER makes sense
because they both ARE the engine."; "We don't need to fix this all in a single plan, we could do this in phases, and
work towards the goal." For these sites the user chose, case by case, among the viewer supplying the fixed-name piece
the engine expects and the engine taking game values as parameters or moving code that names the game into engine
files only gameplay programs compile, "with the simplest most logical paths for the particular code taken on a
case-by-case basis". Sandbox client/server behavior, CRC/determinism, and pack format stay unchanged; ModelViewer
behavior stays unchanged. The `Tools/ModelViewer/AGENTS.md` prohibitions on placeholder game pieces and on instantiating the
sandbox game were agent-written and are rewritten to match the result.

A `kb*` switch cannot remove these gates: the gated code names types and globals the viewer does not declare at all
(`game::gpGame`, `Cell`, `game::HudScreen`, complete `game::Frame`), and an `if constexpr` discarded statement still
needs every name declared.

History: every `BT_MODEL_VIEWER` gate was introduced in 601012a8d and 7d64500ec, both "Opus/Astra plans" (no recorded
reason).

Line numbers cite commit 7d64500ecb21b6d05acc859ab5f417067a8f0796. Phase 1 edits ImGuiManager.cpp:106-111, 152-159,
438-451, 473-480, ImGuiManager.h:139 (deleted), Islands.h:50/52 (gate lines only), ModelViewerApplication.cpp:521-537,
Tools/ModelViewer/Source/Game.h (two constants added after :7; :10-11 deleted), Engine/Source/Graphics/AGENTS.md:57-64,
and Tools/ModelViewer/AGENTS.md; Phase 2 edits Tools/ModelViewer/AGENTS.md, the Engine/Source/AGENTS.md
`## Hub Conventions` Switches bullet, Projects/BrokenEngineSandbox/Source/Pch.h and Tools/ModelViewer/Source/Pch.h (a
`kbAudioStreamingHarnessRig` and `kbProfileScreens` switch block, in the sandbox after the `kbAutoConnect` block and
before `#include "Engine.h"`), and the sandbox client project and filter files (one `ClCompile` item each). No other
region named here is edited by an earlier phase. Locate regions by content when lines have shifted.

## Design

Each item states the path chosen, why, and the sandbox and viewer equivalence. All changed code is client render,
camera, or UI code outside PostRender and the CRC, or include placement.

1. `File/DifferenceStream.h` out of the aggregation (Engine/Source/Engine.h:11-13). Its inline bodies use complete
   `game::Frame` and `game::FrameInput` (DifferenceStream.h:9-30, 206-239); the viewer never uses it. Users include it
   directly, the hub rule for headers that need game types (Engine/Source/AGENTS.md:20-33).
   - Delete Engine.h:11-13.
   - Add `#include "File/DifferenceStream.h"` to Engine/Source/File/Replay.h inside its `BT_SERVER` block beside
     `Save/GameSaveLoad.h` (:5) — Replay.h holds `std::unique_ptr` members of both stream classes (:21, :51, :57), and
     Replay.cpp gets it through Replay.h — and to Engine/Source/Agent/Commands/ClientDesyncProbe.cpp,
     Engine/Source/Network/Client/ReconcileReplayTick.cpp, and
     Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp, the other `TransferViaStream` callers
     (:126, :187, :145/:197). Recommended in Replay.h rather than Replay.cpp because Replay.h declares the members.
   - The viewer Pch.h gets no change: it already has `kbReplayFullFrames` through `EngineSwitches.h`
     (EngineSwitches.h:38/:55/:72), which it keeps including after Phase 1, and no viewer file reaches
     DifferenceStream.h.
   - Sandbox: the same header compiles into the same TUs that use it; no code changes.
2. `GameBase.h` and the generic agent command headers out of the aggregation (Engine.h:91-102). `GameBase.h` stores
   game types by value (GameBase.h:3-14, 68-157). The sandbox `Pch.h` includes the block right after `Engine.h`:
   - Delete Engine.h:91-102; insert its inner lines (:92-101, comment included) verbatim after
     Projects/BrokenEngineSandbox/Source/Pch.h:21 `#include "Engine.h"`, before `Network/GameMessages.h`.
   - Recommended over a new engine aggregation header because one game project exists and four lines move.
   - Sandbox: same headers in the same order relative to everything except Engine.h's trailing formatter
     specializations (Engine.h:104-152), which now precede `GameBase.h`; they name only engine types.
3. Camera update (Engine/Source/Graphics/CameraBase.cpp:9-12, 17-67, 76-368, 380-523). Only three lines reach outside
   generic engine code: the main-menu read (:138), `game::Camera::PullTarget` (:178), and
   `gpRawInputManager->SetVibration` (:195-196; Engine/Source/Input/RawInputManager.cpp:119 reads `game::gpGame`). The
   engine takes the main-menu flag and the pulled target as parameters, and the gameplay host makes the game calls.
   - `CameraBase::Update` (CameraBase.h:168, CameraBase.cpp:122) becomes
     `Update(const FrameInterpolateBase& rFrameInterpolate, float fDeltaTime, bool bMainMenuFrame, const CameraTarget& rPulledTarget)`.
     Delete :138; at :178 use `rPulledTarget` where the `PullTarget` call was (free camera still uses
     `CameraTarget::Direct(mVecPosition)`); delete :195-196; rewrite the :175 and :200-201 comments to the new truth
     (the caller pulled the target before the update).
   - Delete the gate lines (:9-12 with their includes, :17, :67, :76, :368, :380, :523). All remaining names are engine
     names the viewer compiles (Engine/Source/Ui/WrapperBase.cpp:7-11, ShadowWrappersBase.cpp:7,
     LightingWrappersBase.cpp:59). The viewer never calls `CameraBase::Update`; its `game::Camera::Update` hides it.
   - New public client member `void GameBase::UpdateCamera(GridCoord cameraCoordinate);` beside
     `HandleDeferredSwapchain` (GameBase.h:284-285), defined in GameBase.cpp: take
     `mRenderInterpolates.at(cameraCoordinate)`, read `mGameFlags & GameFlags::kMainMenu`, call
     `static_cast<game::Camera&>(*gpCamera).PullTarget(...)`, call `gpCamera->Update(..., static_cast<float>(mfLastRenderFrameSeconds), ...)`,
     then `float fVibration = std::pow(gpCamera->mfShake, 0.5f); gpRawInputManager->SetVibration(fVibration, fVibration);`.
     GameBase.cpp adds `#include "Graphics/Camera.h"` in its client include block (:4-6). Recommended as one helper
     because both callers have the identical shape: GameBase.cpp:854 becomes `UpdateCamera(cameraCoordinate);` and
     Engine/Source/Main.cpp:326 becomes `pGame->UpdateCamera(kOriginCoordinate);`.
   - Sandbox equivalence: `bMainMenuFrame` is the same flag (`game::gpGame` is this `GameBase`). `PullTarget`
     (Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp:19-78) reads game state, the interpolate, its own
     tracked-entity member, and `mVecVisualErrorOffset`, none of which `Update` changes before its old call point
     (`ShiftToRenderedCell`, :88-120, does not touch the offset); in the main menu it returns the menu pose from its
     first branch with no side effect, so pulling it while the free camera drives the pose changes nothing. Vibration
     uses `mfShake` after its decay at :134, which nothing later in `Update` changes, and controller vibration does not
     feed the matrices or visible area.
   - Remaining difference: `PullTarget`'s once-per-second verbose diagnostic throttle (Camera.cpp:54-56) now reads
     `mfTime` before `Update` adds this frame's delta, so that verbose log line may print one render frame later.
     Recommended to accept it, because no rendered, simulated, or persisted value changes, whereas keeping the exact
     order would split `Update` into two public calls around `PullTarget`.
4. Terrain elevation for render projection (Engine/Source/Frame/IslandTerrain.cpp:7-9, 232-256, 324-403, 525-569) and
   the projections (Engine/Source/Graphics/GraphicsUtils.cpp:123-136). The only game reference is
   `game::gpGame->mCells` (:394-395, :558-559). The code that walks the game's cells moves into a gameplay-only engine
   file; the per-cell sampling stays an `IslandTerrain` member.
   - `CellElevation` (:332-377) becomes a public const member
     `float IslandTerrain::CellElevation(const CellStaticData& rStaticData, const XMFLOAT4A& rf4Position) const`,
     declared in IslandTerrain.h beside `CellNormal` (:139), with `rTerrain.` dropped from its body; its gate (:324) and
     the `GlobalNormal` clause of its comment (:325-331) go.
   - New file Engine/Source/Graphics/FlightPlaneProjection.cpp, client-only, sandbox client project only: file-static
     `ResolveLocalPosition` (IslandTerrain.cpp:233-255, comment without the `GlobalNormal` clause), file-static
     `GlobalElevation(GridCoord coord, FXMVECTOR vecLocalPosition)` (:379-401 with `gpIslandTerrain->` for
     `mfSeaFloorElevation` and `CellElevation`, Frame Purity assert kept, comment from IslandTerrain.h:146-150), and
     `ProjectToFlightPlane`/`ProjectToBaseHeight` (GraphicsUtils.cpp:124-135, calling the file-static
     `GlobalElevation`). Rule 67 forbids an `IslandTerrain` member in another file, and the only `GlobalElevation`
     caller is `ProjectToFlightPlane`, so it needs no header. `ProjectTo*` declarations stay in GraphicsUtils.h:51-55;
     their callers are gameplay-only collection render files (AreaLightsRender.cpp:131-134, PointLightsRender.cpp:102,
     PuffsRender.cpp:99, SmokeTrailsRender.cpp:113-114, WindRadialsRender.cpp:81, WindTrailsRender.cpp:102-103).
   - Delete `IslandTerrain::GlobalNormal` (IslandTerrain.cpp:525-569, IslandTerrain.h:152): no caller in Engine/ or
     Projects/ (git grep). Recommended over moving dead code. Delete the `GlobalElevation` declaration and comment
     (IslandTerrain.h:146-151).
   - Comments made false by the deletion are rewritten: IslandTerrain.cpp:515-516 (`CellNormal`),
     Engine/Source/Frame/FrameBase.cpp:239-240, 279-282, Engine/Source/Frame/CellStaticData.h:49 now name
     `GlobalElevation` only.
   - Sandbox: identical arithmetic on the same singleton (`gpIslandTerrain` was `this` at the only call site), same
     assert. Server: `CellElevation` compiles there unused, as `GlobalElevation` did.
5. Island upload (Engine/Source/Graphics/Islands.cpp:192-399). It needs only each active cell's static data, but
   takes `std::unordered_map<GridCoord, Cell>`; the engine takes the static data as a parameter. `CellStaticData`
   already carries its own `coordinate` (Engine/Source/Frame/CellStaticData.h:26), set on every path that fills a
   cell (Engine/Source/GameBase.cpp:990, Engine/Source/Network/Client/ClientSessionBase.cpp:59,
   Engine/Source/File/GridSave.cpp:118), so no new type is needed.
   - Islands.h: replace the `struct Cell;` and `struct GridCoord;` forward declarations (:10-11) with
     `struct CellStaticData;`; the declaration (:51) becomes
     `void UpdateActiveIslands(std::span<const CellStaticData* const> activeCells);`.
   - Islands.cpp: delete the gate lines (:192, :399); each of the three loops (:259-267, :340-349, :364-373) iterates
     `activeCells` as `const CellStaticData* pStaticData`, dropping the map lookup and the empty-ring skip, with
     `const CellStaticData& rStaticData = *pStaticData` and `rStaticData.coordinate` in place of `rCoordinate`.
   - GameBase::UpdateActiveIslands (GameBase.cpp:1129-1143) pushes `&it->second.staticData` into its existing
     workbuffer arena for each coordinate that passes today's filter and then has `iSnapshotCount != 0` (the
     callee's skip, applied after it as today), in `mActiveCoordinates` order, and passes
     `Span<const CellStaticData* const>()`; its comment (:1131-1132) and the GameBase.cpp:833-834 comment (which
     cites the skip's location) are updated.
   - Sandbox: the callee visits exactly the entries it kept before, in the same order, with the same per-entry work;
     the map is not modified during the call, so the pointers stay valid. One lookup per cell instead of four.
6. UI screens (Engine/Source/Graphics/Managers/ImGuiManager.cpp:7-16, 20-22, 171-181, 491-518; ImGuiManager.h:64-66,
   104-113) and the Tweaks load and save around ImGui manager recreation (Engine/Source/Graphics/Graphics.cpp:465-473,
   747-757). The engine calls three host-defined functions; gameplay builds define them in a gameplay-only engine file
   that owns the screens, and the viewer defines its own, as it already supplies `engine::RenderFrameMain`.
   - Recommended because the screens reach the game (`game::HudScreen`, `game::gpGame`), the viewer has its own
     panel, and every alternative either moves engine screen ownership into the game or makes the viewer construct a
     `GameBase`. The Tweaks load and save move with the screens because they exist only to carry the Tweaks screen's
     state across its recreation with the manager.
   - ImGuiManager.h declares, after `gpImGuiManager` (:137), `void CreateUiScreens();`, `void RenderUiScreens();`,
     `void DestroyUiScreens();` with one comment that the host program defines them and the manager calls them at
     construction, between `ImGui::NewFrame` and `ImGui::Render`, and at destruction. Delete the screen members
     (:64-66, :104-113 with their gate lines) and the now-unused forward declarations (:5-8, :13-20); move
     `RenderTextAreas` (:92) to the public section.
   - ImGuiManager.cpp: delete :7-16 and :20-22 and the `Profile/ProfileRows.h` include (:19), whose only users are
     removed; the `Game.h` include (:23) stays for the Phase 1 `game::kfUiReferenceHeight` reads; :171-181 becomes
     `CreateUiScreens();`; :491-518 becomes `RenderUiScreens();`;
     `DestroyUiScreens();` becomes the first statement of `~ImGuiManager` (:286-287).
   - New file Engine/Source/Ui/Screens/UiScreens.cpp, client-only, sandbox client project only: file-static
     `std::unique_ptr`s for the nine screens; `CreateUiScreens` constructs them in ImGuiManager.cpp:172-180 order, then
     runs the Graphics.cpp:466-472 load block (`kbDebugInput`, `game::gpGame != nullptr`, `LoadTweaksSettings()`);
     `RenderUiScreens` is ImGuiManager.cpp:494-517 with `gpImGuiManager->RenderTextAreas()`; `DestroyUiScreens` runs
     the Graphics.cpp:748-756 save block conditioned on `kbDebugInput` and `game::gpGame != nullptr`, then resets the
     nine screens; `TweaksScreen& GetTweaksScreen()` returns the Tweaks screen.
   - Recommended to drop the `gpImGuiManager != nullptr` save condition and its comment (Graphics.cpp:750-752): it
     guarded a second `Graphics::Destroy` pass after `mpImGuiManager.reset()`, and the destructor runs once per manager.
   - Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h declares `TweaksScreen& GetTweaksScreen();` after
     `RegisterEngineTweakSections` (:130); Engine/Source/Ui/TweaksSettings.cpp:38 and :58 use it in place of
     `gpImGuiManager->mpTweaksScreen`.
   - Graphics.cpp: delete :465-473 and :747-757 and the now-unused `Ui/TweaksSettings.h` include (:10).
   - Viewer: Tools/ModelViewer/Source/ModelViewerApplication.cpp:516-519 (`game::RenderViewerUi`) is replaced, beside the
     Phase 1 `engine::RenderFrameMain` definition, by `engine::RenderUiScreens` with the same body (:518) and empty
     `engine::CreateUiScreens` and `engine::DestroyUiScreens`, under one comment that the viewer panel keeps its state
     in `ModelViewerApplication`. Delete the `RenderViewerUi` declaration (Tools/ModelViewer/Source/Game.h:9).
   - Sandbox equivalence: the screens are created in the same order at the end of the manager constructor and the
     Tweaks load runs right after, as Graphics.cpp:463-472 did; render order and conditions are unchanged
     (MainMenu, Modal; the six in-game screens while Tweaks is hidden; Tweaks while shown; ImPlot graphs; text areas);
     the save runs at the start of the destructor, the point Graphics.cpp:747-758 reached, and `mpImGuiManager` is
     reset only at Graphics.cpp:758. Screens are destroyed before the ImGui context instead of after it; their
     destructors are implicit and touch no ImGui state (screen headers hold floats, flags, string views, and
     `std::unordered_set`s). Main.cpp:310 and :531 load and save while the manager, and so the screens, exist, as before.
7. Define removal: delete `BT_MODEL_VIEWER;BT_MODEL_VIEWER;` from Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66.
8. Project membership via `/update-vcxproj`: add FlightPlaneProjection.cpp (Graphics filter) and UiScreens.cpp
   (Ui\Screens filter) to the sandbox client project and filters. Server and ModelViewer source membership unchanged.
9. Documentation (`/update-claude-docs`; each would be wrong after the change):
   - Engine/Source/AGENTS.md:20-33 Aggregation: `GameBase.h` and the generic agent command headers stay out of
     `Engine.h`; the game PCH includes them right after it.
   - Engine/Source/File/AGENTS.md:3: `DifferenceStream.h` names game frame types, stays out of `Engine.h`, and its users
     include it.
   - Engine/Source/Frame/AGENTS.md:109: render code calls `GlobalElevation` only.
   - Engine/Source/Graphics/AGENTS.md:57-77: `GameBase::UpdateCamera` reads the main menu, pulls the target, passes both
     to `CameraBase::Update`, and sets controller vibration from the shake.
   - Engine/Source/Graphics/Managers/AGENTS.md:190 (as Phase 1 leaves the section): ImGuiManager calls the three
     host-defined UI functions; gameplay builds define them in `Ui/Screens/UiScreens.cpp`, which owns the screens and
     the Tweaks save and load across recreation.
   - Engine/Source/Ui/AGENTS.md:56-59: `ChatLog.h` reaches the game PCH through `GameBase.h`.
   - Engine/Source/Ui/Screens/AGENTS.md:3, 18-19, 34 and Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md:16, 29,
     33: `UiScreens.cpp` owns construction and invocation; ImGuiManager owns submission.
   - Tools/ModelViewer/AGENTS.md: the `## Application contracts` prohibition on instantiating the sandbox Game (:5-8) and the
     placeholder-dependency prohibition (:70-71), plus `## Engine integration`, rewritten to the result: no define; the
     viewer compiles only engine files whose game names it supplies (game name and version, HDR clear color, UI
     reference height, flight-plane table, profile rows, frame interpolate, camera `SunAngle`), defines
     `engine::RenderFrameMain` and the three UI functions
     by project membership (empty where it has nothing to do), sets feature switches in its `Pch.h`, and constructs no
     `GameBase` or network session.
   - Unchanged and still true: Projects/BrokenEngineSandbox/Source/Graphics/AGENTS.md:7-8, Engine/Source/Ui/AGENTS.md:44,
     Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md:3, Engine/Source/Graphics/GraphicsUtils.h:43-55.

## Critical files

- Engine/Source/Engine.h, Projects/BrokenEngineSandbox/Source/Pch.h, Engine/Source/File/Replay.h.
- Engine/Source/Graphics/CameraBase.h, CameraBase.cpp, Engine/Source/GameBase.h, GameBase.cpp, Engine/Source/Main.cpp.
- Engine/Source/Frame/IslandTerrain.h, IslandTerrain.cpp, Engine/Source/Graphics/GraphicsUtils.cpp,
  Engine/Source/Graphics/FlightPlaneProjection.cpp (new).
- Engine/Source/Graphics/Islands.h, Islands.cpp.
- Engine/Source/Graphics/Managers/ImGuiManager.h, ImGuiManager.cpp, Engine/Source/Ui/Screens/UiScreens.cpp (new),
  Engine/Source/Ui/TweaksSettings.cpp, Engine/Source/Graphics/Graphics.cpp, Tools/ModelViewer/Source/ModelViewerApplication.cpp.
- Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj; sandbox client vcxproj and filters.

## Risk triggers and invariants

- Tier 3: public engine signatures (`CameraBase::Update`, `Islands::UpdateActiveIslands`, `IslandTerrain`), the
  aggregation header every gameplay TU compiles, and independently owned subsystems (Graphics, Frame, UI, File, the
  sandbox, ModelViewer).
- CRC, determinism, and pack format unchanged: every moved or rewired function is client render, camera, or UI code
  that PostRender and the CRC never read (`GlobalElevation` asserts it is outside a frame tick); include moves change
  no code.
- The one sandbox-observable difference is the verbose camera diagnostic's one-frame throttle shift (Design item 3).
- The engine reaches game code only by name; no callback, function pointer, or virtual added. Engine files the viewer
  compiles name no gameplay symbol beyond the host contract it supplies.
- No viewer behavior change; the viewer compiles no additional engine file.

## In scope

- Engine/Source/Engine.h:11-13, 91-102.
- Projects/BrokenEngineSandbox/Source/Pch.h after :21 — moved include block.
- `#include "File/DifferenceStream.h"` added in Engine/Source/File/Replay.h (BT_SERVER block),
  Engine/Source/Agent/Commands/ClientDesyncProbe.cpp, Engine/Source/Network/Client/ReconcileReplayTick.cpp,
  Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp (include blocks only).
- Engine/Source/Graphics/CameraBase.h:168; CameraBase.cpp:9-12, 17, 67, 76, 122-208 (signature, :138, :175-178,
  :195-196, :200-201), 368, 380, 523.
- Engine/Source/GameBase.h near :284-285 — `UpdateCamera` declaration; GameBase.cpp:4-6 (include), 833-834 (comment),
  852-855 (call), new `GameBase::UpdateCamera`, 1129-1143 (`UpdateActiveIslands`).
- Engine/Source/Main.cpp:326.
- Engine/Source/Frame/IslandTerrain.h:139 region (`CellElevation` declaration), 146-152; IslandTerrain.cpp:7-9,
  232-256, 324-403, 515-516, 525-569.
- Engine/Source/Graphics/GraphicsUtils.cpp:123-136.
- Engine/Source/Graphics/FlightPlaneProjection.cpp (new).
- Engine/Source/Frame/FrameBase.cpp:239-240, 279-282 and Engine/Source/Frame/CellStaticData.h:49 (comments only).
- Engine/Source/Graphics/Islands.h:10-11, 50-52 region; Islands.cpp:192-399.
- Engine/Source/Graphics/Managers/ImGuiManager.h:5-20, 64-66, 92, 104-113, after 137; ImGuiManager.cpp:7-23, 171-181,
  286-287, 491-518.
- Engine/Source/Ui/Screens/UiScreens.cpp (new).
- Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h after :130; Engine/Source/Ui/TweaksSettings.cpp:38, 58.
- Engine/Source/Graphics/Graphics.cpp:10, 465-473, 747-757.
- Tools/ModelViewer/Source/ModelViewerApplication.cpp:516-519; Tools/ModelViewer/Source/Game.h:9.
- Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66.
- Membership: sandbox client vcxproj and filters add FlightPlaneProjection.cpp and UiScreens.cpp.
- Documentation lines in Design item 9.

## Out of scope

- Phase 1 and Phase 2 sites and changes.
- Compiling more engine files into the viewer, giving the viewer a `GameBase`, `game::gpGame`, cells, sessions, or a
  HUD, or defining `kbReplayFullFrames` in the viewer `Pch.h` (it comes from `EngineSwitches.h`; a second
  definition would redefine it).
- Changing screen behavior, screen lifetime across Graphics recreation, the Tweaks file format or version, camera
  behavior beyond the documented throttle shift, terrain sampling, island placement packing, or replay streams.
- Moving `GlobalNormal` anywhere (it is deleted), renaming `CellElevation`, or changing `CellNormal`/`CellElevationSampler`.
- ModelViewer ClInclude items other than what `/update-vcxproj` requires for the files this plan adds or removes.
- Shader lighting, pack format, extra viewer features, descriptor pool, lazy pool, and worktree-startup-build findings.
- New harness commands; ModelViewer harness support.
- Backward compatibility of any kind.

## Acceptance criteria

The execution card's Acceptance checks A1-A5 are the acceptance criteria.

## Execution card

Execution card:
### What does this plan do?
Final step of removing the ModelViewer-only compile flag. Engine code that names the game stops sitting behind the flag:
two headers leave the engine's aggregation header for the files or game precompiled header that need them; the camera
update takes the main-menu flag and the game's camera target as parameters from a small gameplay-host helper that also
sets controller vibration; terrain projection and island upload stop reading the game's cell map directly; and the UI
screens move from the ImGui manager into a gameplay-only engine file behind three functions the viewer defines for
itself. The flag is then deleted from the viewer project.
### Why this is good for the codebase
The user does not want the engine to know the viewer exists; after this no engine file mentions it, and every engine
file the viewer compiles names no gameplay symbol beyond what the viewer supplies. The engine's aggregation header stops
pulling game types into every program, and the dead `GlobalNormal` goes.
- Goal: `BT_MODEL_VIEWER` absent from every file outside Documents/Plans; viewer-compiled engine files changed here name no `game::` symbol other than the `Game.h` constants each program supplies (`game::kfUiReferenceHeight` in ImGuiManager.cpp); sandbox and viewer behavior unchanged except the documented verbose-log throttle shift; all six builds pass.
- Out of scope: earlier-phase sites, adding gameplay pieces or files to the viewer, behavior changes, other review findings, new harness commands (see `## Out of scope`).
- Tier trigger: Tier 3 — public engine signatures, the aggregation header, and independently owned subsystems (Graphics, Frame, UI, File, sandbox, ModelViewer).
- Interfaces and invariants: `CameraBase::Update(rFrameInterpolate, fDeltaTime, bMainMenuFrame, rPulledTarget)` with
  vibration set by new `GameBase::UpdateCamera(GridCoord)`; `IslandTerrain::CellElevation` public, `GlobalElevation`
  file-static in new client-only `Graphics/FlightPlaneProjection.cpp` with `ProjectTo*`, `GlobalNormal` deleted;
  `Islands::UpdateActiveIslands(std::span<const CellStaticData* const>)`; engine-declared
  `CreateUiScreens`/`RenderUiScreens`/`DestroyUiScreens` defined in new client-only `Ui/Screens/UiScreens.cpp` (with
  `GetTweaksScreen`) and by the viewer; `DifferenceStream.h`, `GameBase.h`, and the generic agent command headers out of
  `Engine.h`. Invariants: no PostRender/CRC change; engine reaches game code only by name; no callback to game code.
- Acceptance checks:
  - A1 `git grep -n BT_MODEL_VIEWER -- . ':!Documents/Plans'` and `git grep -n -E
    "RenderViewerUi|RenderViewerFrame|kbModelViewer" -- . ':!Documents/Plans'` print nothing; `git grep -n -i -E
    "modelviewer|model viewer" -- Engine Common Projects` prints nothing; `git grep -n -E "game::|namespace game" --
    Engine/Source/Engine.h Engine/Source/Graphics/CameraBase.cpp Engine/Source/Frame/IslandTerrain.cpp
    Engine/Source/Graphics/Islands.cpp Engine/Source/Graphics/GraphicsUtils.cpp
    Engine/Source/Graphics/Managers/ImGuiManager.h Engine/Source/Graphics/Managers/ImGuiManager.cpp` prints only the
    two ImGuiManager.cpp `game::kfUiReferenceHeight` reads; a read at the session baseline commit shows each of these
    present.
  - A2 `/compile` ModelViewer Debug|x64 and Release|x64 and BrokenEngineSandbox client and server Debug|x64 and Release|x64 all succeed with warnings as errors (Release includes PREfast).
  - A3 Code read of the diff against the session baseline commit, per Design item: moved bodies equal their baseline
    text apart from the stated substitutions; `GameBase::UpdateCamera` passes the same flag and target and computes
    vibration from the decayed shake; the `activeCells` entries point at the static data of the baseline callee's kept
    cells in order; screen creation, render, Tweaks load, and Tweaks save order and conditions match Design item 6;
    viewer `RenderUiScreens` body equals baseline `RenderViewerUi`; `/update-vcxproj` reports client-only membership for
    both new files.
  - A4 Sandbox Debug client+server per `/agent-harness`, once before the first edit and once after: in the main menu,
    `describe_ui` items and rectangles are identical between runs, and `describe_scene` `camera` (eye, visibleArea,
    lod), sampled once two consecutive samples agree, is identical between runs; `key` `F3`, `click` one Tweaks section
    label taken from the baseline `describe_ui`, `resize` to a different extent, then `describe_ui` shows
    `tweaksVisible` true and that section's window still listed in both runs; `key` `F3` again; after connect and spawn,
    `describe_scene` lists islands and `camera.basisCoord` equals `clientGridCoord`, and `query_profile` rows
    `    Terrain` and `    VisibleLights` are nonzero wherever the baseline run had them nonzero; no desync or
    CRC-mismatch log lines in either run.
  - A5 ModelViewer runtime: after implementation the user launches the viewer and confirms models, background, the viewer panel, panel behavior after resizing the window, and sun drag look as before.
- Roles: implementer `/implement-plan` (single slice); `/update-affected-code`; `/update-vcxproj`; `/compile`; `/agent-harness` for A4; reviewers `/repo-code-review`, `/comment-review`, `/code-style-review`, `/over-engineering-review`, `/adversarial-review` (Tier 3); `/update-claude-docs`, then `/coherence-review` and `/progressive-disclosure-review` for the AGENTS.md edits; `/verify-acceptance`; user for A5; plan review `/plan-audit`, `/plan-simplicity-review`, `/external-grill-plan`.
