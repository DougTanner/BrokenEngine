<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:52.471Z","dependsOn":[]} -->
# Validate lazy-pool sizes and allocations before pointer publication

## Context

Final survivor `S005-C009` is a retained HIGH PackChunks allocation finding. `LoadPackFiles` sums rounded manifest sizes in signed `int64_t` without checked addition and publishes unchecked `VirtualAlloc` pool/scratch results and `_aligned_malloc` read buffers (`Engine/Source/File/PackChunks.cpp:357-423`). The final locator verified that failed `VirtualAlloc` returns null. No admission or failure channel stops pointer formation or background loading before null/overflowed storage is used.

The final disposition for `S005-C009` is recorded at `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:122`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:198` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:140`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to use checked cumulative arithmetic for the lazy pool and reject a nonrepresentable total before reservation. Check pool, per-thread scratch, and aligned read-buffer allocation results before forming offsets, pointers, or starting loader work, routing failure through the existing required-asset halt path. Preserve valid allocation sizes and chunk map behavior.

## Critical files

- `Engine/Source/File/PackChunks.cpp:357-423` — cumulative sizing and allocation sites.
- `Engine/Source/File/PackChunks.cpp:368-372,727-785` — pool/read-buffer pointer consumers.
- `Engine/Source/File/PackChunks.h` — pool/thread ownership state.
- `Engine/Source/File/AGENTS.md` — opaque pack allocation and failure contract.

## In scope

- Checked lazy-pool size accumulation and representability rejection.
- Result checks for pool, scratch, and aligned read-buffer allocations before publication/use.
- Existing required-asset failure propagation and valid lazy/eager behavior.

## Out of scope

- Pack layout/version, manifest count policy, chunk type/CRC validation, recommit handling, or allocator implementation changes.
- A fallback placeholder, silent truncation, compatibility mode, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped PackChunks allocation behavior). Trigger: opaque manifest sizes and OS allocations control runtime packed-data storage, but the correction is local arithmetic/result validation with unchanged format and valid loading.

Preserve these invariants:

- Every lazy chunk offset and backing storage pointer is representable and valid before loader publication.
- Allocation failure or cumulative overflow fails the required asset set before worker dereference.
- Valid manifests, pool layout, scratch sizing, and chunk loading remain unchanged.

## Acceptance criteria

- A manifest whose rounded sizes overflow the cumulative type or whose pool/scratch/read-buffer allocation fails is rejected before pointer publication or background work.
- A valid generated manifest loads all lazy chunks with unchanged offsets and bytes.
- Client and server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/PackChunkTypeFlagValidation.md`, `Documents/Plans/Engine/EagerPackReadCompletion.md`, `Documents/Plans/Engine/ChunkRecommitFailureHandling.md`, and `Documents/Plans/Engine/DuplicateChunkCrcRejection.md` cover independent header, stream, state, and map-identity checks. Keep allocation/result validation before those consumers and preserve the shared required-asset failure path.

## Notes

Origin: `S005-C009`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:122`, source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:198`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:140`. External claim `EXT-022` was VERIFIED. No source fix or build was performed during routing.
