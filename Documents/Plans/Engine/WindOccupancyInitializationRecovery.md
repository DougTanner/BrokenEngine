<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:39:28.825Z","dependsOn":[]} -->
# Fix: Initialize wind occupancy with its recreated textures

## Context

The accepted survivor `CAI/shard-0029/001` shows that
`BufferManager::CreateWindHierarchicalBuffers` allocates both device-local
occupancy buffers without clearing them
(`Engine/Source/Graphics/Managers/BufferManager.cpp:654-677`).  The first
Global command records both occupancy dilates before its later per-frame
`vkCmdFillBuffer` (`Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:164-220`),
so the first spread reads unwritten memory.  A pipeline-only rebuild repeats
the problem: `PipelineManager::CreateSmokeWindPipelines` destroys and recreates
the occupancy buffers while retaining the wind images
(`Engine/Source/Graphics/Managers/PipelineManager.cpp:146,323`; the pipeline
recreation path is `Engine/Source/Graphics/Graphics.cpp:537-545`).

`RenderTargetTextures::CreateWindTextures` already clears both newly created
wind images through a one-shot command buffer
(`Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:242-297`), but that
clear is not paired with a pipeline-only occupancy rebuild.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0029.md:93`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:815`.
All manager target rows matched frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source change was made in
this routing session.

Impact: boot or pipeline-rebuild wind spread can publish random active tiles,
and a retained image pair can become permanently desynchronized from its new
occupancy map.

## Design

Author's recommendation: copy the existing smoke initialization pattern into
`CreateWindHierarchicalBuffers`: fill both occupancy buffers with zero in a
one-shot command buffer and add a transfer-to-compute buffer barrier before the
first Global dilate.  On a pipeline-tier recreation, clear/recreate the two
wind images at the same post-drain point before the new hierarchy is created;
the existing `CreateWindTextures` operation is the reset mechanism, so the
retained image pair and newly zeroed occupancy pair always start together.
Keep the per-frame active-tile header reset and do not attempt to infer old
occupancy from the old images.

## Critical files

- `Engine/Source/Graphics/Managers/BufferManager.cpp:654-677` — occupancy
  allocation and initialization.
- `Engine/Source/Graphics/Graphics.cpp:662-721` — safe post-drain pipeline-
  tier recreation hook.
- `Engine/Source/Graphics/Managers/RenderTargetTextures.cpp:242-297` — paired
  wind-image reset (reference and reuse).
- `Engine/Source/Graphics/Managers/PipelineManager.cpp:146,323` — hierarchy
  recreation order.
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:121-248` —
  first read and per-frame clear/barrier sequence.

## In scope

- Zero-filling both newly allocated wind occupancy buffers with the existing
  one-shot command-buffer mechanism and making the fill visible to compute.
- Ensuring any pipeline-only rebuild resets the retained wind images together
  with the new occupancy buffers after the existing worker/device drain.
- Preserving ping-pong bindings, active-tile headers, tile sizing, and the
  existing smoke hierarchy initialization.

## Out of scope

- Wind shader math, wind texture formats/extents, smoke advection, or the
  separate per-axis texel-size finding.
- Preserving wind simulation state across a pipeline rebuild; this Plan chooses
  an explicit empty reset because the current code has no inverse reconstruction
  from image contents.
- Device-loss full teardown, one-shot latch recovery, descriptor registration,
  or new runtime reset APIs.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the fix spans Graphics recreation,
render-target images, device-local buffers, one-shot synchronization, and
record-once compute consumers; GPU resource lifetime and cross-manager
integration are Tier-3 surfaces.

Preserve these invariants:

- The first occupancy dilate always reads initialized zeros for a newly empty
  wind image pair.
- After pipeline recreation, occupancy A/B correspond to the same cleared
  WindOne/WindTwo pair and retain their existing pipeline bindings.
- Every transfer fill is visible to the first compute read, and later per-frame
  dilate/clear barriers remain ordered.
- No simulation CRC, wire, save, replay, or `.pack` format changes.

## Acceptance criteria

- A fresh client boot shows a zeroed wind occupancy pair before the first Global
  dilate; no validation or uninitialized-buffer warning is emitted.
- Toggling a pipeline-only graphics setting while wind is active resets both
  wind images and occupancy buffers together, then produces deterministic empty
  initial spread state rather than stale/random active tiles.
- Client Debug and Release builds pass `/compile`; a harness wind/smoke scenario
  survives the pipeline rebuild without GPU validation or descriptor errors.
- A scoped trace confirms no per-frame full-texture clear or speculative state
  reconstruction was added.

## Notes

The audit's `VEC-EXT-003` catalog entry records the external VMA allocation
initialization claim; the implementation must continue to make initialization
explicit rather than relying on allocator contents.
