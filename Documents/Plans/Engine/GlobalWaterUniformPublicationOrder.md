<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:40:19.251Z","dependsOn":[]} -->
# Fix: Publish Global water state before its command-buffer copy

## Context

The accepted survivor `CAI/shard-0032/001` shows that
`Graphics::RenderGlobal` queues the Global command-buffer copy before
`RenderFrameMain` runs (`Engine/Source/Graphics/Graphics.cpp:231-247`).  Main
then mutates the mapped `GlobalLayout` for active water dimensions and
quality/fade-adjusted wave counts (`Engine/Source/Graphics/Render/MainUniforms.cpp:501-535,623`),
while the queued Global command begins by copying that buffer to device-local
uniform memory (`Engine/Source/Graphics/Managers/CommandBufferRecordGlobal.cpp:33-39`;
`Engine/Source/Graphics/Objects/Buffer.cpp:339-360`).  The current Main
indirect dispatch and wave arrays can therefore describe a different water
state from the copied Global values.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0032.md:53`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:867`.
All seven frozen target rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source was changed in this
routing session.

Impact: ordinary water LOD, quality, or camera-height transitions can render
stale displacement, skip required work, or use dimensions inconsistent with
the current wave arrays.

## Design

Author's recommendation: create one water-publication routine that runs from
`RenderFrameGlobal` before `SubmitGlobalCommandBuffer`.  Move the active LOD
dimensions, water displacement indirect dispatch, and
`PopulateGerstnerWaves` count/array writes into that routine; it may map the
same per-frame Main layout, but it must finish all Global and Main water fields
before the Global submit.  Remove the duplicate Main-phase writes and leave
the rest of Main layout population in `RenderFrameMain`.  Preserve the existing
quality/fade calculations and one current count/dispatch/array publication.

## Critical files

- `Engine/Source/Graphics/Render/GlobalUniforms.cpp:480-511` — Global phase
  and pre-submit insertion point.
- `Engine/Source/Graphics/Render/MainUniforms.cpp:317-436,501-535,591-626` —
  wave, active-dimension, dispatch, and Main phase code to move/split.
- `Engine/Source/Graphics/Render/Render.h` — cross-translation-unit water
  publication declaration if needed.
- `Engine/Source/Graphics/Graphics.cpp:168-247` — Global-before-Main submit
  ordering (reference; do not add a blocking GPU wait).
- `Engine/Data/Shaders/Water/WaterDisplacement.comp:34-42,86-118` and
  `Engine/Data/Shaders/Water/Water.vert:56-65` — matching consumers.

## In scope

- Publishing every GlobalLayout water field used by Global/Main commands before
  the Global submission/wake.
- Moving the current active-LOD dimensions, displacement indirect dispatch, and
  wave count/array generation into the pre-submit Global phase without changing
  their formulas.
- Keeping Main camera, collection, billboard, shield, and other non-water
  fields in their current phase.

## Out of scope

- Adding a GPU wait between Global and Main, changing command-buffer worker
  synchronization, or allocating a second persistent UBO.
- Water shader math, mesh topology, wave tuning, or the separate wind texel-size
  finding.
- Simulation CRC, wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the fix changes CPU publication order
for a shared UBO and indirect dispatch consumed by separate Global/Main GPU
commands across render translation units; threading and CPU/GPU integration are
Tier-3 surfaces.

Preserve these invariants:

- All GlobalLayout water counts and active dimensions are written before the
  Global command-buffer copy is queued.
- WaterDisplacement's dispatch and wave arrays use the same current counts and
  active LOD dimensions that Water.vert reads.
- The existing Global→Main GPU semaphore remains the execution ordering edge;
  no host write occurs after the Global submit for fields that Global copies.
- No simulation CRC, wire, save, replay, or `.pack` bytes change.

## Acceptance criteria

- A camera LOD transition and Low/Medium/High quality change produce matching
  Global UBO counts/dimensions, Main wave arrays, and displacement indirect
  dispatch in the same frame.
- Source inspection shows no `GlobalLayout` water mutation after
  `SubmitGlobalCommandBuffer`; no blocking GPU wait was added.
- Client Debug and Release builds pass `/compile`; a harness water transition
  scenario has no stale-displacement or GPU validation error.
- Existing wave phase, fade, and low-quality bypass behavior remains unchanged
  for a fixed camera/quality input.

## Notes

The report explicitly refuted a separate wave-array capacity issue; this Plan
owns only the host publication ordering.
