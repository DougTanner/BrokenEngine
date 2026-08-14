<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-14T19:14:42.458Z","dependsOn":[]} -->
# Make scene material serialization deterministic

## Context

The forced unchanged-source PreExport acceptance criterion for the scene texture-reconciliation change required byte-identical intermediates, packs, and manifests. The texture orphan predicate passed, but the controlled reruns still produced different `Scene.pack` and `Scene.manifest` bytes. Historical ignored evidence in `Temp/SceneTextureIntermediateReconciliation/Run-20260814-142826086-38188/evidence.md` and `comparison.json` found a 1232-byte `Scene.pack` whose only payload differences were one byte at offsets `469 + 76*n` for ten material records (`0x02` versus `0x01`); `Scene.manifest` differed only in the derived content CRC. Texture and Model outputs, seeds, non-padding Scene bytes, headers, path CRCs, sizes, and logs were identical, and a third forced run reproduced the first payload.

`common::MaterialShaderData` (`Common/DataFile.h:244-267`) has five `uint8_t` texture-index fields followed by `f4BaseColorFactor` at offset 8, leaving a three-byte compiler alignment gap in its 76-byte raw representation. `ExportScene::FillMaterialShaderDatas` (`DataPacker/Source/ExportJobs/ExportScene.cpp:743-827`) value-initializes a local record at line 756 and copies it into the raw scene payload at line 826. The implicit assignment copies the indeterminate gap byte into the serialized material record. This is existing DataPacker serialization/layout debt, outside the active `SceneTextureIntermediateReconciliation` boundary, which excludes encoded bytes, pack/manifest formats, CRC/version, and serialization.

## Design

Replace the compiler-inserted gap after the five texture-index fields with an explicit `uint8_t uiPad[3] {};` member in `common::MaterialShaderData`. Keep the existing field order, `f4BaseColorFactor` offset 8, 76-byte record size, and raw payload offsets unchanged. The existing value-initialized local in `FillMaterialShaderDatas` then supplies zero bytes for the explicit pad, and the existing implicit assignment copies those deterministic bytes; no separate serialization format or reader path is introduced.

Because the emitted object representation changes while the payload size remains 76 bytes, follow the repository's shared-data contract: increment the manual `DataHeader::kiVersion` base from 50 to 51 and the raw `ExportScene::GetVersion` base from 54 to 55. These bumps force stale packs, scene chunks, and `.PreExport` markers to regenerate; no backward decoder, migration path, or compatibility format is added.

## Critical files

- `Common/DataFile.h` — `MaterialShaderData`'s explicit pad and the `DataHeader::kiVersion` base.
- `DataPacker/Source/ExportJobs/ExportScene.h` — `ExportScene::GetVersion` raw base.
- `DataPacker/Source/ExportJobs/ExportScene.cpp` — `FillMaterialShaderDatas` producer/assignment path to verify remains the sole raw material-record writer.

## In scope

- `common::MaterialShaderData`: replace the implicit three-byte gap with zero-initialized `uiPad[3]`, retaining the five texture-index offsets, `f4BaseColorFactor` at offset 8, and `sizeof == 76`.
- `common::DataHeader::kiVersion`: increment the manual base required for the changed serialized object representation.
- `ExportScene::GetVersion`: increment its raw base so cached Scene chunks and `.PreExport` markers are invalidated despite the unchanged `sizeof` fold.
- `ExportScene::FillMaterialShaderDatas`: preserve and verify the existing value-initialization and copy path now serializes explicit zero pad bytes; do not alter material field values or ordering.

## Out of scope

- `ExportScene::ProcessTextures` orphan matching, scene texture staging/publication, and any intermediate ownership or cleanup behavior.
- Texture, model, animation, shader, or IBL encoding and any unrelated padding or serialization audit.
- Changes to `MaterialShaderData` field order, record size, runtime reader math, `PbrMaterialLayout`, or the `.pack`/`.manifest` format beyond deterministic bytes and the required version invalidation.
- A compatibility decoder, migration of old chunks, preservation of old random padding bytes, or changes to CRC algorithms.
- Unit tests and changes to the active `SceneTextureIntermediateReconciliation` Plan.

## Risk tier and invariants

Expected Change Workflow Tier 3. Triggers: this changes serialized `.pack` payload bytes and their derived `.manifest` content CRC, touches the shared `DataFile.h` layout contract, and requires DataHeader/Scene cache-version invalidation. Invariants: `MaterialShaderData` remains 76 bytes with `f4BaseColorFactor` at offset 8; all non-padding material values and the runtime PBR layout remain byte-identical; every exported material record has three explicit zero pad bytes; forced exports from unchanged inputs are bit-identical; and no old-format compatibility path is introduced.

## Acceptance criteria

- DataPacker and the affected runtime target compile with the existing `MaterialShaderData` size/offset assertions passing; the version changes invalidate stale Scene chunks and `.PreExport` markers and cause a clean current-format export.
- Recreate one fresh ordinary three-path fixture from tracked inputs for two successive exports. From the repository root, choose a new GUID and use it in a local fixture directory outside the repository's ignored paths and in the project directory name (for example, `%LOCALAPPDATA%/BrokenEngine/MaterialShaderDataPadding-<guid>/Projects/MaterialShaderDataPaddingFixture-<guid>`); create ordinary (non-reparse) `Engine/Data`, sibling `ThirdParty`, project `Data`, and `Platforms/VisualStudio2026/Output/Data` directories. Copy only `scene.gltf`, `scene.bin`, and `textures/` from tracked `Projects/BrokenEngineSandbox/Data/Models/aim-9_missile` into the project `Data` directory, leaving generated `.MODEL`, `.Texture*`, and `.PreExport` files out of the seed. Set `BT_DATAPACKER_NONINTERACTIVE=1` and `BT_DATAPACKER_FORBID_GAEA_EXPORT=1`, then invoke the built `DataPacker.exe` directly with exactly three path arguments: `<engine-data> <project-data> <output-data>`. Remove `<project-data>/scene.gltf.PreExport` before each of the two successful invocations, record SHA-256 maps for every regular file under the project and output directories after each run, and compare the maps by relative path. Every intermediate, pack, and manifest hash is byte-identical between the two runs, including `Scene.pack` and `Scene.manifest`.
- A field-aware comparison of the two `Scene.pack` payloads reports no differences outside or inside the former three-byte gap, reports each material record's explicit `uiPad[3]` as zero, and confirms all non-padding material bytes are equal. The Scene manifest's derived content CRC is byte-identical and matches the exact Scene chunk bytes in both runs.
- The run leaves unrelated Texture and Model outputs unchanged and emits no compatibility or migration artifact.

## Coordination

No directional dependency or mandatory cross-Plan coordination is required. This Plan owns the shared material-record serialization bytes; unrelated scene texture reconciliation remains independently landable.

## Notes

Origin: user-approved Tier-3 follow-up for the proven out-of-scope acceptance gap. The diagnosis is complete; no instrumentation remains and no tracked source was edited during the controlled runtime pass. Follow the normal Tier-3 plan-audit, implementation, serialization/CRC review, compile, and observable DataPacker verification workflow. No unit tests are added.
