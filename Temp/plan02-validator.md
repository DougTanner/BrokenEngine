# Plan 02 over-engineering validator

## Authorization and scope

Baseline b56f20fa44d0424c2fdf5003157563684ed747f5; starting HEAD f713d7cf526dafca00ffead328751ccaeadec13e; C:/Users/dougt/Documents/BrokenEngine, branch lighting-foundation-work. Reused Temp/over-engineering-review-inventory.json (status pass) and Temp/over-engineering-review-candidates.md ## Removal candidates. User's whole-foundation prototype stripping instruction authorizes OE01-OE07. Main explicitly overrides the older Frame exclusion for OE06 only, preserving simulation/CRC and serialized layout. All changed original lines were mechanically checked against inventory regions before any writes. No new format, wire, pack, GPU, save, replay or Collection layout changes.

## Candidate verdicts

- OE01 applied. Repository-wide symbol searches and direct reads of InitializeAnimation, WriteAnimation, PublishFixtureAnimation and LightingComparison's animation pipeline creation establish that the four fields are assigned only and never read. LightingFixtureAnimation is ordinary private runtime state, with no raw serialization or upload. sceneCrc, iClip, center and scale stay; every asset/name validation stays. Removing the string assignments also removes their now-unused ScopedSuppressAllocationTracking and associated Heap comment; the remaining function performs only lookups, comparisons and scalar assignments.
- OE02 applied. All five snapshot members occur only at declaration and post-vkCreateDevice assignments. The VkDeviceCreateInfo uses local feature objects and deviceExtensions directly. No validation, feature enabling, CHECK_VK, capabilities or allocation policy changed.
- OE03 applied. The stored sampler-create-info array has exactly 13 assignments and no reads. Each sampler is created from its local vkSamplerCreateInfo/vkSmokeSamplerCreateInfo; all 13 CHECK_VK(vkCreateSampler(...)) lines match starting HEAD exactly after removal.
- OE04 applied. The CPU LightingFramebuffer geometry-count member occurs only in its declaration and assignment. EndRender retains local iGeometries for buffer sizes/spans and validation. Shader LightingInstance::uiGeometryCount fields and all their assignments stay unchanged; this is not a GPU record.
- OE05 applied. Repository-wide LightingModels/Write searches find one declaration, one definition and one call, with literal 0,0 offsets. Parameters only initialized the two cursors; both now initialize to zero. Terrain::Write still receives the model counts as offsets and is unchanged.
- OE06 applied. The four client-only IslandTemplate bounds are declared, reset, and reduced by a self-contained loop with no external reader. IslandTemplate is runtime terrain metadata, not a Collection, and its packed data pointers are populated from chunk payloads rather than deserializing the object. The declarations lie inside BT_CLIENT, the residency implementation is wholly BT_CLIENT, and shared terrain sampling and CRC logic are untouched. Main's newer authorization supersedes the old strip-plan Frame exclusion and old Frame/AGENTS.md tile-coverage instruction; the parallel documentation worker removed the latter during this pass.
- OE07 applied. The scratch-size member had only a write in Create, immediate allocation use, and move copy. Buffer size now uses the identical vkSizes.buildScratchSize + vkAlignment expression directly; RecordBuild continues deriving its aligned address from mScratch. Ordinary-RAM placement remains unchanged.

## Modified ranges

Ranges below are post-edit deletion anchors or changed lines, relative to starting HEAD; Temp/plan02-validator.patch contains the exact zero-context patch. Other workers' documentation edits are excluded.

- Engine/Source/Graphics/Lighting/LightingFixture.h:31,33 — removed four fields.
- Engine/Source/Graphics/Lighting/LightingFixture.cpp:73,102 — removed unused string-allocation suppression and four assignments.
- Engine/Source/Graphics/Managers/DeviceManager.h:25 — removed five snapshot members.
- Engine/Source/Graphics/Managers/DeviceManager.cpp:226 — removed eight snapshot writes.
- Engine/Source/Graphics/Managers/TextureManager.h:180 — removed create-info array.
- Engine/Source/Graphics/Managers/TextureManager.cpp:334,344,368,374,380,386,391,397,403,410,419,425,431 — removed thirteen snapshot writes.
- Engine/Source/Graphics/Lighting/LightingState.h:32 — removed CPU geometry count.
- Engine/Source/Graphics/Lighting/Lighting.cpp:295,301 — removed geometry-count write and fixed Write arguments.
- Engine/Source/Graphics/Lighting/LightingModels.h:31 — reduced Write signature.
- Engine/Source/Graphics/Lighting/LightingModels.cpp:366,368-369 — reduced Write signature and zero-initialized cursors.
- Engine/Source/Frame/IslandTerrain.h:96 — removed four client-only bounds.
- Engine/Source/Frame/IslandTerrainResidency.cpp:272,448,470 — removed eviction resets, bounds reduction and teardown resets.
- Engine/Source/Graphics/Lighting/LightingAccelerationStructure.h:29 — removed scratch-size member.
- Engine/Source/Graphics/Lighting/LightingAccelerationStructure.cpp:11,46,52 — removed move copy and retained-size write; allocation uses returned size directly.

## Focused checks

- pwsh -NoProfile -File Temp/plan02-validator-edits.ps1 completed successfully: exact occurrence counts, all original touched lines inside inventory regions, BOM preservation, unchanged newline content, concurrent-edit checks, and full readback equality for all 14 files.
- Read the resulting complete diff for all 14 edited files: only proven candidate sites and the directly obsolete allocation suppression changed.
- git grep over Engine and Projects for mEnabledVkPhysicalDevice, mEnabledExtensions, mpSamplerInfosVkSamplerCreateInfo, fMesh(Min|Max)[XY], mScratchSizeVkDeviceSize, removed mAnimation fields, and rFramebuffer.uiGeometryCount returned no matches (exit 1).
- Compared all CHECK_VK(vkCreateSampler) lines against starting HEAD: all 13 unchanged.
- Compared Buffer.cpp against starting HEAD: identical.
- git diff --check passed.
- No builds, runtime checks, commits, branch changes, new tests, or subagents in this pass.

## Next checks

Return style and correctness reviews to main. Build BrokenEngineSandbox client Debug|x64 and ModelViewer Debug|x64 (compile all changed cpp files); BrokenEngineSandboxServer Debug|x64 (recompiles shared IslandTerrain.h through its header consumers/PCH). Main's plan also rebuilds DataPacker Release|x64, although these removals do not affect its sources. No design suggestions or dropped candidates.
