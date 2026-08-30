<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T22:51:49.694Z","dependsOn":[]} -->
# Fix: Reset smoke state when occupancy is recreated

## Context

`BufferManager::CreateSmokeHierarchicalBuffers` destroys and recreates the two
smoke occupancy buffers and their shared active-tile list, then initializes the
new occupancy buffers to zero
(`Engine/Source/Graphics/Managers/BufferManager.cpp:584-631`). Every pipeline
rebuild calls that function through
`PipelineManager::CreateSmokeWindPipelines`
(`Engine/Source/Graphics/Managers/PipelineManager.cpp:223-253`). Healthy
pipeline-only setting changes retain `SmokeOne` and `SmokeTwo` because they do
not request `DestroyFlags::kSmokeTextures`
(`Engine/Source/Graphics/Graphics.cpp:549-557,595-606,662-668,725-733`). The
result is retained, possibly nonempty images paired with newly empty occupancy
metadata.

Global spread uses occupancy to select the image tiles it processes before it
clears and rebuilds output occupancy
(`Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:263-316,337-382`).
Old smoke can therefore stop decaying or advecting until a later deposit marks
the affected tiles again.

The product decision recorded while converting
`Documents/Investigations/Engine/SmokeOccupancyRecreationPolicy.md` is to reset
smoke rather than reconstruct occupancy or preserve smoke continuity across a
pipeline-only recreation. No existing executable Plan owns this smoke-specific
lifecycle gap. `Documents/Plans/Engine/WindOccupancyInitializationRecovery.md`
owns the analogous wind reset and explicitly excludes smoke behavior.

## Design

Author's recommendation: whenever the smoke hierarchy is recreated, re-arm the
existing one-frame smoke clear by setting `gbSmokeClear`. Keep the current
record-once `SmokeClearA` and `SmokeClearB` indirect draws as the reset
mechanism: `RenderSmokeGlobal` publishes both clear draws for one frame, and
`CommandBufferRecordMain::RecordSmokeEmit` clears both ping-pong images. The
new occupancy buffers already start at zero, so this makes the images and
metadata converge on the same empty state without reconstructing occupancy or
adding a per-frame full-texture clear.

Place the lifecycle hook at the narrowest existing owner that runs for every
smoke-hierarchy recreation, including a pipeline-only rebuild. Preserve the
current Global-before-Main ordering: the first Global spread sees empty
occupancy, Main clears both retained images, and the following spread starts
from the paired empty state.

## Critical files

- `Engine/Source/Graphics/Managers/BufferManager.cpp:584-631` — smoke hierarchy
  recreation and occupancy initialization.
- `Engine/Source/Graphics/Render/Render.h:95-98` — existing `gbSmokeClear`
  lifecycle state.
- `Engine/Source/Graphics/Render/SmokeUniforms.cpp:58-108` — one-frame indirect
  clear publication and smoke-area reset.
- `Engine/Source/Graphics/Managers/CommandBufferRecordMain.cpp:217-239` —
  record-once clear draws for both smoke images.
- `Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:263-316,337-382`
  — first spread and occupancy drain ordering.
- `Engine/Data/Shaders/Smoke/AGENTS.md:16` — recreation clear contract.

## In scope

- Re-arming `gbSmokeClear` whenever `CreateSmokeHierarchicalBuffers` replaces
  smoke occupancy and active-tile storage.
- Preserving the existing one-frame indirect clear and Global-before-Main
  ordering so both images pair with the new zeroed occupancy buffers.
- Updating affected Graphics documentation only if the final code makes the
  recreation owner or ordering materially clearer.
- A focused live scenario that creates visible smoke, triggers a pipeline-only
  graphics setting rebuild, and verifies that both images and occupancy reset
  together without validation errors.

## Out of scope

- Reconstructing occupancy or active tiles from retained smoke images, or
  preserving smoke continuity through pipeline recreation.
- New shaders, readback paths, runtime reset APIs, or per-frame full-texture
  clears.
- Wind lifecycle changes, smoke simulation math, texture formats or extents,
  deposit behavior, and unrelated recreation tiers.
- Simulation CRC, wire, save, replay, serialization, or `.pack` changes.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change couples Graphics pipeline
recreation, GPU image and buffer lifetime, record-once Global/Main command
consumers, and their cross-submission ordering.

Preserve these invariants:

- The first Global spread may temporarily pair newly zeroed occupancy with
  retained ping-pong image contents; the existing Main clear restores the
  paired empty state before the following Global spread.
- A hierarchy recreation clears both images exactly once through the existing
  indirect mechanism, including when the images themselves were retained.
- The first Global spread may consume empty occupancy before Main performs the
  image clear; the next spread must observe the paired empty state.
- Existing enable/disable and explicit smoke-texture recreation behavior stays
  unchanged, with no new per-frame work after the one-frame reset.
- Client-only visual state remains outside deterministic simulation and its
  CRC.

## Acceptance criteria

- Toggling a pipeline-only setting while smoke is visible clears `SmokeOne`
  and `SmokeTwo`; the next Global spread consumes zeroed occupancy matching
  those images, with no frozen old plume tiles.
- The reset occurs once per hierarchy recreation and does not add a per-frame
  image clear, occupancy reconstruction, GPU readback, or command-buffer
  re-record outside the existing recreation flow.
- Fresh boot, smoke enable/disable, explicit smoke-texture recreation, and full
  Graphics recovery retain their existing paired-clear behavior.
- Client Debug and Release builds pass `/compile`; the focused `/agent-harness`
  scenario completes without Vulkan validation, synchronization, descriptor,
  or device-loss errors.

## Notes

The existing contracts already require smoke recreation to clear or drain
stale tiles (`Engine/Source/Graphics/Render/AGENTS.md:24-25` and
`Engine/Data/Shaders/Smoke/AGENTS.md:16`). This Plan applies that policy to the
pipeline-only hierarchy recreation edge; it does not introduce a new smoke
continuity guarantee.
