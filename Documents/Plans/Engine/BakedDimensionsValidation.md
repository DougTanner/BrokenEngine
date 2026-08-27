<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:17.912Z","dependsOn":[]} -->
# Validate cached island dimensions before serialization

## Context

The frozen audit retained `CAI/shard-0007/003`. `ReadBakedDimensions` parses
JSON and converts all fields without semantic checks
(`DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp:292-309`).
`ExportIslandData` checks crop geometry but writes world dimensions directly to
the header (`DataPacker/Source/ExportJobs/ExportIsland.cpp:364-387,694-699`).
Runtime then publishes/uses those values for sampling. Source bytes match
baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

At the cache reader, require an object with finite positive width, height, and
elevation; validate full-texture/crop bounds and alignment; and verify the
world dimensions agree with the owning route's declared geometry before
`ExportIslandData` uses them. Route failures through the existing export
aggregate and preserve valid cached values/output.

## Critical files

- `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp` — JSON decode.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp` — header publication.
- `Engine/Source/Frame/IslandTerrain.cpp` — runtime consumer.

## In scope

- Semantic validation of every decoded `BakedDimensions` field before allocation, crop, or header publication.
- Failure handling for valid-JSON but impossible/ inconsistent values.

## Out of scope

- Gaea mesh/elevation pixel validation, route geometry redesign, or runtime sampler changes.
- JSON schema versioning or compatibility aliases.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: opaque cache values enter serialized
island headers and deterministic client/server terrain sampling. Every
accepted dimension is finite, positive, bounded, and consistent with its
crop/route.

Tier rationale: the fix adds semantic checks at one offline cache-reader
boundary and reports through the existing export aggregate; no header layout,
runtime consumer, or valid cached output changes, so only impossible values
take the new rejection path.

## Acceptance criteria

- Zero, negative, non-finite, or route-inconsistent dimensions fail before an island chunk is published.
- Invalid crop/full-texture relationships fail through the normal export aggregate.
- Valid cache records produce unchanged headers and runtime footprints.

## Notes

Origin: `CAI/shard-0007/003`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0007.md:68`.
No source fix, build, or runtime run was performed here.
