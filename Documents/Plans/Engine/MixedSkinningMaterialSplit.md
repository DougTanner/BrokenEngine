<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:58.338Z","dependsOn":[]} -->
# Split scene materials by skinning deformation state

## Context

The frozen audit retained `CAI/shard-0006/004`. `ResolveEffectiveMaterial`
keys only `(originalMaterial,nodeIndex)` (`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:64-77`),
while `LoadVertices` later marks the shared info skinned when any primitive has
`JOINTS_0` (`:334-345`). The runtime then applies one skinned pipeline state to
all vertices in that material draw. Source bytes match baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, so the mismatch is pre-existing.

## Design

Include deformation mode (skinned versus non-skinned) in effective-material
identity for each node. Keep the existing dummy-joint construction for
non-skinned primitives in animated models, but ensure each resulting material
and draw uses the matching runtime shader path and serialized joint count.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` — key and primitive routing.
- `DataPacker/Source/ExportJobs/ExportScene.cpp` — material serialization.
- `Engine/Data/Shaders/Model/ModelSkinned.vert` — consumer deformation branch.

## In scope

- Effective-material identity and material/index-buffer splitting by skinning state.
- Serialization of the resulting independent material draw states.

## Out of scope

- Skin-joint count narrowing, multiple-skin selection, vertex weighting rules, or shader math.
- Splitting materials for unrelated texture or node properties.

## Risk tier and invariants

Tier 3. Trigger: scene serialization and CPU/GPU pipeline selection cross the
DataPacker/runtime boundary. A draw must never combine vertices requiring
different deformation; existing homogeneous draws stay identical.

## Acceptance criteria

- An animated node containing one skinned and one non-skinned primitive using the same source material produces two compatible material draws.
- The non-skinned primitive is not transformed by an unrelated joint matrix.
- Homogeneous static/skinned scenes retain current geometry and material output.

## Notes

Origin: `CAI/shard-0006/004`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:106`.
No source fix or build was part of this route.
