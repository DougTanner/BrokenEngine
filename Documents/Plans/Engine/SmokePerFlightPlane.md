<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T00:03:09.308Z","dependsOn":["Documents/Plans/Engine/FlightPlaneData.md"]} -->
# One smoke field per flight plane, with between-plane deposits and eye-ray compositing

Tier 2: client-only render behaviour in one subsystem (Graphics plus the two smoke-depositing engine collections and the compositing shaders). Smoke is not CRC'd, no wire, save, replay, or pack format changes, and no threading or trust surface moves. Line numbers cite baseline `c1420821`. Where a statement here and the code disagree, the code wins; report the contradiction instead of matching one side to the other. Flight-plane series; `Documents/Plans/Engine/FlightPlaneData.md` (prerequisite) supplies `engine::FlightPlane`, `engine::FlightPlanes`, and `engine::gpFlightPlanes` with `Count()`, `Plane(i)`, `Lowest()`, `Highest()`, and the island-ceiling guarantee that every heightmap texel lies strictly below `Highest().fHeightMeters`.

## Context

Smoke today is one camera-following 2D density field at the lowest flight plane (`Engine/Data/Shaders/Smoke/AGENTS.md`).

- Storage: two `R16_SFLOAT` ping-pong textures `SmokeOne`/`SmokeTwo` (`shaders::kVkFormatSmoke`, `ShaderLayoutsBase.h:34`; `RenderTargetTextures::CreateSmokeTextures`, `RenderTargetTextures.cpp:203-224`, `.h:28-29`) sized by `SmokeSimulationPixels()`/`SmokeSimulationPixelsY()` (`Graphics.cpp:68-80`; `kfSmokeReferencePixels` 8192 at the 3840 reference width times `gSmokeSimulationPixels`). Wind is a separate `R16G16` pair at the same resolution (`RenderTargetTextures.cpp:238-266`).
- Sparse dispatch: two bit-packed occupancy buffers and one active-tile list (`BufferManager::CreateSmokeHierarchicalBuffers`, `BufferManager.cpp:573-618`, `.h:65-73`); pipelines `kPipelineSmokeClearA/B`, `kPipelineSmokeSpreadComputeA/B`, `kPipelineSmokeOccupancyDilate/DilateRemap` (`PipelineManager.h:30-35`, created in `PipelineManager::CreateSmokeWindPipelines`, `PipelineManager.cpp:198-295`). `Pipeline::Destroy` is a no-op on a never-created pipeline (`Pipeline.cpp:128-133`).
- Per-frame publication: `RenderSmokeGlobal` (`SmokeUniforms.cpp:56-203`) fills the smoke uniforms, the camera-relative `f4SmokeArea`/`f4PreviousSmokeArea` pair (`:87-92`), the cadence, clear, and skip exits, and writes the clear and dilate indirect commands (`WriteSmokeDilateDispatch`, `:41-45`; clears `:158-159,175-176,181-182`).
- Global recording: `CommandBufferRecordGlobal::RecordSmokeSpreadPipeline` (`CommandBufferRecordGlobal.cpp:310-349`) runs `RecordSmokeSpreadHalf` (`:246-307`) for B then A inside `kGpuTimerSmokeSpread`; pass B (`SmokeSpreadTwo.comp`) samples wind through `SmokeWindSample`, applies the extra decay over terrain (`:66-71`), the edge decay, and the exact-zero walk; pass A (`SmokeSpreadOne.comp`) performs the single previous-to-current area remap.
- Main recording: `CommandBufferRecordMain::RecordSmokeEmit` (`CommandBufferRecordMain.cpp:216-238`) clears `SmokeTwo`, then opens a render pass on `SmokeOne`, clears it when flagged, and draws every `kDynamicPipelineSmokeAxisAligned` and `kDynamicPipelineSmoke` deposit pipeline with push constant `{2,0,0,0}` (selects `f4SmokeArea` in `QuadsVisibleArea.vert:44-62` and `QuadsAxisAlignedVisibleArea.vert:44-54`). Deposit pipelines are additive (`PipelineFlags::kAdd`) targets on `SmokeOne` created by `DynamicPipelines::CreateDepositPipeline` (`DynamicPipelines.cpp:239-268`), keyed by the collection CRC, each with its own dynamic storage buffer. `Smoke.frag:40-45` marks the 8x8 occupancy tile of every nonzero deposit.
- Depositors: `SmokeTrailsInterpolate::Render` (`SmokeTrailsRender.cpp:82-83`) and `PuffsInterpolate::Render` (`PuffsRender.cpp:73`) project every vertex with `ProjectToBaseHeight` (`GraphicsUtils.cpp:104-110`), which calls `common::ToBaseHeight` (`Common/Math/MathUtils.cpp:6-11`) toward `gpCamera->mVecEyePosition` onto `max(terrain elevation, lowest plane)`. Pipelines are created at `SmokeTrailsRender.cpp:21` and `PuffsRender.cpp:17`; indirect counts are written at `:139` and `:95`. Wind depositors (`WindTrails`, `WindRadials`) also project to base and are not smoke.
- Compositing: `BaseHeightPosition` (`ShaderFunctions.h:50-56`) intersects the fragment-to-eye ray with the lowest plane, clamped to the fragment itself when the fragment is above it. `Terrain.frag:124,155-160` and `Water.frag:168,221-226` sample smoke there and blend with `BlendSmokePrecomputed`; `Model.frag:374-377` uses `AddSmoke` with a fade over `gSmokeObjectHeight` (10 m, `SmokeWrappersBase.cpp:35`, `fSmokeObjectHeightInverse`). `SmokeShadow` (`ShaderFunctions.h:224-230`) is a vertical, sun-independent attenuation at the fragment XY used by `Terrain.frag:144`, `Water.frag:150`, and `ParticlesRender.frag:55-57` (with the same height fade). Smoke sampler bindings: `WorldLightingShadowPipelines.cpp:302` (terrain), `:366` (water), `ModelPipeline.cpp:40-41`, `PipelineManager.cpp:406` (particles render).
- Sampler arrays with a dynamically uniform index are already in use (`Water.frag:41`, `WaterNormalSampling.h:68`), bound through `DescriptorInfo{.iCount, .ppTextures}` (`PipelineDescriptorWriter.cpp:240-271`; a null view in `ppTextures[k]` falls back to `ppTextures[0]`).
- Capture: `dump_render_target` resolves `SmokeOne`/`SmokeTwo` (`Screenshot.cpp:236-243`) and already accepts an `index` argument (`:38` in `commands-client.md`). Continuity: `gPresentationContinuity.smoke` reports the one shared area pair (`Render/AGENTS.md:26`).
- Command buffers are recorded once (`Graphics/AGENTS.md:8`), so the plane count must be fixed at texture creation. The device does not enable `multiview`, `geometryShader`, or `shaderOutputLayer` (`DeviceManager.cpp:188-215`), so layered rendering is not available without a new required feature.
- Harness: missiles that are not `kFalling` have their Z velocity zeroed every tick (`MissilesUpdate.cpp:209`) and keep their position Z, so an `edit_frame` write of a missile's `pVecPositions` Z holds while it flies and trails smoke; `commands-server.md` documents `edit_frame` and `inject_payload`.

## Decisions

Each decision is recorded as made; the alternatives are not open.

1. **One field per flight plane, N separate texture pairs.** `RenderTargetTextures` holds `std::array<Texture, shaders::kiMaxFlightPlanes> mSmokeTexturesOne, mSmokeTexturesTwo`; `CreateSmokeTextures` creates the first `gpFlightPlanes->Count()` of each (names `SmokeOne0`.., `SmokeTwo0`..) with the existing `TextureInfo`, clears them all, and asserts `Count() <= kiMaxFlightPlanes`. A 2D array image was rejected: `Texture` builds one view and one render pass over all layers, so rendering into layer i would need per-layer views and framebuffers, or layered rendering, which needs a device feature not required today. Separate textures reuse every existing path unchanged.
2. **`shaders::kiMaxFlightPlanes = 8`** in `ShaderLayoutsBase.h` with `static_assert(shaders::kiMaxFlightPlanes == engine::kiMaxFlightPlanes)` in `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp`. Shader arrays are sized by it; the live count comes from `gpFlightPlanes`.
3. **Per-plane occupancy and active-tile buffers.** `BufferManager` gains `mSmokeOccupancyVkBuffers[kiMaxFlightPlanes][2]`, matching allocations and `mSmokeActiveTileVkBuffers[kiMaxFlightPlanes]`, created for `Count()` planes. A packed (plane, tile) list was rejected: every spread pipeline binds one input and one output texture through descriptors, so planes are separate dispatches anyway, and separate lists keep each dilate and spread shader byte-identical to today apart from the plane-height uniform read.
4. **Per-plane pipelines in the flat enum.** `Pipelines` replaces the six smoke enumerators with `kPipelineSmokeFirst` and `kPipelineSmokeLast = kPipelineSmokeFirst + kiSmokePipelineKindCount * shaders::kiMaxFlightPlanes - 1`, where `enum SmokePipelineKind : int64_t { kSmokeClearA, kSmokeClearB, kSmokeSpreadComputeA, kSmokeSpreadComputeB, kSmokeOccupancyDilate, kSmokeOccupancyDilateRemap, kiSmokePipelineKindCount }` and `PipelineManager::SmokePipeline(int64_t iPlane, SmokePipelineKind eKind)` returns `mpPipelines[kPipelineSmokeFirst + iPlane * kiSmokePipelineKindCount + eKind]`. `CreateSmokeWindPipelines` loops planes `0..Count()-1`; slots for absent planes are never created and `Pipeline::Destroy` ignores them.
5. **Deposits: one draw per (collection, plane).** `DynamicPipelines::CreateDepositPipeline` gains an `int64_t iPlane` parameter; the map key and the dynamic buffer key become `common::Crc(std::span<const int64_t>(std::array<int64_t, 2> {static_cast<int64_t>(crc), iPlane}))` computed by a `SmokeDepositKey(crc, iPlane)` helper in `DynamicPipelines`. `SmokeTrails` and `Puffs` create `Count()` pipelines each, target plane i's `SmokeTexturesOne[i]` and occupancy `[i][0]`, write one quad per deposited plane into that plane's buffer, and write every plane's indirect count in `EndRender`. `RecordSmokeEmit` loops planes: clear `Two[i]`, open `One[i]`, clear when flagged, draw the plane's pipelines. The same `kGpuTimerSmokeEmit` wraps the whole loop.
6. **Weights (user rule).** `engine::ComputeSmokePlaneWeights(float fZ)` in `GraphicsUtils.h/.cpp` returns `SmokePlaneWeights {int64_t iLowerPlane; float fUpperWeight;}`: with heights `h_0 < ... < h_{n-1}`, `fZ < h_0` gives `{0, 0}`, `fZ >= h_{n-1}` gives `{n - 1, 0}`, otherwise `i` with `h_i <= fZ < h_{i+1}` and `w = (fZ - h_i) / (h_{i+1} - h_i)`. Plane `i` receives `1 - w`, plane `i + 1` receives `w`; a plane whose weight is 0 receives no quad. The weight multiplies the deposit intensity (`f4Parameters.x` for puffs; `fQuantity` in `pf4Parameters[].x` for trails), so the total is constant. A trail uses the current position's Z for both endpoints.
7. **Projection per plane.** `ProjectToFlightPlane(XMVECTOR vecLocalPosition, const RenderBasis& rBasis, int64_t iPlane)` projects toward the eye onto `max(terrain elevation, Plane(iPlane).fHeightMeters)`; `ProjectToBaseHeight` becomes `ProjectToFlightPlane(.., 0)` so lights and wind are unchanged. The clamp applies to every plane: for the lowest plane it is today's behaviour, and for the highest it is a no-op because the island ceiling is below `Highest()`. No `kTerrainCollision` read.
8. **Terrain decay per plane.** `SmokeSpreadTwo.comp` reads its plane from push constant `f4Pipeline.x` and measures elevation relative to the plane: `fRelative = fElevation - (pfFlightPlaneHeights[iPlane] - pfFlightPlaneHeights[0])`; the condition becomes `fRelative > 0` and the exponent `max(1, fRelative - 5)`. For plane 0 both are bit-identical to today; for the highest plane the condition never fires. One rule, no flag, no behaviour change on the low plane.
9. **Shared uniforms.** All planes share `f4SmokeArea`, `f4PreviousSmokeArea`, cadence, step count, decay, tile counts, and the clear flag; `GlobalLayout` gains `float pfFlightPlaneHeights[kiMaxFlightPlanes]` (scalar stride) and `uint32_t uiFlightPlaneCount`, filled by `RenderGlobal` beside `fBaseHeight` (`GlobalUniforms.cpp:484-486`). Wind stays one field: `SmokeWindSample` and the wind pipelines are untouched.
10. **Compositing, surfaces (terrain, water).** `FlightPlanePosition(globalLayout, mainLayout, f3InPosition, iPlane)` generalises `BaseHeightPosition` with `fMult = (h_i - z) / toEye.z` clamped at 0; `BaseHeightPosition` calls it with plane 0. Each surface fragment loops `i = 0..uiFlightPlaneCount-1` in ascending order (nearest the eye last), samples `pSmokeSamplers[i]` at that plane's position, and applies `BlendSmokePrecomputed` per plane with the one ambient sum already fetched at the lowest-plane position (lighting falloff is out of scope). The clamp keeps a surface above a plane reading that plane at its own XY, which matches decision 7's deposit clamp.
11. **Compositing, models and particles.** `Model.frag` loops planes with per-plane fade `1 - clamp((z - h_i) * fSmokeObjectHeightInverse, 0, 1)^2` and `AddSmoke` at the plane's eye-ray position: planes above the fragment get full weight, the nearest plane below keeps today's 10 m fade, lower planes fade to zero. `ParticlesRender.frag` uses the same fade per plane inside the shadow product.
12. **`SmokeShadow` = product.** The helper takes the sampler array and multiplies `1 - fMulti_i * attenuation_i` over every plane at the fragment XY, still vertical and sun-independent; terrain and water pass the same `fMulti` for every plane, particles pass the per-plane fade.
13. **Sampler arrays, one binding.** Every smoke consumer binding becomes `sampler2D pSmokeSamplers[kiMaxFlightPlanes]` bound with `{.iCount = kiMaxFlightPlanes, .ppTextures = mppSmokeTexturesOne}` where `RenderTargetTextures::mppSmokeTexturesOne` is a `Texture*[kiMaxFlightPlanes]` whose slots at or beyond `Count()` point at plane 0 (never sampled). Indexing uses the dynamically uniform loop counter as `WaterNormalSampling.h` does.
14. **Capture.** `dump_render_target` names `SmokeOne`/`SmokeTwo` take `index` as the plane, bounds-checked against `Count()`. The continuity probe is unchanged: all planes share the one area pair.
15. **Costs, recorded.** Memory at the 3840 reference width and `gSmokeSimulationPixels` 1.0: 8192 x 4608 x 2 bytes = 75.5 MB per texture, 151 MB per plane pair, 453 MB for the game's three planes (today 151 MB); occupancy and active-tile buffers add under 3 MB per plane. GPU: an empty plane costs its two dilate dispatches over every tile plus the clears each refresh; spread stays proportional to occupied coverage per plane; terrain, water, and model fragments take `Count()` smoke samples and `Count()` shadow samples instead of one each.
16. **No backward compatibility.** `mSmokeTextureOne`/`Two`, the single occupancy pair, the six smoke enumerators, and `BaseHeightPosition`'s private use at smoke sites are replaced, not kept beside the arrays.

## Design

Header-style declarations; names are exact.

`Engine/Data/Shaders/ShaderLayoutsBase.h`: `CONSTEXPR int kiMaxFlightPlanes = 8;` beside `kiComputeTileSize`.

`Engine/Data/Shaders/ShaderGlobalLayout.h`, after `fBaseHeightInverse`:

```cpp
float pfFlightPlaneHeights[kiMaxFlightPlanes] INIT; // Ascending flight-plane heights in meters; [0] == fBaseHeight (smoke per-plane projection and terrain decay)
uint32_t uiFlightPlaneCount INIT;                   // Live plane count, at most kiMaxFlightPlanes
```

`Engine/Data/Shaders/ShaderFunctions.h`:

```glsl
vec2 FlightPlanePosition(GlobalLayout globalLayout, MainLayout mainLayout, vec3 f3InPosition, int iPlane);  // BaseHeightPosition with plane i's height
vec2 BaseHeightPosition(GlobalLayout globalLayout, MainLayout mainLayout, vec3 f3InPosition);              // FlightPlanePosition(.., 0)
float SmokeShadow(GlobalLayout globalLayout, vec3 f3InPosition, sampler2D pSmokeSamplers[kiMaxFlightPlanes], float fMulti);          // product over planes
float SmokeShadowFaded(GlobalLayout globalLayout, vec3 f3InPosition, sampler2D pSmokeSamplers[kiMaxFlightPlanes], float fMulti);     // product with decision 11's per-plane fade (ParticlesRender.frag)
```

`AddSmoke` and `BlendSmokePrecomputed` keep their single-sampler signatures and are called once per plane by the loops in `Model.frag`, `Terrain.frag`, and `Water.frag`.

`Engine/Source/Graphics/GraphicsUtils.h/.cpp`:

```cpp
struct SmokePlaneWeights
{
	int64_t iLowerPlane = 0;
	float fUpperWeight = 0.0f;  // Plane iLowerPlane + 1 receives this; iLowerPlane receives 1 - fUpperWeight
};
SmokePlaneWeights ComputeSmokePlaneWeights(float fZ);
XMVECTOR ProjectToFlightPlane(XMVECTOR vecLocalPosition, const RenderBasis& rBasis, int64_t iPlane);
XMVECTOR ProjectToBaseHeight(XMVECTOR vecLocalPosition, const RenderBasis& rBasis);  // ProjectToFlightPlane(.., 0)
```

`Engine/Source/Graphics/Managers/RenderTargetTextures.h`:

```cpp
std::array<Texture, shaders::kiMaxFlightPlanes> mSmokeTexturesOne;
std::array<Texture, shaders::kiMaxFlightPlanes> mSmokeTexturesTwo;
Texture* mppSmokeTexturesOne[shaders::kiMaxFlightPlanes] {};  // Sampler-array binding; slots >= plane count alias plane 0
```

`Engine/Source/Graphics/Managers/BufferManager.h`:

```cpp
VkBuffer mSmokeOccupancyVkBuffers[shaders::kiMaxFlightPlanes][2] {};
VmaAllocation mSmokeOccupancyVmaAllocations[shaders::kiMaxFlightPlanes][2] {};
VkBuffer mSmokeActiveTileVkBuffers[shaders::kiMaxFlightPlanes] {};
VmaAllocation mSmokeActiveTileVmaAllocations[shaders::kiMaxFlightPlanes] {};
```

`Engine/Source/Graphics/Managers/PipelineManager.h`: decision 4's `SmokePipelineKind`, the `kPipelineSmokeFirst`/`kPipelineSmokeLast` range, and `Pipeline& SmokePipeline(int64_t iPlane, SmokePipelineKind eKind)`.

`Engine/Source/Graphics/Managers/DynamicPipelines.h`: `static common::crc_t SmokeDepositKey(common::crc_t crc, int64_t iPlane);` and the extra `int64_t iPlane` parameter on `CreateDepositPipeline`, which targets `mSmokeTexturesOne[iPlane]` and `mSmokeOccupancyVkBuffers[iPlane][0]` itself, so callers pass neither.

**Recording.** `RecordSmokeSpreadPipeline` loops planes inside the existing timer, calling `RecordSmokeSpreadHalf` with plane i's pipelines, occupancy buffers, active-tile buffer, and textures; `RecordSmokeSpreadHalf` takes the active-tile buffer as a parameter and passes `{float(iPlane), 0, 0, 0}` as the spread pipeline's push constant, so `RecordComputeIndirectFrom` gains a `const XMFLOAT4& rf4PushConstants = {}` parameter mirroring `RecordComputeIndirect` and the two spread pipelines gain `PipelineFlags::kPushConstants`. `RecordSmokeEmit` loops planes per decision 5. `RenderSmokeGlobal` writes every plane's clear and dilate indirect commands at each of its four exits through plane loops in `WriteSmokeDilateDispatch` and a new `WriteSmokeClears(iCommandBuffer, iInstanceCount)`.

**Depositors.** `SmokeTrailsInterpolate::Render`: compute `ComputeSmokePlaneWeights(XMVectorGetZ(Rebase(rBasis, vecLocalPosition)))` once per trail, then for each plane with nonzero weight project both endpoints with `ProjectToFlightPlane(.., iPlane)`, build the ribbon exactly as today, scale `fQuantity` by the weight, and append to that plane's buffer; `siRendered` becomes a per-plane array that `EndRender` writes per plane. `PuffsInterpolate::Render` does the same with `f4Parameters.x` scaled (`f4Parameters.y`, the falloff input, stays the raw intensity).

**Shaders.** `Smoke.frag` is unchanged. `SmokeSpreadTwo.comp` adds the push-constant block and decision 8. `SmokeSpreadOne.comp`, both dilate shaders, and the wind shaders are unchanged. `Terrain.frag`, `Water.frag`, `Model.frag`, and `ParticlesRender.frag` replace `smokeSampler` with `pSmokeSamplers[kiMaxFlightPlanes]` at the same binding and apply decisions 10 to 12.

## Critical files

- `Engine/Data/Shaders/ShaderLayoutsBase.h`, `ShaderGlobalLayout.h`, `ShaderFunctions.h`
- `Engine/Data/Shaders/Smoke/SmokeSpreadTwo.comp`, `Terrain/Terrain.frag`, `Water/Water.frag`, `Model/Model.frag`, `Particles/ParticlesRender.frag`
- `Engine/Source/Graphics/GraphicsUtils.h`, `GraphicsUtils.cpp`, `Screenshot.cpp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.h/.cpp`, `BufferManager.h/.cpp`, `PipelineManager.h/.cpp`, `DynamicPipelines.h/.cpp`, `CommandBufferRecordGlobal.cpp`, `CommandBufferRecordMain.cpp`, `WorldLightingShadowPipelines.cpp`
- `Engine/Source/Graphics/Objects/ModelPipeline.cpp`, `Pipeline.h/.cpp`
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp`, `GlobalUniforms.cpp`
- `Engine/Source/Frame/Collections/SmokeTrails/SmokeTrailsRender.cpp`, `Puffs/PuffsRender.cpp`

## In scope

- `ShaderLayoutsBase.h`: `kiMaxFlightPlanes`. `ShaderGlobalLayout.h`: `pfFlightPlaneHeights`, `uiFlightPlaneCount`. `ShaderFunctions.h`: `FlightPlanePosition`, `BaseHeightPosition`, `SmokeShadow`, `SmokeShadowFaded`.
- `SmokeSpreadTwo.comp`: push-constant block and the terrain-decay block (`:66-71`). `Terrain.frag`: binding 9 declaration, the `SmokeShadow` call (`:144`), the smoke sample and blend (`:154-160`). `Water.frag`: binding 10, `:150`, `:220-226`. `Model.frag`: binding 11, the `ENABLE_SMOKE` block (`:372-378`). `ParticlesRender.frag`: binding 3, `:55-57`.
- `GraphicsUtils`: `SmokePlaneWeights`, `ComputeSmokePlaneWeights`, `ProjectToFlightPlane`, `ProjectToBaseHeight` (`GraphicsUtils.cpp:104-110`).
- `RenderTargetTextures`: the two texture members, `mppSmokeTexturesOne`, `CreateSmokeTextures` (`:160-235`), the `static_assert` of decision 2, and the matching destroy path.
- `BufferManager`: the four array members, `CreateSmokeHierarchicalBuffers` (`:573-618`), `DestroySmokeHierarchicalBuffers`.
- `PipelineManager.h`: the `Pipelines` enum smoke range, `SmokePipelineKind`, `SmokePipeline`. `PipelineManager.cpp`: `CreateSmokeWindPipelines` smoke section (`:198-295`), the particles render binding (`:406`).
- `DynamicPipelines`: `SmokeDepositKey`, `CreateDepositPipeline` (`DynamicPipelines.cpp:239-268`) and its declaration.
- `Pipeline`: `RecordComputeIndirectFrom` push-constant parameter (`Pipeline.cpp:300-308`, `.h:151`).
- `CommandBufferRecordGlobal.cpp`: `RecordSmokeSpreadHalf` (`:246-307`), `RecordSmokeSpreadPipeline` (`:310-349`). `CommandBufferRecordMain.cpp`: `RecordSmokeEmit` (`:216-238`).
- `WorldLightingShadowPipelines.cpp:302,366` and `ModelPipeline.cpp:40-41`: the smoke descriptor becomes the sampler array.
- `SmokeUniforms.cpp`: `WriteSmokeDilateDispatch` (`:41-45`), the new `WriteSmokeClears`, every clear and dilate write in `RenderSmokeGlobal` (`:158-160,175-177,181-182,196,202`), and the tile-count reads at `:74-77` (plane 0's textures). `GlobalUniforms.cpp:484-486`: fill `pfFlightPlaneHeights` and `uiFlightPlaneCount`.
- `SmokeTrailsRender.cpp`: pipeline creation (`:21`), the descriptor update (`:41`), `Render` (`:60-130`), `EndRender` (`:139`). `PuffsRender.cpp`: `:17`, `:40`, `Render` (`:55-85`), `EndRender` (`:95`).
- `Screenshot.cpp`: `ResolveRenderTarget` for `SmokeOne`/`SmokeTwo` (`:236-243`) with the plane index bound, and the `kpcValidDumpNames` text (`:211`).
- Vcxproj membership is unchanged (no new files). The `AGENTS.md` lines `/update-claude-docs` finds stale, at least `Engine/Data/Shaders/Smoke/AGENTS.md` (one field per flight plane, between-plane weights, per-plane terrain decay), `Engine/Source/Graphics/Render/AGENTS.md:26-28` (shared area pair across planes), `Engine/Source/Frame/Collections/SmokeTrails/AGENTS.md` and `Puffs/AGENTS.md` (project to the weighted planes, not "base height"), and `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md` (`dump_render_target` smoke index).

## Out of scope

- Per-plane wind: one wind field feeds every plane (user decision). `WindTrails`, `WindRadials`, the wind shaders, `WindUniforms.cpp`, and `SmokeWindSample` are untouched.
- Flight-plane data, loader, and `gpFlightPlanes` (prerequisite Plan); ship climbing; lighting falloff or per-plane lighting and ambient fetches; shadow, water, camera, audio height work.
- Reading `kTerrainCollision` anywhere in smoke (decisions 7 and 8 need no flag).
- Layered rendering, 2D array images, new device features, or a packed multi-plane active-tile list.
- Renaming `fBaseHeight`, `fBaseHeightInverse`, `BaseHeightPosition`, or `ProjectToBaseHeight`; the lighting, area-light, point-light, hex-shield, and visible-light uses of them.
- Per-plane smoke tweak values, a per-plane cadence, a per-plane simulation area, or a UI to show one plane.
- The debug texture display pipeline and the continuity probe schema.
- Any change to `Frame::kiVersion`, save, replay, wire, or pack formats.

## Risk triggers and invariants

- Client-only and outside the CRC: every changed file is `BT_CLIENT` or GLSL, and no simulation state reads a weight, projection, or texture; `ComputeSmokePlaneWeights` and `ProjectToFlightPlane` are render-only.
- Record-once command buffers: the plane count is read from `gpFlightPlanes` once at texture creation; every per-frame variation (clears, dilate dispatches, deposit counts) is an indirect write per plane.
- Sparse-dispatch invariants hold per plane: each plane has its own occupancy pair and active list, the dilate variants keep their direct and remap roles, and pass B keeps the only extinction terms; decision 8 preserves the exact-zero walk and plane 0's decay bit-for-bit.
- Baseline equivalence: with every emitter on the lowest plane, weights are `{0, 0}`, so only plane 0 receives deposits and the other planes stay at zero; plane 0's deposit, spread, projection, and compositing math is unchanged, so its screenshots match the baseline.
- Energy conservation: a between-plane emitter deposits `1 - w` and `w` of its intensity; nothing is deposited twice at full strength.
- Descriptor safety: sampler-array slots at or beyond `Count()` alias plane 0; texture pointers are stable across the texture-tier recreation that rebuilds pipelines.
- `uiFlightPlaneCount <= kiMaxFlightPlanes` is asserted at texture creation; shaders loop to the uniform count, never past the array.
- Memory and GPU cost follow decision 15; no quality-level change hides them.

## Acceptance

| Criterion | Expected observation | Method |
|---|---|---|
| Client builds | Client build succeeds; shaders compile through DataPacker | `/compile` BrokenEngineSandbox client after a DataPacker shader export |
| Lowest plane unchanged | With every ship on the lowest plane, screenshots of smoke over water and over terrain match a baseline session taken at `c1420821` plus the prerequisite, and `dump_render_target {"name":"SmokeOne","index":1}` and `index 2` are all-zero | `/agent-harness` `launch.md`, `verification.md` smoke run, `screenshot`, `dump_render_target` |
| Between-plane deposit | A missile moved to Z 28 with `edit_frame {"collection":"missiles","index":k,"writes":{"pVecPositions":[x,y,28]}}` (paused, then unpaused) deposits into `SmokeOne` planes 0 and 1 and not plane 2; the two dumps show the same trail footprint with plane 1 at half the intensity of plane 0 | Harness `inject_payload` `UpdatePlayer` if ShipTypeSplit has not landed, `UpdateShip` after, with `bUseMissiles:true` on fleet ships (Players before ShipTypeSplit, Omnis after), `query_frame` for the missile row, `edit_frame`, `dump_render_target` for indices 0, 1, 2 |
| Eye-ray compositing | In a screenshot of that trail near the screen edge, the plane-1 smoke is displaced toward the eye relative to the plane-0 smoke of the same trail, and the displacement grows with distance from the screen center | `screenshot` at two camera positions |
| Shadow product | A ship under both the plane-0 and plane-1 trail is darker than under either alone | `screenshot` comparison |
| GPU cost recorded | `kGpuTimerSmokeSpread` and `kGpuTimerSmokeEmit` averages before and after, same scene, warm-up discarded | `query_profile` cohorts per `commands-client.md` |
| No determinism change | No `LogDifferences CRC` or `CONFIRMED DESYNC` over a multi-minute connected session | `get_logs` diff |
