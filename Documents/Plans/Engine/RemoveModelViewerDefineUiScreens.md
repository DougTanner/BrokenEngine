<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-25T18:12:34.181Z","dependsOn":[]} -->
# Remove BT_MODEL_VIEWER, Phase 3 Stage 2: UI Screens and the Define

## Context

The engine must not know that the ModelViewer exists. `BT_CLIENT` and `BT_SERVER` stay, because both executables are
the engine; `BT_MODEL_VIEWER` names one application and goes. Phases 1 and 2 landed earlier. Phase 3 was split into two
stages, one Plan each (the user chose the split); stage 1 has landed and this Plan is stage 2, the last one. Afterwards no
file outside Documents/Plans mentions `BT_MODEL_VIEWER`.

State this Plan starts from (stage 1, landed):

- `File/DifferenceStream.h`, `GameBase.h`, and the generic agent command headers are out of `Engine.h`; the sandbox
  `Pch.h` includes the last two right after `Engine.h`.
- The camera update takes the main-menu flag and the pulled target as parameters from `GameBase::UpdateCamera`, which
  also sets controller vibration.
- Terrain elevation for render projection lives in the client-only, sandbox-client-only
  Engine/Source/Graphics/FlightPlaneProjection.cpp, a precedent for an engine file only gameplay programs compile;
  `Islands::UpdateActiveIslands` takes `std::span<const CellStaticData* const>`.
- The define remains only at this Plan's sites: Engine/Source/Graphics/Graphics.cpp,
  Engine/Source/Graphics/Managers/ImGuiManager.h, Engine/Source/Graphics/Managers/ImGuiManager.cpp,
  Tools/ModelViewer/AGENTS.md (the `## Engine integration` sentence, which after stage 1 says the define excludes only
  sandbox screen ownership), and Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj.

What is still gated, and why a `kb*` switch cannot replace it: ImGuiManager owns the nine UI screens as members
(ImGuiManager.h:64-66 `mpTweaksScreen`, :104-113 the other eight, including `std::unique_ptr<game::HudScreen>`),
constructs them at the end of its constructor (ImGuiManager.cpp:170-180), and renders them between `ImGui::NewFrame` and
`ImGui::Render` (ImGuiManager.cpp:481-508, reading `game::gpGame->mbShowImGui`); in the viewer the same region calls
`game::RenderViewerUi()` (:482), defined at Tools/ModelViewer/Source/ModelViewerApplication.cpp:667-670 and declared
at Tools/ModelViewer/Source/Game.h:11. Graphics.cpp loads Tweaks settings right after constructing the manager
(:465-473) and saves them right before resetting it (:747-757), both through `game::gpGame`. The viewer declares
neither `game::gpGame` nor `game::HudScreen`, and an `if constexpr` discarded statement still needs every name
declared.

Approach (user choice this session, over three alternatives): the UI screens move from ImGuiManager into a new
gameplay-only engine file behind three engine-declared host functions that the viewer defines for itself, as it
already defines `engine::RenderFrameMain`; the Tweaks load and save move with the screens. The Tweaks save and load
round-trip across manager recreation stays, and no UI function pointer is added.

Line numbers cite commit d50b7fb5a21bbc9371c675a3a95101eb23c2eba0. Stage 1 edits none of this Plan's C++ files, so
their lines hold unless other work lands first; it does edit Tools/ModelViewer/AGENTS.md `## Engine integration` and
Engine/Source/AGENTS.md, so locate those docs' sentences by content. Locate every region by the symbol named when
lines have shifted.

## Design

All changed code is client UI code outside PostRender and the CRC, or project and documentation text.

1. Host-function declarations in Engine/Source/Graphics/Managers/ImGuiManager.h, after `gpImGuiManager` (:137):
   `void CreateUiScreens();`, `void RenderUiScreens();`, `void DestroyUiScreens();`, under one comment saying the host
   program defines them and the manager calls them at construction, between `ImGui::NewFrame` and `ImGui::Render`, and
   at destruction.
   - Delete the screen members with their gate lines (:64-66, :104-113) and the forward declarations only they used:
     the `namespace game` block (:5-8) and the eight screen classes (:13-20).
   - Move `RenderTextAreas` (:92) from the private section to the public section, because the screens file calls it.
2. Engine/Source/Graphics/Managers/ImGuiManager.cpp:
   - Delete the gated screen includes (:7-16), the `Profile/ProfileRows.h` include and the gated `HudScreen.h` include
     (:19-22): nothing left in the file uses them (`kGpuTimerUiRender` comes from Engine/Source/Profile/ProfileManager.h).
     Keep `Ui/GraphicsSettingsWrappersBase.h` (:17) and `Game.h` (:23), which the two `game::kfUiReferenceHeight` reads
     (:157, :443) need.
   - The constructor's screen block (:170-180) becomes `CreateUiScreens();`.
   - The render block (:481-508) becomes `RenderUiScreens();`.
   - `DestroyUiScreens();` becomes the first statement of `~ImGuiManager` (:285-287).
3. New file Engine/Source/Ui/Screens/UiScreens.cpp, whole-file `BT_CLIENT`, in the sandbox client project only. It
   takes the includes the moved code needs (the screen headers from ImGuiManager.cpp:8-15 and :21,
   `Ui/TweaksSettings.h`, `Game.h`) and holds:
   - file-static `std::unique_ptr`s for the nine screens;
   - `CreateUiScreens`: constructs them in ImGuiManager.cpp:171-179 order (Tweaks, MainMenu, Modal, PauseMenu,
     GraphicsMenu, SoundMenu, GameSettings, Hud, Chat), then runs the Graphics.cpp:466-472 load block unchanged
     (`if constexpr (kbDebugInput)`, `game::gpGame != nullptr`, `LoadTweaksSettings()`);
   - `RenderUiScreens`: the body of ImGuiManager.cpp:484-507, comments included, with the screen members replaced by
     the file statics and `RenderTextAreas()` by `gpImGuiManager->RenderTextAreas()`; `RenderImPlotGraphs()` stays in
     place;
   - `DestroyUiScreens`: runs the Graphics.cpp:748-756 save block under `if constexpr (kbDebugInput)` and
     `game::gpGame != nullptr`, then resets the nine screens;
   - `TweaksScreen& GetTweaksScreen()`, returning the Tweaks screen.
   - Recommended: drop the save block's `gpImGuiManager != nullptr` condition and its comment (Graphics.cpp:750-752).
     It guarded a second `Graphics::Destroy` pass after `mpImGuiManager.reset()` had nulled the global; the manager is
     destroyed only by Graphics.cpp:758 inside `Destroy` (which `~Graphics`, :112-116, also reaches), so the destructor
     runs once per manager, and `gpImGuiManager` is still set inside the destructor body.
4. Tweaks screen access: Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h declares `TweaksScreen& GetTweaksScreen();`
   after `RegisterEngineTweakSections` (:130); Engine/Source/Ui/TweaksSettings.cpp:38 and :58 call
   `GetTweaksScreen()` in place of `gpImGuiManager->mpTweaksScreen`. These two lines and ImGuiManager.cpp are the only
   users of the screen members (git grep over Engine, Projects, Tools, Common, .agents). The viewer does not compile
   TweaksSettings.cpp, so it needs no `GetTweaksScreen` definition.
5. Engine/Source/Graphics/Graphics.cpp: delete the load block (:465-473), the save block (:747-757), and the
   `Ui/TweaksSettings.h` include (:10), whose only users are those blocks. The `RenderMainPresentAcquire` signature
   (:206) keeps its viewer-supplied `game::FrameInterpolate`.
6. Viewer: in Tools/ModelViewer/Source/ModelViewerApplication.cpp, delete `game::RenderViewerUi` (:667-670) and, in the
   `namespace engine` block beside `engine::RenderFrameMain` (:674-695), define `RenderUiScreens` with the same body
   (`game::gpModelViewerApplication->RenderUi();`, qualified as `RenderFrameMain` already qualifies it) and empty
   `CreateUiScreens` and `DestroyUiScreens`, under one comment that the viewer panel keeps its state in
   `ModelViewerApplication`. Delete the `RenderViewerUi` declaration (Tools/ModelViewer/Source/Game.h:11) and the blank
   line before it (:10).
7. Define removal: delete both `BT_MODEL_VIEWER;` tokens from the `PreprocessorDefinitions` line of
   Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66 (the only definition site; one
   ItemDefinitionGroup covers every configuration).
8. Membership via `/update-vcxproj`: add UiScreens.cpp to the sandbox client project and filters under filter
   `Engine\Ui\Screens`. Server and ModelViewer membership unchanged.
9. Documentation (`/update-claude-docs`; each would be false after the change):
   - Engine/Source/Graphics/Managers/AGENTS.md `### ImGuiManager` first bullet (:191): ImGuiManager calls the three
     host-defined UI functions; gameplay builds define them in `Ui/Screens/UiScreens.cpp`, which owns the screens and
     the Tweaks load and save across manager recreation.
   - Engine/Source/Ui/Screens/AGENTS.md :3, :18-19, :34, :58 (the `## See Also` route for screen invocation) and
     Projects/BrokenEngineSandbox/Source/Ui/Screens/AGENTS.md :16, :29, :33: `UiScreens.cpp` owns screen construction and
     invocation; ImGuiManager owns submission.
   - Tools/ModelViewer/AGENTS.md: the `## Application contracts` prohibition on instantiating the sandbox Game or
     starting a network session (:6-7) and `## Engine integration` (:84-106 at the commit above: the define sentence,
     `RenderViewerUi` at :92, the placeholder prohibition at :105-106) rewritten to the result: no define; the viewer
     compiles only engine files whose game names it supplies (game name and version, HDR clear color, UI reference
     height, flight-plane table, profile rows, frame interpolate, camera `SunAngle`), defines `engine::RenderFrameMain`
     and the three UI functions by project membership (empty where it has nothing to do), sets feature switches in its
     `Pch.h`, and constructs no `GameBase` or network session. The rewrite does not spell the define name (A1).
   - Unchanged and still true: Engine/Source/AGENTS.md:37 and Engine/Source/Profile/AGENTS.md:28 (the user chose to
     leave both, scoping A1's viewer-word grep to non-Markdown files), Engine/Source/Ui/Screens/TweaksScreen/AGENTS.md:3,
     Documents/UserInterfaceDesign.txt:22.
10. Citation: Documents/Plans/Engine/ModelViewerWorldUniforms.md:22 states the after-Phase-3 state as pending and names
    this Plan's file. On completion it becomes a present-tense fact citing the viewer's `engine::RenderFrameMain` in
    Tools/ModelViewer/Source/ModelViewerApplication.cpp, with the Plan file name dropped; that clause only.

Sandbox equivalence: the screens are created in the same order at the end of the manager constructor and the Tweaks
load runs right after, as Graphics.cpp:463-472 did; `LoadTweaksSettings` (TweaksSettings.cpp:43-60) reads only
`game::gpGame`, the Tweaks screen, and `gSunAngleOverride`, and `Graphics::mpImGuiManager` is read only at
Graphics.cpp:461, :463, :758, so running the load inside the constructor changes nothing. Render order and conditions
are unchanged (MainMenu, Modal; the six in-game screens while Tweaks is hidden; Tweaks while shown; ImPlot graphs; text
areas). The save runs at the start of the destructor, the point Graphics.cpp:747-758 reached. Screens are now
destroyed before the ImGui context instead of after it; their destructors are implicit and touch no ImGui state.
Engine/Source/Main.cpp:325 (`LoadTweaksSettings`) and :525 (`SaveTweaksSettings`) run while the manager, and so the
screens, exist, as before. Viewer equivalence: `RenderUiScreens` runs the same panel call at the same point.

## Critical files

- Engine/Source/Graphics/Managers/ImGuiManager.h, ImGuiManager.cpp.
- Engine/Source/Ui/Screens/UiScreens.cpp (new), Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h,
  Engine/Source/Ui/TweaksSettings.cpp, Engine/Source/Graphics/Graphics.cpp.
- Tools/ModelViewer/Source/ModelViewerApplication.cpp, Tools/ModelViewer/Source/Game.h,
  Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj.
- Sandbox client vcxproj and filters.

## Risk triggers and invariants

- Change Workflow Tier 3, trigger: a change spanning independently owned subsystems (Graphics managers, UI screens,
  ModelViewer, sandbox project) that also changes the engine's public interface (ImGuiManager public members removed,
  three engine-declared functions every client program must define).
- CRC, determinism, pack format, Tweaks file format and `kiVersion` unchanged: every moved function is client UI code
  that PostRender and the CRC never read.
- The engine reaches game code only by name; no callback, function pointer, or virtual is added. Engine files the
  viewer compiles and this Plan changes (ImGuiManager.h, ImGuiManager.cpp, Graphics.cpp) name no gameplay symbol
  beyond the names the viewer supplies.
- Screen behavior and screen lifetime across Graphics recreation unchanged; the viewer compiles no additional engine
  file; no viewer behavior change.

## In scope

- Engine/Source/Graphics/Managers/ImGuiManager.h:5-8, 13-20, 64-66, 92, 104-113, after 137.
- Engine/Source/Graphics/Managers/ImGuiManager.cpp:7-16, 19-22, 170-180, 285-287 (`~ImGuiManager` first statement),
  481-508.
- Engine/Source/Ui/Screens/UiScreens.cpp (new).
- Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h after :130; Engine/Source/Ui/TweaksSettings.cpp:38, 58.
- Engine/Source/Graphics/Graphics.cpp:10, 465-473, 747-757.
- Tools/ModelViewer/Source/ModelViewerApplication.cpp:667-670 and the `namespace engine` block (:674-695);
  Tools/ModelViewer/Source/Game.h:10-11.
- Tools/ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66 (the two define tokens only).
- Membership: sandbox client vcxproj and filters add UiScreens.cpp.
- Documentation lines in Design item 9.
- Documents/Plans/Engine/ModelViewerWorldUniforms.md:22 (that clause only; Design item 10).

## Out of scope

- Phase 1, Phase 2, and stage 1 sites and changes, including Engine/Source/AGENTS.md:37 and
  Engine/Source/Profile/AGENTS.md:28.
- Compiling more engine files into the viewer, giving the viewer a `GameBase`, `game::gpGame`, cells, sessions, a HUD,
  or a Tweaks screen.
- Changing screen behavior, screen lifetime across Graphics recreation, the Tweaks file format or version, or the
  Tweaks save and load round-trip itself; adding a UI function pointer or callback.
- ModelViewer ClInclude items other than what `/update-vcxproj` requires for the file this Plan adds.
- Other Plans' prose beyond Design item 10, including the "Phase 3" mentions in
  Documents/Plans/Engine/ModelViewerModelOwnSpace.md and Documents/Plans/Engine/ModelViewerModelOverWorld.md.
- New harness commands; ModelViewer harness support.
- Backward compatibility of any kind.

## Acceptance criteria

- A1 `git grep -n BT_MODEL_VIEWER -- . ':!Documents/Plans'` and
  `git grep -n -E "RenderViewerUi|RenderViewerFrame|kbModelViewer" -- . ':!Documents/Plans'` print nothing;
  `git grep -n -i -E "modelviewer|model viewer" -- Engine Common Projects ':!*.md'` prints nothing;
  `git grep -n -E "game::|namespace game" -- Engine/Source/Graphics/Managers/ImGuiManager.h
  Engine/Source/Graphics/Managers/ImGuiManager.cpp Engine/Source/Graphics/Graphics.cpp` prints only the two
  ImGuiManager.cpp `game::kfUiReferenceHeight` reads and the Graphics.cpp `RenderMainPresentAcquire` signature's
  `game::FrameInterpolate`. A read at the session baseline commit shows the define at the five sites listed in
  `## Context`, `RenderViewerUi`, and `game::` matches in ImGuiManager.h, ImGuiManager.cpp, and Graphics.cpp (three
  there).
- A2 `/compile` ModelViewer Debug|x64 and Release|x64 and BrokenEngineSandbox client and server Debug|x64 and
  Release|x64 all succeed with warnings as errors (Release includes PREfast).
- A3 Code read of the diff against the session baseline commit: moved bodies equal their baseline text apart from the
  substitutions Design items 2-6 state; screen creation, render, Tweaks load, and Tweaks save order and conditions match
  Design item 3; viewer `RenderUiScreens` body equals baseline `RenderViewerUi`; `/update-vcxproj` reports
  client-only membership for UiScreens.cpp.
- A4 Sandbox Debug client+server per `/agent-harness`, once before the first edit and once after, with the same
  steps: in the main menu, `describe_ui` items and rectangles are identical between runs; `window_state`
  `{"minimized":false}` (agent clients start minimized and `click`/`resize` reject a minimized client), then `key`
  `F3`, `click` one Tweaks section label taken from the baseline `describe_ui`, `resize` to a different extent, then
  `describe_ui` shows `tweaksVisible` true and that section's window still listed in both runs; `key` `F3` again; no
  desync or CRC-mismatch log lines in either run. The supplied evidence records no observing run of these steps; they
  are the setup to run.
- A5 ModelViewer runtime: an `/agent-harness` ModelViewer capture of the default scene with `-RequireStable`, once
  before the first edit and once after, produces identical stable captures; after implementation the user launches the
  viewer and confirms the viewer panel, and the panel's behavior after resizing the window, look as before.

## Notes

- `dependsOn` is empty: this Plan is committed in the same landing as stage 1, so it is not present at the primary tip,
  and cannot be claimed, before stage 1 has landed.
- Documents/Plans/Engine/ModelViewerWorldUniforms.md depends on this Plan; that chain's stage 3 edits
  Tools/ModelViewer/AGENTS.md's list of game names the viewer supplies, which Design item 9 writes, and
  ModelViewerApplication.cpp.
- Live verification: A4 needs a sandbox Debug client+server build at the baseline commit before the first edit, and A5
  a ModelViewer Debug build at the baseline commit; both baseline runs happen before any edit.
- Review route: Tier 3 Change Workflow reviews, including `/adversarial-review`; `/update-claude-docs` then
  `/coherence-review` and `/progressive-disclosure-review` for the AGENTS.md edits.
