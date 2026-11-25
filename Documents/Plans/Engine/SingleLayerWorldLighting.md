<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T21:46:44.438Z","dependsOn":[]} -->
# Single World Lighting Layer

## Context

World lighting keeps one layer per flight plane: `shaders::kiLightingLayers`
is 2 (`Engine/Data/Shaders/ShaderLayoutsBase.h:216-217`), each layer owns its
combine, history, and ambient textures, area latch, and reset flag, and one
shared deposit, spread, combine, temporal, and history-copy chain refreshes
at most one layer per frame (`giLightingRefreshLayer`,
`Engine/Source/Graphics/Render/Render.h:260-263`). Scheduled frames
alternate between the layers (`LightingUniforms.cpp:121-125`), a pending
layer refreshes first on the next frame (`:41-75`), lights deposit into the
layer of their nearest flight plane, and terrain, water, and model shaders
blend the two layers by height.

The user asked for the multi-layer concept to be removed entirely, delivered
as a series of Plans; lighting goes to one layer. The gameplay stage already
put every ship on one flight height (the base height, 10 m in BES), so the
second layer lights nothing that flies. Earlier fixed decisions from the
user: the Lighting Update Cadence slider stays, and no backward
compatibility. Smoke planes, object-shadow plane channels, and the
flight-plane table itself are later stages; this stage keeps smoke working on
its existing planes.

## Design

One lighting layer at the base height (`gpFlightPlanes->Lowest()`, published
as `GlobalLayout::fBaseHeight`).

- Schedule: a frame refreshes lighting when the shared chain needs it: a
  temporal reset (`gbLightingTemporalReset`, first use, recreation), a
  scheduled frame (`miFrameCounter % gLightingUpdateCadence == 0` with
  Lighting enabled), or a teleport (the visible-area center outside the
  latched area, with Lighting enabled). The alternation, the pending-layer
  rule, and the `static_assert(kiLightingLayers == 2)` go. Refresh work per
  scheduled frame is unchanged (one layer then, one layer now).
- `giLightingRefreshLayer` becomes `inline bool gbLightingRefreshFrame` in
  `Render.h`, written once per frame by `PopulateLightingParameters` and read
  by the light collections' `EndRender` and `RenderLightingSpreadIndirect`.
- `GlobalLayout` drops `uiLightingRefreshLayer`, `pf4LightingAreas`,
  `pLightingAreaToUvs`, and `fLightingLayerHeightInverse`. With one latch,
  `f4LightingArea` and `lightingAreaToUv` always hold the same area those
  arrays held for layer 0, so consumers read them instead (author's
  recommendation: one copy of one value). `lightingAreaPreviousToUv` and
  `fLightingTemporalBlend` keep their roles.
- Render targets and pipelines: the per-layer arrays collapse to one set —
  three color combine textures and three history textures, one ambient
  combine texture and one ambient history texture, one combine, one temporal,
  and one history-copy pipeline. The final-lighting sampler binding becomes a
  three-entry array; the ambient binding becomes a single `sampler2D`
  (author's recommendation over a one-entry array). Debug names drop the
  layer digit (`CombineRed`, `CombineAmbient`, `LightingHistoryRed`,
  `LightCombine`, `LightingTemporal`, `LightingHistoryCopy`, and so on).
- Light deposits: AreaLights, PointLights, and HexShields keep one deposit
  buffer, one cursor, and one lighting pipeline each, keyed
  `DynamicPipelines::FlightPlaneDepositKey(crc, 0)` (its removal, keying by
  the collection CRC, is a later stage). Every deposit projects with
  `ProjectToBaseHeight`;
  `NearestPlaneIndex` and the per-layer projection go. A deposit count is
  nonzero only on a refresh frame with Lighting enabled, as today for the
  refreshing layer; the HexShields main draw count is unaffected.
- Shaders: `ReadLighting` and `ReadAmbientLighting` drop the layer index.
  Terrain, Water, and Model read the one layer the way they read the low
  layer today (Terrain directional at the fragment's world position and
  ambient at the base-height projection, Water at the base-height projection,
  Model along the eye line by the model centre's height above the base
  height) through `lightingAreaToUv`, and drop the high-layer read, the
  height blend, and `fLayerBlend`. Water's reads (`Water.frag:203-210`) and
  Model's disabled `#else` lighting branch (`Model.frag:332-345`) keep their
  positions, replace `pLightingAreaToUvs[0]` with `lightingAreaToUv`, and
  drop the layer argument. Every smoke plane's lighting in those three
  shaders uses that same base-height read (author's recommendation: smoke
  goes to one plane in the next stage, so the per-plane ambient re-reads are
  not worth keeping for one stage).
  `LightingSpread.frag` always applies the terrain height fade (it applied
  only to layer 0). `HexShield.vert` and `DebugTexture.vert` read
  `fBaseHeight` where they read the refreshing layer's plane height, and
  `DebugTexture.vert` reads `f4LightingArea` where it read
  `pf4LightingAreas[0]`.
- Agent: `presentation_continuity_probe` returns `lighting` as one object
  `{"currentArea","previousArea"}` instead of an array
  (`PresentationContinuitySnapshot::lighting`). `dump_render_target`
  `Combine` and `LightingHistory` take `index` 0-2 (the color), and
  `AmbientCombine` and `AmbientHistory` take no index, like the other single
  textures.

No persisted format changes; nothing to bump. `GlobalLayout` is a CPU/GPU
contract compiled from one header on both sides.

## Critical files

- `Engine/Source/Graphics/Render/LightingUniforms.cpp`, `Render.h`,
  `GlobalUniforms.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.h`,
  `RenderTargetTexturesLighting.cpp`, `PipelineManager.h`,
  `WorldLightingShadowPipelines.cpp`, `CommandBufferRecordMain.cpp`
- `Engine/Data/Shaders/ShaderLayoutsBase.h`, `ShaderGlobalLayout.h`,
  `ShaderFunctions.h`, `Terrain/Terrain.frag`, `Water/Water.frag`,
  `Model/Model.frag`
- `Engine/Source/Frame/Collections/{AreaLights,PointLights,HexShields}/*Render.cpp`

## In scope

- `Engine/Data/Shaders/ShaderLayoutsBase.h`: `kiLightingLayers` and its
  comment (`:216-217`).
- `Engine/Data/Shaders/ShaderGlobalLayout.h`: the lighting block
  (`:158-165`): remove `uiLightingRefreshLayer`, `pf4LightingAreas`,
  `pLightingAreaToUvs`, `fLightingLayerHeightInverse`; reword the
  `f4LightingArea`, `lightingAreaPreviousToUv`, and `fLightingTemporalBlend`
  comments that name the refreshing layer.
- `Engine/Data/Shaders/ShaderFunctions.h`: `ReadLighting`,
  `ReadAmbientLighting` (`:113-124`) and their comment.
- `Engine/Data/Shaders/Terrain/Terrain.frag`: lighting sampler bindings
  (`:19`, `:37`), the layer read and blend (`:120-141`), and the smoke loop's
  lighting choice (`:168-187`, `bLayerRead`, the per-plane
  `ReadAmbientLighting`).
- `Engine/Data/Shaders/Water/Water.frag`: bindings (`:18`, `:27`), the
  lighting reads and their comment (`:199-210`), the smoke loop's per-plane
  ambient (`:253-270`).
- `Engine/Data/Shaders/Model/Model.frag`: binding (`:61`), the layer reads and
  blend (`:307-326`), the `#else` branch's lighting reads (`:332-345`), the
  smoke loop's lighting choice (`:389-425`).
- `Engine/Data/Shaders/Lighting/LightingSpread.frag`: the layer condition
  around the height fade (`:69-71`).
- `Engine/Data/Shaders/Objects/HexShield.vert`: the deposit height
  (`:91-94`).
- `Engine/Data/Shaders/DebugTexture.vert`: the lighting branches (`:36-37`,
  `:41`).
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`: the `static_assert`
  (`:18-19`), `PopulateLightingParameters` (`:21-100`), the schedule in
  `RenderLightingGlobal` (`:121-125`, `:192`), `RenderLightingSpreadIndirect`
  (`:328-354`). `RenderLightingMain`'s smoke-shadow loop (`:319-325`) stays.
- `Engine/Source/Graphics/Render/Render.h`:
  `PresentationContinuitySnapshot::pLighting` to `lighting` (`:221`) and the
  struct comment naming `pLighting` (`:193`);
  `giLightingRefreshLayer` to `gbLightingRefreshFrame` with its comment
  (`:260-263`).
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`: the
  `fLightingLayerHeightInverse` write (`:632`).
- `Engine/Source/Graphics/Managers/RenderTargetTextures.h`: the lighting
  members (`:52-66`): three color combine and three history textures, one
  ambient combine and one ambient history texture, the three-entry
  `mppLightingFinalTextures`; `mppAmbientCombineTextures` goes.
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp`: the
  `Frame/FlightPlaneTable.h` include (`:8`), the table-size `static_assert`
  and name tables (`:13-20`), the destroy loop (`:28-36`), the create, reset,
  and binding loops (`:371-405`), the debug slot (`:433`).
- `Engine/Source/Graphics/Managers/PipelineManager.h`: `mpCombinePipelines`,
  `mpLightingTemporalPipelines`, `mpLightingHistoryCopyPipelines`
  (`:105-111`) to single members.
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`: the
  pipeline name tables (`:14-16`), the per-layer creation loop (`:63-120`),
  the combine-texture references (`:210`, `:222`), and the terrain and water
  ambient descriptors (`:275`, `:346`), which bind the single ambient texture
  through `.pTexture`. The final-lighting descriptors (`:255`, `:337`, and
  `Engine/Source/Graphics/Objects/ModelPipeline.cpp:23`) take their count from
  `std::size(mppLightingFinalTextures)` and need no edit.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`: combine and
  temporal recording (`:163-200`).
- `Engine/Source/Graphics/Screenshot.cpp`: `kpcValidDumpNames`, the
  `AmbientCombine`, `AmbientHistory`, `Combine`, `LightingHistory` branches.
- `Engine/Source/Agent/Commands/PresentationContinuityProbe.cpp`: the
  `lighting` field (`:61-65`).
- `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp`
  (`:20-31`, `:48-51`, `:75-78`, `:90`, `:170-177`, `:198`, `:209-212`),
  `PointLights/PointLightsRender.cpp` (`:19-30`, `:47-50`, `:72-75`,
  `:111-122`, `:175-178`), `HexShields/HexShieldsRender.cpp` (`:18-28`,
  `:44-51`, `:66-69`, `:87-89`, `:115`, `:125-130`): per-layer pipeline and
  buffer creation, cursors, buffer fetch, the nearest-layer choice and
  projection, and the indirect-count publication.
- Prose this stage makes wrong:
  - `Engine/Source/Frame/AGENTS.md` `## Architecture` `:121-123`: the clause
    requiring exactly two planes for the client.
  - `Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`:
    the world-lighting bullet (`:33-45`); the lighting half of the schedule
    bullet (`:64-80`: the shared rule, the layer index, "each lighting
    layer's", "both layers", "each layer whose area"); the disable bullet's
    "each layer".
  - `Engine/Source/Graphics/Managers/AGENTS.md` `:347`: one pipeline per
    lighting layer.
  - `Engine/Data/Shaders/Lighting/AGENTS.md` `## Pipeline Contracts`
    `:11-16`; `Engine/Data/Shaders/Objects/AGENTS.md` `:19-20`;
    `Engine/Data/Shaders/Terrain/AGENTS.md` `:38-43`;
    `Engine/Data/Shaders/Water/AGENTS.md` `:106-110`;
    `Engine/Data/Shaders/Model/AGENTS.md` `:51-56` (the lighting sample and
    "plane k's smoke is lit by"); `Engine/Data/Shaders/Smoke/AGENTS.md` `:7`
    ("lighting plane k's smoke with lighting layer k").
  - `Engine/Source/Frame/Collections/AreaLights/AGENTS.md`,
    `PointLights/AGENTS.md`, `HexShields/AGENTS.md`: the per-layer buffer,
    nearest-plane, `ProjectToFlightPlane`, and refreshing-layer wording.
  - `.agents/skills/agent-harness/references/command-reference.md`
    `dump_render_target` (`:258-262`) and `presentation_continuity_probe`
    (`:366-368`; the `smoke` sentence stays).
- Stale non-Plan document: update
  `Documents/Features/Graphics/PlanarHolographicRadianceCascades.md` so its
  statements about two lighting layers, layer 1, the high plane,
  `giLightingRefreshLayer`, `pf4LightingAreas`/`pLightingAreaToUvs`,
  `fLightingLayerHeightInverse`, `NearestPlaneIndex` deposit routing, and
  cross-layer transport describe the one layer at the base height; delete the
  items that exist only for layer 1.

## Out of scope

- Smoke planes and their tuning (`pfSmokeShadowIntensities`,
  `FlightPlanePosition`, `pfFlightPlaneHeights`, `uiFlightPlaneCount`,
  `ComputeSmokePlaneWeights`, `ProjectToFlightPlane`), the object-shadow
  plane channels, and `f4PositionLayer.w`.
- `engine::FlightPlanes` and `game::kFlightPlanes`; the removal of
  `FlightPlaneDepositKey`.
- The Lighting Update Cadence slider, lighting tuning values, the deposit and
  spread passes themselves, and the terrain-shadow and object-shadow
  schedules.
- Gameplay code. Unit tests.

## Risk triggers and invariants

Tier 3: the CPU/GPU `GlobalLayout` contract changes, and the change spans
Graphics, engine collection renderers, shaders, and the agent probe.

- Main never writes `GlobalLayout`; `gbLightingRefreshFrame` is written in
  Global before Main reads it.
- Deposit counts and data stay paired: a zero count with no array write, as
  today.
- Recreation still re-seeds history from current data for one frame
  (`gbLightingTemporalReset`).
- Retained lighting areas keep following camera-cell shifts
  (`RetainedAreaBasis`).
- Render stays outside the CRC; no simulation byte changes.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and the ModelViewer,
   including their shader data.
2. No `kiLightingLayers`, `giLightingRefreshLayer`, `uiLightingRefreshLayer`,
   `pf4LightingAreas`, `pLightingAreaToUvs`, or `fLightingLayerHeightInverse`
   identifier remains under `Engine/` or `Projects/`.
3. `/agent-harness` client: `presentation_continuity_probe` returns
   `lighting` as one object with `currentArea` and `previousArea`.
4. `dump_render_target` `{"name":"Combine","index":2}` succeeds and
   `{"name":"Combine","index":3}` fails with
   `index out of range [0, 3)`; `{"name":"AmbientCombine"}` succeeds.
5. `describe_ui` on the graphics settings screen lists the Lighting Update
   Cadence slider, and `set_slider` changes it.
6. With `pin_graphics`, a `screenshot` of a cell with Fighter beams or point
   lights shows their light on water and terrain, and a screenshot with
   Lighting turned off does not.
7. A ModelViewer flight-mode screenshot (`/agent-harness` ModelViewer
   reference) renders the world lit.

## Coordination

- Preserve independent visible-sprite culling and projected deposit emission in
  `PointLightsInterpolate::Render` (`PointLightsRender.cpp`) and
  `AreaLightsInterpolate::Render` (`AreaLightsRender.cpp`) when collapsing
  plane selection to `ProjectToBaseHeight` and the area latches to one layer;
  GPU clipping against `GlobalLayout::f4LightingArea` owns off-area culling.
  Both collections already use a single deposit buffer, cursor, and pipeline
  keyed by `kCrc` and `kuiCrc`, so keep those keys instead of converting them
  to `FlightPlaneDepositKey(crc, 0)` (only `HexShieldsRender.cpp` still
  converts from `FlightPlaneDepositKey`). Currently `giLightingRefreshLayer`
  (`Render.h`) encodes `-1` on disabled or skipped frames while
  `LightingUniforms.cpp` keeps a file-static combine refresh layer; when
  replacing `giLightingRefreshLayer` with `gbLightingRefreshFrame`, carry
  forward that split so disabled or skipped frames emit no deposits or spread
  draws while disabling lighting still runs its separate combine reset to clear
  output to black.
- With `SkipEmptyLightingSpread.md`, retain the effective deposit count
  publication when collapsing the collection buffers and refresh gate. The
  new gate still applies to spread draws while combine, temporal and history
  copy follow the refresh.
