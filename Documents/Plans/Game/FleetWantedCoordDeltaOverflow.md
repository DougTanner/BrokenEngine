<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-12T00:11:26.931Z","dependsOn":[]} -->
# Compute the fleet-wanted coordinate delta without signed overflow

## Context

`UpdateFleetAndFlagshipNavigation` derives the direction a fleet should move by
subtracting the cell's own coordinate from the fleet's wanted coordinate in
`int32_t`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:70-71`):

```
int32_t iDeltaX = fleetWantedCoord.x - rStaticData.coord.x;
int32_t iDeltaY = fleetWantedCoord.y - rStaticData.coord.y;
```

Both operands span the full signed-int32 `GridCoord` domain, so the subtraction
overflows — undefined behavior in deterministic simulation code — whenever the
two coordinates are more than `INT32_MAX` apart. A wanted coordinate that far
from the cell is reachable only through the Debug agent fixtures, which accept
an unbounded `fleetWantedCoord` and store it verbatim
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp:454-455,498`,
parsed by `CoordFromParam` at
`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:284-291`,
which applies no range or adjacency rule). Gameplay never reaches it: fleet
wanted coordinates follow adjacency and stay one cell away.

This is pre-existing at the baseline of
`Documents/Plans/Engine/UnboundedRenderCoordinates.md`, which changed neither
this subtraction nor the injection path; it was recorded as an adversarial
review residual of that change and is outside its approved boundary. The cited
Plan's checked-arithmetic invariant ("checked coordinate addition never wraps",
`Engine/Source/Frame/AGENTS.md`) covers neighbor and transfer arithmetic; this
subtraction is the remaining unchecked coordinate arithmetic in the simulation.

Impact: undefined arithmetic inside a deterministic, CRC-contributing tick path
whose result selects a navigation direction.

## Design

Author's recommendation: compute both deltas in `int64_t` from the two
`int32_t` coordinates, which cannot overflow, and keep every downstream use as
a sign comparison. The four `bAlreadyValid` comparisons and the direction
selection that follow only test the sign and relative magnitude of the deltas,
so widening the two locals is sufficient and leaves the selected direction
identical for every coordinate pair the game itself produces.

Rationale for fixing the arithmetic rather than rejecting a distant wanted
coordinate at the injection site: the subtraction is the defect, it is the only
place the pair is combined, and the fix keeps the fixtures free to inject any
representable coordinate. Coordinate type and range validation in the shared
`CoordFromParam` helper belongs to
`Documents/Plans/Game/AgentCoordinateIntegralValidation.md` and is not needed
here.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:65-110` — the delta computation and every consumer of the two locals.
- `Engine/Source/Frame/AGENTS.md` — the checked coordinate arithmetic invariant this restores.

## In scope

- Overflow-free computation of the fleet-wanted delta in
  `UpdateFleetAndFlagshipNavigation`, and the types of the locals' direct
  consumers within that function where widening requires it.

## Out of scope

- Agent parameter validation, adjacency rules for injected fleet-wanted
  coordinates, and any change to `CoordFromParam`.
- Fleet navigation policy, direction selection rules, island destination
  choice, the pending-ticks countdown, or any other navigation behavior.
- Other coordinate arithmetic, transfer or neighbor paths, collection layout,
  serialization, wire format, and version gates.
- New unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: arithmetic inside a deterministic,
CRC-contributing simulation path. The change tightens one computation inside a
single function with the data format, trust boundary, and selected direction
unchanged for all representable inputs.

Preserve these invariants:

- The selected navigation direction is identical to today's for every
  coordinate pair that does not overflow, so simulation CRCs are unchanged for
  every reachable gameplay scenario.
- No undefined signed arithmetic remains on the path from a fleet-wanted
  coordinate to a navigation direction.
- Collection layout, transfer payloads, replay streams, save data, and wire
  format are unchanged.

## Acceptance criteria

- Injecting `fleetWantedCoord` at the extreme ends of the signed-int32 domain
  against an opposite-sign cell coordinate produces a defined direction
  consistent with the true sign of the separation, with no wrap.
- An ordinary adjacent fleet-wanted coordinate produces the same navigation
  direction and the same frame CRC as before the change.
- Server `Debug|x64` builds clean through `/compile`, and a recorded and
  replayed fleet-navigation scenario stays CRC-identical between client and
  server.
