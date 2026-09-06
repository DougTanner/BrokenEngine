<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:59.200Z","dependsOn":[]} -->
# Aggregate client game population counters across the render transaction

## Context

The retained aggregate `CAI/shard-0050/001` identifies a render-publication
gap for Players/Blasters/Spaceships and subsumes the same-root Missile and
Spaceship findings `CAI/shard-0047/001` (Missiles) and
`CAI/shard-0049/002` (Spaceships).  Main rendering begins a collection
transaction and calls `FrameInterpolate::Render` for the camera and
each renderable active coordinate
(`Engine/Source/Graphics/Render/MainUniforms.cpp:536-562`;
`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:603-615`).  The Spaceship
renderer writes `kCpuCounterSpaceships` directly from the current cell on each
call (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp:81-90`), and the Blaster renderer does the same for
its population and rendered counters (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersRender.cpp:10-15`), so later
cells overwrite earlier values.  The Players renderer never writes
`kCpuCounterPlayers` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:182-230,288-292`), and the Missile
renderer writes `kCpuCounterMissiles` directly from the current cell
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp:74-77`).  When the renderable map has no camera entry,
the existing no-renderable-coordinate path runs BeginRender/EndRender but no
per-cell Render; however, the `rActiveCoords.empty()` guard at
`Engine/Source/Graphics/Render/MainUniforms.cpp:475-478` returns before those
phases, so direct counters can retain a prior populated frame.  Because
`GameBase::UpdateRenderInterpolation` skips its map update when active
coordinates are empty (`Engine/Source/GameBase.cpp:684-733`), the phase-only
fix must execute at this guard rather than rely on the later missing-camera
path.  `ProfileManagerBase::SetCount` is an overwrite, while the server
counter path sums active cells
(`Engine/Source/Profile/ProfileManagerBase.cpp:453-456`;
`Projects/BrokenEngineSandbox/Source/Frame/ServerCellStats.cpp:38-55`).

The source selectors are
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0047.md:50`,
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0049.md:71`,
and
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0050.md:63`;
the consolidated selectors are
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1125`,
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1161`, and
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1170`.  The consolidated duplicate catalog calls
this one publication-root family `DUP-006` and lists all three candidate IDs.
All assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the publication defect is
pre-existing, unresolved, and outside the audit work.

Impact: client profile diagnostics hide Players and under-report, report zero,
or retain stale populations for the other collections even while the same
render transaction publishes entities and rendered-subset counts.

## Design

Use a client-side population accumulator in the existing
BeginRender/Render/EndRender transaction.  At the
`rActiveCoords.empty()` boundary in
`Engine/Source/Graphics/Render/MainUniforms.cpp:475-478`, run the same
phase-only transaction before returning: call the game-level
`FrameInterpolate::BeginRender` and `EndRender` so collection accumulators are
reset and publish zero, without requiring camera or cell `Render` calls.  Keep
the existing phase-only indirect/debug flush ordering and the already-present
missing-camera phase-only path intact.  Reset the accumulator in BeginRender,
add each renderable cell's population for Players, Blasters, Missiles, and
Spaceships in Render, and publish the totals in EndRender
(`kCpuCounterPlayers`, `kCpuCounterBlasters`, `kCpuCounterMissiles`, and
`kCpuCounterSpaceships`); the Begin/End no-Render path must publish zero.  Add
the missing `BlastersInterpolate::BeginRender` and `EndRender` declarations to
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h` with the existing collection phase signatures so
`GameInterpolateTypes` reaches them through `ForEachBeginRender` and
`ForEachEndRender`.  Blasters' collection-owned transaction accumulator must
add each rendered cell's `iCount` and publish both
`kCpuCounterBlasters` and `kCpuCounterBlastersRendered` once at EndRender;
reset both at BeginRender so an empty render transaction cannot retain a prior
frame.  Keep the Players, Missiles, and Spaceships rendered/GPU-visible subset
counters and indirect GPU writes on their existing collection-owned lifecycles.
This one transaction contract resolves the shared publication root represented
by all three `DUP-006` candidates while retaining their distinct metric rows.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:591-627` — game BeginRender/Render/EndRender dispatch, including the per-coordinate render call.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h:33-56` — client render transaction declarations.
- `Engine/Source/Graphics/Render/MainUniforms.cpp:475-498,536-565` — empty-active and no-renderable early-return boundaries plus the per-coordinate render transaction path.
- `Engine/Source/GameBase.cpp:684-733` — empty-active interpolation behavior that can retain the previous render-interpolate map.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h:25-49` — owning `BlastersInterpolate` render-phase declaration surface, including the BeginRender/EndRender declarations required for tuple dispatch.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameCollections.h:3-21` — `GameInterpolateTypes` includes `BlastersInterpolate` in the game collection tuple.
- `Engine/Source/Frame/FrameUtils.h:167-212` — `ForEachBeginRender`/`ForEachEndRender` detect and invoke collection phase declarations.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersRender.cpp:182-230,288-292` — missing Players publication.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersRender.cpp:1-18` — Blaster population and rendered-counter transaction lifecycle.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Missiles/MissilesRender.cpp:18-131` — per-cell Missile overwrite and rendered-subset publication.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/SpaceshipsRender.cpp:55-90,207-212` — per-cell Spaceship overwrite and rendered-subset publication.
- `Engine/Source/Profile/ProfileManagerBase.cpp:453-456` — counter storage semantics.

## In scope

- Population aggregation and zero publication for client
  `kCpuCounterPlayers`, `kCpuCounterBlasters`, `kCpuCounterMissiles`, and
  `kCpuCounterSpaceships` across the existing main render transaction.
- The collection BeginRender/Render/EndRender declarations and definitions
  needed to add the accumulators and the missing Players publication,
  including `BlastersInterpolate`'s owning declarations in
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h`, its
  `GameInterpolateTypes`/`ForEachBeginRender`/`ForEachEndRender` participation,
  and the three `DUP-006` candidate regions.
- Aggregating and publishing `kCpuCounterBlastersRendered` from the
  collection-owned lifecycle, with a BeginRender reset and an EndRender zero
  publication when no per-cell Render ran.
- Updating the `Engine/Source/Graphics/Render/MainUniforms.cpp:475-478`
  empty-active boundary so the collection BeginRender reset and EndRender zero
  publication execute before the early return, without a camera/cell Render.
- Preserving existing rendered/GPU-visible subset meanings and indirect counts,
  alongside the required Blasters rendered-row aggregate, camera-first
  ordering, and the all-rings-empty Begin/End path.

## Out of scope

- Server counters, rendered/GPU-visible subset semantics other than the required
  `kCpuCounterBlastersRendered` transaction aggregation, GPU layout/geometry
  beyond existing indirect publication, simulation CRC, replay, save, wire
  data, or profile visibility policy.
- New profiling rows, per-cell display semantics, or collection/render
  refactors unrelated to these four client population counters.

## Risk tier and invariants

Expected Change Workflow Tier 3.  Trigger: independently owned Frame,
collection-render, and Profile publication surfaces must share one client-only
per-frame value; render remains outside simulation CRC.

Preserve these invariants:

- Each client population counter describes the sum across the current
  renderable active-coordinate set and is zero when that set has no renderable
  rows.
- `kCpuCounterBlastersRendered` describes the sum of Blaster `iCount` values
  encountered by this transaction, including zero when the rendered subset is
  empty; it is not overwritten by the last cell.
- The `rActiveCoords.empty()` boundary at
  `Engine/Source/Graphics/Render/MainUniforms.cpp:475-478` is a phase-only
  render transaction: collection BeginRender resets state and EndRender
  publishes zero before return, without touching camera/cell Render or
  simulation state.
- The Players row is refreshed in every transaction; rendered-subset cursors
  and indirect GPU publication retain their current meaning, with the required
  Blaster rendered-row aggregation performed at the collection transaction
  boundary.
- No server counter, simulation state, CRC, replay, save, wire, or `.pack`
  behavior changes.

## Acceptance criteria

- A camera cell with rows followed by an empty active neighbor reports the
  camera-plus-neighbor population for Players, Blasters, Missiles, and
  Spaceships rather than zero or the last cell's count; Blasters also reports
  the camera-plus-neighbor sum in `kCpuCounterBlastersRendered`, rather than
  the empty neighbor's zero.
- A frame with no renderable coordinates publishes zero population and zero
  rendered/GPU-visible subset counters, including `kCpuCounterBlastersRendered`,
  with no stale prior-frame value.  The check covers both an empty
  `rActiveCoords` at `Engine/Source/Graphics/Render/MainUniforms.cpp:475-478`
  (BeginRender/EndRender execute with no cell Render) and the existing
  non-empty active set with no camera entry at `:480-498`.
- Single-cell and multi-cell valid rendering retain existing indirect draws and
  rendered/GPU-visible subset counts while publishing all four aggregate
  population rows; a multi-cell Blaster case proves both Blaster counters
  equal the sum of each cell's `iCount`.
- Client `Debug|x64` builds clean through `/compile`; a profile-enabled render
  scenario observes all four corrected population rows.

## Coordination

The consolidated index's `DUP-006` groups
`CAI/shard-0047/001`, `CAI/shard-0049/002`, and `CAI/shard-0050/001` as one
client global population-publication root.  The final disposition retains
`CAI/shard-0050/001` as the aggregate primary: this Plan owns Players,
Missiles, Spaceships, and Blasters population publication, together with the
same transaction's rendered/GPU-visible counter preservation.  The Missile
and Spaceship records are subsumed here; no additional sibling Plan remains.
Their metric-specific acceptance cases remain represented in this Plan.

## Notes

No existing live Plan owned this aggregate metric set at routing time.  This
Plan is the durable owner for the retained `DUP-006` primary and contains all
three collapsed findings with their distinct metric coverage.
