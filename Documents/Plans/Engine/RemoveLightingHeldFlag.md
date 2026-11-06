<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T12:41:11.685Z","dependsOn":[]} -->
# Remove the Per-Layer Lighting Held Flag

## Context

`PopulateLightingParameters` (`Engine/Source/Graphics/Render/LightingUniforms.cpp`) keeps two
function-local flags per lighting layer, `spbHeldVisibleAreas` and `spbTemporalResets`, and its
refresh predicate tests both. Originating gap: the held flag adds no information, because the
predicate's `!spbHeldVisibleAreas[i]` term is never true unless `spbTemporalResets[i]`, tested just
before it, is also true. Line numbers below are from `5c2e6a8d`.

Verified root cause, as an invariant over the two arrays: after the block at lines 76-80 runs,
`!spbHeldVisibleAreas[i]` implies `spbTemporalResets[i]`.

- First frame: both arrays start false, but `RenderTargetTextures::Create` calls
  `CreateLightingTextures`, which sets `gbLightingTemporalReset`
  (`Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:71`) before any frame reads the
  textures, so lines 76-80 set every layer's reset before line 85 first runs.
- The held flag is cleared at one site, line 72, inside the multi-cell basis branch whose line 69
  sets `gbLightingTemporalReset`, so lines 76-80 set every layer's reset in the same call.
- A layer's reset is cleared at one site, `TemporalAreaLatch::Update`
  (`Engine/Source/Graphics/Render/Render.h:136`), reached from line 103 only for `iRefreshLayer`;
  line 106 sets that same layer's held flag in the same branch.

Nothing outside this function reads either array (a search of `Engine/Source` finds them only in
`LightingUniforms.cpp`). Pre-existing: the redundancy landed with the split into two lighting
layers in `10e5a7dd`, whose message records it as the unaccepted held-flag design suggestion. The
plan text that named a held flag per layer is not in tracked history, and it scoped that landed
change only. `Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication` lists each
layer's area latch, held area, and reset flag, and names no held flag, so no document changes.

## Design

The author recommends deleting the flag and leaving every other term of the predicate as it is,
because the invariant makes the deletion a pure simplification:

- Delete the `spbHeldVisibleAreas` declaration (line 50).
- Delete the clear at line 72, keeping the `iHistoryResets` increment in that loop.
- Delete the `!spbHeldVisibleAreas[i]` term at line 85.
- Delete the set at line 106.

`spf4HeldVisibleAreas`, the held rectangles the crop test reads, stays. On a first frame or after a
reset the predicate still short-circuits on `spbTemporalResets[i]` before the crop test can read an
unset rectangle.

## Critical files

- `Engine/Source/Graphics/Render/LightingUniforms.cpp`

## In scope

- `PopulateLightingParameters`: the `spbHeldVisibleAreas` declaration, its clear, its term in the
  `pbNeedsRefresh` predicate, and its set.

## Out of scope

- `spf4HeldVisibleAreas`, `spbTemporalResets`, `gbLightingTemporalReset`, `siPendingLayer`, and the
  crop test `IsVisibleAreaInsideHeldCombineCrop`.
- The parallel `!sTemporalAreaLatch.bInitialized` term in the shadow refresh predicate
  (`Engine/Source/Graphics/Render/GlobalUniforms.cpp:327`).
- `TemporalAreaLatch` and `RetainedAreaBasis`.
- Any change to lighting cadence, layer refresh order, or published uniforms.

## Risk and invariants

- Tier 1. Trigger: local behavior-preserving work with no public signature or invariant exposure;
  the flag is function-local state.
- Client-only render state outside the CRC. No determinism, serialization, wire, shader, or
  threading exposure; `PopulateLightingParameters` keeps its single-entry-per-frame latch rule.
- `pbNeedsRefresh` holds the same value for every layer on every frame, given the invariant above,
  so refresh choice, `giLightingRefreshLayer`, and every published area are unchanged.

## Acceptance criteria

- `spbHeldVisibleAreas` no longer appears in `Engine/Source`, and the diff touches only the four
  sites in `## Design`.
- The client builds through `/compile`.
- The diff settles behavior through the invariant in `## Context`, so no harness run is required.

## Notes

- The server is unaffected because the file is whole-file `BT_CLIENT`.
