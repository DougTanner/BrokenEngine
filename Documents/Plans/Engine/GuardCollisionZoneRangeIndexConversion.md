<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-27T02:27:41.267Z","dependsOn":[]} -->
# Guard Collision::CalculateZoneRange Against Float-to-int64 Overflow

## Context

`Collision::CalculateZoneRange` (`Engine/Source/Frame/Collision.cpp:214-223`)
converts each broad-phase zone coordinate to `int64_t` via
`static_cast<int64_t>` before clamping the result with `std::clamp` to
`[0, kiCollisionZonesX - 1]` or `[0, kiCollisionZonesY - 1]`. Under C++23
`[conv.fpint]/1`, converting a non-finite `float` (`NaN`, `+/-inf`) or a finite
`float` whose truncated value lies outside `[INT64_MIN, INT64_MAX]` to
`int64_t` is undefined behavior; clamping after the cast does not prevent it.

`Collision::CalculateZoneRange` is called through
`Collision::CalculateObjectZoneRange` (`Engine/Source/Frame/Collision.cpp:225-251`)
from `Collision::InsertLayerObjectsIntoZones` (`Collision.cpp:286`) and
`Collision::Collide` (`Collision.cpp:768`). When a swept layer pair (for
example, a projectile layer paired with a ship layer) carries a huge finite or
non-finite endpoint, `fMinimumX`, `fMaximumX`, `fMinimumY`, or `fMaximumY` can
produce a float zone index outside `int64_t` range or non-finite before
`std::clamp` runs.

This pre-existing broad-phase issue was surfaced as a code-review residual on
`Engine/Source/Frame/Collision.cpp:219` outside the scope of terrain-trace work.

## Design

Recommended approach: assert finite inputs and bound each float zone coordinate
before converting to `int64_t` inside `Engine/Source/Frame/Collision.cpp` via a
small file-local helper (for example,
`ClampZoneIndex(float fZone, int64_t iZoneCount)`):

1. At entry to `Collision::CalculateZoneRange`, assert that all five float
   inputs (`fMinimumX`, `fMaximumX`, `fMinimumY`, `fMaximumY`, and `fRadius`)
   are finite via `ASSERT(std::isfinite(...))`. Under
   `.agents/references/cpp-conventions.md`, invalid internal non-finite values
   must assert rather than be silently substituted with valid zone indices
   (corrupt external inputs belong to the read boundary).
2. Compute the float zone index from the existing formula using
   `kfCollisionAreaMinimumX`, `kfCollisionAreaMinimumY`,
   `kfCollisionZoneWidth`, and `kfCollisionZoneHeight` (`Collision.cpp:10-13`).
3. Bound finite zone coordinates in `float` before `static_cast<int64_t>`,
   preserving existing edge saturation for coordinates whose original cast was
   defined and saturating finite coordinates outside `int64_t` range to `0` or
   `iZoneCount - 1`:
   - If `fZone <= 0.0f`, return `0`.
   - If `fZone >= static_cast<float>(iZoneCount - 1)`, return
     `iZoneCount - 1`.
   - Otherwise return `static_cast<int64_t>(fZone)`, whose operand is locally
     proven finite and in `(0.0f, static_cast<float>(iZoneCount - 1))`.

Rationale: `kiCollisionZonesX` and `kiCollisionZonesY` (`Collision.h:8-9`) are
small integers that are exact in `float`. For every finite zone coordinate
whose original `static_cast<int64_t>` was defined (truncated value in
`[INT64_MIN, INT64_MAX]`), bounding in `float` before the cast produces the
exact same `int64_t` index or edge saturation as `std::clamp(static_cast<...>)`,
while finite coordinates outside `[INT64_MIN, INT64_MAX]` saturate to `0` or
`iZoneCount - 1` without `[conv.fpint]/1` UB and non-finite internal inputs
assert.

## Critical files

- `Engine/Source/Frame/Collision.cpp` — `Collision::CalculateZoneRange`
  (`:214-223`)

## In scope

- `Collision::CalculateZoneRange` in `Engine/Source/Frame/Collision.cpp`
  (`:214-223`) and an optional file-local helper directly above it to bound
  float zone indices before `static_cast<int64_t>`.

## Out of scope

- Changing `Collision::CalculateObjectZoneRange`, `Collision::AddLayer`,
  `Collision::InsertObjectIntoZones`, `Collision::Collide`, or zone grid
  dimensions (`kiCollisionZonesX`, `kiCollisionZonesY`).
- Changing game-side save-load validation in
  `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`.
- Changing `Frame::kiVersion`, save/replay formats, wire protocol, or
  `/fp:strict` simulation/CRC output for normal in-cell gameplay.

## Acceptance criteria

1. Code inspection of `Engine/Source/Frame/Collision.cpp:214-223` confirms
   `Collision::CalculateZoneRange` asserts that `fMinimumX`, `fMaximumX`,
   `fMinimumY`, `fMaximumY`, and `fRadius` are finite
   (`ASSERT(std::isfinite(...))`) instead of silently substituting valid indices
   for non-finite inputs, and that every `static_cast<int64_t>` operand in
   `Collision::CalculateZoneRange` (or its local helper) is bounded in `float`
   to `(0.0f, static_cast<float>(kiCollisionZonesX - 1))` or
   `(0.0f, static_cast<float>(kiCollisionZonesY - 1))`, saturating finite
   out-of-range coordinates (including those outside `int64_t` range) to `0` or
   `iZoneCount - 1` without `[conv.fpint]/1` UB.
2. Code inspection confirms finite in-bounds coordinates produce the identical
   `ZoneRange` indices as before, preserving `/fp:strict` collision and CRC
   outcomes.
3. `/compile` builds `BrokenEngineSandbox` (`Debug|x64`, `Release|x64`) and
   `BrokenEngineSandboxServer` (`Debug|x64`, `Release|x64`) cleanly.

## Notes

- **Change Workflow tier:** Tier 2 — scoped runtime behavior change inside one
  engine subsystem (`Engine/Source/Frame/Collision.cpp:214-223`); preserves
  bit-identical broad-phase zone indices and `/fp:strict` CRC output for valid
  in-bounds inputs, with no change to `Frame::kiVersion`, serialization,
  save/replay compatibility, wire protocol, threading, or trust boundaries.
- **Exposure:** Shared client/server compilation (`BT_CLIENT` and `BT_SERVER`;
  `ModelViewer.vcxproj` does not compile `Collision.cpp`); no allocation,
  shader, `.pack`, or live-harness verification required (non-finite and
  huge-coordinate broad-phase inputs are not produced by ordinary harness
  commands and are verified by code inspection plus multi-target build).
