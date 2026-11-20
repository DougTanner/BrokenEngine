<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T21:46:45.079Z","dependsOn":["Documents/Plans/Engine/SingleLayerWorldLighting.md"]} -->
# Single Smoke Layer

## Context

Smoke is a client-only density field per flight plane. Each plane owns a
ping-pong texture pair (`RenderTargetTextures.h:31-34`), an occupancy buffer
pair (`BufferManager.h:64-66`), a smoke pipeline set
(`PipelineManager.h:10-18`, `:43-44`, `:88`), a held area, a step count, and a
decay (`SmokeUniforms.cpp:122-211`). Deposits split a source between the two
planes bracketing its height (`ComputeSmokePlaneWeights`,
`GraphicsUtils.cpp:79-96`), and world shaders loop over the planes. Smoke
shadow intensity and wind-noise quantity have Bottom and Top tuning lerped
over plane index (`FlightPlaneLerp`, `Render.h:237-241`). With
`gSmokeLayerStagger` on, a scheduled frame refreshes one plane in turn; the
Graphics menu shows it as the "Stagger Smoke Layer Updates" checkbox
(`Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp:206`), persisted as
`GraphicsSettingsFlags::kSmokeLayerStagger` in `GraphicsSettings.bin`
(`Engine/Source/Ui/GraphicsSettings.cpp:24`, `:125`, `:283`, `:324`).

The user asked for the multi-layer concept to be removed entirely, delivered
as a series of Plans; smoke goes to one layer. Earlier stages put every ship
on one flight height (the base height, 10 m in BES) and world lighting on one
layer at that height. Earlier fixed decisions from the user: the "Stagger
Smoke Layer Updates" checkbox goes; the Smoke Update Cadence and Lighting
Update Cadence sliders both stay; no backward compatibility, so the changed
`GraphicsSettings.bin` format takes a version bump. The flight-plane table and
the object-shadow plane channels are the last stage.

## Design

One smoke field at the base height.

- Schedule: the field refreshes on a scheduled frame
  (`miFrameCounter % gSmokeUpdateCadence == 0`) or a teleport, unpaused, as
  plane 0 does today with the stagger off; clear, disabled, paused, and skip
  exits keep their behaviour for the one field. `gSmokeLayerStagger`, its
  menu toggle, its flag bit, and its load, save, and reset lines go.
  `GraphicsSettings::kiVersion` goes from 29 to 30; the loader's existing
  version rejection makes an older file fall back to defaults.
- Layouts: `GlobalLayout` drops `pfFlightPlaneHeights` and
  `uiFlightPlaneCount`, and the per-plane smoke arrays become scalars:
  `f4SmokeArea`, `f4PreviousSmokeArea`, `f2PreviousSmokeAreaSizeInverse`,
  `fSmokeDecay`, `fSmokeStepCount`, `fSmokeWindNoiseQuantity`.
  `MainLayout::pfSmokeShadowIntensities` becomes `fSmokeShadowIntensity`. The
  shader constant `kiMaxFlightPlanes` and its comment go; the C++
  `engine::kiMaxFlightPlanes` stays with the table for the last stage, and the
  `static_assert` tying the two (`RenderTargetTextures.cpp:15`) goes.
- Tuning: `gSmokeWindNoiseQuantityBottom` becomes `gSmokeWindNoiseQuantity`
  and `gSmokeShadowIntensityBottom` becomes `gSmokeShadowIntensity`, keeping
  their defaults and ranges; the Top wrappers and their Tweaks sliders go, and
  the remaining sliders are labelled "Smoke Wind Noise Quantity" and "Smoke
  Shadow Intensity". Tweaks values are not persisted. `FlightPlaneLerp` goes.
- Resources: one `mSmokeTextureOne` and one `mSmokeTextureTwo`; the
  sampler-array pointer table `mppSmokeTexturesOne` goes and every smoke
  sampler binding becomes a single `sampler2D` bound through `.pTexture`
  (author's recommendation over a one-entry array). One occupancy buffer pair
  `mpSmokeOccupancyVkBuffers[2]`. One smoke pipeline set:
  `SmokePipeline(eKind)` indexes `kPipelineSmokeFirst + eKind`, and
  `kPipelineSmokeLast` spans `kSmokePipelineKindCount` entries.
- Recording: `RecordSmokeSpreadPipeline` records the two spread halves once,
  with no plane push constant (`RecordSmokeSpreadHalf` loses its
  `rf4PushConstants` parameter; the recorded calls use the default) and no
  between-planes barrier. `RecordSmokeEmit` records the one field; the smoke
  deposit push constant keeps x as `kiAreaSelectorSmoke` and y 0.
- Deposits: Puffs and SmokeTrails keep one deposit buffer, cursor, and
  pipeline each, keyed `DynamicPipelines::FlightPlaneDepositKey(crc, 0)` (its
  removal is the last stage), deposit every source whole, and project with
  `ProjectToBaseHeight`. `ComputeSmokePlaneWeights`, `SmokePlaneWeights`, and
  `ProjectToFlightPlane` (no caller remains) go; `ProjectToBaseHeight` takes
  over its body with the base height.
- Shaders: `FlightPlanePosition` folds into `BaseHeightPosition` reading
  `fBaseHeight`; `SmokeShadow`, `SmokeShadowFaded`, `ReadSmoke`, and
  `SmokeObjectHeightFade` read the one field, the last measuring height above
  `fBaseHeight`. Terrain, Water, and Model drop their plane loops and sample
  the one field at the base-height projection, keeping the object-shadow R
  factor on its base lighting as plane 0 has today; Model drops its
  planes-below skip (`iInLayer` in the smoke loop) and fades the field by the
  model's height above the base height. The smoke compute shaders drop the
  plane push constant; `SmokeSpreadTwo.comp`'s terrain decay drops its
  plane-relative offset (0 for plane 0). The quad vertex shaders' smoke
  selector reads `f4SmokeArea` and ignores push-constant y. Wind shaders read
  the scalar areas. `DebugTexture.vert`'s directional-lighting branch reads
  `fBaseHeight` where it reads `pfFlightPlaneHeights[0]`.
- Agent: `dump_render_target` `SmokeOne` and `SmokeTwo` become single
  textures that take no index, like `WindOne`.

## Critical files

- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`, `LightingUniforms.cpp`,
  `GlobalUniforms.cpp`, `Render.h`
- `Engine/Source/Ui/GraphicsSettings.cpp`, `GraphicsSettingsWrappersBase.h`,
  `Screens/GraphicsMenuScreen.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.{h,cpp}`,
  `BufferManager.{h,cpp}`, `PipelineManager.{h,cpp}`,
  `CommandBufferRecordGlobal.{h,cpp}`, `CommandBufferRecordMain.cpp`
- `Engine/Data/Shaders/ShaderGlobalLayout.h`, `ShaderMainLayout.h`,
  `ShaderFunctions.h`, `Smoke/*.comp`, `Smoke/SmokeSpreadCommon.h`
- `Engine/Source/Frame/Collections/{Puffs,SmokeTrails}/*Render.cpp`

## In scope

- `Engine/Source/Ui/GraphicsSettings.cpp`: `kSmokeLayerStagger` (`:24`),
  `kiVersion` 29 to 30 (`:29`), the save, load, and reset lines (`:125`,
  `:283`, `:324`).
- `Engine/Source/Ui/GraphicsSettingsWrappersBase.h`: `gSmokeLayerStagger`
  (`:39`).
- `Engine/Source/Ui/Screens/GraphicsMenuScreen.cpp`: the "Stagger Smoke Layer
  Updates" toggle (`:206`).
- `Engine/Source/Ui/SmokeWrappersBase.h` (`:22-23`) and
  `Engine/Source/Ui/ShadowWrappersBase.h` (`:37-38`): the Bottom/Top wrappers
  as the Design renames and removes them; the curl-strength comment's
  flight-plane-count and stagger clause (`SmokeWrappersBase.h:24-26`).
- `Engine/Source/Ui/Screens/TweaksScreen/TweaksScreenSmoke.cpp` (`:39-40`)
  and `TweaksScreenShadow.cpp` (`:47-48`): the sliders.
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`: the per-plane publish
  helpers (`:11-61`), the plane-0 extent read and its comment (`:83-85`), the
  curl-offset comment's per-plane wording (`:97-103`), the wind-noise loop
  (`:122-128`), the held area, frame counter, basis shift, and the clear,
  disabled, and schedule exits (`:130-211`), and their comments.
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`: the smoke-shadow
  intensity loop in `RenderLightingMain` (`:319-325`).
- `Engine/Source/Graphics/Render/GlobalUniforms.cpp`: the plane-height loop
  and count (`:627-631`).
- `Engine/Source/Graphics/Render/Render.h`: `FlightPlaneLerp` (`:237-241`).
- `Engine/Source/Graphics/GraphicsUtils.h` (`:35-58`) and `.cpp` (`:79-96`):
  `SmokePlaneWeights`, `ComputeSmokePlaneWeights`, `ProjectToFlightPlane`'s
  declaration, and the comments naming flight-plane projections.
- `Engine/Source/Graphics/FlightPlaneProjection.cpp`: `ProjectToFlightPlane`
  folded into `ProjectToBaseHeight` (`:34`, `:60-74`).
- `Engine/Source/Graphics/Managers/RenderTargetTextures.h` (`:31-34`) and
  `.cpp` (`:15`, `:21-23`, `:210-231`): the smoke textures, names, creation,
  binding table, and clears.
- `Engine/Source/Graphics/Managers/BufferManager.h` (`:64-66`) and `.cpp`
  (`:526-578`): the occupancy buffers.
- `Engine/Source/Graphics/Managers/PipelineManager.h` (`:10-18`, `:43-44`,
  `:88`) and `.cpp` (`:190-192`, `:203-204`, `:207-208`, `:230-300`,
  `:411`): smoke pipeline range, `SmokePipeline`, the smoke-clear comment's
  per-plane wording, creation loop, and smoke descriptors.
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp` (`:266`,
  `:345`) and `Engine/Source/Graphics/Objects/ModelPipeline.cpp` (`:27`): the
  smoke sampler descriptors.
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.h` (`:18`) and
  `.cpp` (`:163-226`): `RecordSmokeSpreadHalf`, `RecordSmokeSpreadPipeline`.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp`:
  `RecordSmokeEmit` (`:211-240`).
- `Engine/Source/Graphics/Screenshot.cpp`: `kpcValidDumpNames`, the `SmokeOne`
  and `SmokeTwo` branches (`:235-242`).
- `Engine/Source/Frame/Collections/Puffs/PuffsRender.cpp` (`:18-26`,
  `:42-45`, `:63-66`, `:86-117`) and
  `SmokeTrails/SmokeTrailsRender.cpp` (`:17-23`, `:43-46`, `:64-67`,
  `:100-161`): per-plane creation, cursors, buffers, weights, projection, and
  counts.
- `Engine/Data/Shaders/ShaderLayoutsBase.h`: shader `kiMaxFlightPlanes` and
  its comment (`:213-215`).
- `Engine/Data/Shaders/ShaderGlobalLayout.h` (`:29-30`, `:77-88`, and the
  `f2SmokeCurlOffsetScale` comment naming `pf4SmokeAreas` at `:96`) and
  `ShaderMainLayout.h` (`:135`).
- `Engine/Data/Shaders/ShaderFunctions.h`: `FlightPlanePosition`,
  `BaseHeightPosition` (`:55-67`), `SmokeShadow`, `SmokeObjectHeightFade`,
  `SmokeShadowFaded`, `ReadSmoke` (`:234-277`) and their comments.
- `Engine/Data/Shaders/Terrain/Terrain.frag` (`:28`, `:158`, `:168-187`),
  `Water/Water.frag` (`:26`, `:182`, `:252-270`), `Model/Model.frag` (`:63`,
  `:389-425`), `Particles/ParticlesRender.frag` (`:19`, `:55`): smoke
  bindings, shadow calls, and plane loops.
- `Engine/Data/Shaders/Smoke/SmokeOccupancyDilateRemap.comp` (`:14`,
  `:58-64`), `SmokeSpreadOne.comp` (`:15`, `:58-71`), `SmokeSpreadTwo.comp`
  (`:15`, `:69-107`), `SmokeSpreadCommon.h` (`:1-43`).
- `Engine/Data/Shaders/Wind/WindOccupancyDilate.comp` (`:45-51`),
  `WindSpreadOne.comp` (`:55-56`), `WindSpreadTwo.comp` (`:56-57`).
- `Engine/Data/Shaders/Quads/QuadsVisibleArea.vert` (`:55-57`) and
  `QuadsAxisAlignedVisibleArea.vert` (`:52-54`): the smoke selector.
- `Engine/Data/Shaders/DebugTexture.vert`: the
  `kiDebugTextureFormatUnormLightingDirectional` branch's height (`:42`).
- Prose this stage makes wrong:
  - `Engine/Source/Graphics/Render/AGENTS.md`: the smoke half of the schedule
    bullet in `## Ordering and Publication` (`:64-80`) and the per-plane smoke
    bullet in `## Precision and History` (`:167-175`).
  - `Engine/Source/Graphics/Managers/AGENTS.md` `:209-229`: per-plane
    occupancy and textures.
  - `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation` `:240-242`:
    `ProjectToFlightPlane`.
  - `Engine/Data/Shaders/Smoke/AGENTS.md` (the overview, `## Plane Controls`,
    and the pass B terrain-decay sentence); `Wind/AGENTS.md` `:43-44`;
    `Quads/AGENTS.md` `:8-12`; `Terrain/AGENTS.md`, `Water/AGENTS.md`, and
    `Model/AGENTS.md` (`:55-62`) where they describe per-plane smoke.
  - `Engine/Source/Frame/Collections/Puffs/AGENTS.md` `:19-20` and
    `SmokeTrails/AGENTS.md` `:3`, `:18-20`.
  - `Documents/UserInterfaceDesign.txt` `:355`: the Stagger toggle.
  - `.agents/skills/agent-harness/references/command-reference.md`
    `presentation_continuity_probe` `:369` ("flight plane 0's area pair").
    Its `dump_render_target` entry (`:257-265`) names no smoke target or
    index, so it needs no edit for `SmokeOne`/`SmokeTwo`.
- Stale non-Plan documents: update
  `Documents/Features/Graphics/SmokeColorBySource.md` `:9-10` and `:81-82`
  (one smoke field, not one per plane), and
  `Documents/Features/Graphics/AmbientWindAroundIslands.md` `:94-98` (drop
  the nonexistent `Plans/Engine/SmokePerFlightPlane.md` reference and the
  per-plane decay and deflection reasoning).

## Out of scope

- World lighting; object-shadow channels and `ObjectShadowAbove`;
  `f4PositionLayer` and `FillModelLayout`'s plane argument.
- `engine::FlightPlanes`, `game::kFlightPlanes`, C++ `kiMaxFlightPlanes`,
  `FlightPlaneDepositKey`'s removal, and the `FlightPlaneProjection.cpp` file
  name.
- The Smoke Update Cadence and Lighting Update Cadence sliders, smoke tuning
  defaults, the smoke gradient, the wind field and its own cadence, the
  sparse-dispatch scheme.
- Gameplay code. Unit tests.

## Risk triggers and invariants

Tier 3: the `GraphicsSettings.bin` format changes, the CPU/GPU layouts
change, and the change spans Graphics, Ui, engine collection renderers, and
shaders.

- Smoke outputs still converge to exact zero; pass B keeps the extinction
  terms only it has.
- Occupancy keeps describing persistent texture contents; replacing smoke
  hierarchy storage still re-arms the one-frame clear.
- Smoke publishes before wind within Global, and wind's remap stays the
  identity on frames smoke skips.
- Paused smoke still skips cadence and teleport refreshes while held areas
  follow camera-cell shifts.
- Render stays outside the CRC.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and the ModelViewer,
   including their shader data.
2. `GraphicsSettings::kiVersion` is 30. No `kSmokeLayerStagger`,
   `gSmokeLayerStagger`, `ComputeSmokePlaneWeights`, `SmokePlaneWeights`,
   `FlightPlaneLerp`, `ProjectToFlightPlane`, `pfFlightPlaneHeights`,
   `uiFlightPlaneCount`, `FlightPlanePosition`, `pf4SmokeAreas`, or
   `gSmoke*Top`/`gSmoke*Bottom` identifier remains under `Engine/` or
   `Projects/`, and no shader declares `kiMaxFlightPlanes`.
3. `/agent-harness` client `describe_ui` on the graphics settings screen lists
   the Smoke Update Cadence and Lighting Update Cadence sliders and no
   "Stagger Smoke Layer Updates" item; with Tweaks shown, the smoke and shadow
   sections list "Smoke Wind Noise Quantity" and "Smoke Shadow Intensity" and
   no Bottom or Top slider.
4. `dump_render_target` `{"name":"SmokeOne"}` and `{"name":"SmokeTwo"}`
   succeed, and the valid-name list in an unknown-name error shows them
   without an index range.
5. `presentation_continuity_probe` still reports `smoke` with both areas.
6. With `pin_graphics`, a `screenshot` of combat with missiles or explosions
   shows smoke trails and puffs and their shadow on water.
