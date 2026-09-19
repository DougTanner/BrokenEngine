<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-19T20:04:31.901Z","dependsOn":[]} -->
# Bound per-vertex JOINTS_0 indices against the skin joint count at export

## Context

`BuildVertices` fetches the glTF `JOINTS_0` attribute and converts its raw
`uint16_t` indices straight to float into `ModelVertex::fJoint` and
`ModelVertex::f4Joint0`, with no bound against the skin's joint count
(`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:123-125,157-165`).
The only check there is an `ASSERT` on the accessor's component type.

At runtime the skinned vertex shader indexes the joint-matrix storage buffer by
that value plus the material's `uiJointMatrixOffset`
(`Engine/Data/Shaders/Model/ModelSkinned.vert:22-25`), and that buffer is
runtime-sized rather than a fixed array
(`Engine/Data/Shaders/Model/ModelCommon.h:74-78`). A vertex index at or above
the skin's joint count therefore reads another material's or another instance's
joint matrices, or past the written region entirely — a GPU out-of-bounds read
of a storage buffer with no shader-side clamp to catch it.

glTF files are untrusted input to the exporter: the neighbouring scene loader
already rejects an out-of-range `node.skin` for exactly this reason
(`DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:24-30`), and
rejects other unsupported or malformed scene shapes the same way
(`SceneSkeletonLoader.cpp:35-37,228-231`). Per-vertex joint indices are the one
remaining untrusted field on that path with no bound.

Pre-existing at session baseline `45d74910b4c9c2f0b80f06dc3c44c8c746f2ea36`, and
outside the implementation boundary of the Plan the session claimed
(`Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md`, whose `## In scope`
removes the three artificial skeleton limits across `Common/DataFile.h`,
`Engine/Source/Graphics/AnimationData.cpp`, and DataPacker, widens
`MaterialInfo::uiJointCount`, bumps three format versions, and re-exports the
four tracked `.MODEL` intermediates, and which explicitly excludes any
vertex-attribute work).

## Design

Author's recommendation: reject the asset at the export trust boundary, matching
the shape the scene loaders already use, rather than clamping indices at export
or adding a shader-side clamp.

In `BuildVertices`, while reading each vertex's four `JOINTS_0` components,
check every component whose paired `WEIGHTS_0` value is non-zero against the
selected skin's joint count and throw `std::runtime_error` with a diagnostic
naming the primitive, the vertex, the offending index, and the joint count —
the same `std::runtime_error` rejection the scene loaders raise and the export
aggregate already handles. Components with zero weight contribute nothing to the
skinned position and are left unchecked, because exporters commonly emit index 0
or stale indices in unused slots and rejecting those would fail valid assets.

The joint count to compare against is the selected skin's
`uiSkinJointCount`. `BuildVertices` currently receives only `bHasSkeleton`, so
the count is threaded down from the caller that already holds it; whether it
arrives as an extra parameter or through the skeleton data already built is a
trivial local choice.

Clamping the index instead is recommended against: it would silently pose
vertices against the wrong joint and hide a malformed asset, where a failed
export is visible at build time. A shader-side clamp is also recommended
against: it costs per-vertex work in the hot skinned path to compensate for data
the exporter can reject once.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:109,123-125,157-165`
  — `BuildVertices`: the `JOINTS_0` fetch and the unchecked per-vertex
  conversion, plus the signature that must learn the joint count.
- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:24-30,35-37,228-231`
  — the existing `std::runtime_error` rejection shape to match.
- `Engine/Data/Shaders/Model/ModelSkinned.vert:22-25`,
  `Engine/Data/Shaders/Model/ModelCommon.h:74-78` — the unclamped consumer that
  makes an out-of-range index a storage-buffer overread.

## In scope

- Adding the per-vertex `JOINTS_0` bound check inside `BuildVertices`'
  per-vertex loop, applied to components with non-zero weight, failing through
  `std::runtime_error`.
- Threading the selected skin's joint count into `BuildVertices` from its
  caller, and updating that call site.

## Out of scope

- The skin-wide joint-count limit and the `kiMaxJointsPerMesh` disagreement,
  owned by `Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md`.
- Any change to `ModelVertex`, the `.pack` scene chunk layout, `kiVersion`, or
  the vertex dedup path.
- Any shader change, including a clamp in `ModelSkinned.vert`.
- Clamping, remapping, renumbering, or pruning joint indices.
- Weight normalization or `WEIGHTS_0` validation beyond reading the paired
  weight to decide whether a component is used.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: it tightens a check inside one
DataPacker unit at an existing trust boundary (opaque glTF input); the exported
format, the `.pack` layout, and what the runtime trusts are all unchanged, which
is that unit's scoped behavior rather than a serialization change.

Preserve these invariants:

- Every published skinned vertex carries, in each weighted component, a joint
  index below the skin's joint count.
- Assets that already satisfy the bound export byte-identically: no `.pack`
  version, manifest CRC, replay, save, or wire change.

## Acceptance criteria

- A glTF whose skinned primitive carries a weighted `JOINTS_0` index at or above
  the skin's joint count fails export with a diagnostic naming the file, the
  offending index, and the joint count, and publishes no scene chunk.
- A glTF with a zero-weight component holding an out-of-range index still
  exports.
- The current repository assets export byte-identically to the current output.
- DataPacker builds through `/compile` and a run over the current assets reports
  no new warning or failure.

## Notes

Origin: `/plan-audit` finding PA-F-002 on the snapshot of the change made during
the `/next-plan` run on
`Documents/Plans/Engine/OversizedSkinJointIndexMismatch.md`; the manager judged
it pre-existing and out of that change's scope.
