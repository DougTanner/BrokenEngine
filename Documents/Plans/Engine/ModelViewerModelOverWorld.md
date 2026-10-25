<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T18:18:35.011Z","dependsOn":["Documents/Plans/Engine/ModelViewerModelOwnSpace.md"]} -->
# ModelViewer world, stage 3: seeded islands and water behind the model

## Context

Stage 3 of three (stage 1 `Documents/Plans/Engine/ModelViewerWorldUniforms.md` holds the shared context; stage 2
`Documents/Plans/Engine/ModelViewerModelOwnSpace.md`). User's words (agent-recorded): islands "spawned 'randomly'
from a chosen seed"; "Instead of main-menu it would be the ModelViewer's UI"; the viewer keeps "basically the same
functionality as it does now (just has water/terrain rendered behind instead of grey framebuffer)". Recorded
decisions (user choices, agent-recorded) this stage implements:

- D1: the seed is a grid coordinate passed to the existing `GenerateIslandChain`
  (Engine/Source/Frame/IslandChainPlacement.h:40); no engine generator change.
- D2: the viewer builds one cell's static data and calls `Islands::UpdateActiveIslands` with Phase 3's
  `std::span<const CellStaticData* const>` signature, plus the island residency code it already compiles; no
  local game session.
- D3: one cell, with the world camera framed inside it.
- D6: one shared sun: right-drag and Sun angle relight terrain and water too (already true once they are visible:
  every sun uniform carries the world-space sun rotation, Phase 1 Site 6).
- D7: world light deposits, smoke, and terrain shadow stay excluded from the model (the viewer's four PBR
  isolation writes stay).
- D8: the gray background goes: terrain and water write every pixel, so the HDR color target needs no clear outside
  `kbVulkanWireframe` builds and the per-program `game::kVkHdrClearColor` is deleted (user: "The water/island render
  will write over the framebuffer completely, so it will no longer need to be cleared").
- D10: a seed field and a reroll button in the existing viewer panel; the seed is not saved and resets each launch.

Facts (commit bae72d574b2f422ad876e9804304065b040627c3; locate by content after earlier stages):

- `GenerateIslandChain` clears and fills the vector; the coordinate only seeds its RNG streams, positions are
  cell-local in [-450, 450], and output index 0 is the Huge anchor, always placed
  (IslandChainPlacement.h:33-40; IslandChainPlacement.cpp:227-252). The viewer already compiles it, `Islands.cpp`,
  `IslandTerrain.cpp`, and `IslandTerrainResidency.cpp` (ModelViewer.vcxproj:160-168), and constructs
  `IslandTerrain`, `FlightPlanes`, and `IslandTerrainResidency` (Tools/ModelViewer/Source/Main.cpp:119-121).
- `UpdateActiveIslands` must run once per frame after the camera update and before `RenderGlobal`, once the
  acquired framebuffer index is valid (GameBase.cpp:942-943; Islands.cpp:194-199). It acquires texture slots
  itself (Islands.cpp:276); `RenderGlobal` runs eviction and restoration (Graphics.cpp:144-154). Placement offsets
  come from `MakeRenderBasis(staticData.coordinate, gpCamera->mBasisCoordinate)`, and the viewer camera's basis is
  `kOriginCoordinate`, so the viewer's cell keeps `coordinate` = `kOriginCoordinate` (the seed only feeds the
  generator).
- The viewer's `elapsedTime` passed to `RenderMainPresentAcquire` grows monotonically (`mTimer.GetDeltaNs()`
  without reset, ModelViewerApplication.cpp:375), as the waves need.
- After Phase 1, HDR resolve takes no push constants and grades `scene.rgb` for both programs, and the HDR color
  attachment clears to each program's `game::kVkHdrClearColor` (sandbox `{0, 0, 0, 0}`; viewer gray
  `{0.153, 0.153, 0.153, 1}`, which grades to about sRGB 0.5), so the gray shows wherever the viewer draws nothing.

## Design

1. Seed and cell (ModelViewerApplication.h private, ModelViewerApplication.cpp):
   - Members `engine::GridCoord mIslandSeed = engine::kOriginCoordinate;`, `engine::CellStaticData mIslandCell;`
     (its `coordinate` stays `kOriginCoordinate`), `common::RandomEngine mSeedRandomEngine;`.
   - `void GenerateIslands();` runs `engine::GenerateIslandChain(mIslandSeed, mIslandCell.islands);`.
   - The constructor calls `mSeedRandomEngine.TimeSeed()` and `GenerateIslands()`.
2. `Render()`:
   - Before `pCamera->Update()`, set the world camera's XY to the anchor:
     `pCamera->mVecPosition = XMVectorSet(rAnchor.f2WorldPosition.x, rAnchor.f2WorldPosition.y, 0.0f, 1.0f)` with
     `rAnchor = mIslandCell.islands.at(0)`. Stage 2's `Update` sets Z and the eye.
   - After the camera update and before `RenderGlobal`:
     `const engine::CellStaticData* pIslandCell = &mIslandCell;`, then
     `engine::gpIslands->UpdateActiveIslands(std::span(&pIslandCell, 1));`.
3. Viewer `engine::RenderFrameMain`: call `engine::RenderWaterMain(iCommandBuffer, currentTime);` after
   `RenderLightingMain`, as the game definition does; drop `[[maybe_unused]]` from `currentTime`.
4. Panel (`RenderUi`, "Model Viewer" window): after the Render quality block's closing `ImGui::Separator()` and
   before `Scenes:`, add `ImGui::SeparatorText("Islands")`; a `ImGui::InputInt2("Seed", ...)` over a local
   `int[2]` copy of `mIslandSeed`, which on change stores the pair and calls `GenerateIslands()`; and
   `ImGui::Button("Reroll")`, which sets `mIslandSeed = engine::GridCoord::FromKey(common::RandomNext(mSeedRandomEngine))`
   and calls `GenerateIslands()`; then `ImGui::Separator()`. Nothing is saved.
5. Gray background removal (D8):
   - Delete `kVkHdrClearColor` from Projects/BrokenEngineSandbox/Source/Game.h and Tools/ModelViewer/Source/Game.h.
   - CommandBufferRecordMain.cpp `RecordImageRenderPass`: the begin call passes a local `VkClearColorValue {}` (the
     depth clear still needs the clear values), and the `Game.h` include Phase 1 added goes.
   - SwapchainManager.cpp HDR render pass: the HDR color load op becomes `kbVulkanWireframe ?
     (gMultisampling.Get<bool>() ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR) :
     VK_ATTACHMENT_LOAD_OP_DONT_CARE` and the MSAA color load op `kbVulkanWireframe ? VK_ATTACHMENT_LOAD_OP_CLEAR :
     VK_ATTACHMENT_LOAD_OP_DONT_CARE`, the sandbox's form before Phase 1, so wireframe builds still clear to black.
6. Docs (`/update-claude-docs`):
   - Tools/ModelViewer/AGENTS.md `## Application contracts` :7-10: a seeded one-cell world (terrain and water) is drawn
     behind the selected model under the same grading; the model draws in its own space over cleared depth; the
     seed is a grid coordinate edited or rerolled in the panel and never saved.
   - `## Engine integration`: the viewer supplies one cell's static data to `UpdateActiveIslands` each frame,
     frames the world camera on the anchor island, and calls `RenderWaterMain`.
   - Engine/Data/Shaders/AGENTS.md `## Top-Level Shaders` HDR resolve: world draws cover every pixel, so no program
     depends on an HDR clear color; the line naming `game::kVkHdrClearColor` goes.
   - Tools/ModelViewer/AGENTS.md: every line naming `game::kVkHdrClearColor` or the viewer's gray HDR clear color,
     including the list of game names the viewer supplies (Phase 1 and Phase 3 placed them).

## Critical files

- Tools/ModelViewer/Source/ModelViewerApplication.h, ModelViewerApplication.cpp.
- Projects/BrokenEngineSandbox/Source/Game.h, Tools/ModelViewer/Source/Game.h.
- Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp, Engine/Source/Graphics/Managers/SwapchainManager.cpp.
- Tools/ModelViewer/AGENTS.md, Engine/Data/Shaders/AGENTS.md.

## Risk triggers and invariants

- Tier 3: the image-pass clear value and HDR load ops for both programs and both programs' `Game.h` beside viewer
  residency, upload, and per-frame indirect publication.
- Sandbox: the HDR color and MSAA color load ops return to their form before Phase 1 (`DONT_CARE` outside wireframe
  builds); water and terrain write every pixel, so the frame is unchanged; no CRC, PostRender, or determinism change.
- The viewer constructs no `GameBase`, cell map, or session; its cell is real data the renderer consumes.
- `UpdateActiveIslands` runs exactly once per rendered frame, only after `Render()`'s swapchain early return.

## In scope

- ModelViewerApplication.h members and `GenerateIslands` declaration; ModelViewerApplication.cpp constructor,
  `GenerateIslands`, `Render`, `RenderUi` panel rows, `engine::RenderFrameMain`.
- Both `Game.h` `kVkHdrClearColor` definitions; CommandBufferRecordMain.cpp image-pass clear value and `Game.h`
  include; SwapchainManager.cpp HDR color and MSAA color load ops.
- The doc lines in Design item 6.

## Out of scope

- Engine generator, residency, or `Islands` changes; neighbor cells; navigation, elevation grids, or
  `BuildRenderPlacementCache`.
- Saving the seed; world-camera pan or zoom; changing the model's orbit camera, sun drag, presets, or comparison.
- Any shader change.

## Acceptance criteria

- A1 `git grep -n kVkHdrClearColor -- . ':!Documents/Plans'` prints nothing; ModelViewerApplication.cpp calls
  `GenerateIslandChain`, `UpdateActiveIslands`, and `RenderWaterMain` once each.
- A2 `/compile` ModelViewer Debug|x64 and Release|x64 and BrokenEngineSandbox client and server Debug|x64 succeed
  with warnings as errors.
- A3 Code read against the session baseline: the HDR color and MSAA color load ops equal Design item 5's form and the
  image-pass clear value is `{}`; the island update runs after the camera update and before `RenderGlobal` and after
  the deferred-swapchain return.
- A4 Sandbox Debug client+server per `/agent-harness`, before the first edit and after with the same steps: a
  client `screenshot` of the same camera pose shows the same scene; no `DEBUG_BREAK at` lines in either client log.
- A5 ModelViewer Debug per `/agent-harness` `references/model-viewer.md` `## Capture` (no `-RequireStable`, since
  water animates), once before the first edit and once after: a script under `Temp` counts pixels within 1 of
  (128, 128, 128) in each final PNG; the before capture has more than 10% such pixels and the after capture fewer
  than 1%; no `DEBUG_BREAK at` line in the after log.
- A6 The user launches the viewer and confirms: the startup model draws over terrain and water and is never hidden
  by them; the Seed field and Reroll change the islands; right-drag and Sun angle relight the terrain and water as
  well as the model; the lighting and grading sliders change the world too; model selection, animation, presets,
  Q comparison, and F9 behave as before.
