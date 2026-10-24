<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T13:20:12.998Z","dependsOn":[]} -->
# Remove BT_MODEL_VIEWER, Phase 1: Image Pass Convergence and Program Switches

## Context

The engine must not know that the ModelViewer exists. `BT_CLIENT` and `BT_SERVER` stay, because both executables are
the engine; `BT_MODEL_VIEWER` names one application and goes. The removal is phased:

- Phase 1 (this plan): delete `kbModelViewer` and `kbFramebufferClearColor`. The image pass runs one path for both
  programs; each difference that remains is a purpose-named `inline constexpr bool kb*` switch defined in each
  program's own `Pch.h`, or a viewer-supplied definition selected by project membership. Gates that need no replacement
  are deleted. Each switch names a feature, never the application.
- Phase 2 (`Documents/Plans/Engine/RemoveModelViewerDefineEngineServices.md`): removes the gates around four engine
  services the viewer neither uses nor can link.
- Phase 3 (`Documents/Plans/Engine/RemoveModelViewerDefineGameCoupling.md`): removes the gates around engine code that
  names the game and deletes the define.

Recorded decisions (user choices during planning):

- Converge the image pass instead of naming its differences: coverage alpha is always on, the HDR color attachment
  always clears, and the world draws run in the viewer as zero-instance draws. Every difference left after that is a
  per-program switch in each program's `Pch.h`, following the engine-read `kbAutoConnect`
  (Projects/BrokenEngineSandbox/Source/Pch.h:5-11, read at Engine/Source/Network/Client/ClientSessionRuntime.cpp:780).
  ModelViewer/Source/Pch.h keeps including `EngineSwitches.h`; it copies no switch table.
- The ModelViewer keeps its current look and behavior (gray background); before/after viewer screenshots prove it.
- The sun rotation is always applied, with no switch, and the Normal Tilt goes through the same rotation.
- `HdrResolve.frag`'s viewer-named local is renamed in this phase.

Sandbox client/server simulation, CRC/determinism, and pack format are unchanged. The `BT_MODEL_VIEWER` define itself
survives until Phase 3.

History: every `BT_MODEL_VIEWER` gate and `kbModelViewer` were introduced in 601012a8d and 7d64500ec, both
"Opus/Astra plans" (no recorded reason). `kbFramebufferClearColor = kbVulkanWireframe` predates them (d8ce779b3).

Line numbers cite commit bae72d574b2f422ad876e9804304065b040627c3. When later commits have shifted lines, locate each
region by the content the citation describes.

## Design

### Why the convergence changes nothing on screen

- Coverage alpha in the game. HDR alpha is read only by HDR resolve: `mHdrTexture` is sampled only by the
  `kPipelineHdrResolve` descriptor (Engine/Source/Graphics/Managers/PipelineManager.cpp:158;
  `git grep mHdrTexture` finds no other reader), alpha-to-coverage is off
  (Engine/Source/Graphics/Objects/PipelineCreator.cpp:418), and HDR resolve uses `scene.a` only when its push value x is
  nonzero (Engine/Data/Shaders/HdrResolve.frag:46-52, 73). The game pushes 0 there and keeps doing so. The model push
  value x only forces opaque materials' output alpha to 1 (Engine/Data/Shaders/Model/Model.frag:390-393), and
  `kCoverageAlpha` only changes the blend's destination alpha factor
  (Engine/Source/Graphics/Objects/PipelineCreator.cpp:456). Both touch alpha only; RGB is unchanged.
  Model.frag is used only by the main-pass model pipelines (Engine/Source/Graphics/Managers/DynamicPipelines.cpp:104),
  whose only draws are the two main-pass sites below; the object-shadow model draw (CommandBufferRecordMain.cpp:293)
  uses a different pipeline map and keeps its push value.
- Always clearing HDR color. The game's attachment loads `DONT_CARE` today, so every pixel it shows is written by a
  draw; a cleared value under those draws is overwritten. The begin call already passes `kClear` and clear values for
  every attachment (Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:332-339,
  Engine/Source/Graphics/Objects/Texture.cpp:70-100), with clear color `{0, 0, 0, 0}` as the viewer uses today. The
  wireframe case, the reason `kbFramebufferClearColor` existed, is covered because the attachment always clears.
- World draws in the viewer. Every world draw is indirect and gets zero instances in the viewer:
  - Terrain: Islands' per-framebuffer indirect records are written with `instanceCount = 0` at construction
    (Engine/Source/Graphics/Islands.cpp:59-68); only `UpdateActiveIslands` rewrites them and it is compiled out of the
    viewer (Islands.cpp:192-399). The bound mesh arena exists (Islands.cpp:16-23); Graphics constructs `Islands` in
    the viewer too (Engine/Source/Graphics/Graphics.cpp:378-381).
  - Water and the four debug pipelines: `kIndirectHostVisible`, zeroed at creation (PipelineCreator.cpp:84-100;
    WorldLightingShadowPipelines.cpp:349-352; `PipelineManager::CreateDebugRenderPipelines`).
    Their writers are the game's `RenderFrameMain` (MainUniforms.cpp:523) and `DebugRender::EndRender`, called from
    that same game path; the viewer supplies its own `RenderFrameMain` (Site 5) and calls neither.
  - Hex shields, visible lights, billboards: dynamic pipeline maps filled only by collection `*Render.cpp` files, none
    of them a ModelViewer.vcxproj item, so the loops iterate nothing.
  - Long and square particles: device-local indirect records, written every frame by the spawn compute
    (Engine/Data/Shaders/Particles/ParticlesSpawn.comp:57-61, 130-134), which the viewer already records
    (CommandBufferRecordGlobal.cpp:54-57; ModelViewerApplication.cpp:376 calls `Graphics::RenderGlobal`). The first
    frame resets (`ParticleManager::mbReset` starts `true`, ParticleManager.h:21, sent at ParticleManager.cpp:78-79),
    writing `instanceCount = 0`; afterwards the spawn count is 0 because nothing viewer-compiled calls
    `ParticleManager::Spawn` (its caller is ExplosionsSpawn.cpp:228), so `iLastCount` stays 0. This is a code-read
    argument; A5's viewer screenshots are its runtime proof.
  The draws were already compiled into the viewer's objects except for the gate; every name in them is
  engine-declared, and the pipelines and their render targets exist in the viewer, which keeps world render targets'
  normal initialization and transitions (ModelViewer/AGENTS.md:93-94).

### Program switches

Two differences remain after the convergence. Each is defined in both `Pch.h` files, in a block placed directly after
`#include "ExternalHeaders.h"` and before `#include "Log/LogTypes.h"` (in the sandbox, directly after the
`kbAutoConnect` block, Projects/BrokenEngineSandbox/Source/Pch.h:11). The sandbox `Pch.h` is shared by the sandbox
client and server; the server never reads either name, as with `kbAutoConnect`.

| Switch | Sandbox `Pch.h` | Viewer `Pch.h` | Controls | Real difference (evidence) |
| --- | --- | --- | --- | --- |
| `kbHdrResolveGrayBackground` | `false` | `true` | HDR resolve push value x (CommandBufferRecordMain.cpp:454) | resolve then unpremultiplies scene coverage and composites a gray background where alpha is 0 (HdrResolve.frag:46-52, 73-77); game world draws do not write coverage alpha |
| `kbScaleUiToFramebufferHeight` | `true` | `false` | ImGui scale = framebuffer height / `kfUiReferenceHeight` (ImGuiManager.cpp:155-162, 441-454) | viewer keeps `UiScale()` at 1.0 by design (Engine/Source/Graphics/Managers/AGENTS.md:195-196) |

Both are config-independent, with no comment (the names describe them; the owning AGENTS.md lines in Site 12 carry the
contract). Neither name is defined in `EngineSwitches.h`: the viewer `Pch.h` includes it, so a second definition
would collide. No `ProjectSwitches.h` (recorded decision from the earlier planning round).

Engine/Source/EngineSwitches.h: delete :17-23 (the `#if defined(BT_MODEL_VIEWER)` block holding `kbModelViewer` and
both `kbFramebufferClearColor` definitions). No other EngineSwitches.h line changes.

### Site decisions

Each sandbox site's code after the change produces its baseline sandbox behavior and each viewer site its baseline
`BT_MODEL_VIEWER` behavior, except the alpha-only and cleared-pixel differences argued above.

1. Coverage alpha. CommandBufferRecordMain.cpp:368 and :407 push `{1.0f, 0.0f, 0.0f, 0.0f}`. ModelPipeline.cpp:119-122:
   the `if constexpr (kbModelViewer)` wrapper goes and `pipelineInfo.flags.Set(PipelineFlags::kCoverageAlpha);` runs
   unconditionally in the transparent branch. Model.frag keeps its x test (always true now); removing it is out of
   scope.
2. HDR resolve. CommandBufferRecordMain.cpp:454 pushes `{kbHdrResolveGrayBackground ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f}`.
   Rename the `viewerCoverage` local in Engine/Data/Shaders/HdrResolve.frag:46, 47, 52, 73 to `grayBackground`;
   behavior unchanged. This is a data input, so the game builds resolve Local data and DataPacker recompiles the shader.
3. HDR clear. Engine/Source/Graphics/Managers/SwapchainManager.cpp:39-41: the HDR color attachment's load op becomes
   `gMultisampling.Get<bool>() ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR` (with multisampling it
   is the resolve target, fully overwritten). :65, the multisample color attachment: `VK_ATTACHMENT_LOAD_OP_CLEAR`.
4. World draws. CommandBufferRecordMain.cpp:372 and :399, :412 and :442: delete the `#if !defined(BT_MODEL_VIEWER)`
   and `#endif` lines; the bodies are unchanged and now run in both programs.
5. Viewer `RenderFrameMain`. Engine/Source/Graphics/Render/MainUniforms.cpp:16-21 and :625: delete the viewer
   `RenderFrameMain`, the `#else`, and the closing `#endif`; remove MainUniforms.cpp from ModelViewer.vcxproj:198 and
   ModelViewer.vcxproj.filters:126. ModelViewer/Source/ModelViewerApplication.cpp defines `engine::RenderFrameMain`
   (declared Engine/Source/Graphics/Render/Render.h:157) with the exact signature, `[[maybe_unused]]` on
   `rActiveCoordinates` and `currentTime` as at baseline, and the body of `game::RenderViewerFrame`
   (ModelViewerApplication.cpp:660-676) applied to `rRenderInterpolates.at(cameraCoordinate)`. Delete
   `RenderViewerFrame` and, in ModelViewer/Source/Game.h, its declaration and the `struct FrameInterpolate;` forward
   declaration (:10-11).
6. Sun rotation (no switch).
   - `glm::quat mSunRotation {1.0f, 0.0f, 0.0f, 0.0f};` moves verbatim from ModelViewer/Source/Graphics/Camera.h:19 into
     the public section of `engine::CameraBase` beside `mfSunAngle` (Engine/Source/Graphics/CameraBase.h:177), with a
     one-line comment that it is a world-space sun rotation, identity unless the host writes it. The sandbox never
     writes it; viewer `RotateSun` (ModelViewer/Source/Graphics/Camera.cpp:61) keeps writing the inherited member.
     glm is in every client PCH (Common/ExternalHeaders.h:265-267); CameraBase.h is client-only.
   - GlobalUniforms.cpp:51-59 (tilt matrix and both gate branches) becomes one path:
     `glm::quat sunRotation = gpCamera->mSunRotation * glm::angleAxis(gSunMoonNormalTilt.mfCurrent, glm::vec3(1.0f, 0.0f, 0.0f));`,
     rotate the day-cycle direction's xyz by it (the baseline `ApplySunRotation` body, Camera.cpp:66-68), and store
     `XMVector4Normalize` of the result with w 0. The :51-52 comment states that the Normal Tilt is applied first, then
     the camera's sun rotation, and keeps its shadow-term clause.
   - Delete viewer `ApplySunRotation` (Camera.h:17, Camera.cpp:64-69); its body is inlined at the single caller, so
     no `CameraBase` member function is added and CameraBase.cpp is untouched.
   - The `Normal Tilt` tweak stays live and is composed every frame; baking it into the initial rotation would delete a
     Tweaks row (Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSunMoon.cpp:34).
   - Equivalence: `glm::angleAxis(t, +X)` applied with glm's `quat * vec3` gives y' = y cos t - z sin t,
     z' = y sin t + z cos t, the same rotation as row-vector `XMVector4Transform(v, XMMatrixRotationX(t))`; glm composes
     `R * T` as T first, the baseline viewer order. Low-order bits of f4SunMoonNormal, f4TerrainSnowSunNormal,
     f4WaterBiasedSunNormal and f2ShadowStretchTranslation may differ from baseline; all are client GPU uniforms
     written by `RenderFrameGlobal`, read by nothing PostRender or CRC. `gf2ShadowHeightOffsetPerMeter` derives from
     the raw sun angle and is unchanged.
7. `SunAngle`. GlobalUniforms.cpp:526-530 keeps only `static_cast<game::Camera*>(engine::gpCamera)->SunAngle()`. The
   viewer `game::Camera` (resolved by GlobalUniforms.cpp:16 from the viewer's own `Source`) declares
   `float SunAngle() const;` and defines it in ModelViewer/Source/Graphics/Camera.cpp as `return mfSunAngle;`, the
   documented `game::Camera` policy member (Engine/Source/Graphics/AGENTS.md:62-64).
8. UI scale. ImGuiManager.cpp:155-162 and 441-454: `if constexpr (kbScaleUiToFramebufferHeight) { ... }` around the
   unchanged bodies.
9. Cursor-change flag. ImGuiManager.cpp:109-114: delete. `ModelViewerApplication::RenderUi()`
   (ModelViewerApplication.cpp:388, called every frame inside the ImGui frame) sets, after its allocation-tracking
   scope line, `if (engine::gLaunchOptions.iAgentPort != 0) { ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; }`.
   Per frame, because Graphics recreates ImGuiManager and its ImGui context. Accepted delta: each ImGuiManager
   construction's bootstrap `ImGui_ImplWin32_NewFrame` may issue one `SetCursor(IDC_ARROW)` before the flag is set; it
   equals the viewer window-class cursor, and an agent-port viewer window is never shown.
10. Cursor park. ImGuiManager.cpp:469-473: the gated park becomes `if (gpAgentInput != nullptr)` around the unchanged
    `AddMousePosEvent` sentinel call; the :470-471 comment states the present truth (park when agent input exists; a
    host without agent input keeps its own queued pointer events). Sandbox: `PhysicalInputSuppressed()` is
    `kbAgent && iAgentPort != 0` (Engine/Source/LaunchOptions.h:35-38), the exact condition under which
    Engine/Source/Main.cpp:168-192 creates `AgentInput` (declared :166, before `pGraphics` at :301 and :455 in the same
    scope), so `gpAgentInput` is non-null whenever suppression is on. Viewer: ModelViewer/Source never constructs
    `AgentInput`, so `gpAgentInput` stays null, as the baseline gate behaved. No harness check exercises the park
    (`click` activates by item ID and never moves the mouse); A3's code read settles it.
11. Islands.h:50, 52: delete the gate lines; `Cell` and `GridCoord` are forward-declared, so the declaration compiles in
    the viewer. The definition stays gated in Islands.cpp (Phase 3); the viewer never calls it.
12. Profile name tables. ProfileManager.cpp:664-668 and 673-677 keep only the `game::kpGameCpu*Names` lines. Form
    settled from code: C arrays, because every other name table is one (Engine/Source/Profile/ProfileManager.h:90, 134,
    197, 276; Projects/BrokenEngineSandbox/Source/Profile/ProfileRows.h:58, 73), and a deduced extent catches a missing
    name, which `std::array<T, N>` with a literal N does not. ModelViewer/Source/Profile/ProfileRows.h:18-21 becomes the
    sandbox form (`inline constexpr std::string_view kp...[]`, one entry per line with a trailing comma, `std::size`
    static_assert after each):
    - `kpGameCpuTimerNames[]` holds `"Viewer frame",`; its static_assert is the sandbox one.
    - `kpGameCpuCounterNames[]` holds one placeholder `"",`, because the viewer has zero game counters (:8-9) and C++
      has no zero-length array. Its static_assert is the sandbox one with `+ 1` on the right-hand side, preceded by a
      one-line comment that C++ has no zero-length array and the placeholder is never read because the viewer defines
      no game counters.
    - Never read: every viewer-compiled `GetCpuCounterName` caller bounds its index by `miCpuCounterCount`
      (ProfileManager.cpp:531-535), which the constructor sets to `game::kGameCpuCounterCount` (:28), equal to
      `kEngineCpuCounterCount` in the viewer. ProfileScreens.cpp and the agent command callers are not viewer items.
      Sandbox ProfileRows.h is unchanged.
13. Project membership via `/update-vcxproj`: ModelViewer.vcxproj(+filters) remove MainUniforms.cpp (Site 5). The
    `EngineSwitches.h` ClInclude stays. Sandbox client and server membership unchanged. ModelViewer.vcxproj:66 keeps
    both `BT_MODEL_VIEWER` entries.
14. Docs (`/update-claude-docs`; each would be wrong after the change):
    - Engine/Source/AGENTS.md:33: `EngineSwitches.h` holds the engine-read switches with one value for every program
      and the `keLogLevel*` compile floors, and each program's `Pch.h` (game and ModelViewer) includes it after
      `Log/LogTypes.h` and before `Common.h`; an engine-read switch whose value differs by program is defined instead
      in each program's `Pch.h`, before that include, as `kbAutoConnect` is.
    - Engine/Source/Graphics/Managers/AGENTS.md:191-202: UI scale stated by `kbScaleUiToFramebufferHeight` instead of
      "gameplay builds"/"ModelViewer"; the cursor park stated by the agent-input condition.
    - Engine/Source/Graphics/AGENTS.md:57-64: `CameraBase` also holds the world-space sun rotation, identity unless the
      host writes it, applied after the Normal Tilt.
    - Engine/Data/Shaders/AGENTS.md:33-38: HDR resolve described for every program; model draws always keep coverage
      alpha; the gray-background composition runs under `kbHdrResolveGrayBackground`, without naming the viewer.
    - ModelViewer/AGENTS.md `## Engine integration` (:77-98): the viewer `Pch.h` defines its values of the two program
      switches and includes `EngineSwitches.h`; it supplies `engine::RenderFrameMain` by membership and
      `game::Camera::SunAngle`, writes the inherited sun rotation, and sets its own cursor flag; :93-95 becomes: the
      image pass clears HDR color, its world draws record zero instances because the viewer writes no world indirect
      records and registers no collection pipelines, and only model draws and UI depth produce pixels before HDR
      resolve. The remaining define list (:79-83) is unchanged.
    - Engine/Source/Profile/AGENTS.md:7: "exact extent" holds except that a game with no counters keeps one unread
      placeholder entry, because C++ has no zero-length array (without naming the viewer).
    - Unchanged and still true: Documents/C++StyleGuide.txt:74, Projects/BrokenEngineSandbox/Source/AGENTS.md:34,
      Common/Log/AGENTS.md:40, Common/Log/Log.h:191, the code-style-review rule-6 text, ModelViewer/AGENTS.md:5-10
      (the world passes stay invisible) and :64-75.

## Critical files

- Engine/Source/EngineSwitches.h, Projects/BrokenEngineSandbox/Source/Pch.h, ModelViewer/Source/Pch.h — switches.
- Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp, Engine/Source/Graphics/Objects/ModelPipeline.cpp,
  Engine/Source/Graphics/Managers/SwapchainManager.cpp, Engine/Data/Shaders/HdrResolve.frag — image pass.
- Engine/Source/Graphics/Render/GlobalUniforms.cpp, Engine/Source/Graphics/CameraBase.h,
  ModelViewer/Source/Graphics/Camera.h, ModelViewer/Source/Graphics/Camera.cpp — sun direction and `SunAngle`.
- Engine/Source/Graphics/Render/MainUniforms.cpp, ModelViewer/Source/ModelViewerApplication.cpp,
  ModelViewer/Source/Game.h — viewer-supplied `engine::RenderFrameMain` and cursor flag.
- Engine/Source/Graphics/Managers/ImGuiManager.cpp — UI scale, cursor flag, cursor park.
- Engine/Source/Profile/ProfileManager.cpp, ModelViewer/Source/Profile/ProfileRows.h — profile name tables.
- Engine/Source/Graphics/Islands.h; ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj and .filters.

## Risk triggers and invariants

- Tier 3: the change spans independently owned subsystems (Graphics, Profile, shaders, both programs' PCH, ModelViewer
  and its project membership) and changes sandbox GPU state (blend factor, push value, load op).
- Sandbox CRC, determinism, and pack format are unchanged: no PostRender or CRC code changes and no CRC-read value
  changes.
- Render-only differences, none visible: sandbox HDR alpha values (read by nothing at push value 0), sandbox cleared
  pixels under full-coverage draws, and low-order bits of the sun-normal uniforms (Site 6). One extra fast clear per
  frame in the sandbox.
- The viewer's world draws produce no pixels: zero instances by Design `### Why the convergence changes nothing on
  screen`; A5 proves it at runtime, including particles.
- No callback or function pointer into game code; no switch names the application; no switch is defined twice.

## In scope

- Engine/Source/EngineSwitches.h:17-23 — block deleted.
- Projects/BrokenEngineSandbox/Source/Pch.h after :11 and ModelViewer/Source/Pch.h after :3 — the two-switch block
  (`### Program switches`).
- Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:368, 372, 399, 407, 412, 442, 454.
- Engine/Source/Graphics/Objects/ModelPipeline.cpp:119-122.
- Engine/Source/Graphics/Managers/SwapchainManager.cpp:39-41, 65.
- Engine/Data/Shaders/HdrResolve.frag:46, 47, 52, 73 — local rename only.
- Engine/Source/Graphics/Render/GlobalUniforms.cpp:51-59, 526-530.
- Engine/Source/Graphics/CameraBase.h beside :177 — `mSunRotation` (data member only).
- Engine/Source/Graphics/Render/MainUniforms.cpp:16-21, 625.
- Engine/Source/Graphics/Managers/ImGuiManager.cpp:109-114, 155-162, 441-454, 469-473 (gate, condition, comment).
- Engine/Source/Graphics/Islands.h:50, 52.
- Engine/Source/Profile/ProfileManager.cpp:664-668, 673-677.
- ModelViewer/Source/Graphics/Camera.h:17, 19 (removed) plus `SunAngle` declaration;
  ModelViewer/Source/Graphics/Camera.cpp:64-69 (removed) plus `SunAngle` definition.
- ModelViewer/Source/ModelViewerApplication.cpp:388-392 (cursor flag in `RenderUi`), 660-676 (`RenderViewerFrame`
  replaced by `engine::RenderFrameMain`).
- ModelViewer/Source/Game.h:10-11.
- ModelViewer/Source/Profile/ProfileRows.h:18-21 — C-array tables (Site 12).
- Membership: ModelViewer.vcxproj:198 and ModelViewer.vcxproj.filters:126 (MainUniforms.cpp removed).
- Documentation lines in Site 14.

## Out of scope

- Phase 2 (`Documents/Plans/Engine/RemoveModelViewerDefineEngineServices.md`) — these `BT_MODEL_VIEWER` sites remain:
  - Engine/Source/CrashReport.cpp:344, 521.
  - Engine/Source/File/PackChunks.cpp:8, 655, 727, 734, 764, 848, 891 and Engine/Source/File/PackChunkLoader.cpp:7, 89,
    193, 242, 301.
  - Engine/Source/Profile/ProfileManager.cpp:617-647.
  - Engine/Source/Graphics/Managers/PipelineManager.cpp:164-166.
- Phase 3 (`Documents/Plans/Engine/RemoveModelViewerDefineGameCoupling.md`) — these sites remain:
  - Engine/Source/Engine.h:11-13, 91-102.
  - Engine/Source/Graphics/CameraBase.cpp:9-12, 17-67, 76-368, 380-523.
  - Engine/Source/Frame/IslandTerrain.cpp:7-9, 232-256, 324-403, 525-569.
  - Engine/Source/Graphics/Islands.cpp:192-399.
  - Engine/Source/Graphics/GraphicsUtils.cpp:123-136.
  - Engine/Source/Graphics/Graphics.cpp:465-473, 747-757.
  - Engine/Source/Graphics/Managers/ImGuiManager.cpp:7-16, 20-22, 174-184, 484-511 and ImGuiManager.h:64-66, 104-113.
  - ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj:66 and the ModelViewer/AGENTS.md define description.
- Model.frag (its x test stays), PipelineCreator blend logic, the HDR clear color value, and every shader except the
  HdrResolve.frag local rename; no pack-format change.
- Any other EngineSwitches.h value, moving `kbAutoConnect`, a `ProjectSwitches.h`, copying the switch table into a
  `Pch.h`, or a style-guide rule-6 or code-style-review edit.
- Removing or re-homing the `Normal Tilt` tweak or its Tweaks row.
- New harness commands; ModelViewer harness support.
- Rendering the world behind the viewer's model, seeded islands, or any other new viewer feature.
- Backward compatibility of any kind.

## Acceptance criteria

The execution card's Acceptance checks A1-A5 are the acceptance criteria.

## Execution card

Execution card:
### What does this plan do?
First step of removing the ModelViewer-only compile flag from the engine. The image pass becomes one path for the game
and the viewer: model draws always keep coverage alpha, the HDR color target always clears, and the world draws run in
the viewer too, drawing nothing there. The two differences left, the gray background behind viewer models and the UI
scale, become narrowly named switches defined in each program's own precompiled header. The viewer supplies its own
version of one render function and one camera member, the sun's northward tilt and the viewer's sun drag go through one
rotation path, and the broad `kbModelViewer` constant and the gates that need no replacement are deleted.
### Why this is good for the codebase
The user does not want the engine to know the viewer exists. About a third of the application-named gates go away,
most of them without any replacement because both programs now run the same image pass, and the remaining gates are
inventoried for the two follow-up plans.
- Goal: `kbModelViewer` and `kbFramebufferClearColor` gone; every Phase 1 `BT_MODEL_VIEWER` gate gone; remaining sites exactly the `## Out of scope` Phase 2/3 list; all builds pass; sandbox and viewer look and behave as before.
- Out of scope: Phase 2/3 sites, other shader edits, pack format, the Normal Tilt tweak itself, new viewer features, new harness commands (see `## Out of scope`).
- Tier trigger: Tier 3 — spans independently owned subsystems and changes sandbox GPU state.
- Interfaces and invariants: switches `kbHdrResolveGrayBackground` and `kbScaleUiToFramebufferHeight` defined in both
  `Pch.h` files (sandbox `false`/`true`, viewer `true`/`false`), never in `EngineSwitches.h`; model draws push 1.0 and
  blended model materials always set `kCoverageAlpha`; HDR color always clears; world draws unconditional;
  `CameraBase` gains public `glm::quat mSunRotation`; the sun direction is always rotated by
  `mSunRotation * angleAxis(Normal Tilt, +X)`; viewer `game::Camera` gains `SunAngle()` and loses
  `ApplySunRotation`/`mSunRotation`; the viewer defines `engine::RenderFrameMain` and no longer compiles
  MainUniforms.cpp; the cursor park runs only when `gpAgentInput != nullptr`; viewer profile name tables become C arrays
  (`kpGameCpuCounterNames` with one unread placeholder). Invariants: no PostRender/CRC change; no callback to game code;
  no switch names the application.
- Acceptance checks:
  - A1 `git grep -n -E "kbModelViewer|kbFramebufferClearColor" -- . ':!Documents/Plans'` prints nothing;
    `git grep -n -i viewer -- Engine/Data/Shaders/HdrResolve.frag` prints nothing;
    `git grep -n -E "kbHdrResolveGrayBackground|kbScaleUiToFramebufferHeight" -- '*.h'` lists definitions only in the
    two `Pch.h` files;
    `git grep -n BT_MODEL_VIEWER -- Engine ModelViewer/Source ModelViewer/Platforms/VisualStudio2026/ModelViewer.vcxproj`
    lists exactly the `## Out of scope` Phase 2/3 sites (re-numbered as edits shift lines); a read at the session
    baseline commit shows the Phase 1 sites present.
  - A2 `/compile` ModelViewer Debug|x64 and Release|x64 and BrokenEngineSandbox client and server Debug|x64 succeed with warnings as errors (the viewer Release build runs PREfast); the game builds resolve Local data because Engine/Data changed, so DataPacker compiles the renamed HdrResolve.frag; the viewer builds check the ProfileRows static_asserts.
  - A3 Code read of the diff against the session baseline commit, per Site: sandbox and viewer switch values reproduce
    the baseline push value and UI-scale behavior; the convergence evidence in `### Why the convergence changes nothing
    on screen` still holds at the changed tree; viewer `RenderFrameMain` body equals baseline `RenderViewerFrame`;
    viewer `SunAngle` returns `mfSunAngle`; the sun path applies the Normal Tilt quaternion about +X then
    `mSunRotation`; the park condition matches Site 10; every viewer-compiled `GetCpuCounterName` caller is bounded by
    `miCpuCounterCount`.
  - A4 Sandbox Debug client+server per `/agent-harness`, run once before the first edit and once after with the same
    steps: main-menu `describe_ui` items and rectangles identical (UI scale); after connect and spawn, a client
    `screenshot` of the same camera pose in both runs, compared side by side, shows the same scene (water, islands,
    units, UI) with no black, gray, or garbage pixels and no difference beyond time-driven animation; no desync,
    CRC-mismatch, or Vulkan validation-error log lines in either run.
  - A5 ModelViewer Debug per `/agent-harness` `references/model-viewer.md` `## Capture` with `-RequireStable` (the
    capture `.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1` runs), run once before the first edit and
    once after: the startup model and camera capture shows the same model shading, the same gray background with no
    terrain, water, or particle pixels, and the same UI panel size and layout; neither viewer log has a
    `DEBUG_BREAK at` (Vulkan validation) line.
- Roles: plan review `reviewer` `/plan-audit` and fresh `reviewer` `/plan-simplicity-review`, then `implementer`
  `/external-grill-plan`; `/plan-alternatives` (new switches and a new viewer function definition). Implement:
  `implementer` `/implement-plan` (single slice), then `/update-affected-code`; `mechanic` `/update-vcxproj`;
  `mechanic` `/code-style-review`; `builder` `/compile` (A2, plus the client and server for A4); `/agent-harness` for
  A4 and A5, each run once before the first edit. Review: `/over-engineering-review`, `reviewer` `/repo-code-review`,
  `mechanic` `/comment-review` (C++ and GLSL), `reviewer` `/adversarial-review`; `implementer` `/update-claude-docs`
  (Site 14), then `/coherence-review` and `/progressive-disclosure-review`; `/verify-acceptance`.
