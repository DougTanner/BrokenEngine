<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:11.872Z","dependsOn":[]} -->
# Preserve the selected glTF skin identity

## Context

The frozen audit retained `CAI/shard-0006/011`. `LoadSkeletonData` and
`DetermineAnimationPath` read `rModel.skins[0]` (`SceneSkeletonLoader.cpp:26-63`;
`SceneAnimationLoader.cpp:127-147`), while `LoadVertices` decides only whether
`JOINTS_0` exists and never carries `rNode.skin`. `BuildMaterialInfos` again
uses the first skin (`ExportScene.cpp:490-507`). Source/runtime bytes are
unchanged from baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Collect the skin identities actually referenced by skinned nodes. Select the
sole referenced skin for the existing single-skeleton serialization; if more
than one distinct skin is referenced, reject the scene through the export
aggregate with a clear diagnostic until a multi-skin format exists. Thread the
selected identity through skeleton, animation, vertex/material setup so no
accepted primitive silently uses skin 0's mapping.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp` — skeleton selection.
- `DataPacker/Source/ExportJobs/Scene/SceneAnimationLoader.cpp` — animation mapping.
- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` and `ExportScene.cpp` — node/material identity.

## In scope

- Referenced-skin collection, sole-skin selection, and rejection of multiple distinct referenced skins.
- Propagation of the selected identity through all existing single-skeleton scene serialization steps.

## Out of scope

- Designing a multi-skin runtime/pack format, joint-count clamping, or shader changes.
- Unskinned nodes and scenes with one coherent selected skin.

## Risk tier and invariants

Tier 3. Trigger: skin identity is serialized across scene exporter/runtime and
animation consumers. Every accepted skinned primitive uses its selected skin's
joint/inverse-bind mapping; unsupported multi-skin scenes do not publish.

## Acceptance criteria

- A scene with two distinct referenced skins fails before publication rather than deforming one mesh with skin 0.
- A scene with one referenced skin uses that skin consistently in skeleton, animation, and material paths.
- Existing one-skin assets render and animate unchanged.

## Notes

Origin: `CAI/shard-0006/011`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:232`.
No source fix, build, or runtime scenario was performed while routing this item.
