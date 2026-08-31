<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:59.138Z","dependsOn":[]} -->
# Reject duplicate chunk identities before pack publication

## Context

Final survivor `S005-C012` is a promoted HIGH PackChunks identity finding. Both lazy and eager map insertion paths log `Duplicate chunk CRC` and call `DEBUG_BREAK`, but continue with the first entry (`Engine/Source/File/PackChunks.cpp:330-341,447-468`). Since `ReadChunkData` resolves one map entry by CRC, a malformed manifest can publish two valid-looking payload locations under one asset identity and silently discard one.

The final disposition for `S005-C012` is recorded at `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:125`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:240` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:143`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to treat a failed eager or lazy `try_emplace` as a malformed pack-generation error and reject the complete required asset set before map publication. Preserve unique CRC identities, valid location/header checks, deterministic map behavior, and the existing required-asset failure channel; do not choose a duplicate winner.

## Critical files

- `Engine/Source/File/PackChunks.cpp:330-341,447-468` — eager/lazy map insertion.
- `Engine/Source/File/PackChunks.cpp:930-973` — single-identity lookup consumer.
- `Engine/Source/File/AGENTS.md` — chunk CRC identity and corrupt-pack policy.

## In scope

- Duplicate CRC detection and failure propagation before eager/lazy map publication.
- Valid unique chunk records and their existing lookup/loading behavior.

## Out of scope

- CRC algorithm changes, path-hash generation, chunk format/version, duplicate repair/merging, or type/stream/allocation checks owned by sibling Plans.
- Changing valid map ordering, required-asset policy, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped packed-asset identity behavior). Trigger: opaque manifest records select runtime assets by CRC, but the correction is a local duplicate-insertion failure with unchanged format and valid map behavior.

Preserve these invariants:

- Each published CRC identifies exactly one chunk location for a pack generation.
- Duplicate records fail before consumers can observe an arbitrary first entry.
- Valid unique records retain their current eager/lazy loading, CRC, and decoder behavior.

## Acceptance criteria

- A manifest with duplicate CRCs and independently valid locations fails before either map is published and follows the required-asset failure path.
- A manifest with unique CRCs continues to load all required chunks unchanged on client and server.
- Client and server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/PackChunkTypeFlagValidation.md`, `Documents/Plans/Engine/EagerPackReadCompletion.md`, `Documents/Plans/Engine/LazyPoolAllocationValidation.md`, and `Documents/Plans/Engine/ChunkRecommitFailureHandling.md` own separate header, stream, allocation, and state predicates. Keep duplicate identity rejection independent while sharing the same pre-publication failure boundary.

## Notes

Origin: `S005-C012`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:125`, source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:240`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:143`. No source fix or build was performed during routing.
