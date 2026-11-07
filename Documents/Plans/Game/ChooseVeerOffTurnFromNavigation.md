<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T22:08:09.395Z","dependsOn":["Documents/Plans/Game/RouteShipFleeReturnAndVeerOffGoals.md"]} -->
# Choose a Fighter's Veer-off Turn Direction From Navigation

## Context

While planning the ship avoidance change, which adds rectangles of tagged large ships to A* path
searches, the user said a veer-off should maybe choose its turn direction based on navigation.
Originating gap: nothing chooses that side.

Paths below are relative to `Projects/BrokenEngineSandbox/Source/Frame/Collections/`, and line
numbers are from `dcd383ae`. A Fighter within the veer-off start distance of its target stores the
enemy position (`AttackOrStartVeerOff`, `Ships/FleetCoordination.cpp:67-77`), and `VeerOffTactic`
(`:29`) aims directly away from that point. The Fighter was flying at the enemy, so the wanted heading
is close to directly behind it. The turn side then comes only from the sign of a nearly degenerate
cross product (`Fighters/Fighters.cpp:637-638` for a fleet row, `SteerHeadingTowards` at `:659` for a
random-spawn row), never from what lies on either side. Beside terrain or a Battleship other than the
target, one side can be blocked while the other is clear. The target Battleship's own rectangle almost
never decides: a side test's segment runs perpendicular to the away direction from the ship, so it
reaches that box only when the ship is closer to the Battleship than the box's half diagonal. A
straight attack run starts its veer-off near the `kfVeerOffStartDistanceSquared` distance, which is
farther, and a start inside the box drops the rectangle from both side tests.

`Documents/Plans/Game/RouteShipFleeReturnAndVeerOffGoals.md` routes the away goal around terrain. That
reports a side only when the straight line away is blocked, and keeps the goal directly away, so it
leaves this gap open when the away line is clear and one turn side is not.

## Design

The author recommends the following; each choice carries its rationale. The user's wording was
tentative, so the claim session confirms the intent at its plan interview.

- Side choice, once, at veer-off start in `AttackOrStartVeerOff`: test the two segments from the ship
  to the points the veer-off end distance out on each side, perpendicular to the away direction,
  with `HasShipLineOfSight` (`Ships/ShipBehaviours.h`) on the Fighter's clearance tier and stored
  plane, over the previous frame its callers hold, which tests each Battleship's model box with no
  per-tier inflation. Rationale: a side test covers a clear away line that a route cannot, and it
  reuses the one blocked test both terrain and rectangles already go through. Two tests per veer-off
  start is rare work.
- Only a difference decides. One side clear and the other blocked selects the clear side. Both clear,
  both blocked, or a zero horizontal offset keep today's stored point, so an open veer-off does not
  change. On the highest plane, with no graph, and for a climbing ship, that is every veer-off. A
  start inside a rectangle's box drops that rectangle under the sight end exemption. The difference
  therefore comes from terrain or another Battleship's rectangle: the target Battleship's own counts
  only for a start outside its box and closer than its half diagonal (`## Context`).
- Keeping the side: store the veer-off point shifted sideways, away from the chosen side, by the
  veer-off start distance, instead of the exact enemy position. The away direction then leans to the
  chosen side from the first tick, the existing steering turns that way, and `VeerOffTactic` and
  `ContinueVeerOff` read the point unchanged. Rationale: no new column, flag, transfer reset, or
  layout version, the same reasoning that kept the away goal in the queued Plan unstored. The shift
  leaves Z alone, so the point's nearest plane is unchanged.
- Alternative for the claim session's `/plan-alternatives`: a `ShipFlags` bit naming the side, read by
  steering. It is not recommended, because it edits the steering that
  `Documents/Plans/Game/ConvertFightersToRampedTurn.md` rewrites.
- Derive `kfVeerOffEndDistance` and the start distance as named constants that their squared forms
  use, so no tuning value is copied or added.
- Bump the `Frame::kiVersion` base (`Frame.cpp`) by one from the value found at landing: steering
  changes CRC'd positions with no collection bump (`../AGENTS.md`).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/AGENTS.md`

## In scope

- `AttackOrStartVeerOff`: signature and body, and its call sites inside `FleetCoordination.cpp`.
- The veer-off distance constants in `FleetCoordination.cpp`.
- The header comments of `ChooseFighterFleetTactic` (the fleet-path veer-off comment) and the attack
  tactic in `FleetCoordination.h`, and the `pVecVeerOffPoints` comment in `Fighters.h`.
- The `Frame::kiVersion` base.
- `/update-claude-docs` sync of the veer-off sentence in `Fighters/AGENTS.md`
  `## Behavior Invariants`.

## Out of scope

- `VeerOffTactic`, `ContinueVeerOff`, and `FleeRouteWaypoint` bodies.
- Fighter steering, turn rates, and the veer-off start and end distances' values.
- Attack, escort, and target picking.
- Battleship and Omni behavior: only Fighters veer off.
- New columns, collection versions, and wire or save formats.

## Risk and invariants

- Tier 3. Trigger: determinism/CRC, because this changes CRC-visible Fighter steering and the
  meaning of the CRC'd `pVecVeerOffPoints` column.
- No random draw is added, and the side tests read only the previous frame the callers of
  `AttackOrStartVeerOff` already hold, which that function gains as an argument.
- Ordering: the veer-off continuation still precedes `FindAttackTarget`, so a veer-off leaves a kept
  attack target untouched (`Ships/AGENTS.md` `## Fleet Coordination Invariants`).
- The end distance is measured from the shifted point, so a shifted veer-off ends slightly earlier or
  later than an unshifted one. The attack that follows reads the enemy's live position, so it is
  unaffected.
- `kVeerOff` still clears as today, on spawn and transfer arrival among others, so a stored shifted
  point never outlives its veer-off.

## Acceptance criteria

- A Fighter starting a veer-off on the lowest plane with terrain or a Battleship rectangle on one side
  and clear space on the other turns to the clear side, for a fleet row and a random-spawn row. The
  rectangle case needs a start outside that rectangle's box.
- Code reading shows that a start with both sides clear, both blocked, or a zero horizontal offset
  stores the exact enemy position, so an open veer-off keeps its turn and end distance.
- Each veer-off clears `kVeerOff` and returns to attack while an enemy remains.
- Code reading shows no added `common::Random` call and no new column or flag.
- The `Frame::kiVersion` base rose by exactly one, and no collection version changed.
- Client and server build through `/compile`. Repeated replays give matching final tick CRCs by the
  `.agents/skills/agent-harness/references/replay.md` `### Replay determinism acceptance` steps, and
  client and server show no desync. No unit tests.
- Harness setup: the supplied evidence records none for this scenario. Reach weapons free with
  `inject_payload` enemy spawns near a flagship
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`), place the obstacle on one
  side with `edit_frame`, and record the command sequence.

## Coordination

`dependsOn` carries the prerequisite, the routed `VeerOffTactic` and its static-data parameters,
which also rests on the ship avoidance change, which has no Plan file.

## Notes

- `Documents/Plans/Game/ConvertFightersToRampedTurn.md` replaces the fleet and random-spawn steering at
  `Fighters.cpp:637-638` and `:659`. This Plan edits neither site and only relies on steering turning
  toward the leaning away direction, so neither order constrains the other; the turn acceptance runs
  on whichever steering is present.
