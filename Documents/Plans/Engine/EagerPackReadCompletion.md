<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:41.336Z","dependsOn":[]} -->
# Require complete eager pack reads before publication

## Context

Final survivor `S005-C008` is a retained HIGH runtime file-boundary finding. The eager load task sizes `mPackFileData` from `file_size`, calls `fstream::read`, closes the stream, and validates ranges against the expected vector extent without checking stream state or `gcount` (`Engine/Source/File/PackChunks.cpp:425-483`). It then release-publishes `mbEagerLoadComplete`, so a short/error read after valid headers can expose zero-filled or partial required asset bytes.

The final disposition for `S005-C008` is recorded at `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:121`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:184` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:139`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to require the stream to report success and `gcount()` to equal the requested pack size before validating locations or release-publishing the eager map. Route any short/error read through the existing required-asset failure path; preserve valid range/header checks, eager map publication, and lazy loading.

## Critical files

- `Engine/Source/File/PackChunks.cpp:425-483` — eager read task and publication flag.
- `Engine/Source/File/PackChunks.cpp:445-471,930-949` — range validation and acquire consumer.
- `Engine/Source/File/AGENTS.md` — complete-read and packed-asset failure contract.

## In scope

- Complete-read validation for the eager pack stream before any map/data publication.
- Existing failure signaling and valid eager/lazy pack behavior.

## Out of scope

- Pack format, location/header validation rules, decoder changes, lazy-pool allocation, or duplicate identity handling.
- New retry/repair semantics, asynchronous architecture, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped PackChunks I/O behavior). Trigger: an opaque pack read is published to runtime consumers, but the correction is a local complete-read gate with unchanged format and valid loading behavior.

Preserve these invariants:

- `mbEagerLoadComplete` is released only after every requested pack byte was read successfully.
- No partial eager map/data reaches Scene, Model, Shader, or Raw consumers.
- Valid complete reads retain existing map, header, and consumer behavior.

## Acceptance criteria

- A forced short/error eager read after a valid header fails before `mbEagerLoadComplete` publication and follows the required-asset failure path.
- A complete eager pack still validates and publishes with unchanged bytes; lazy packs remain unaffected.
- Client and server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/PackChunkTypeFlagValidation.md`, `Documents/Plans/Engine/LazyPoolAllocationValidation.md`, `Documents/Plans/Engine/ChunkRecommitFailureHandling.md`, and `Documents/Plans/Engine/DuplicateChunkCrcRejection.md` own separate pack-header, allocation, state, and identity predicates. Keep stream completeness as the earliest read gate and reuse the common failure path.

## Notes

Origin: `S005-C008`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:121`, source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:184`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:139`. No source fix or build was performed during routing.
