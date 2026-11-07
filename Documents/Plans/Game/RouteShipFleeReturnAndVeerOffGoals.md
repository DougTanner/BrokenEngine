<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-01T13:13:24.962Z","dependsOn":[]} -->
# Route Ship Flee, Island Return, and Veer-off Goals Around Terrain

## Context

The user asked for navigation to cover more cases, for example a fleeing ship choosing a navigable
point a certain distance away. Hidden kept-target chases already route for every type
(`NextRouteWaypoint` in `Collections/Battleships/Battleships.cpp` (`bRouteToTarget`),
`Collections/Omnis/Omnis.cpp` random-spawn and flagship-chase branches, and `RouteTactic` in
`Collections/Ships/FleetCoordination.cpp`), and fleet destinations go through `ComputeNavigation`
(`Collections/Ships/ShipBehaviours.cpp`). Paths here are relative to
`Projects/BrokenEngineSandbox/Source/Frame/`. Three goals still steer straight at a point and can
drive a ship into terrain on a plane with terrain collision; evidence checked at `60a977a7`:

- Island-center goal, random-spawn rows of all three types. `ComputeFleeOrReturnDestination`
  (`ShipBehaviours.cpp`) returns the nearest island center while `kReturnToIslandCenter` is set or
  no enemy is found. Fighters (`Fighters.cpp`, the `ChooseAttackTactic` false branch), Battleships
  (`Battleships.cpp`, the wanted heading `vecAiDirection` toward the destination after
  `ComputeFleeOrReturnDestination`) and Omnis
  (`Omnis.cpp`, `vecAiDirection` from that destination) steer straight at it. An island center lies
  over terrain by construction.
- Random-spawn Battleship flee. With `kFlee` set and not returning, `Battleships.cpp` takes the
  enemy as the destination and negates the wanted heading toward it, turning away from the threat
  with no goal point.
- Fighter veer-off, fleet and random-spawn. `VeerOffTactic` (`FleetCoordination.cpp`) sets the
  destination one meter from the ship directly away from `pVecVeerOffPoints`, reached through
  `ContinueVeerOff` and `AttackOrStartVeerOff`.

Omni finding: `ComputeFleeOrReturnDestination` sets `kFlee` on random-spawn Omnis by its distance
hysteresis, but no Omni steering or movement reads it (`ApplyAcceleratedMovement` takes no flags);
its only reader is the `describe_scene` flag list (`Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp`), so there is
no Omni flee to route.

Per-clearance-tier navigation graphs have landed (`Collections/Ships/AGENTS.md`
`## Ship Invariants`), and `NextRouteWaypoint` takes the ship's clearance tier.

## Design

The author recommends the following; each choice carries its rationale.

- Routing query: only `NextRouteWaypoint`. It already snaps its destination to the nearest point
  outside every navigation polygon (`NavQueryDirection` snaps with `SnapOutsidePolygon`), so no
  separate snap and no new pathfinding is needed. It reads whichever graph `NavigationPlaneData`
  returns for the ship's navigated plane, and takes the ship's clearance tier and a required
  `NavRectangleBoard&`, which each type's PostRender Update already builds.
- Island-center goal: route inside `ComputeFleeOrReturnDestination`, so one site covers all three
  types. It gains the cell static data, the ship's clearance tier, the pass's `NavRectangleBoard`,
  and the ship's target flight plane; when it returns the island center it returns
  `NextRouteWaypoint` toward that center instead. Its hysteresis keeps measuring the raw center, so `kfReturnDistance` and
  `kfReturnedDistance` keep their meaning.
  Callers pass `GetTargetFlightPlane` of the row's orders: the center sits at the lowest plane's
  height, and its nearest plane would route a higher Fighter or Omni around the lowest plane's
  larger contours although a return does not change plane. The Omni's in-sight override
  (`OmniCirclingPoint` of the enemy) and the Battleship flee and hidden-target overrides still
  replace the result where they apply today.
- Away goal: one shared helper beside `NextRouteWaypoint` in `ShipBehaviours.h/.cpp`, for example
  `FleeRouteWaypoint(rStaticData, iClearanceTier, vecPosition, vecThreat, rNavRectangleBoard, iTargetFlightPlane)`.
  It returns `NextRouteWaypoint` toward the point `kfFleeEndDistance` from the ship along the horizontal
  direction away from `vecThreat`. Rationale: the existing constant measured from the ship puts the
  point past the flee end distance from the threat (the ship flees from inside it), so it never
  degenerates to the ship's own position as a point at `kfFleeEndDistance` from the threat would
  near the end of a flee. The point is recomputed each tick from current positions, like the
  hidden-target route, so it follows a moving threat and needs no stored state; a stored point was
  considered and not recommended because it adds a PostRender column, transfer reset, and layout
  version for no gain. Decision for the claim session, at its plan interview: the user's wording
  recorded in `## Coordination` reads as one chosen location, while this recommendation moves the
  point every tick, so it confirms the recompute or takes the stored point with its column, transfer
  reset, and layout version. A zero horizontal offset yields the ship's own position, as
  `VeerOffTactic` does today.
- Battleship flee: while `kFlee` is set and the row is not returning, the destination becomes
  `FleeRouteWaypoint` from the kept target on the row's target flight plane, with the wanted
  heading toward it left unnegated. This also covers an island-free cell, where the enemy is the
  only destination.
- Veer-off: `VeerOffTactic` takes the cell static data and the pass's `NavRectangleBoard` and returns
  `FleeRouteWaypoint` from the veer-off point on the plane it already selects (`NearestFlightPlane`
  of the point). Rationale: veer-off is the same fly-away-from-a-point goal, so the same helper
  covers it; `ContinueVeerOff` and `AttackOrStartVeerOff` pass both through, and the veer-off start
  and end distances stay.
- No new random draw: draw counts and the arrival-grace, dying, and Omni transfer-lock steering locks
  are unchanged, because every edit sits inside a branch those locks already gate. No column and no
  saved or wire field, so transfer behavior is unchanged, unless the claim session takes the stored
  point in the away goal's decision, which adds only that point's column, transfer reset, and layout
  version.
- Bump the `Frame::kiVersion` base (`Frame.cpp`) by one: steering changes CRC'd
  positions without a collection version bump (`AGENTS.md` in that directory).
- On a plane without terrain collision the graph is empty and the route returns its destination,
  so steering there keeps today's direction.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `AGENTS.md` under `Collections/Ships`, `Fighters`, `Battleships`, `Omnis`

## In scope

- `ComputeFleeOrReturnDestination`: signature and the island-center return value.
- New `FleeRouteWaypoint` declaration and definition in `ShipBehaviours.h/.cpp`.
- `VeerOffTactic`, `ContinueVeerOff`, `AttackOrStartVeerOff` signatures and call sites inside
  `FleetCoordination.cpp`.
- The random-spawn branch of `FightersPostRender` Update, of `BattleshipsPostRender` Update (flee
  destination and wanted-heading negation), and of `OmnisPostRender` Update: the
  `ComputeFleeOrReturnDestination` calls and the Battleship flee destination.
- The `Frame::kiVersion` base.
- `/update-claude-docs` sync of the hidden-target route exception in
  `Collections/Ships/AGENTS.md` `## Ship Invariants`, the flee and return text in
  `Collections/Battleships/AGENTS.md`, the veer-off and island-return text in
  `Collections/Fighters/AGENTS.md`, and the random-spawn return text in `Collections/Omnis/AGENTS.md`.

## Out of scope

- Omni flee semantics and the unread Omni `kFlee` flag (recorded above).
- `OmniCirclingPoint` steering at a visible target and Fighter attack at a visible target: line of
  sight already means the straight segment clears the navigation polygons.
- Fleet Fighter escort slots: they fly the highest Fighter plane, whose graph is empty.
- `ComputeNavigation`, fleet modes, and the path-query cadence.
- `NavQuery`, the navigation bake, clearance, and clearance tiers.
- Flee, return, and veer-off distances, turn rates, and accelerations; new constants; new columns,
  except the stored point's column when the claim session takes it (`## Design` away goal).
- Debug waypoint rendering for these routes.

## Risk and invariants

- Tier 3. Trigger: determinism/CRC, because this changes CRC-visible steering for every ship type.
- Client and server run the same deterministic route queries on server-built, wire-shipped graphs;
  no RNG draw is added. Route cost: one `NextRouteWaypoint` per tick per fleeing, returning, or
  veering row; A* runs only when the straight segment is blocked.
- Return arrival still uses `kfReturnedDistance` from the raw center. Where the nearest clear point
  of an island's center lies farther than that, a returning ship circles that point instead of
  pressing into terrain; the acceptance run checks the scenario islands clear the return state.

## Acceptance criteria

- On a plane with terrain collision, a random-spawn Battleship fleeing with terrain on its away
  side steers along route waypoints around it, makes sustained progress, and clears `kFlee` by the
  existing hysteresis.
- A random-spawn Fighter, Omni, and Battleship returning to an island with another island on the
  straight line route around it and clear `kReturnToIslandCenter`, without sustained terrain
  contact.
- A Fighter veering off on a plane with terrain collision, with terrain on its away side, routes
  around it and clears `kVeerOff` at the existing end distance.
- On a plane without terrain collision or in an island-free cell, these goals keep today's
  direction.
- Code reading shows no added `common::Random` call, and no new column unless the claim session took
  the stored point, which adds only that point's column.
- Repeated replays give matching final tick CRCs, and client and server show no desync.
- Verification: `/compile` client and server, then `/agent-harness`. The supplied evidence records
  no harness setup for these scenarios; construct the smallest ones with `edit_frame` or
  `inject_payload` (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`) and
  record the command sequence. No unit tests.

## Coordination

No directional prerequisites. During the ship avoidance change the user worded this Plan's flee goal
as "flee behaviour should instead be 'choose a location and navigate to it'"; the `## Design` away
goal carries the decision that wording leaves to the claim session.

Reciprocal coordination with
`Documents/Plans/Game/SeparateFighterVeerOffFromFleetTactic.md`: that Plan moves the
`ContinueVeerOff` and `AttackOrStartVeerOff` call sites into a new `ChooseFighterFleetTactic`
inside `FleetCoordination.cpp`, so whichever lands second refreshes those call sites. Each Plan
keeps its own acceptance.

Reciprocal coordination with `Documents/Plans/Game/ConvertFightersToRampedTurn.md`: that Plan
rewrites the Fighter steering call directly below the `ComputeFleeOrReturnDestination` call and
also bumps the `Frame::kiVersion` base, so whichever lands second refreshes that block and takes
its own bump from the value it finds.

## Notes

- `NextRouteWaypoint`'s `rbFinal` output is unused by these callers, as in the hidden-target route.
