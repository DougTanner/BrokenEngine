# Smoke occupancy policy across pipeline recreation

Status: Open investigation; no implementation decision has been made.

Area: Engine

Record type: Non-executable reference material. This document intentionally has
no scheduler metadata and is not a Plan input.

Audit source: `CPT/shard-0029/001` in the frozen C++ Plan Trace Audit.

Frozen audit commit: `80896f33661aaab99cf180a96db54600099be652`

## Finding under investigation

`BufferManager::CreateSmokeHierarchicalBuffers` destroys and recreates smoke
occupancy/active-tile buffers and explicitly initializes the new occupancy
buffers (`Engine/Source/Graphics/Managers/BufferManager.cpp:584-631`).
`PipelineManager::CreateSmokeWindPipelines` invokes the hierarchy creation when
the pipeline manager is rebuilt
(`Engine/Source/Graphics/Managers/PipelineManager.cpp:223-253`). A healthy
pipeline-only graphics setting change can destroy the PipelineManager without
requesting `kSmokeTextures` (`Engine/Source/Graphics/Graphics.cpp:549-559,662-668,725-733`),
so retained `SmokeOne`/`SmokeTwo` images can outlive the newly empty occupancy
metadata.

Smoke deposit and spread shaders mark occupancy from image writes
(`Engine/Data/Shaders/Smoke/Smoke.frag:39-45`;
`SmokeSpreadOne.comp:65-78`; `SmokeSpreadTwo.comp:104-115`), while the Global
spread reads the occupancy buffers before later per-frame clearing
(`Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:337-382`).
The durable source trace establishes a lifecycle mismatch on the
pipeline-only recreation edge; no runtime scenario was used.

## Controlling contract and invariant

`Engine/Source/Graphics/Managers/BufferManager.AGENTS.md:17-21` requires smoke
occupancy and active-tile state to remain paired with smoke/wind ping-pong
textures. `Engine/Source/Graphics/AGENTS.md:7-12` and
`Engine/Source/Graphics/Managers/AGENTS.md:8-13` define partial recreation and
resource-consumer validity. The invariant under investigation is that the
occupancy metadata and retained image pair represent the same smoke state at
the first Global spread after a pipeline rebuild.

## Boundary and impact

The open boundary is healthy pipeline-only recreation: whether the new
occupancy hierarchy must reconstruct the retained image state or whether the
retained images must be reset together with the new hierarchy. Full device
recreation and explicit smoke-texture recreation already reset the image side
and are comparison paths, not the unresolved policy.

If the new occupancy map is empty while retained images are nonempty, the next
Global spread can skip old active tiles and leave their decay/advection
schedule frozen until a later deposit reseeds occupancy. If occupancy is
reconstructed incorrectly, the same boundary can publish stale or extra
active tiles.

## Open choices

These alternatives are recorded for a future decision; none is selected here.

1. **Rebuild occupancy from retained images.** Define a deterministic GPU or
   CPU-visible reconstruction of active tiles from `SmokeOne`/`SmokeTwo`, its
   synchronization point, and the mapping to ping-pong buffers and active-list
   headers. Prove that no stale image data is mistaken for a live tile.
2. **Reset the paired images.** Treat pipeline recreation as an explicit empty
   smoke-state boundary: clear/recreate both retained images together with the
   new occupancy and active-tile buffers, using the existing image-reset
   mechanism and preserving the first-spread ordering.

The choice must state whether preserving smoke simulation state across a
pipeline setting change is required by product behavior or whether an empty
reset is acceptable. It must not silently retain one half of the pair.

## Decisive questions and acceptance evidence

- Does a pipeline-only setting change promise continuity of existing smoke,
  or may it reset the simulation state? Which authority owns that decision?
- Can a focused scenario create nonempty smoke, rebuild only pipelines, and
  show that the first Global spread consumes a metadata/image pair with the
  same active tiles and ping-pong orientation?
- If reconstruction is chosen, how are image reads made visible before the
  hierarchy's first compute read, and how are stale tiles excluded?
- If reset is chosen, are both images, both occupancy buffers, active lists,
  headers, and descriptor bindings reset together without a per-frame full
  texture clear?
- Do boot, full device recovery, explicit smoke-texture recreation, and
  pipeline-only recreation retain their documented distinctions?

The eventual executable Plan is expected Tier 3 because the decision crosses
Graphics recreation, GPU image/buffer lifetime, compute synchronization, and
record-once consumers. Until the choice is made, no source fix is authorized.

## Provenance

- Frozen source candidate: `CPT/shard-0029/001`.
- Frozen consolidated index: `Temp/CppPlanTraceAudit/80896f33661aaab99cf180a96db54600099be652/consolidated-index.md`.
- `Documents/Plans/Engine/WindOccupancyInitializationRecovery.md` was reviewed;
  it owns wind occupancy initialization and wind-image pairing, not this
  smoke-specific retained-image policy. No exact existing record owns this
  finding.
- No source, shader, or scheduler change is part of this investigation.
