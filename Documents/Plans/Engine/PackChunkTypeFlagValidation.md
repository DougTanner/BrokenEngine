<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:38.004Z","dependsOn":[]} -->
# Reject multiple primary chunk types before pack publication

## Context

Final survivor `S005-C007` is a retained HIGH runtime pack-boundary finding. `DataTypeFromFlags` returns the first primary type bit in a fixed order, and `ValidateChunkHeader` compares only that result with the manifest type (`Engine/Source/File/PackChunks.cpp:97-129,179-190`). A header carrying `kIsland | kTexture` can therefore pass as an Island, enter the map, and take client texture-upload branches before Island consumers wait for it (`PackChunks.cpp:330-341,711-718,824-830`).

The final disposition for `S005-C007` is recorded at `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:120`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:170` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:138`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to require exactly one primary data-type bit before a chunk location enters either the eager or lazy map. Reject a union or missing primary type through the existing required-asset failure path before client routing or publication. Preserve valid subtype/compression flags, manifest matching, and ordinary client/server chunk loading.

## Critical files

- `Engine/Source/File/PackChunks.cpp:97-129,179-190,330-341,447-468,711-830` — type decoding, validation, and map/consumer routing.
- `Engine/Source/File/AGENTS.md` — opaque `.pack` trust boundary and failure policy.
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp` — client upload consumer (read-only evidence).

## In scope

- Exact-one-primary-type validation for eager and lazy chunk records.
- Existing corrupt-pack/required-asset failure propagation before map publication.
- Valid type, subtype, compression, client upload, and server read behavior.

## Out of scope

- Chunk layout/version, manifest format, CRC algorithm, decoder behavior, or texture/island consumer redesign.
- Changing valid combined subtype/compression flags, allocation policy, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped packed-asset admission behavior). Trigger: opaque `.pack` header flags select runtime consumers, but the correction is a local type-cardinality check with unchanged layout, valid data, and client/server phase structure.

Preserve these invariants:

- Every published chunk has exactly one primary data type and any allowed auxiliary flags.
- Invalid union/missing-type headers fail before eager/lazy map publication or consumer routing.
- Valid pack headers, CRCs, extents, eager/lazy loading, and required-asset handling remain unchanged.

## Acceptance criteria

- A header with two primary type bits, especially `kIsland | kTexture`, is rejected before map insertion and cannot reach texture upload or Island readiness.
- Valid Island, Texture, Model, Shader, and Raw headers with compression/subtype flags continue to load normally on client and server.
- Client and server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/EagerPackReadCompletion.md`, `Documents/Plans/Engine/LazyPoolAllocationValidation.md`, `Documents/Plans/Engine/ChunkRecommitFailureHandling.md`, and `Documents/Plans/Engine/DuplicateChunkCrcRejection.md` harden separate PackChunks boundaries. Keep type cardinality independent from read completeness, allocation failure, recommit state, and identity uniqueness, while preserving the common required-asset failure path.

## Notes

Origin: `S005-C007`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:120`, source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:170`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:138`. No source fix or build was performed during routing.
