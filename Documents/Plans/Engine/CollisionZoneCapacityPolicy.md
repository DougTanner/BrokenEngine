<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T18:28:41.828Z","dependsOn":[]} -->
# Collision Zone Capacity Policy At High Unit Density

## Context

The per-zone collision index buffers are fixed-size and overflow once one cell
holds enough units. With 1280 players in cell (0,0), both server and client
logged:

`Collision: ZonePair.indicesA overflow (count: 1024, capacity: 1024) pair A=0(cat=8,total=1280) B=1(cat=4,total=16) zone=(4,3). Increase kiCollisionZonePreallocate in Collision.h`

The capacity is `kiCollisionZonePreallocate = 1024`
(Engine/Source/Frame/Collision.h:11), applied by the `resize` calls at
Engine/Source/Frame/Collision.cpp:88-89, and the overflow warning is emitted at
Engine/Source/Frame/Collision.cpp:196, which drops the indices that do not fit.

Pre-existing and independent of any transport work: the same overflow appeared
in an unpaused control run with no network capacity breach, on both client and
server.

## Design

Decide and apply one capacity or overflow-handling policy for
`kiCollisionZonePreallocate`, choosing the smallest option that removes the
silent index loss at realistic densities:

1. Raise the constant to a density the game supports, justified by the
   observed per-zone counts, and accept the fixed memory cost per zone pair.
2. Grow the index buffers on demand, honoring the main-loop allocation
   tracking rules (`ScopedSuppressAllocationTracking` plus a `// Heap:`
   comment) and keeping the growth deterministic on both client and server.

Dropping indices past capacity must not remain the silent steady-state
behavior: whichever option is selected, an overflow that still occurs must
stay observable and must produce identical results on client and server so the
CRC does not diverge.

## Critical files

- Engine/Source/Frame/Collision.h (`kiCollisionZonePreallocate`)
- Engine/Source/Frame/Collision.cpp (zone buffer sizing at 88-89, overflow
  handling at 196)

## In scope

- The value of `kiCollisionZonePreallocate` or the growth policy for the
  zone-pair index buffers.
- The overflow diagnostic and the behavior when capacity is still exceeded.

## Out of scope

- The collision broadphase zone partitioning scheme and zone dimensions.
- Collision response, categories, layers, and pair selection.
- Per-cell unit caps and spawn policy.
- Any change to collision results at densities that already fit.

## Risk tier and invariants

Expected Change Workflow Tier 3: collision index selection feeds PostRender
state covered by the per-tick CRC, so client and server must behave
identically. Preserve bit determinism, existing collision results below the
current capacity, and the main-loop allocation-tracking rules.

## Acceptance criteria

- A harness run with 1280 units in one cell produces no
  `ZonePair.indices` overflow line on client or server.
- Client and server CRCs match across that run.
- No allocation-tracking `DEBUG_BREAK()` fires in the main loop.
- Collision behavior at low density is unchanged.
- Client and server both build.
