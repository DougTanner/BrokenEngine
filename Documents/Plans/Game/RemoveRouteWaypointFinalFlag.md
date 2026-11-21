<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-20T23:51:28.701Z","dependsOn":[]} -->
# Drop the Unread NextRouteWaypoint Final Flag

## Context

`NextRouteWaypoint`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h:83-85`)
returns its waypoint and also fills a `bool& rbFinal` out parameter, which
`ShipBehaviours.cpp:318-319` computes with an extra
`engine::NavQuerySnapToNavigable` call and an `XMVector2Equal`. All six callers
declare a `bool bFinalWaypoint = false;` local only to pass it, and none reads
it (paths under `Projects/BrokenEngineSandbox/Source/Frame/Collections/`):

- `Battleships/Battleships.cpp:488-489`
- `Omnis/OmnisUpdate.cpp:321-322` and `:345-346`
- `Ships/ShipBehaviours.cpp:340-341` (`FleeRouteWaypoint`) and `:382-383`
  (`ComputeReturnDestination`)
- `Ships/FleetCoordination.cpp:116-119` (`RouteTactic`)

The snap is a pure query on a const `NavData`
(`Engine/Source/Frame/NavQuery.h:20`), so removing it changes no simulation
state and saves one snap query per route call.

## Design

Remove the `rbFinal` parameter, the `rbFinal` clause of its declaration
comment, and the comment and assignment that fill it. At each caller delete the
`bFinalWaypoint` local and the argument. Battleships keeps its
`if (bRouteToTarget)` block braces.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/OmnisUpdate.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp`

## In scope

- `NextRouteWaypoint`: declaration, declaration comment, and definition.
- The `bFinalWaypoint` local and argument in the Battleships update's route to
  target, the two Omnis update routes, `FleeRouteWaypoint`,
  `ComputeReturnDestination`, and `RouteTactic`.

## Out of scope

- The ignored return value of `engine::NavQueryDirection`.
- The flight-plane argument of `NextRouteWaypoint` and its callers, which
  `Documents/Plans/Game/SingleFlightHeightShips.md` removes.
- Any other route helper.

## Acceptance criteria

- No `rbFinal` or `bFinalWaypoint` remains under
  `Projects/BrokenEngineSandbox/Source/`.
- Clean `/compile` builds of the BrokenEngineSandbox client and server.

## Notes

- Change Workflow tier: Tier 2. Trigger: a game header signature in
  `ShipBehaviours.h` used from four collection files. Behavior-preserving; no
  determinism, CRC, save, or replay exposure because the removed value is never
  read and the removed query is pure.
- `SingleFlightHeightShips.md` edits the same signature and call lines; that is
  a textual overlap, not an ordering constraint, so this Plan carries no
  `dependsOn` edge. `## In scope` names functions rather than line numbers so it
  stays valid whichever Plan lands first.
