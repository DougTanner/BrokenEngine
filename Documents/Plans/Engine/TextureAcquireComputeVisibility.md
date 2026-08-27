<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:40:12.181Z","dependsOn":[]} -->
# Fix: Publish lazy texture writes to compute consumers

## Context

The accepted survivor `CAI/shard-0031/002` shows that
`Texture::RecordAcquireBarrier` uses `VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT`
as the destination for every adopted lazy texture
(`Engine/Source/Graphics/Objects/Texture.cpp:151-168`).  The shipped smoke
noise texture is registered by the smoke compute pipelines and sampled by
`SmokeSpreadComputeA/B` (`Engine/Source/Graphics/Managers/PipelineManager.cpp:286-320`;
`Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h:23-41`), so its first consumer is
compute, not fragment.  The transfer-side same-family release also names only
fragment (`Engine/Source/Graphics/Managers/TextureUploadManager.cpp:554-594`).

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0031.md:92`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:859`.
All 16 frozen target rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this routing session made no
source edits.

Impact: on a valid separate-transfer-queue setup, smoke compute can read stale
or not-yet-visible noise data after adoption, corrupting occupancy/output state
or surfacing a synchronization validation/device error.

## Design

Author's recommendation: broaden the lazy-texture transfer release/acquire
shader stage scope to include both compute and fragment consumers.  Use
`VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT`
with `VK_ACCESS_SHADER_READ_BIT` for the acquire barrier, and use the same
destination stage mask for the same-family transfer-side publication.  Keep
the queue-family ownership indices, layouts, and QFOT optional branch unchanged;
the broad mask covers current model/terrain fragment users and smoke compute
without adding per-texture stage metadata.

## Critical files

- `Engine/Source/Graphics/Objects/Texture.cpp:151-169` — graphics-side acquire
  barrier.
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp:554-594` —
  transfer-side release/publication barrier.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:538-619` — adoption and
  acquire-command recording.
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:331-382` —
  smoke compute consumer ordering.
- `Engine/Data/Shaders/Smoke/SmokeSpreadCommon.h:23-41` — first-party compute
  sampler consumer (reference; no shader behavior change).

## In scope

- Updating the release/acquire destination stage masks so transfer writes are
  visible to compute and fragment shader reads.
- Preserving the current queue-family transfer, layout transitions, acquire
  command batching, descriptor registration, and lazy adoption state machine.
- Updating only local synchronization comments needed to describe the broader
  consumer scope.

## Out of scope

- Changing shader sampling, descriptor layouts, texture formats, transfer queue
  selection, or maintenance9 capability negotiation.
- Per-texture stage metadata, a new synchronization abstraction, and unrelated
  image barriers.
- Simulation CRC, wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger:
transfer-queue release/acquire barriers publish GPU image data to compute and
fragment consumers across Texture, TextureUploadManager, and Global command
recording.

Tier rationale: the Design specifies the exact edit — add
`VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` to two matching destination stage masks
inside the Graphics texture subsystem. Queue-family indices, layouts, the QFOT
branch, descriptors, and all serialized data stay as they are.

Preserve these invariants:

- Every adopted lazy texture's first shader read is covered by the acquire
  destination stage/access scope.
- Smoke noise transfer writes are visible before Global smoke compute executes,
  including when explicit queue-family ownership transfer is active.
- QFOT optional same-layout behavior and normal fragment/model texture adoption
  remain valid.
- No simulation CRC, wire, save, replay, or `.pack` bytes change.

## Acceptance criteria

- Synchronization validation passes a smoke-noise adoption on separate transfer
  and graphics queue families, with compute reads ordered after transfer writes.
- Model/terrain lazy textures still adopt and render through fragment consumers.
- Client Debug and Release builds pass `/compile`; a harness smoke scenario after
  lazy load has no stale-noise, queue-ownership, or barrier validation error.
- The release and acquire barriers expose matching compute+fragment destination
  scope, and no later semaphore is relied on to repair the missing visibility.

## Notes

The audit catalog identifies this as `CAI-EXT-010`; the Vulkan synchronization
stage/access rule is an external API contract to preserve during implementation.
