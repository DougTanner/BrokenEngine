# Plan02 GLSL review

## Review coverage
Baseline b56f20fa44d0424c2fdf5003157563684ed747f5 through f713d7cf526dafca00ffead328751ccaeadec13e. Inventory status pass, truncated false; 51 shader/shared-header paths (41 present, 10 removed). Every listed changed region was reviewed; no source edits.
- M Engine/Data/Shaders/DebugTexture.frag: deleted old 13-15, 28-28, deleted old 70-129; covered.
- D Engine/Data/Shaders/Lighting/AreaLight.frag: deleted old 1-90; covered.
- D Engine/Data/Shaders/Lighting/LightCombine.comp: deleted old 1-144; covered.
- D Engine/Data/Shaders/Lighting/LightingBlurH.comp: deleted old 1-43; covered.
- D Engine/Data/Shaders/Lighting/LightingBlurV.comp: deleted old 1-47; covered.
- A Engine/Data/Shaders/Lighting/LightingCommon.h: 1-103; covered.
- A Engine/Data/Shaders/Lighting/LightingCompose.h: 1-13; covered.
- A Engine/Data/Shaders/Lighting/LightingFixture.frag: 1-36; covered.
- A Engine/Data/Shaders/Lighting/LightingFixture.vert: 1-31; covered.
- A Engine/Data/Shaders/Lighting/LightingFixtureReceiver.frag: 1-13; covered.
- D Engine/Data/Shaders/Lighting/LightingHistoryCopy.comp: deleted old 1-32; covered.
- A Engine/Data/Shaders/Lighting/LightingModelHit.h: 1-90; covered.
- A Engine/Data/Shaders/Lighting/LightingModelReceiver.frag: 1-36; covered.
- A Engine/Data/Shaders/Lighting/LightingModels.comp: 1-77; covered.
- A Engine/Data/Shaders/Lighting/LightingModelsCommon.h: 1-48; covered.
- A Engine/Data/Shaders/Lighting/LightingNeutralModel.frag: 1-4; covered.
- A Engine/Data/Shaders/Lighting/LightingNeutralModelReceiver.frag: 1-11; covered.
- A Engine/Data/Shaders/Lighting/LightingQuery.h: 1-129; covered.
- A Engine/Data/Shaders/Lighting/LightingRadiometry.h: 1-42; covered.
- A Engine/Data/Shaders/Lighting/LightingReceiver.h: 1-23; covered.
- A Engine/Data/Shaders/Lighting/LightingReference.comp: 1-56; covered.
- D Engine/Data/Shaders/Lighting/LightingSpread.frag: deleted old 1-184; covered.
- D Engine/Data/Shaders/Lighting/LightingTemporal.comp: deleted old 1-52; covered.
- A Engine/Data/Shaders/Lighting/LightingTerrain.comp: 1-50; covered.
- A Engine/Data/Shaders/Lighting/LightingTerrainCommon.h: 1-35; covered.
- A Engine/Data/Shaders/Lighting/LightingTerrainReceiver.frag: 1-43; covered.
- A Engine/Data/Shaders/Lighting/LightingWaterReceiver.frag: 1-43; covered.
- D Engine/Data/Shaders/Lighting/PointLight.frag: deleted old 1-90; covered.
- A Engine/Data/Shaders/Lighting/TerrainElevation.h: 1-14; covered.
- M Engine/Data/Shaders/Model/Model.frag: 9-9; covered.
- A Engine/Data/Shaders/Model/ModelDiffuse.h: 1-13; covered.
- A Engine/Data/Shaders/Model/ModelForward.h: 1-252; covered.
- A Engine/Data/Shaders/Model/ModelMaterial.h: 1-97; covered.
- M Engine/Data/Shaders/Objects/HexShield.vert: 84-84; covered.
- D Engine/Data/Shaders/Objects/HexShieldLighting.frag: deleted old 1-65; covered.
- A Engine/Data/Shaders/Quads/QuadsAxisAligned.h: 1-25; covered.
- M Engine/Data/Shaders/Quads/QuadsAxisAlignedVisibleArea.vert: 5-5, 41-41, deleted old 52-55, 54-54, deleted old 60-60, 57-61; covered.
- M Engine/Data/Shaders/Quads/QuadsVisibleArea.vert: deleted old 55-58, 57-57, deleted old 63-63; covered.
- M Engine/Data/Shaders/ShaderFunctions.h: deleted old 104-213, 148-148, 156-156, 161-161, 164-164, deleted old 307-312; covered.
- M Engine/Data/Shaders/ShaderGlobalLayout.h: deleted old 8-8, deleted old 53-53, deleted old 71-71, deleted old 117-179, deleted old 305-305; covered.
- M Engine/Data/Shaders/ShaderLayoutsBase.h: deleted old 32-33, deleted old 37-37, 125-126, 161-161, deleted old 165-166, deleted old 168-170, deleted old 300-300, deleted old 305-305, 363-364; covered.
- M Engine/Data/Shaders/ShaderMainLayout.h: deleted old 85-118, deleted old 139-142; covered.
- M Engine/Data/Shaders/Terrain/Terrain.frag: 7-8, 21-23, 26-37, 39-41, 65-72, 84-84, 87-87, 93-93; covered.
- M Engine/Data/Shaders/Terrain/Terrain.vert: 3-3; covered.
- M Engine/Data/Shaders/Terrain/TerrainElevation.frag: 7-7, 28-30; covered.
- A Engine/Data/Shaders/Terrain/TerrainMaterial.h: 1-92; covered.
- A Engine/Data/Shaders/Terrain/TerrainVertex.h: 1-128; covered.
- M Engine/Data/Shaders/Water/Water.frag: 5-5, 36-43, 59-59, 88-89, 92-92, 131-131, 161-162, 164-164, 170-170; covered.
- M Engine/Data/Shaders/Water/Water.vert: 17-17; covered.
- A Engine/Data/Shaders/Water/WaterMaterial.h: 1-48; covered.
- D Engine/Data/Shaders/Water/WaterReflectionProjection.h: deleted old 1-51; covered.

## Contract checks
LightingCommon.h: source stride 64, publication 16, instance 112, geometry 64 and hit 72 agree with scalar CPU declarations/static assertions. ShaderLayoutsBase maps vec3 to 12-byte XMFLOAT3 and defines 32/64-bit scalar types through shared declarations. Uniform scalar default and explicit scalar SSBO/reference blocks match actual storage. LightingModelsCommon: vertex 24, material 48, deform push data 56; CPU push range uses sizeof(LightingModelDeform). Terrain work and material both 24 bytes, work count at byte 16; Terrain.cpp writes the exact records and XYZ stride is 12 bytes.
Lighting/Lighting.cpp BindQueryDescriptors/AddQueryDescriptorInfos match bindings 20-26; terrain producer bindings 1,2,3,5 match Terrain.cpp. Receiver output order position/shading/geometric/albedo matches Comparison receivers and compute bindings 18,19,27,28. Global composition bindings 29/30 match TextureDescriptors creation/writes, control is uint32 at offset zero. Forward terrain/water renumbering matches WorldLightingShadowPipelines.cpp.
C++ direction: Sandbox and ModelViewer Pch.h -> project ShaderLayouts.h -> ShaderLayoutsBase -> Global/Main layouts and LightingCommon. LightingModels.cpp directly includes ModelsCommon; Terrain.h/cpp directly include TerrainCommon. Shader direction: every current entry includes ShaderLayouts.h, apart from simple receiver fragments needing no shared records; Reference -> Radiometry -> Query -> ModelHit/ModelsCommon and TerrainCommon/Material; Terrain compute -> TerrainCommon; Fixture vertex -> ModelHit; forward/neutral model -> ModelForward/Material/Diffuse/Compose; terrain -> TerrainVertex or Material/Compose; water -> Material/Compose; all includes use DataPacker roots.
ExportShader.cpp PreprocessShader uses glslc -MD/-MF and calls CaptureDependencies; ParseDependencyFile resolves engine/project includes; fingerprints are stored by input root and compared by IsDirty. Shared-header edits therefore invalidate affected top-level shaders through their include graph.
DeviceManager/InstanceManager enable and require rayQuery, accelerationStructure, bufferDeviceAddress, shaderInt64, scalarBlockLayout, runtimeDescriptorArray and nonuniform sampled-image indexing. No executable mat3/mat4 inverse was introduced. Nonuniform model/island indices are decorated; water foam selectors and DebugTexture index are uniform.

## Algorithm and synchronization checks
Model compute skinning mirrors ModelSkinned.vert and ModelCommon.h, including mesh slot, compact joint rows, transform order, normal approximation, vertex-material ownership and 16/32-bit indices. CPU geometry metadata and BLAS primitive ordering match query indexing. Closest-hit material lookup remains intentionally unconsumed and untested until stage 2; no new finding raised for that binding omission.
Direct point power is divided by 4*pi and albedo/pi once; rectangle power/area, area/sample-count, emitter cosine, XYZ distance and visibility segment shortening are applied once. Rectangle cross product is nonzero for fixture half edges; samples are 256/512. Reference dispatch is exactly one 4x4 workgroup per nonoverlapping tile and writes zero for invalid receiver pixels. 1920x1080 is exactly divisible by 4.
Terrain dispatch chunks cover each work vertex once with tail guard, output allocated for every published placement, and work-buffer binding 5 refreshed after possible growth. Global elevation color writes precede compute/vertex reads; compute geometry writes precede BLAS build; BLAS precedes TLAS; query barrier precedes receiver/reference reads. Capture receivers have matching stage interfaces and RGBA32F attachments. Composition reads final pixel with texelFetch only when per-framebuffer control is enabled; BLEND contributes zero.
Terrain producer uses raster placement/world-UV/own-UV arithmetic, visible elevation LOD zero and own-heightmap sink. The bilinear-footprint coverage test and sea-floor outside coverage are the authorized approximation; no exact-equivalence instrumentation requested. Existing ordinary sun/IBL/shadow/smoke/water algorithms remain after local-light deletion.

## Scope and performance
Authorization, minimum sufficient change and KISS passes: PASS. All shader changes map to legacy-light removal, resident-scene/query support, shared material receiver expressions, fixture/reference or composition seam. Closest-hit lookup and 512-sample option are explicitly kept by the binding handoff; one-bounce reference is absent as required. No speculative safety machinery recommended.
Measured all 41 present changed shader/header files together with Measure-Tokens.ps1 -Json: total 35,823 bt-token-v1; largest ShaderGlobalLayout.h 3,776, ModelForward.h 2,800, Water.frag 2,637, all below 5,000. Exhaustive capture loops are authorized reference work, not gameplay work. No evidence-supported device-specific performance finding.

## TerrainVertex recommendation
Recommend the small shared-function option requested by plan02: make Terrain/TerrainVertex.h contain a pure helper for island-local XY plus rect/rotation/visible-area to world XY, island UV, visible UV and rotation cos/sin; move its stage declarations/main back into Terrain.vert, and call the helper from Terrain.vert and LightingTerrain.comp. Preserve each caller elevation sampling and the compute coverage branch. ShaderFunctions.h WorldToVisibleArea has a different algebraic arrangement; QuadsAxisAligned.h converts normalized corner coordinates and would add divide/multiply round trips. Reuse neither for an exact spelling-preserving extraction. This is a requested cleanup choice, not a reachable correctness finding; no tolerance/proof machinery needed.

## Outcome
PASS: no reachable correctness/performance/contract findings; no external claim verification requests; no source changes; no build requested by this findings-only review. Required sibling repo-code-review covers ShaderGlobalLayout.h, ShaderMainLayout.h, ShaderLayoutsBase.h, LightingCommon.h, LightingTerrainCommon.h and LightingModelsCommon.h. Runtime acceptance remains plan03, outside this static review.
