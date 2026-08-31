<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:55.814Z","dependsOn":[]} -->
# Fail packed-texture loads when range recommit fails

## Context

Final survivor `S005-C011` is a retained HIGH runtime state finding. `LoadChunk` calls `RecommitChunkRange`; on false it stores `ChunkState::kReady`, notifies waiters, and returns even though the range can remain decommitted (`Engine/Source/File/PackChunks.cpp:705-717,1074-1081`). `ReadChunkData` treats `kDiskLoaded` and above as readable, while texture processing skips `kReady`, so failure is published as usable residency.

The final disposition for `S005-C011` is recorded at `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:124`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:226` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:142`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to route a failed recommit to the existing fatal/failed chunk result and leave the chunk out of `kReady`; do not notify waiters with a state that promises readable bytes. Preserve the normal eviction/device-reset reload sequence, successful recommit/read path, and required-asset failure policy.

## Critical files

- `Engine/Source/File/PackChunks.cpp:705-717` — texture load failure branch.
- `Engine/Source/File/PackChunks.cpp:959-973,1074-1081` — readable-state consumer and recommit helper.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:531-538` — ready-state adoption (read-only consumer).
- `Engine/Source/File/AGENTS.md` — recommit and corrupt-data state contract.

## In scope

- Failed `RecommitChunkRange` state/error propagation before `kReady` publication.
- Waiter/consumer behavior for failed and successful range reloads.
- Existing device-loss/eviction reset and valid texture loading behavior.

## Out of scope

- Pool allocation policy, texture validation, placeholder design, eviction strategy, or Vulkan resource recreation.
- Chunk format/state enum redesign, unrelated eager/lazy failures, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped packed-texture recovery behavior). Trigger: an OS-backed range-reload result controls runtime asset state, but the correction is local state/error propagation with unchanged chunk format and successful reload behavior.

Preserve these invariants:

- A chunk reaches `kReady` only after its range is committed and bytes are loaded/validated.
- Recommit failure cannot be read, adopted, or reported as usable texture data.
- Successful initial and post-eviction loads retain existing state transitions and notifications.

## Acceptance criteria

- Inject a recommit failure after eviction/device reset and verify no `kReady` publication or usable-texture waiter notification occurs; the existing failure path is reached.
- A successful recommit still reads, validates, and publishes the texture normally.
- Client and server `Debug|x64` builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/PackChunkTypeFlagValidation.md`, `Documents/Plans/Engine/EagerPackReadCompletion.md`, `Documents/Plans/Engine/LazyPoolAllocationValidation.md`, and `Documents/Plans/Engine/DuplicateChunkCrcRejection.md` own distinct admission failures. Keep failed-recommit state separate from header, stream, allocation, and identity validation.

## Notes

Origin: `S005-C011`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-007.md:124`, source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-005.md:226`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:142`. No source fix or build was performed during routing.
