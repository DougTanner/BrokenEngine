<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:25.499Z","dependsOn":[]} -->
# Validate pack chunk ranges before publication

## Context

The frozen audit retained `CAI/shard-0001/001`. `PackChunks::LoadPackFiles`
reads eager packs into a vector and forms pointers from manifest offsets at
`Engine/Source/File/PackChunks.cpp:327-344`; its lazy path subtracts the header
offset from `ChunkLocation.uiSize` at `:213-226`. Neither path proves the
location and `ChunkHeader` extents fit the actual pack. DataPacker publishes the
manifest before the pack (`DataPacker/Source/Main.cpp:140-168`), so a torn
pair is reachable. `git diff` against baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5` has no runtime/source changes; this
is pre-existing.

## Design

At the runtime pack trust boundary, read the exact pack size and validate every
location's checked offset/size range, complete header, header CRC/path identity,
and compressed/uncompressed extents before forming eager pointers, lazy slices,
or publishing maps. Preserve the existing required-asset failure and soft lazy
load policies for the respective asset phases.

## Critical files

- `Engine/Source/File/PackChunks.cpp` — eager and lazy pack loading.
- `Common/DataFile.h` — `ChunkLocation`/`ChunkHeader` layout contract.

## In scope

- `PackChunks::LoadPackFiles` and its eager/lazy range publication checks.
- Header extent checks needed to keep every published pointer and decompressor input within its location.

## Out of scope

- DataPacker publication ordering, `contentCrc` verification, and consumer-specific validation.
- Changes to `.pack` layout, compatibility versions, or source asset generation.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: `.pack` serialization and opaque
external-data trust boundary cross the runtime asset-loading boundary. Every
published range and header extent must be inside the actual file, and valid
packs must retain current eager/lazy behavior.

Tier rationale: the Design fully specifies bounds checks added inside one
loader function that reject malformed packs through the existing
required-asset and soft-lazy failure policies. The `.pack` layout, compatibility
version, and producer are out of scope, so valid packs load through exactly
their current path.

## Acceptance criteria

- A manifest/pack pair with a short, shifted, or oversized location is rejected before pointer/map publication.
- Header compressed and uncompressed sizes that exceed the location are rejected before copy/decompression.
- Valid client and server packs load with unchanged maps, pointers, and asset behavior.

## Notes

Origin: `CAI/shard-0001/001`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0001.md:51`.
This routing stage creates no source fix, build, or harness result.

## Coordination

`Documents/Plans/Engine/PackChunkContentCrcVerification.md` also changes the
`PackChunks.cpp` load boundary; keep range rejection and payload-digest
verification as separate checks and re-read the current file when either Plan
is implemented.
