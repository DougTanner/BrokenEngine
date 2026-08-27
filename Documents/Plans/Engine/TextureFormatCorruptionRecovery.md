<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:43.589Z","dependsOn":[]} -->
# Soft-fail unsupported packed texture formats

## Context

The frozen audit retained `CAI/shard-0001/004`. The runtime trusts the on-disk
`TextureHeader::vkFormat`; `TextureUploadManager::ValidateTextureDimensions`
calls `ComputeImageByteSize` (`Engine/Source/Graphics/Managers/TextureUploadManager.cpp:429-449`),
whose unsupported-format branch asserts in `Common/TextureFormat.cpp:39-42`.
The upload worker catches `CorruptStreamException` but routes other exceptions
out of the thread. The source tree is unchanged from baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`, so this trust-boundary gap is
pre-existing.

## Design

Validate `vkFormat` against the supported texture set before shared size math
in both transfer-upload and same-queue fallback paths. Convert an unknown enum
to the existing `CorruptStreamException` flow, mark the chunk ready without
adoption, and preserve the white/neutral placeholder; retain fatal assertions
for internal callers that are not reading opaque pack metadata.

## Critical files

- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp` — transfer validation.
- `Engine/Source/Graphics/Managers/TextureManager.cpp` — fallback validation.
- `Common/TextureFormat.cpp` and `Common/DataFile.h` — format contract.

## In scope

- Opaque `TextureHeader::vkFormat` validation and soft-failure propagation in both runtime upload paths.
- Placeholder and chunk-state handling needed to avoid thread/main-loop termination.

## Out of scope

- Adding new Vulkan formats, changing DataPacker format selection, or changing placeholder visuals.
- Generic assertion policy outside packed texture metadata.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: opaque pack trust data crosses a
threaded upload boundary and client runtime adoption. Corrupt metadata must not
terminate the upload path; valid formats and placeholder ownership remain
unchanged.

Tier rationale: the fix adds a pre-specified enum membership check on two named
upload paths and routes failures into the existing `CorruptStreamException`
handling. It changes no format definition, no pack layout, and no behavior for
any currently emitted format.

## Acceptance criteria

- An unsupported on-disk format reaches a `CorruptStreamException`/ready-without-adopt path and leaves the placeholder bound.
- Neither upload worker nor render loop receives an unexpected format exception.
- All currently emitted formats upload as before.

## Notes

Origin: `CAI/shard-0001/004`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0001.md:102`.
No source fix, build, or runtime run was part of this routing stage.
