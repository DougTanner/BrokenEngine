<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:02.018Z","dependsOn":[]} -->
# Validate model indices against the vertex allocation

## Context

The frozen audit retained `CAI/shard-0006/006`. `ExportModel::Export` checks
index byte extents and copies decoded 16/32-bit values at
`DataPacker/Source/ExportJobs/ExportModel.cpp:64-126`, but never checks each
value against `uiVertexCount`. Runtime `BufferManager` likewise validates
counts/bytes, not index values. The source tree has no diff from baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

After decoding the selected index width and before `AllocateHeaderAndData`,
scan every index and reject any value greater than or equal to
`uiVertexCount` through the existing export-job failure aggregate. Keep exact
file-size and runtime structural checks as defense in depth; do not alter
valid index width selection or model layout.

## Critical files

- `DataPacker/Source/ExportJobs/ExportModel.cpp` — decoded index validation.
- `Common/DataFile.h` — model header/index contract.
- `Engine/Source/Graphics/Managers/BufferManager.cpp` — runtime structural consumer.

## In scope

- Value-range validation for all decoded `.MODEL` index elements.
- Structured rejection before model chunk allocation/publication.

## Out of scope

- Runtime GPU validation, topology policy, vertex remapping, or model format redesign.
- Changes to scene-generated model writers beyond preserving their valid output.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: opaque intermediate values cross a
serialized GPU-buffer boundary. Every emitted index is below the declared
vertex count; valid model chunks keep their current bytes and draw behavior.

Tier rationale: the Design fully specifies one localized producer-side scan
that rejects out-of-range decoded indices through the existing export-job
failure aggregate. No model layout, index width selection, or runtime consumer
changes, and valid models export byte-identical output.

## Acceptance criteria

- An exact-size model containing `[0, 1, 99]` for three vertices fails export and is not packed.
- Valid 16-bit and 32-bit models export unchanged.
- Runtime never receives a newly packed out-of-range index from this producer.

## Notes

Origin: `CAI/shard-0006/006`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:142`.
No source fix or build was performed in this routing stage.
