<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T22:07:38.970Z","dependsOn":[]} -->
# Block Ship Line of Sight With Large Ships

## Context

The ship avoidance change adds rectangles of tagged large ships to A* path searches only: a
per-query rectangle list, built from the `NavRectangleBoard`, for models carrying the
`navigationObstacle` tag. The user deferred two sight questions from it to a follow-up: a blocked
check for attack runs in line-of-sight queries, phrased as something that may be needed, and
whether a Battleship blocks line of sight, answered yes. Originating gap: nothing in sight reads
those rectangles.

Paths below are relative to `Projects/BrokenEngineSandbox/Source/Frame/Collections/`, and line
numbers are from `dcd383ae`, before that change. `HasLineOfSight` (`Ships/ShipBehaviours.cpp:761-765`)
tests one segment against one terrain graph, so a Battleship between shooter and target hides
nothing. Its callers fall into three groups:

- Attack-run movement, on the ship's own clearance tier: `Ships/FleetCoordination.cpp:195`, `:231`,
  `:240`, `Battleships/Battleships.cpp:568`, `:577`, and `Omnis/Omnis.cpp:666`. Sight here picks
  between flying straight (`AttackTactic`, `OmniCirclingPoint`) and routing (`RouteTactic`,
  `NextRouteWaypoint`), so a path search that sees rectangles never runs while sight reports the
  straight line clear.
- Target pick and firing, on `kiFiringClearanceTier`: `FindAttackTarget`
  (`Ships/ShipQueries.cpp:100`, `:119`), `AcquireTarget` (`Ships/ShipBehaviours.cpp:721`), the Fighter
  beam (`Fighters/FightersCombat.cpp:109`), and the Battleship missile launch
  (`Battleships/BattleshipsCombat.cpp:297`). Omnis circle on the pick's own sight
  (`Omnis/Omnis.cpp:689`).
- Terrain-avoidance gates: `Fighters/FightersNavigation.cpp:47` and
  `Battleships/BattleshipsNavigation.cpp:47` skip avoidance for a ship flying at a nearby enemy.

Battleships fly only the lowest plane (`kiBattleshipHighestFlightPlane`); Fighters and Omnis also
reach the highest plane, whose graphs are empty. A climbing ship stores `kuiNoFlightPlane`, gets the
empty graph, and so sees every enemy (`Ships/AGENTS.md` `## Ship Invariants`).

Both deferred items are one Plan: they share the root cause (the sight test reads only terrain), the
boundary (`HasLineOfSight` and its callers), the invariant, and the verification. A rectangle test in
`HasLineOfSight` delivers the attack-run blocked check as a direct effect of a Battleship blocking
sight.

## Design

The author recommends the following; each choice carries its rationale.

- One test: `HasLineOfSight` gains a `const NavRectangleBoard*` and also reports blocked when the
  segment between the two unsnapped positions crosses a rectangle. The test is
  `common::ClipSegmentToPrism` with `fEnter` zero, `fExit` one, and a zero Z range at Z 0, the clip
  `RectangleCrossingPenalty` makes. That function is file-static in `NavQuery.cpp`, so sight calls
  the shared clip directly, and sight and route keep one crossing rule.
- Attack runs: no new branch. A blocked line makes the existing hidden-target paths route, and the
  route avoids the rectangle once `Select`'s reach takes it in. The user's wording on this half was
  conditional, so the claim session confirms it at its plan interview.
- Membership: every rectangle the board holds, either alignment, past `Select`'s reach included,
  because the hull is the same to both sides, A* treats them alike, and a Battleship hides what lies
  behind it at any distance. `Select` applies reach, so `HasLineOfSight` reads a new
  `NavRectangleBoard` accessor over all its rectangles, with no reach or endpoint filter, and
  `Select` stays the route query. Which ships, models, and dying rows count stays the board's rule;
  this Plan adds no membership rule.
- End exemption: a rectangle holding either end of the segment does not block, or a Battleship could
  neither see out of its own rectangle nor be seen inside its own. `Select` drops any rectangle
  holding either point, tested on `pf2Box`. The accessor has no such filter, so `HasLineOfSight`
  skips a rectangle when `common::PointInPolygon` puts either unsnapped position inside that
  rectangle's model box, which also skips the shooter's own. This mirrors the snap of both ends out
  of a terrain polygon in `HasLineOfSight`, and testing by position threads no row identity.
- Plane: a rectangle blocks only where `NavRectangleBoard::Select`'s plane rule applies it: the
  `CappedOccupiedPlanes` range of its ship's height overlaps the `CappedOccupiedPlanes` range of the
  shooter's position height on the board's `meSelfType`. A height between two planes covers both, so
  that rule alone would block a climbing ship; `HasLineOfSight` therefore skips rectangles on a
  stored `kuiNoFlightPlane`, which sees every enemy as today. A shooter on a plane above the
  Battleships sees over them, because the two ranges do not overlap.
- Box: the sight test reads each rectangle's model box, posed with no per-tier inflation, unless the
  claim session finds a reason otherwise; it confirms this at its plan interview. Rationale: the
  board inflates `pf2Box` once, in its constructor, for `eSelfType`'s tier, so `pf2Box` cannot serve
  both `kiFiringClearanceTier` and a ship's own tier, and the hull is what hides a target.
  `BoardRectangle` gains that box, posed by `PoseInflatedBox` at zero clearance, and `iClearanceTier`
  keeps selecting only the terrain graph. Sight therefore reads clear across the inflation band a
  route would round; ships have no ship-to-ship collision, so flying straight there costs no damage,
  though pushers, whose range exceeds the hitbox sphere, still separate the ships
  (`Ships/AGENTS.md` `## Ship Invariants`).
- Frame source: each site reads rectangles from the frame its enemy positions come from, the
  previous frame in PostRender Update, the current frame in the Fighter beam and the Battleship
  launch. Rationale: `Ships/AGENTS.md` `## Ship Invariants` records that changing a read's source
  frame shifts CRC-visible behavior by one tick.
- Terrain-avoidance gates pass a null board, staying terrain-only: a Battleship's rectangle says
  nothing about whether terrain avoidance should turn the ship.
- Ripple: `FindAttackTarget` and `AcquireTarget` gain the board; `ChooseFleetTactic` and
  `ChooseAttackTactic` already take `NavRectangleBoard&` and pass it through. A pass that lacks a
  board builds one at the top of the pass, as Fighters and Omnis build `FleetBoard` in PostRender
  Update.
- Bump the `Frame::kiVersion` base (`Frame.cpp`) by one from the value found at landing: sight changes
  CRC'd targeting and steering with no collection bump (`../AGENTS.md`).

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.h`, `.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`,
  `FightersCombat.cpp`, `FightersNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`,
  `BattleshipsCombat.cpp`, `BattleshipsNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `AGENTS.md` under `Collections/Ships`

## In scope

- `HasLineOfSight`: signature, body, and header comment.
- `NavRectangleBoard`: the new accessor over all its rectangles, and the model box in `BoardRectangle`
  and the constructor.
- Every caller listed in `## Context`, and the signatures of `FindAttackTarget` and `AcquireTarget`.
- Board construction at the top of a pass that lacks one.
- The `Frame::kiVersion` base.
- `/update-claude-docs` sync of the line-of-sight sentences in `Ships/AGENTS.md`
  `## Ship Invariants`.

## Out of scope

- `ComputeNavigation`, `NextRouteWaypoint`, A*, and the path-query cadence.
- `NavRectangleBoard::Select`, board membership, and the `navigationObstacle` tag.
- The terrain side of sight (`Documents/Investigations/Engine/ShipLineOfSightRealShapes.md`).
- Weapon collision and damage: blasters, missiles, and beam hits keep their hit tests, so a shot
  already fired is not stopped by a rectangle.
- Veer-off (`Documents/Plans/Game/ChooseVeerOffTurnFromNavigation.md`).
- New columns, collection versions, and wire or save formats.

## Risk and invariants

- Tier 3. Trigger: determinism/CRC, because sight decides CRC-visible target choice, firing, and
  steering for every ship type.
- No random draw is added and no read moves to another frame. Client and server compute the same
  rectangles from shared frame state.
- Order: `FindAttackTarget` keeps its rule that an enemy in line of sight always wins, so a newly
  hidden target can change a pick. The kept-target rules are unchanged.
- Cost: a hidden target routes every tick, an exception to the path-query cadence
  (`Ships/AGENTS.md` `## Ship Invariants`), so ships behind a Battleship add per-tick A*.
  `query_profile`'s `NavQuery` row shows it (`Projects/BrokenEngineSandbox/Source/Profile/AGENTS.md`);
  compare it on the same scenario before and after and surface a large rise instead of adding
  caching here.

## Acceptance criteria

- A shooter whose straight line to an enemy crosses another Battleship's model box on its plane, at
  any distance, holds beam, blaster, and missile fire and picks a target it can see instead, when
  there is one.
- A ship on an attack run with that line blocked routes around the rectangle instead of flying
  through it, and attacks in a straight line once the line clears.
- A Battleship stays a valid target: a ship with a clear line to its rectangle sees and fires at it.
  A Battleship with a clear line out of its own rectangle sees and fires.
- A shooter on the highest plane and a climbing ship are not blocked by a rectangle on the lowest.
- The Fighter and Battleship terrain-avoidance gates behave as before.
- The `Frame::kiVersion` base rose by exactly one, and no collection version changed.
- Client and server build through `/compile`. Repeated replays give matching final tick CRCs by the
  `.agents/skills/agent-harness/references/replay.md` `### Replay determinism acceptance` steps, and
  client and server show no desync. No unit tests.
- Harness setup: the supplied evidence records none for this scenario. Construct the smallest one
  with `edit_frame` or `inject_payload`
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`), placing a Battleship
  between a shooter and its enemy, and record the command sequence.

## Coordination

The ship avoidance change is a prerequisite: this Plan needs its `NavRectangleBoard` and
`navigationObstacle` model tag. That change has no Plan file, so no `dependsOn` edge can name it. If `NavRectangleBoard` is absent at the primary tip at claim, this Plan
is not ready, so defer it with `plan unclaim`.

Reciprocal coordination with `Documents/Plans/Game/RouteShipFleeReturnAndVeerOffGoals.md`: that Plan
edits the Battleship flee destination beside this Plan's sight edits at `Battleships.cpp:568-582`, so
whichever lands second refreshes that block. Reciprocal coordination with
`Documents/Plans/Game/SeparateFighterVeerOffFromFleetTactic.md`: that Plan rewrites the
`ChooseFleetTactic` signature and adds `ChooseFighterFleetTactic`, and both take `NavRectangleBoard&`,
so whichever lands second refreshes the `FindAttackTarget` and `HasLineOfSight` calls in both.
Reciprocal coordination with
`Documents/Plans/Game/MoveAcquireTargetToShipQueries.md`: that Plan moves `AcquireTarget` verbatim, so
whichever lands second works from the function where it finds it. Each Plan keeps its own acceptance.

## Notes

- `Ships/ShipBehaviours.cpp` measures 11,905 `bt-token-v1` by `.agents/scripts/Measure-Tokens.ps1`
  with the ship avoidance change, over the 10,000 threshold. `AcquireTarget` measures 709 of those,
  so the file stays over it after `Documents/Plans/Game/MoveAcquireTargetToShipQueries.md` lands.
  This Plan adds the accessor, the model box, and the `HasLineOfSight` test there.
