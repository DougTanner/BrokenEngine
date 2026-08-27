<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:03.854Z","dependsOn":[]} -->
# Validate raw texture headers before forming payload ranges

## Context

The frozen audit retained `CAI/shard-0006/007`. `ProcessRawTexture` parses a
header and immediately forms `fileBytes.begin() + header.iPayloadOffset` at
`DataPacker/Source/ExportJobs/ExportTexture.cpp:113-143`. The shared parser
documents parse/locate-only behavior (`Texture.cpp:651-687`), and the migration
filter can leave a short raw intermediate for this reader. No source change
exists relative to baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Validate minimum header shape, recognized format, nonnegative/representable
dimensions and mip count, payload offset bounds, and codec-specific payload
extent before constructing the vector range or allocating inflated storage.
Throw the existing structured export error so the job aggregate handles the
bad intermediate; retain valid raw and cubemap paths.

## Critical files

- `DataPacker/Source/ExportJobs/ExportTexture.cpp` — raw reader boundary.
- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` and `.h` — header parser contract.
- `DataPacker/Source/ExportJobs/Texture/MigrateLegacyIntermediates.cpp` — upstream routing.

## In scope

- Header/payload bounds and dimension/mip/codec validation before all raw-reader pointer arithmetic.
- Failure propagation through the existing DataPacker export aggregate.

## Out of scope

- Rewriting the shared parser, changing legacy formats, or compression algorithms.
- The separate Gaea mesh/raw-elevation trust boundaries.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: opaque intermediate files drive
serialized texture payloads and allocation/pointer arithmetic. No malformed raw
file may reach iterator, decompression, or LZ4 input; valid intermediates remain
byte-compatible.

Tier rationale: the change adds pre-specified bounds and shape checks inside one
raw-reader function and reports failures through the existing structured export
error. No header layout, codec, or valid-intermediate output byte changes.

## Acceptance criteria

- A short or offset-out-of-range raw intermediate fails as one export error without undefined iterator arithmetic.
- Invalid dimensions, mips, and codec payload sizes are rejected before allocation/decompression.
- Valid encoded and cubemap intermediates still pack unchanged.

## Notes

Origin: `CAI/shard-0006/007`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:160`.
No source fix, build, or producer run was part of this route.
