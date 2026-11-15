<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:16:37.780Z","dependsOn":[]} -->
# Remove dead members and duplicate creation code in BufferManager

## Context

A path without a directory is under `Engine/Source/Graphics/Managers/`. Lines
are those at commit `401e4b75`; searches covered `Engine`, `Projects`,
`Tools`, `DataPacker`, and `Common`.

- Three public members are written, used on the next lines, and zeroed, with
  no other reader: `miSmokeActiveTileBufferSize` (`BufferManager.h:70`;
  `BufferManager.cpp:623`, `:624`, `:648`), `miWindActiveTileBufferSize`
  (`BufferManager.h:80`; `BufferManager.cpp:667`, `:673`, `:695`), and
  `InstanceManager::mMaxMultisampleCountVkSampleCountFlagBits`
  (`InstanceManager.h:49`; `InstanceManager.cpp:529-531`).
  `.agents/references/cpp-conventions.md` (derived members) applies. The two
  occupancy sizes are read elsewhere
  (`CommandBufferRecordGlobal.cpp:206-207`, `RenderTargetTextures.cpp:355`)
  and are not part of this.
- The `BufferManager` constructor (`BufferManager.cpp:91-94`) and
  `CreateSwapchainDependentBuffers` (`:296-299`) hold the same three
  statements: read the framebuffer count, assert it, call
  `InitializePerCommandBufferBuffers`. That function has exactly those two
  callers. `CreateSwapchainDependentBuffers` has one outside caller
  (`Engine/Source/Graphics/Graphics.cpp:425`), on the branch where the
  manager already exists, so the two paths never both run.
- Five copies of one 14-line block pack 16-bit indices and then vertices
  into one buffer: `BufferManager.cpp:137-150`, `:180-193`, `:213-226`,
  `:233-246` (debug meshes, identical except the member and the name) and
  `:21-34` (`Quads`, same shape with stride `sizeof(float) * 2` and the
  literal count 6, which equals the index array size).

Originating gap: the architecture phase (cohesion lens, the dead members)
and the in-function phase (the two duplicates) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criteria: none of the
three names remains a member; the framebuffer count is read and asserted in
one place; the index-then-vertex packing for 16-bit meshes is written once.

## Design

Recommended:

- Turn each of the three members into a local at its write site and delete
  the zeroing stores. The `ASSERT` at `InstanceManager.cpp:531` keeps working
  on the local.
- The constructor calls `CreateSwapchainDependentBuffers` at the position of
  its three statements, between the model loop and the particle buffers, so
  no ordering changes.
- One file-local function with five parameters (buffer, name, indices,
  vertices, stride) replaces the five blocks.

## Critical files

- `Engine/Source/Graphics/Managers/BufferManager.cpp`
- `Engine/Source/Graphics/Managers/BufferManager.h`
- `Engine/Source/Graphics/Managers/InstanceManager.cpp`
- `Engine/Source/Graphics/Managers/InstanceManager.h`

## In scope

- The three members and their writes, listed above.
- `BufferManager.cpp:91-94` and `:294-302`, and the declaration at
  `BufferManager.h:123` as far as the merge needs it.
- The five packing blocks and one new file-local function.

## Out of scope

- The water mesh (`BufferManager.cpp:783-796`): it uses 32-bit indices from
  vectors.
- The two skinning bump allocators and their grow paths (`:487-571`), and
  the creation-time identity fills (`:359-407`).
- `ASSERT(bInserted)` at `BufferManager.cpp:88`.
- `miSmokeOccupancyBufferSize` and `miWindOccupancyBufferSize`.

## Risk tier

Tier 2 — scoped behavior. Trigger: it removes three public members from
headers `Engine.h` aggregates, which Tier 1's "no public signature" excludes.
No runtime behavior, threading, determinism, layout, or wire exposure: every
removed member is unread and both merges keep their statements and order.

## Acceptance criteria

1. The three criteria in `## Context` hold in the source.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on boots,
   renders a world frame, and survives one window resize (the
   swapchain-dependent buffer path) with no validation error. The supplied
   evidence records no harness setup.

## Coordination

None of these Plans depends on another. Whichever lands later re-derives its
lines by symbol from the landed files:

- `Documents/Plans/Engine/ShareWholeBufferBarrierBuilder.md` rewrites the
  barrier at `BufferManager.cpp:605-616`, next to the smoke active-tile size.
- `Documents/Plans/Engine/ValidateEnabledOptionalDeviceFeatures.md` adds rows
  to the required-feature table at `InstanceManager.cpp:475-484`, in the same
  function as the multisample lines at `:529-531`.

## Notes

- The executing session confirms no private-game use of the three removed
  members where that source is available, and reports it as unconfirmed
  otherwise.
