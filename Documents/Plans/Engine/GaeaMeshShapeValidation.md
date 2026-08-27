<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:15.740Z","dependsOn":[]} -->
# Validate Gaea mesh shape before subdivision

## Context

The frozen audit retained `CAI/shard-0007/002`. `LoadMesherMesh` forms position
and index pointers from TinyGLTF accessors without buffer-span, finite-value,
triangle-count, or index-range checks (`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:436-494`).
`SubdivideBeachBand.cpp:274-286` assumes complete triangles, and
`ProcessBakedRegion.cpp:228-250` indexes positions directly. Baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5` has no source changes.

## Design

Before any mesh loop, validate accessor byte ranges against exact buffers
(including stride/last element), finite XYZ coordinates, supported triangle
mode, index count divisible by three, and every decoded index below the vertex
count. Reject the island export through its existing aggregate and do not write
`MeshProcessed.bin`; keep valid Gaea output and downstream runtime checks.

## Critical files

- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp` — Gaea mesh reader.
- `DataPacker/Source/ExportJobs/Island/SubdivideBeachBand.cpp` and `ProcessBakedRegion.cpp` — unsafe consumers.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp` — serialized island output.

## In scope

- Complete accessor/payload/value validation before subdivision and cache publication.
- Failure propagation for malformed Gaea mesh output.

## Out of scope

- TinyGLTF implementation changes, topology conversion, subdivision policy, or runtime GPU validation.
- Elevation/BakedDimensions validation owned by separate Plans.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: opaque Gaea intermediates feed
serialized island geometry and runtime/GPU buffers. Every accepted mesh is
in-bounds finite triangle data; valid meshes retain current subdivision and
output.

Tier rationale: the Design enumerates every check to add in one offline
DataPacker reader before its existing loops, failing through the existing
export aggregate. No serialized layout, subdivision policy, or valid-mesh
output changes.

## Acceptance criteria

- An out-of-range index, incomplete triangle, invalid accessor span, or non-finite position fails before unsafe reads.
- No invalid `MeshProcessed.bin` or island chunk is published after the failure.
- Valid Gaea meshes produce unchanged geometry and indices.

## Notes

Origin: `CAI/shard-0007/002`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0007.md:51`.
No source fix, build, or harness was performed here.
