<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T13:20:12.998Z","dependsOn":[]} -->
# Remove BT_MODEL_VIEWER, Phase 1: Feature Switches

## Context

The engine must not know that the ModelViewer exists. `BT_CLIENT` and `BT_SERVER` stay, because both executables are
the engine; `BT_MODEL_VIEWER` names one application and goes. The removal is phased:

- Phase 1 (this plan): delete `kbModelViewer`. Every site where the viewer genuinely renders or behaves differently from
  the game becomes a purpose-named `inline constexpr bool kb*` switch, defined in `Engine/Source/EngineSwitches.h` with
  the game's value and in the viewer's own `Pch.h` with the viewer's value, or becomes a viewer-supplied definition
  selected by project membership. Gates that need no replacement are deleted. Each switch names a feature, never the
  application.
- Phase 2 (`Documents/Plans/Engine/RemoveModelViewerDefineEngineServices.md`): removes the gates around four engine
  services the viewer neither uses nor can link (client crash upload, audio streaming harness rig hooks, profile overlay
  screens, engine-collection graphics resources), through a gameplay-only engine file, two more feature switches, and
  the game frame type creating engine-collection resources.
- Phase 3 (`Documents/Plans/Engine/RemoveModelViewerDefineGameCoupling.md`): removes the gates around engine code that
  names the game, case by case by passing game values in, moving game-naming code into gameplay-only engine files, or
  viewer-supplied definitions, and deletes the define.

Sandbox client/server simulation, CRC/determinism, and pack format are unchanged; ModelViewer keeps its current
behavior. The `BT_MODEL_VIEWER` define itself survives until Phase 3.

History: every `BT_MODEL_VIEWER` gate and `kbModelViewer` were introduced in 601012a8d and 7d64500ec, both
"Opus/Astra plans" (no recorded reason). `kbFramebufferClearColor = kbVulkanWireframe` predates them (d8ce779b3).

Line numbers throughout cite commit 7d64500ecb21b6d05acc859ab5f417067a8f0796. When later commits have shifted lines,
locate each region by the content the citation describes.

## Design

### Switches

Recorded decision (user choice during planning): there is no `ProjectSwitches.h`. `ModelViewer/Source/Pch.h` stops
including `EngineSwitches.h` and defines every switch the engine sources it compiles read. Only the two `Pch.h` files
include `EngineSwitches.h` (Projects/BrokenEngineSandbox/Source/Pch.h:14, ModelViewer/Source/Pch.h:5;
`git grep -n EngineSwitches` finds no other include). A project `Pch.h` defining its own switches is the existing rule-6
form (Documents/C++StyleGuide.txt:74) and the existing DataPacker precedent (DataPacker/Source/Pch.h:6-21), so no
style-guide or code-style-review edit is needed.

New feature switches (config-independent):

| Switch | EngineSwitches.h (game) | Viewer Pch.h | Controls | Real difference (evidence) |
| --- | --- | --- | --- | --- |
| `kbCoverageAlpha` | `false` | `true` | model pipelines set `PipelineFlags::kCoverageAlpha` on blended materials; push constant x for model draws and HDR resolve | Model.frag forces opaque alpha and Engine/Data/Shaders/HdrResolve.frag:46-52,73 unpremultiplies and composites a gray background only when x != 0 |
| `kbFramebufferClearColor` | `kbVulkanWireframe` (unchanged) | `true` | HDR color attachment load op (SwapchainManager.cpp, unchanged) | without world draws nothing covers every pixel, and coverage reads cleared alpha 0 |
| `kbWorldRendering` | `true` | `false` | Main-pass terrain, water, hex shield, particle, light, billboard and debug draws | viewer draws models only (ModelViewer/AGENTS.md:66-68); all gated names are engine-declared and the viewer compiles their owners |
| `kbScaleUiToFramebufferHeight` | `true` | `false` | ImGui scale = framebuffer height / `kfUiReferenceHeight` | viewer keeps `UiScale()` at 1.0 by design (Engine/Source/Graphics/Managers/AGENTS.md:195-196) |

There is no `kbSunRotation` switch (recorded decision; Site decisions item 3).

Engine/Source/EngineSwitches.h: delete :17-23 (the `#if defined(BT_MODEL_VIEWER)` block holding `kbModelViewer` and
both `kbFramebufferClearColor` definitions); directly after `kbVulkanWireframe` (:16) add, in this order,
`kbCoverageAlpha = false`, `kbFramebufferClearColor = kbVulkanWireframe`, `kbScaleUiToFramebufferHeight = true`,
`kbWorldRendering = true`. No other EngineSwitches.h value changes.

ModelViewer/Source/Pch.h: replace `#include "EngineSwitches.h"` (:5, between `#include "Log/LogTypes.h"`
(Common/Log/LogTypes.h) and `Common.h`, the position Common/Log/Log.h:191 requires for `keLogLevel*`) with a definition
block. It defines exactly the switches that a viewer-compiled source, or a header the viewer's PCH reaches, reads
(membership grep of every ModelViewer.vcxproj item against each EngineSwitches.h name;
Engine/Source/File/DifferenceStream.h, the only reader of `kbReplayFullFrames`, is excluded by
Engine/Source/Engine.h:11-13). Each takes the value EngineSwitches.h gives it at the baseline commit for the same
configuration unless the table above says otherwise:

- Config-independent: `kbLogging`, `kbRenderThread`, `kbAlsoLogToPrintf`, `kbGraphicsProcessingUnitAssistedValidation`,
  `kbRenderDoc`, `kbScreenshots`, `kbAgent`, `kbVulkanPipelineCache`, `kbVulkanWireframe`, then the four feature
  switches above with viewer values.
- Per configuration, `#if defined(BT_DEBUG)` / `#elif defined(BT_RELEASE)` / `#endif` only (the viewer builds Debug|x64
  and Release|x64 only, ModelViewer.vcxproj:4-5): `kbMimallocDiagnostics`, `kbDebugBreak`, `kbDebugInput`,
  `kbDebugRender`, `kbFreeCamera`, `kbProfiling`, `kbProfilingDump`, `kbVulkanDebugLayers`,
  `kbRandomlyInvalidatePhysicallyBasedRenderingCubemapCache`, `kbShowProfileTextByDefault`.
- Log floors: `keLogLevelTemporary`, `keLogLevelReplay`, `keLogLevelDefault`, `keLogLevelAudio`, `keLogLevelGraphics`,
  `keLogLevelLoading`, `keLogLevelNavigationData`, `keLogLevelNetwork`, `keLogLevelInput`.
- Not defined (no viewer-compiled reader): `kbDesynchronizationRecovery`, `kbDesynchronizationDebugFrames`,
  `kbFrameDispatch`, `kbRecording`, `kpcBuildConfigurationName`, `kbDxDiag`, `kbDebugNavigationCrossingCheck`,
  `kbProfilingFrameSpike`, `kbReplayFullFrames`, `kbAutoConnect`.

The A2 ModelViewer Debug and Release builds are the completeness check: a missing name fails to compile.

### Site decisions

Each site's sandbox text after the change equals its baseline sandbox (non-viewer) text; each viewer text equals its
baseline `BT_MODEL_VIEWER` text, except where stated.

1. Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:368,407,454 and
   Engine/Source/Graphics/Objects/ModelPipeline.cpp:119 — `kbModelViewer` becomes `kbCoverageAlpha`.
2. CommandBufferRecordMain.cpp:372-399 and 412-442 — `#if !defined(BT_MODEL_VIEWER)` ... `#endif` becomes
   `if constexpr (kbWorldRendering) { ... }` with the body unchanged. The viewer compiles CommandBufferRecordMain.cpp
   and every name in the bodies is engine-declared; an entity named only in a discarded statement needs no definition.
3. Sun rotation. Recorded decision (user choice during planning): the sun rotation is always applied, with no switch,
   and the existing northward offset goes through the same rotation. The existing offset is the `Normal Tilt` tweak
   `gSunMoonNormalTilt`, default -0.45 rad (Engine/Source/Ui/SunMoonWrappersBase.cpp:37), applied as a separate
   `XMMatrixRotationX` at Engine/Source/Graphics/Render/GlobalUniforms.cpp:53-58; it tilts the noon sun toward +y
   (north).
   - `glm::quat mSunRotation {1.0f, 0.0f, 0.0f, 0.0f};` moves verbatim from ModelViewer/Source/Graphics/Camera.h:19 into
     the public section of `engine::CameraBase` beside `mfSunAngle` (Engine/Source/Graphics/CameraBase.h:177), with a
     one-line comment that it is a world-space sun rotation, identity unless the host writes it. The sandbox never
     writes it (identity, meaning no rotation); viewer `RotateSun` (ModelViewer/Source/Graphics/Camera.cpp:61) keeps
     writing the now-inherited member. glm is in every client PCH (Common/ExternalHeaders.h:265-267); CameraBase.h is
     client-only.
   - GlobalUniforms.cpp:53-59 (tilt matrix, both gate branches) is replaced by one path for both projects: build
     `glm::quat sunRotation = gpCamera->mSunRotation * glm::angleAxis(gSunMoonNormalTilt.mfCurrent, glm::vec3(1.0f, 0.0f, 0.0f));`,
     rotate the day-cycle direction's xyz by it (the baseline viewer `ApplySunRotation` body,
     ModelViewer/Source/Graphics/Camera.cpp:66-68), and store `XMVector4Normalize` of the result with w 0. The
     :51-52 comment is rewritten to state that the Normal Tilt is applied first, then the camera's sun rotation.
   - Delete viewer `ApplySunRotation` (Camera.h:17, Camera.cpp:64-69); it has no other caller. Recommended: inline its
     body at the single caller rather than moving it as a `CameraBase` member function, because no member function then
     moves, CameraBase.cpp stays untouched, and rule 67 is not engaged.
   - Recommended: keep the `Normal Tilt` tweak live and compose it every frame rather than baking it into the initial
     rotation, because baking would delete a user-facing Tweaks row
     (Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSunMoon.cpp:34) that this change has no reason to remove.
   - Equivalence: `glm::angleAxis(t, +X)` applied with glm's `quat * vec3`
     (ThirdParty/glm/glm/ext/quaternion_trigonometric.inl:30-36, ThirdParty/glm/glm/detail/type_quat.inl:332-339)
     gives y' = y cos t - z sin t, z' = y sin t + z cos t, the same rotation as the row-vector
     `XMVector4Transform(v, XMMatrixRotationX(t))` (DirectXMath: m[1][1]=cos, m[1][2]=sin, m[2][1]=-sin, m[2][2]=cos);
     glm composes `R * T` as T first, matching the baseline viewer order (tilt, then rotation). The sandbox therefore
     gets the same direction; the viewer gets its baseline direction.
   - Bits: f4SunMoonNormal and its derived f4TerrainSnowSunNormal, f4WaterBiasedSunNormal and
     f2ShadowStretchTranslation may differ from baseline in low-order bits (glm `sin`/`cos` and quaternion arithmetic
     instead of `XMScalarSinCos` and a matrix transform). All are client GPU uniforms written by `RenderFrameGlobal`;
     `gf2ShadowHeightOffsetPerMeter` derives from the raw sun angle (GlobalUniforms.cpp:296-303) and is unchanged.
     Nothing PostRender or CRC reads any of them. Verified by A3 (equivalence) and A4 (no CRC lines); A5 covers the
     viewer's sun drag.
4. GlobalUniforms.cpp:526-530 — keep only the sandbox line `static_cast<game::Camera*>(engine::gpCamera)->SunAngle()`.
   The viewer's `game::Camera` (resolved by GlobalUniforms.cpp:16 `#include "Graphics/Camera.h"` from the viewer's
   own `Source`) declares `float SunAngle() const;` and defines it in ModelViewer/Source/Graphics/Camera.cpp as
   `return mfSunAngle;` — the baseline viewer value. `SunAngle` is the documented `game::Camera` policy member
   (Engine/Source/Graphics/AGENTS.md:62-64).
5. Engine/Source/Graphics/Render/MainUniforms.cpp:16-21 and :625 — delete the viewer `RenderFrameMain`, the `#else`,
   and the closing `#endif`. Remove MainUniforms.cpp from ModelViewer.vcxproj:198 and ModelViewer.vcxproj.filters:126.
   ModelViewer defines `engine::RenderFrameMain` (declared Engine/Source/Graphics/Render/Render.h:157) in
   ModelViewer/Source/ModelViewerApplication.cpp with the exact signature, `[[maybe_unused]]` on `rActiveCoordinates`
   and `currentTime` as at baseline, and the body of `game::RenderViewerFrame` (ModelViewerApplication.cpp:521-537)
   applied to `rRenderInterpolates.at(cameraCoordinate)`. Delete `RenderViewerFrame` and its declaration plus the
   `struct FrameInterpolate;` forward declaration (ModelViewer/Source/Game.h:10-11).
6. Engine/Source/Graphics/Managers/ImGuiManager.cpp:152-159 and 438-451 — `if constexpr (kbScaleUiToFramebufferHeight)`
   around the unchanged bodies.
7. ImGuiManager.cpp:106-111 — delete. ModelViewer sets the flag itself at the top of
   `ModelViewerApplication::RenderUi()` (ModelViewerApplication.cpp:257, called every frame inside the ImGui frame):
   `if (engine::gLaunchOptions.iAgentPort != 0) { ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; }`.
   Recommended per frame, because Graphics recreates ImGuiManager and its ImGui context internally. This leaves one
   delta, recommended as acceptable: each ImGuiManager construction's bootstrap `ImGui_ImplWin32_NewFrame` issues one
   `SetCursor(IDC_ARROW)` before the flag is set; it equals the viewer window-class cursor, and an agent-port viewer
   window is never shown.
8. ImGuiManager.cpp:473-480 — delete the gate; the condition becomes
   `gpAgentInput != nullptr && !gpAgentInput->mbImGuiMousePositionPinned`; rewrite the comment at :474-475 to the
   present truth (park when agent input exists and holds no pin; a host without agent input keeps its own pointer
   events). Sandbox equivalence: `PhysicalInputSuppressed()` is `kbAgent && iAgentPort != 0`
   (Engine/Source/LaunchOptions.h:35-38); under exactly that condition Engine/Source/Main.cpp:168-192 creates
   `AgentInput`, declared at :166 before `pGraphics` at :301 and :455 in the same scope, so `gpAgentInput` is non-null
   whenever suppression is on and ImGuiManager renders. Viewer equivalence: the viewer never constructs `AgentInput`,
   so `gpAgentInput` is null and the cursor is not parked, as at baseline.
9. Engine/Source/Graphics/Islands.h:50,52 — delete the gate lines; `Cell` and `GridCoord` are forward-declared, so the
   declaration compiles in the viewer. The definition stays gated in Islands.cpp (Phase 3); the viewer never calls it.
10. Engine/Source/Profile/ProfileManager.cpp:664-668 and 673-677 — keep only the sandbox `game::kpGameCpu*Names`
    lines. Recorded decision (user choice during planning): the viewer name tables match the engine's C-array style,
    with a placeholder entry for the empty table. ModelViewer/Source/Profile/ProfileRows.h:18-21 is replaced by the
    sandbox form (Projects/BrokenEngineSandbox/Source/Profile/ProfileRows.h:58-100: `inline constexpr std::string_view
    kp...[]`, one entry per line with a trailing comma per Documents/C++StyleGuide.txt rule 22, `std::size`
    static_assert after each):
    - `kpGameCpuTimerNames[]` holds `"Viewer frame",`; its static_assert is the sandbox one
      (`std::size(kpGameCpuTimerNames) == static_cast<size_t>(kGameCpuTimerCount) - static_cast<size_t>(engine::kEngineCpuTimerCount)`).
    - `kpGameCpuCounterNames[]` holds one placeholder entry `"",`, because the viewer has zero game counters
      (`kGameCpuCounterCount == kGameCpuCounterFirst == engine::kEngineCpuCounterCount`, :8-9) and C++ has no
      zero-length array (`T a[] = {}` and `T a[0]` are ill-formed). Its static_assert is the sandbox one with `+ 1` on
      the right-hand side, preceded by a one-line comment (rule 64) stating that C++ has no zero-length array and the
      placeholder is never read because the viewer defines no game counters. The placeholder text `""` is a trivial
      choice, since it is never read.
    - Never read: every `GetCpuCounterName` caller bounds its index by `miCpuCounterCount`
      (Engine/Source/Profile/ProfileManager.cpp:531-535; Engine/Source/Profile/ProfileScreens.cpp:67-82 and
      Engine/Source/Agent/AgentCommandsClientGeneric.cpp:1184 / AgentCommandsServerGeneric.cpp:218 are not
      ModelViewer.vcxproj ClCompile items), and the constructor sets `miCpuCounterCount = game::kGameCpuCounterCount`
      (ProfileManager.cpp:28), which in the viewer equals `kEngineCpuCounterCount`, so the `iIndex <
      kEngineCpuCounterCount` branch of the kept line (baseline ProfileManager.cpp:667) is always taken.
    - Rule 21 (Documents/C++StyleGuide.txt:164-172) allows C arrays for fixed-size storage; the `kp` prefix is rule 3's
      constexpr + C-array prefix (:24, :32). A null-pointer table was considered and rejected: the viewer's Release
      build runs PREfast, which reports indexing a null pointer. Sandbox ProfileRows.h and
      Projects/BrokenEngineSandbox/Source/Profile/AGENTS.md:27 are unchanged. No other user of the viewer names exists
      (git grep).
11. Shader naming. Recorded decision (user choice during planning): rename in Phase 1. Rename the `viewerCoverage` local
    in Engine/Data/Shaders/HdrResolve.frag:46,47,52,73 to `coverageAlpha`; behavior unchanged. This is a data input
    (Engine/Data/**), so the A2 game builds resolve Local data and DataPacker recompiles the shader; pack format
    unchanged.
12. Project membership via `/update-vcxproj`: ModelViewer.vcxproj(+filters) remove MainUniforms.cpp (item 5) and the
    `EngineSwitches.h` ClInclude (ModelViewer.vcxproj:271, filters :199), which no viewer file includes after the
    `Pch.h` change. Sandbox client and server membership unchanged. ModelViewer.vcxproj:66 keeps both
    `BT_MODEL_VIEWER` entries.
13. Docs (`/update-claude-docs`; each would be wrong after the change):
    - Engine/Source/Graphics/Managers/AGENTS.md:191-202 (UI scale and cursor park stated by switch/agent-input
      condition instead of "gameplay builds"/"ModelViewer");
    - Engine/Source/Graphics/AGENTS.md:57-64 (CameraBase also holds the world-space sun rotation, identity unless the
      host writes it, applied after the Normal Tilt);
    - Engine/Data/Shaders/AGENTS.md:33-36 (HDR resolve coverage alpha described by the feature, not "game and
      ModelViewer"/"Viewer rendering");
    - ModelViewer/AGENTS.md:50-71 (viewer `Pch.h` defines its own switches and does not include `EngineSwitches.h`;
      supplies `engine::RenderFrameMain` by membership, `game::Camera::SunAngle`, writes the inherited sun rotation,
      sets its own cursor flag; the remaining define excludes only the Phase 2/3 areas);
    - Engine/Source/Profile/AGENTS.md:7 ("Name tables preserve enum order and exact extent" becomes false for the
      viewer counter table: state that a game with no counters keeps one unread placeholder entry, because C++ has no
      zero-length array, without naming the viewer).
    - Unchanged and still true: Engine/Source/AGENTS.md:33, Projects/BrokenEngineSandbox/Source/AGENTS.md:34,
      Documents/C++StyleGuide.txt:74, Common/Log/AGENTS.md:40, Common/Log/Log.h:191, the code-style-review rule-6
      text.

## Critical files

- Engine/Source/EngineSwitches.h and ModelViewer/Source/Pch.h — the game and viewer switch values.
- Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp, Engine/Source/Graphics/Objects/ModelPipeline.cpp —
  coverage alpha and world-rendering gates.
- Engine/Source/Graphics/Render/GlobalUniforms.cpp, Engine/Source/Graphics/CameraBase.h,
  ModelViewer/Source/Graphics/Camera.h, ModelViewer/Source/Graphics/Camera.cpp — sun direction and `SunAngle`.
- Engine/Source/Graphics/Render/MainUniforms.cpp, ModelViewer/Source/ModelViewerApplication.cpp,
  ModelViewer/Source/Game.h — viewer-supplied `engine::RenderFrameMain` and cursor flag.
- Engine/Source/Graphics/Managers/ImGuiManager.cpp — UI scale, cursor flag, cursor park.
- Engine/Source/Profile/ProfileManager.cpp, ModelViewer/Source/Profile/ProfileRows.h — profile name tables.
- Engine/Source/Graphics/Islands.h, Engine/Data/Shaders/HdrResolve.frag.
- ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj and ModelViewer.vcxproj.filters.

## Risk triggers and invariants

- Tier 3: the change spans independently owned subsystems (Graphics, Profile, shaders, ModelViewer and its project
  membership).
- Sandbox CRC, determinism, and pack format are unchanged: no PostRender or CRC code changes and no CRC-read value
  changes.
- The only bit-level change is render-only: the sun-normal uniforms in Site decisions item 3 may differ in low-order
  bits; all are client GPU uniforms outside the CRC.
- Sandbox text at each switch site equals its baseline sandbox text; viewer behavior equals its baseline
  `BT_MODEL_VIEWER` behavior.
- No callback or function pointer into game code; no switch names the application.

## In scope

- Engine/Source/EngineSwitches.h:16-23 — block deleted, four feature switches added after `kbVulkanWireframe`.
- ModelViewer/Source/Pch.h:5 — include replaced by the viewer switch block (`### Switches`).
- Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:368,372-399,407,412-442,454.
- Engine/Source/Graphics/Objects/ModelPipeline.cpp:119.
- Engine/Source/Graphics/Render/GlobalUniforms.cpp:51-59, 526-530.
- Engine/Source/Graphics/CameraBase.h beside :177 — `mSunRotation` (data member only).
- Engine/Source/Graphics/Render/MainUniforms.cpp:16-21, 625.
- Engine/Source/Graphics/Managers/ImGuiManager.cpp:106-111, 152-159, 438-451, 473-480 (gate, condition, comment).
- Engine/Source/Graphics/Islands.h:50, 52.
- Engine/Source/Profile/ProfileManager.cpp:664-668, 673-677.
- Engine/Data/Shaders/HdrResolve.frag:46-73 — local rename only.
- ModelViewer/Source/Graphics/Camera.h:17,19 (removed) plus `SunAngle` declaration;
  ModelViewer/Source/Graphics/Camera.cpp:64-69 (removed) plus `SunAngle` definition.
- ModelViewer/Source/ModelViewerApplication.cpp:257 (cursor flag at top of `RenderUi`), 521-537 (`RenderViewerFrame`
  replaced by `engine::RenderFrameMain`).
- ModelViewer/Source/Game.h:10-11.
- ModelViewer/Source/Profile/ProfileRows.h:18-21 — C-array `kpGameCpuCounterNames` (one placeholder) and
  `kpGameCpuTimerNames` (Site decisions item 10).
- Engine/Source/Profile/AGENTS.md:7 — placeholder clause (Site decisions item 13).
- Membership: ModelViewer.vcxproj(+filters) remove MainUniforms.cpp and EngineSwitches.h.
- Documentation lines in Site decisions item 13.

## Out of scope

- Phase 2 (`Documents/Plans/Engine/RemoveModelViewerDefineEngineServices.md`: engine services the viewer neither uses
  nor can link) — these `BT_MODEL_VIEWER` sites remain after Phase 1:
  - Engine/Source/CrashReport.cpp:344, 521 (client crash upload).
  - Engine/Source/File/PackChunks.cpp:8, 650, 722, 729, 759, 843, 886 and Engine/Source/File/PackChunkLoader.cpp:7, 89,
    193, 242, 301 (audio streaming harness rig).
  - Engine/Source/Profile/ProfileManager.cpp:617-647 (profile screens).
  - Engine/Source/Graphics/Managers/PipelineManager.cpp:164-166 (collection graphics resources; recorded decision, user
    choice during planning: Phase 2).
- Phase 3 (`Documents/Plans/Engine/RemoveModelViewerDefineGameCoupling.md`: engine code that names the game; deletes
  the define) — these sites remain after Phase 1:
  - Engine/Source/Engine.h:11-13, 91-102 (the headers leave the aggregation, so the viewer Pch.h never needs
    `kbReplayFullFrames`).
  - Engine/Source/Graphics/CameraBase.cpp:9-12, 17-67, 76-368, 380-523 (includes the raw-input vibration call).
  - Engine/Source/Frame/IslandTerrain.cpp:7-9, 232-256, 324-403, 525-569.
  - Engine/Source/Graphics/Islands.cpp:192-399.
  - Engine/Source/Graphics/GraphicsUtils.cpp:123-136.
  - Engine/Source/Graphics/Graphics.cpp:465-473, 747-757 (Tweaks load and save around ImGui manager recreation, tied to
    screen ownership).
  - Engine/Source/Graphics/Managers/ImGuiManager.cpp:7-16, 20-22, 171-181, 491-518 (including the
    `game::RenderViewerUi` call, tied to screen ownership) and ImGuiManager.h:64-66, 104-113.
  - ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66 (both define entries) and the ModelViewer/AGENTS.md
    define description.
- Engine/Data/Shaders/** other than the HdrResolve.frag local rename; no pack-format change.
- Removing or re-homing the `Normal Tilt` tweak (`gSunMoonNormalTilt`) or its Tweaks screen row; it stays a live tweak
  composed into the sun rotation each frame.
- A `ProjectSwitches.h` file, any style-guide rule-6 or code-style-review reference edit.
- Moving `kbAutoConnect` or changing any existing EngineSwitches.h value.
- Shader lighting, pack format, extra viewer features, descriptor pool, lazy pool, and worktree-startup-build findings.
- New harness commands; ModelViewer harness support.
- Backward compatibility of any kind.

## Acceptance criteria

The execution card's Acceptance checks A1-A5 are the acceptance criteria.

## Execution card

Execution card:
### What does this plan do?
First step of removing the ModelViewer-only compile flag from the engine. Each place where the viewer really draws or
behaves differently from the game gets a narrowly named on/off switch: the engine's switch file holds the game's
setting, and the viewer's own precompiled header, which no longer includes the engine's switch file, holds the
viewer's. The viewer also supplies its own version of one render function and one camera member, and the sun's
northward tilt and the viewer's sun drag now go through one rotation path for both programs. The broad `kbModelViewer`
constant and the gates that need no replacement are deleted; the remaining gates are listed for two later phases.
### Why this is good for the codebase
The user does not want the engine to know the viewer exists. About a third of the application-named gates go away
now and engine code names features instead (coverage alpha, world rendering, UI scaling, clear color), the sun
direction has one code path instead of two, and the remaining gates are inventoried by phase.
- Goal: `kbModelViewer` gone; every Phase 1 `BT_MODEL_VIEWER` gate gone; ModelViewer no longer includes `EngineSwitches.h`; remaining sites exactly the `## Out of scope` Phase 2/3 list; both projects build; sandbox and viewer behavior unchanged.
- Out of scope: Phase 2/3 sites, other shader edits, pack format, the Normal Tilt tweak itself, other review findings, new harness commands (see `## Out of scope`).
- Tier trigger: Tier 3 — spans independently owned subsystems (Graphics, Profile, shaders, ModelViewer and its project membership).
- Interfaces and invariants: four new engine feature switches defined in `EngineSwitches.h` (game values) and in
  `ModelViewer/Source/Pch.h` (viewer values); the viewer `Pch.h` defines every switch its compiled engine sources read
  and includes no `EngineSwitches.h`; `CameraBase` gains public `glm::quat mSunRotation` (identity unless the host
  writes it); the sun direction is always rotated by `mSunRotation * angleAxis(Normal Tilt, +X)`; ModelViewer's
  `game::Camera` gains `SunAngle()` and loses `ApplySunRotation`/`mSunRotation`; ModelViewer defines
  `engine::RenderFrameMain` and no longer compiles MainUniforms.cpp; viewer ProfileRows name tables become C arrays
  `kpGameCpuCounterNames` (one never-read placeholder, static_assert `+ 1`) and `kpGameCpuTimerNames`, and
  ProfileManager reads only the `kp` names. Invariants: no PostRender/CRC code changes and no CRC-read value changes
  (sun-direction bit changes are client GPU uniforms only); sandbox text at each switch site equals baseline sandbox
  text; no callback or function pointer to game code; no switch names the application.
- Acceptance checks:
  - A1 `git grep -n kbModelViewer` prints nothing; `git grep -n EngineSwitches -- ModelViewer` prints nothing; `git grep -n -i viewer -- Engine/Data/Shaders/HdrResolve.frag` prints nothing; `git grep -n BT_MODEL_VIEWER -- Engine ModelViewer/Source ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj` lists exactly the Phase 2/3 sites in `## Out of scope` (by file and gate, re-numbered as edits shift lines); a read at the session baseline commit shows the Phase 1 sites present.
  - A2 `/compile` ModelViewer Debug|x64 and Release|x64, BrokenEngineSandbox client and server Debug|x64: all succeed with warnings as errors (Release includes the viewer's PREfast pass); the game builds resolve Local data because Engine/Data changed, so DataPacker compiles the renamed HdrResolve.frag. The ModelViewer builds also check viewer switch completeness and the ProfileRows static_asserts.
  - A3 Code read of the diff against the session baseline commit, per Site decisions item: sandbox switch values
    reproduce the baseline sandbox text and viewer values reproduce the baseline `BT_MODEL_VIEWER` text; every viewer
    `Pch.h` value equals the baseline EngineSwitches.h value for that configuration except the four feature switches;
    viewer `RenderFrameMain` body equals baseline `RenderViewerFrame`; viewer `SunAngle` returns `mfSunAngle`; the
    park-condition equivalence holds from Engine/Source/Main.cpp:166-192, 301, 455 and
    Engine/Source/LaunchOptions.h:35-38; the sun path applies the Normal Tilt quaternion about +X then `mSunRotation`,
    matching the baseline matrix rotation per Site decisions item 3; the viewer timer name table holds the baseline
    `"Viewer frame"` entry and the counter table only the placeholder, and every viewer-compiled `GetCpuCounterName`
    caller is bounded by `miCpuCounterCount` per Site decisions item 10.
  - A4 Sandbox client+server harness session per `/agent-harness`, run once before the first edit and once after:
    main-menu UI widget list with rectangles identical (UI scale and suppressed-cursor path); one synthetic menu click
    succeeds; after connect and spawn, client `query_profile`
    (.agents/skills/agent-harness/references/command-reference.md:220-221) returns the `kbWorldRendering`-gated GPU
    timer rows `    Terrain`, `    Water`, `    Hex Shields`, `    Long Particles Render`,
    `    Square Particles Render`, `    VisibleLights`, `    Billboards`
    (Engine/Source/Profile/ProfileManager.h:226-232), and every one of them with nonzero `averageUs` in the baseline run
    is nonzero after; no desync or CRC-mismatch log lines. Debug draws carry no timer and are covered by A3.
  - A5 ModelViewer runtime (recorded decision, user choice during planning: the user's manual launch is the viewer runtime check): after implementation the user launches the viewer and confirms models, gray background (clear color and coverage), UI size, cursor, and right-drag sun rotation look as before.
- Roles: implementer `/implement-plan` (single slice); `/update-affected-code`; `/update-vcxproj`; `/compile`;
  `/agent-harness` for A4; reviewers `/repo-code-review`, `/comment-review` (C++ and GLSL), `/code-style-review`,
  `/over-engineering-review`, `/adversarial-review` (Tier 3); `/update-claude-docs`, then `/coherence-review` and
  `/progressive-disclosure-review` for the AGENTS.md edits; `/verify-acceptance`; user for A5; plan review
  `/plan-audit`, `/plan-simplicity-review`, `/external-grill-plan`.
