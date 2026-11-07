<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T13:48:41.020Z","dependsOn":[]} -->
# Engine export, render, and viewer cleanups

## Context

An audit of commits cfd6116e, 13e4ae19, and 5c2e6a8d found the cleanups below in
Engine, Common, DataPacker, ModelViewer, shader, and UI code, each proven by
reading. They are pre-existing relative to that session's documentation change.

Line numbers are from 5c2e6a8d. The item labels are local to this Plan.

## Design

Each item is independent. Apply them in one change, re-reading each proof at its
site first, and drop any item whose proof no longer holds, reporting it. Two items
change what a user sees: R07 accepts repeated graph indices and shortens its errors,
and R10 removes two sliders from the Smoke section. The rest preserve behavior.

Names below are the author's recommendations.

## Critical files

- `DataPacker/Source/ExportJobs/ExportModel.cpp`, `ExportModel.h`
- `Common/DataFile.h`, `Common/ModelMetadata.h`
- `Engine/Source/Graphics/Render/LightingUniforms.cpp`, `SmokeUniforms.cpp`, `Render.h`
- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`
- `Engine/Data/Shaders/ShaderMainLayout.h`, `Terrain/Terrain.frag`, `Water/Water.frag`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`, `.h`, `Main.cpp`

## In scope

- R03 Export offsets. In `ExportModel::Export`
  (`DataPacker/Source/ExportJobs/ExportModel.cpp`), delete the
  `:38-41` guard: a hardpoint vector cannot exceed `uint32_t` entries, and
  `DataPacker/Source/AGENTS.md` limits representability checks to destinations that
  can be exceeded. Replace `:213-221` with
  `common::ModelHeader::FighterLaunchBaysOffset` plus `common::kiChunkDataOffset` and
  a plain size, and merge the finite and zero-direction checks at `:143-151`, which
  throw the same message. Keep the `memcpy` empty checks and the checks themselves,
  which `DataPacker/Source/ExportJobs/AGENTS.md` requires.
- R04 Hardpoint record. `PackedModelMissileTube` and `PackedModelFighterLaunchBay`
  (`Common/DataFile.h:384-402`) are the same struct with the same assertions.
  Merge them into `PackedModelHardpoint`, remove the conversion at
  `ExportModel.cpp:158`, drop one `sizeof` from the `ExportModel::kiVersion` fold
  (`ExportModel.h:14`), and update the readers in
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`
  and `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`.
- R05 Paused render gates. Six `!gbGameplayPaused` conjuncts in `EndRender` are dead:
  `BeginRender` zeroes the counters on both `RenderFrameMain` paths and `Render`
  returns before incrementing them. Sites: `PuffsRender.cpp:129`,
  `SmokeTrailsRender.cpp:173`, `WindRadialsRender.cpp:99-100`,
  `WindTrailsRender.cpp:159-160`, under `Engine/Source/Frame/Collections/`. Drop
  them, and merge the early returns at `PuffsRender.cpp:61`,
  `SmokeTrailsRender.cpp:62`, and `WindRadialsRender.cpp:55` into the following
  condition, as `WindTrailsRender.cpp:62` already does.
- R06 Skybox sample offsets. In `LightingUniforms.cpp:294-315`, write
  `rMainLayout.pf2LightingWaterSkyboxOneSampleOffsets[i]` directly, dropping the
  temporary array, its zero-fill, the copy loop, and two single-use locals.
  `WaterSpecular.h` reads only count-minus-one entries. Moving the angle step into
  the loop removes the `iSampleCount > 1` guard. Name that array's extent once in
  `ShaderMainLayout.h`, as `shaders::kiMaxFlightPlanes` names its own.
- R07 Probe validation. In `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp`,
  remove the unsigned range clause at `:77`, dead because `:82` rejects the negative
  value it converts to, remove the distinctness check, and collapse the graphs
  checks to one error. Root `AGENTS.md` exempts the agent command channel. Update
  the `graphs` text in
  `.agents/skills/agent-harness/references/command-reference.md`.
- R08 Pusher duplicate check. In `PushersUpdate.cpp:161-170`,
  `engine::FlightPlanes::OccupiedPlanes` returns one layer or two adjacent ones, so
  only `iLayerStart` can be earlier.
  Test `iLayer != iLayerStart` plus one `std::binary_search` on `iLayerStart`'s
  zone, removing the iota, the lambda, and the flag.
- R09 Plane lerp. The normalized plane-index lerp at `LightingUniforms.cpp:395` and
  `SmokeUniforms.cpp:120` becomes one `FlightPlaneLerp(fBottom, fTop, i, iPlaneCount)`
  in `Engine/Source/Graphics/Render/Render.h`.
- R10 Duplicate smoke sliders. `TweaksScreenSmoke.cpp:37-39` repeats the two smoke
  shadow sliders that `TweaksScreenShadow.cpp` also draws. Delete the Smoke copies,
  their separator, and the `Ui/ShadowWrappersBase.h` include of
  `TweaksScreenSmoke.cpp` that only they need.
- R11 Hardpoint kind clause. Remove `!rKind.is_string()` at `Common/ModelMetadata.h:101`:
  a non-string value never equals a name, so `std::find` already returns end.
- R12 Smoke shadow read. `f4SmokeObjectShadows` is read only through `.x`
  (`Terrain/Terrain.frag:172,187`, `Water/Water.frag:237,251`). Read one float.
- R13 Shadow bounds argument. `ComputeModelShadowBounds`
  (`Engine/Source/Graphics/Render/Render.h:22`) takes the `EagerChunk`, replacing
  the cast and `OutlineOffset` expression in `BattleshipsRender.cpp:339`,
  `FightersRender.cpp:136`, `MissilesRender.cpp:81`, `OmnisRender.cpp:152`, and
  `ModelViewerApplication.cpp:766`.
- R14 Small items:
  - `Engine/Source/Frame/Collision.cpp:555-556`: delete the comment restating
    `Collision.h:47`.
  - `ModelViewerApplication.cpp:54,58`: merge the two guards in `SelectModel`.
  - `ModelViewerApplication.h:29`: make `miSelected` private with a relative-select
    method for `Projects/ModelViewer/Source/Main.cpp:55`.
  - `TweaksScreen.cpp:174`: drop the clamp; `SliderInt` with `ImGuiSliderFlags_NoInput`
    keeps the index in range.
  - `Engine/Source/Frame/FrameUtils.h:168`: use `const ARGS&...` in
    `ForEachInterpolateUpdate`. Its two callers pass nothing or one lvalue.
  - `Render.h:15`: remove the stray blank line.
  - `Engine/Data/Shaders/ShaderMainLayout.h`: rename the
    `fPhysicallyBasedRendering...` fields to the `Pbr`, `Brdf`, `Ibl`, and `Lod`
    forms the `gPbr...` wrappers use, which style guide rule 56 approves, with their
    writers, shader readers, and `Documents/Features/Graphics/SkyboxRenderPass.md`.

## Out of scope

- The inline `Wrapper` lines in `ShadowWrappersBase.h` and `SmokeWrappersBase.h`:
  `Documents/Plans/Engine/InlineUiWrapperInitialization.md` makes inline the target
  form, so they stay.
- Removing the `cell_coordinate_probe` navigation feature, which is the user's call.
- Tweaks slider identity: each `TweaksScreen::WrapperSlider` call passes its
  wrapper and its label is its only name
  (`Engine/Source/Ui/Screens/TweaksScreen/TweaksScreen.h`).
- The hardpoint lookup in game code beyond the R04 rename:
  `Documents/Plans/Game/ShipCombatAndFleetCleanups.md`.
- Any change to packed bytes, layouts, or a uniform's offset or value.

## Risk triggers and invariants

- Tier 3. Trigger: a change spanning independently owned subsystems: DataPacker,
  Common, Engine frame and render, shaders, UI, ModelViewer, and game call sites.
- `.pack`: R03 and R04 keep every record's bytes, so `DataHeader::kiVersion` stays.
  The fold in `ExportModel::kiVersion` changes value, so cached model chunks
  re-export; keep the merged struct's `sizeof` in the fold.
- Shader layout: R06 and the R14 rename keep offsets. `ShaderMainLayout.h` is a
  dual-language header, so C++ and GLSL stay in step, and both builds and
  ModelViewer compile it.
- Determinism: R08 runs in Pushers Update, in fixed-tick code. It must keep the same
  pairs in the same order, so PostRender stays bit-identical with no version bump.
- R05 and R12 are render-only and stay out of the CRC. R07 is the exempt agent
  channel.

## Acceptance criteria

- `/compile` builds DataPacker, ModelViewer, and the BrokenEngineSandbox client and
  server, and data re-exports.
- `/agent-harness` replay determinism check passes across R08.
- A live `/agent-harness` run with a screenshot of water and terrain in the game and
  of a model in ModelViewer shows no visual change, and pausing keeps Puffs, Smoke
  Trails, and Wind Radials frozen with no validation error. The audit read source
  only: the supplied evidence records no harness run or setup.
- `cell_coordinate_probe` with a valid navigation request answers as before.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

R04 and G01 of `Documents/Plans/Game/ShipCombatAndFleetCleanups.md` touch the same
hardpoint reads. Either order works.
