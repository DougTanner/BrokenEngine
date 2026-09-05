<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-29T15:55:52.197Z","dependsOn":[]} -->
# Resolve joint indices above the shader joint limit for oversized skins

## Context

A skin may be accepted with up to `common::Skeleton::kiMaxSkinJoints` = 256
joints (`Common/DataFile.h:190`; `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:30-31`),
but only `common::kiMaxJointsPerMesh` = 128 joint matrices are ever written:
the write loop is bounded by
`i < mHeader.skeleton.uiSkinJointCount && i < common::kiMaxJointsPerMesh`
(`Engine/Source/Graphics/AnimationData.cpp:451`). The joint-matrix buffer is
allocated from the skeleton's full joint count
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:275`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp:145`),
so slots 128..255 exist but never receive the joint transforms those vertices
need. Their contents are whatever the buffer already holds: identity matrices
while the initial fill survives
(`Engine/Source/Graphics/Managers/BufferManager.cpp:392-416`), and leftover
values from an earlier allocation or from the uncopied tail of a grown buffer
afterwards (`Engine/Source/Graphics/Managers/BufferManager.cpp:557-577`).

The skinned vertex shader indexes those matrices by the per-vertex attribute
`f4Joint0`, whose values are copied straight from the source skin's `JOINTS_0`
attribute and can reach 255
(`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:128,165-169`;
`Engine/Data/Shaders/Model/ModelSkinned.vert:22-25`). For any accepted skin with
129..256 joints, every vertex weighted to a joint index >= 128 therefore blends
matrices the runtime never wrote for it: those vertices stay at bind pose while
the identity fill is still intact, and are posed by unrelated leftover matrices
once reuse or buffer growth has replaced it. There is no out-of-bounds read,
because the buffer is sized to the full skeleton count.

The DataPacker warning at
`DataPacker/Source/ExportJobs/ExportScene.cpp:586-588` states "Skinning will use
first 128 joints only", which is not what the runtime guarantees — those
vertices read slots the runtime never writes, so their bind-pose appearance
holds only for as long as the initial identity fill does.

Pre-existing at session baseline `e4977a08c9b7082779943a77e1fb481eed341f70`.
Not reachable with current assets: the repository has four `.gltf` files and no
`.glb`, and none declares `"skins"`.

## Design

Author's recommendation: reject an oversized skin at the export trust boundary
rather than publishing a scene the runtime cannot pose correctly. In
`BuildMaterialInfos`, when the selected skin's joint count exceeds
`common::kiMaxJointsPerMesh`, fail the scene export through the existing export
aggregate with a path-specific diagnostic instead of logging the current
warning and continuing. This matches the boundary-rejection shape already used
for unsupported scene input in `SceneSkinIdentityValidation.md` and
`SceneSkeletonTopologicalOrder.md`, keeps the shader, the `.pack` layout, and
the runtime evaluator untouched, and removes the misleading warning text.

The rejection also removes the only path by which the clamp landed by
`SkinJointCountNarrowing.md` can be reached with a joint count above the shader
limit; that clamp stays as the narrowing guard for the serialized byte.

Two alternatives were considered and are recommended against for now, both
because they are larger than the reachable problem: remapping or pruning joint
indices per material at export (needs a new index table and vertex rewrite), and
raising `kiMaxJointsPerMesh` to 256. The second alternative costs no extra
buffer memory — both render paths already allocate joint matrices from the full
skeleton joint count
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:275`;
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp:145`),
and the shader declares no joint capacity at all, indexing a runtime-sized
storage-buffer array (`Engine/Data/Shaders/Model/ModelCommon.h:74-78`). Its real
cost is elsewhere: 256 does not fit `MaterialInfo::uiJointCount`, a `uint8_t`
(`Common/DataFile.h:204`), so that alternative also has to widen the serialized
field, changing the `MaterialInfo` layout and forcing the version bump its
`static_assert` at `Common/DataFile.h:208` requires; and the per-mesh joint
evaluation loop (`Engine/Source/Graphics/AnimationData.cpp:451`) would write up
to twice as many matrices per skinned mesh each frame. It is also a capability
addition belonging in `Documents/Features/`, not a debt Plan. If a real asset
ever needs more than 128 joints, re-plan against that asset rather than
pre-building capacity here.

## Critical files

- `DataPacker/Source/ExportJobs/ExportScene.cpp:580-592` — `BuildMaterialInfos`
  joint-count warning and narrowing; the site that must fail instead.
- `Common/DataFile.h:190,230` — `Skeleton::kiMaxSkinJoints` (256) and
  `kiMaxJointsPerMesh` (128), the two limits that disagree.
- `Engine/Source/Graphics/AnimationData.cpp:451` — the 128-bounded joint-matrix
  write loop that defines the real runtime capacity.
- `Engine/Data/Shaders/Model/ModelSkinned.vert:22-25` — the consumer that
  indexes by raw source joint index.

## In scope

- Failing the scene export in `BuildMaterialInfos` when the selected skin's
  joint count exceeds `common::kiMaxJointsPerMesh`, routed through the export
  aggregate's existing failure path so no scene chunk is published.
- Replacing the current inaccurate warning text at
  `ExportScene.cpp:586-588` with the diagnostic that failure emits.

## Out of scope

- Raising `kiMaxJointsPerMesh`, the 128 bound on the joint-matrix write loop, or
  the joint-matrix buffer budget.
- Remapping, pruning, or renumbering joint indices; any vertex-attribute
  rewrite.
- Changing `Skeleton::kiMaxSkinJoints`, the skeleton serialization, the
  joint-matrix buffer allocation in the render paths, or `ModelSkinned.vert`.
- Multi-skin identity selection and mixed-skinning material splitting, owned by
  `SceneSkinIdentityValidation.md` and `MixedSkinningMaterialSplit.md`.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: it tightens a check inside one
DataPacker unit at an existing trust boundary (opaque glTF input), with the
serialized format, the `.pack` layout, and what the runtime trusts all
unchanged; that is scoped tool behavior, not a serialization change.

Preserve these invariants:

- Every published scene's skin joint count is <= `common::kiMaxJointsPerMesh`,
  so every joint index a vertex can carry addresses a matrix the runtime
  actually writes.
- Scenes at or below 128 joints, and scenes with no skin, export
  byte-identically; no `.pack` version, replay, save, or wire change.

## Acceptance criteria

- A glTF whose selected skin declares more than 128 joints fails export with a
  diagnostic naming the file and the limit, and publishes no scene chunk.
- A glTF with 128 or fewer joints, and the existing unskinned repository assets,
  export byte-identically to the current output.
- No warning remains that claims skinning falls back to the first 128 joints.
- DataPacker builds through `/compile`, and a DataPacker run over the current
  assets reports no new warning or failure.

## Notes

Origin: `/next-plan` Plan review step plan-audit finding `PA-F-001`
(`Temp/next-plan-skinjoint/audit-out.md`) and the preparation residual from the
session implementing `Documents/Plans/Engine/SkinJointCountNarrowing.md`, whose
`## Out of scope` explicitly excludes raising the 128
`kiMaxJointsPerMesh` bound.
