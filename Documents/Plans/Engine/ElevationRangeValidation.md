<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:19.721Z","dependsOn":[]} -->
# Reject invalid Gaea elevation samples

## Context

The frozen audit retained `CAI/shard-0007/004`. `LoadElevationMeters` checks
file byte count and replaces only non-finite samples; every finite value is
scaled without a normalized-range or transformed-finiteness check
(`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:328-365`). Those values feed
crop/downsample, half-float serialization, and runtime sampling. The source
tree matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`, proving the
gap is pre-existing.

## Design

Validate every raw sample against the documented normalized `[0,1]` contract
and require the sea-level/elevation transform to remain finite and within the
configured representable range before crop, mask, quantization, or publish.
Treat non-finite and out-of-range input as a structured route export failure;
retain current valid terrain math and output.

## Critical files

- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp` — raw elevation validation.
- `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp` — downstream use.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp` — serialization/peak.

## In scope

- Raw normalized-range and transformed-finiteness/range checks in `LoadElevationMeters`.
- Failure propagation before any derived terrain data or island chunk publication.

## Out of scope

- Gaea output generation, BakedDimensions/mesh validation, half-float format redesign, or runtime terrain sampling.
- Changes to valid sea-level/elevation configuration.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: opaque terrain values feed
deterministic simulation/render payloads and quantization. Every accepted
sample is finite, in-range, and representable after transformation; valid
samples remain unchanged.

Tier rationale: the fix is a pre-specified range and finiteness test inside one
offline loader, failing the route through the existing structured export
failure. Terrain math, half-float serialization, and runtime sampling are
untouched, so valid Gaea output still produces identical bytes.

## Acceptance criteria

- A finite raw value outside `[0,1]`, NaN/Inf, or a transform overflow fails the route before serialization.
- No infinite or grossly out-of-range terrain value reaches half-float output or runtime sampling.
- Valid Gaea elevation outputs produce the same terrain bytes and peak values.

## Notes

Origin: `CAI/shard-0007/004`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0007.md:85`.
No source fix, build, or harness was performed during routing.
