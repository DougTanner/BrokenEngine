<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:45:50.323Z","dependsOn":[]} -->
# Skip the Missile-Update registry window in a cell with no missiles

## Context

Line numbers are from commit `629ced2e`, except those in
`Ships/ShipQueries.cpp`, which follow `IsFlagship` becoming a file-static
function there after that commit; locate by symbol if they moved. Paths
without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Nothing was built or
run for this Plan: every statement is from a code read.

`MissilesPostRender::Update` builds the registry query window before its row
loop on every call (`Missiles/MissilesUpdate.cpp:95`,
`BuildShipRegistryWindow`). The only read of `window` is the
`ResolveRegistryHandle` call at `:151`, inside the loop that starts at `:97`,
and the function ends when the loop does (`:216`). So with zero Missile rows
the whole build is unused.

The build is not free. `BuildShipRegistryWindow` walks every Fighter, Omni,
and Battleship row twice (`Ships/ShipQueries.cpp:320-329`, `:344-362`), calls
`NearestFlightPlane` per row, and reserves workbuffer storage (`:337`).
`Update` runs every tick in every awake cell, and a cell holds missiles only
after a Battleship launch or a transfer arrival.

No authority requires a window on every tick.
`Engine/Source/Frame/AGENTS.md` `## Frame Registry` says the registry keeps
no state between query windows, and
`Documents/Architecture/FrameUpdatePipeline.md`
`## Frame Registry Query Windows` describes when the window opens, not that
it must. The window writes no frame byte.

Originating gap: the in-function analysis phase of a findings-only deep
analysis of that directory (recursive), confirmed by an independent
verification read. Pre-existing debt, not an unmet acceptance criterion of
any change. Unmet criterion: a missile-free cell does no registry-window
work in Missiles Update.

## Design

Recommended (the author's recommendation): return from
`MissilesPostRender::Update` when `rCurrent.iCount == 0`, before the
`BuildShipRegistryWindow` call, in the shape `OmnisPostRender::Update` uses
(`Omnis/Omnis.cpp:563-566`). Rationale: it is the smallest change, and it
keeps the window build where the pipeline document places it.

Caveat to carry into the change: the debug-only
`ValidateRegistrySourceLayers` call inside `BuildShipRegistryWindow`
(`Ships/ShipQueries.cpp:370-372`) then runs only in cells that hold
missiles. That check guards the one reader the skipped cells do not run, so
nothing loses protection, but a duplicate ship registry id is reported later
than today. If the wording of `Engine/Source/Frame/AGENTS.md`
`## Frame Registry` or of the pipeline document reads as "every tick" after
the change, sync it.

Risk tier: Tier 2 (the author's classification). Trigger: scoped runtime
behavior of one subsystem: the change alters what a simulation phase does
at runtime. `MissilesPostRender::Update` skips the registry window build in
a cell with no missiles, and the debug-only `ValidateRegistrySourceLayers`
check then runs only in cells that hold missiles. That is more than Tier
1's behavior-preserving work. No CRC byte changes, so the determinism
surface is untouched, and the replay check below settles it. No wire,
serialization, `kiVersion`, or threading exposure. The workbuffer
reservation is thread-local scratch, not frame state.

## Critical files

- `Missiles/MissilesUpdate.cpp`

## In scope

- `MissilesPostRender::Update`: one early return before the
  `BuildShipRegistryWindow` call.
- Documentation sync, only if the caveat above requires it.

## Out of scope

- `BuildShipRegistryWindow`, `ValidateRegistrySourceLayers`, and the engine
  frame registry.
- The row loop of `MissilesPostRender::Update`, including the steering arms
  `Documents/Plans/Game/MergeMissileSteeringArms.md` owns.
- The `iCount == 0` guard in Missiles `PreCollision`.

## Acceptance criteria

1. `MissilesPostRender::Update` returns before `BuildShipRegistryWindow` when
   the collection is empty; the diff settles this.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, a replay recorded on the build before the
   change, covering ticks with no missiles and ticks with homing missiles in
   one cell, plays back on the changed build with no per-tick CRC mismatch.
   The supplied evidence records no harness setup, so the executing session
   builds the scene.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
