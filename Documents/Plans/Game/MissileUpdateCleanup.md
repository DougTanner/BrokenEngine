<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:45:50.323Z","dependsOn":[]} -->
# Missile update cleanup: skip the empty-cell registry window, drop an unused parameter

## Context

Paths without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`. Nothing was built or
run for this Plan: every statement is from a code read. Locate by symbol if
line numbers moved.

### Registry window in a cell with no missiles

Line numbers are from commit `629ced2e`, except those in
`Ships/ShipQueries.cpp`, which follow `IsFlagship` becoming a file-static
function there after that commit.

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

### Unused fDeltaRotation parameter of SynchronizeMissile

Line numbers are from commit `213d2577`.

`SynchronizeMissile` in `Missiles/Missiles.cpp` (definition at line 88,
inside `#if defined(BT_CLIENT)`) ends its parameter list with
`[[maybe_unused]] float fDeltaRotation`. Its body (lines 89-135) syncs the
exhaust area light, the smoke trail, and the looping sound, and never reads
`fDeltaRotation`. Both callers still load `pfDeltaRotations` from the
PostRender row and pass it:

- `MissilesInterpolate::ClientInitialize`, `Missiles.cpp:163`;
- `MissilesInterpolate::Update`, `MissilesUpdate.cpp:81`.

The function is not declared in any header; its only other declaration is the
client-only forward declaration at `MissilesUpdate.cpp:34`. Logged as a
refactor candidate on 2026-11-13.

## Design

1. Registry window. Recommended (the author's recommendation): return from
   `MissilesPostRender::Update` when `rCurrent.iCount == 0`, before the
   `BuildShipRegistryWindow` call, in the shape `OmnisPostRender::Update`
   uses (`Omnis/Omnis.cpp:563-566`). Rationale: it is the smallest change,
   and it keeps the window build where the pipeline document places it.

   Caveat to carry into the change: the debug-only
   `ValidateRegistrySourceLayers` call inside `BuildShipRegistryWindow`
   (`Ships/ShipQueries.cpp:370-372`) then runs only in cells that hold
   missiles. That check guards the one reader the skipped cells do not run,
   so nothing loses protection, but a duplicate ship registry id is reported
   later than today. If the wording of `Engine/Source/Frame/AGENTS.md`
   `## Frame Registry` or of the pipeline document reads as "every tick"
   after the change, sync it.

2. Unused parameter. Recommended: delete the parameter and the argument at
   each call, because the value is dead and the `[[maybe_unused]]` attribute
   exists only to silence the warning the dead parameter would raise.
   1. Definition, `Missiles.cpp:88`: remove
      `, [[maybe_unused]] float fDeltaRotation`.
   2. Forward declaration, `MissilesUpdate.cpp:34`: remove
      `, float fDeltaRotation`.
   3. Caller `Missiles.cpp:163`: remove the trailing argument
      `, rPostRender.pfDeltaRotations[iIndex]`.
   4. Caller `MissilesUpdate.cpp:81`: remove the trailing argument
      `, rPreviousPostRender.pfDeltaRotations[i]`.

   The parameter is the last one and a scalar `float`, so the `XM_CALLCONV`
   vector-argument order (`FXMVECTOR` x3, `GXMVECTOR`) is unchanged.

## Critical files

- `Missiles/MissilesUpdate.cpp`
- `Missiles/Missiles.cpp`

## In scope

- `MissilesPostRender::Update` (`MissilesUpdate.cpp`): one early return
  before the `BuildShipRegistryWindow` call.
- `SynchronizeMissile` definition parameter list (`Missiles.cpp`) and its
  forward declaration (`MissilesUpdate.cpp`).
- The `SynchronizeMissile` call in `MissilesInterpolate::ClientInitialize`
  (`Missiles.cpp`) and in `MissilesInterpolate::Update`
  (`MissilesUpdate.cpp`): the last argument only.
- Documentation sync of `Engine/Source/Frame/AGENTS.md` `## Frame Registry`
  or `Documents/Architecture/FrameUpdatePipeline.md`, only if the caveat in
  Design step 1 requires it.

## Out of scope

- `BuildShipRegistryWindow`, `ValidateRegistrySourceLayers`, and the engine
  frame registry.
- The row loop of `MissilesPostRender::Update`, including the steering arms
  `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` owns.
- The `iCount == 0` guard in Missiles `PreCollision`.
- The `pfDeltaRotations` collection member, its serialization, CRC
  membership, and every other reader (missile homing in Missiles Update,
  agent queries and frame edits): it stays as is.
- `SynchronizeMissileTrail` and the body of `SynchronizeMissile`.
- Any other `[[maybe_unused]]` parameter in these files.
- Any other `AGENTS.md` change: none describes the `fDeltaRotation`
  parameter.

## Risk tier

Tier 2 (the author's classification). Trigger: scoped runtime behavior of
one subsystem: the change alters what a simulation phase does at runtime.
`MissilesPostRender::Update` skips the registry window build in a cell with
no missiles, and the debug-only `ValidateRegistrySourceLayers` check then
runs only in cells that hold missiles. That is more than Tier 1's
behavior-preserving work. The parameter removal alone is Tier 1: local
behavior-preserving work on a client-only free function declared in no
header, whose removed value was never read.

No CRC byte changes, so the determinism surface is untouched, and the replay
check below settles it. No wire, serialization, `Frame::kiVersion`, replay
format, threading, or trust exposure; the collection layout is untouched. The
workbuffer reservation is thread-local scratch, not frame state.

## Acceptance criteria

1. `MissilesPostRender::Update` returns before `BuildShipRegistryWindow` when
   the collection is empty; the diff settles this.
2. The `SynchronizeMissile` definition (`Missiles.cpp`) and forward
   declaration (`MissilesUpdate.cpp`) declare no `fDeltaRotation` parameter
   and have eleven parameters, ending at `float fPitch`; its two calls pass
   eleven arguments, ending at the `pfPitches` element, and pass no
   `pfDeltaRotations` element. Other `fDeltaRotation` names in both files,
   such as the local in `MissilesUpdate.cpp` and the `SpawnInfo` designator in
   `Missiles.cpp`, stay.
3. `/compile` builds the BrokenEngineSandbox client and server without errors
   or new warnings. The server build does not compile `SynchronizeMissile` or
   either call (all inside `BT_CLIENT` guards); for that change it is built
   only to confirm the guards still balance.
4. Through `/agent-harness`, a replay recorded on the build before the
   change, covering ticks with no missiles and ticks with homing missiles in
   one cell, plays back on the changed build with no per-tick CRC mismatch.
   The supplied evidence records no harness setup, so the executing session
   builds the scene.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
