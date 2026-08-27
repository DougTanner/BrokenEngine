<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:07.612Z","dependsOn":[]} -->
# Reject scene primitives the runtime cannot draw faithfully

## Context

The frozen audit retained `CAI/shard-0006/009`. `LoadVertices` checks for
indices but never checks `tinygltf::Primitive::mode` before `AppendIndices`
(`DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp:318-351`). The
export then optimizes as triangles, while model pipelines use triangle-list
topology. The source tree is unchanged from baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

At the scene input boundary, require `TINYGLTF_MODE_TRIANGLES` and an index
count divisible by three before appending/optimizing. Reject strip, fan, line,
point, and incomplete triangle primitives through the existing export
aggregate; preserve the current triangle-list runtime pipeline and valid
triangle output.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneVerticesLoader.cpp` — mode/count gate.
- `DataPacker/Source/ExportJobs/ExportScene.cpp` — export failure/optimization path.
- `Engine/Source/Graphics/Objects/PipelineCreator.cpp` — runtime topology contract.

## In scope

- Primitive mode and triangle-count validation before index append/optimization.
- Structured rejection of unsupported scene topology.

## Out of scope

- Topology conversion, line/point runtime pipelines, mesh optimization changes, or shader work.
- Accessor byte/value validation owned by other Plans.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: source mesh topology is serialized and
consumed by a fixed GPU triangle-list pipeline. Every accepted emitted index
stream has triangle-list semantics; valid triangle scenes remain unchanged.

Tier rationale: the fix is a two-condition gate — triangle mode and an index
count divisible by three — at one loader site, rejecting through the existing
export aggregate. No topology conversion, serialized layout change, or pipeline
change is involved, and valid triangle scenes emit identical geometry.

## Acceptance criteria

- A parseable triangle-strip/fan/line/point primitive fails export before publication.
- A triangle primitive whose index count is not divisible by three fails clearly.
- Existing triangle-list scenes produce the same geometry and index order.

## Notes

Origin: `CAI/shard-0006/009`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:196`.
No source fix or build was performed while routing this item.
