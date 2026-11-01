# Ship Firing Line of Sight Against Real Terrain Shapes

The user asked for an investigation of testing firing line of sight against real terrain shapes and of profiling its cost. This record lists options only; no option is chosen.

## Baseline

- `HasLineOfSight` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`) snaps both ends with `NavQuerySnapToNavigable` and tests the 2D segment with `NavQuerySegmentBlocked` (`Engine/Source/Frame/NavQuery.h`) against the shooter's plane graph.
  Firing and target picking pass `kiFiringClearanceTier` (`ShipBehaviours.h`), tier 0, whose polygons are each plane's threshold contour inflated by `kfOmniRadius` plus `kfPushMargin` (`Projects/BrokenEngineSandbox/Source/Frame/TerrainUtils.h`).
- Firing and target-pick call sites: `AcquireTarget` (`ShipBehaviours.cpp`, Omni firing and facing), `FindAttackTarget` (`ShipQueries.cpp`, every type), the Fighter beam (`FightersCombat.cpp`), and the Battleship missile launch (`BattleshipsCombat.cpp`). Movement decisions test sight on the ship's own tier and are outside this record.
- Because the polygons are inflated, a shot that passes within the clearance of the threshold contour is blocked although it clears the terrain, the early block near terrain that `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md` `## Ship Invariants` records. The test also reads no height along the shot.

## Alternatives to test

- Elevation-grid march: sample `CellElevationSampler::Sample` (`Engine/Source/Frame/IslandTerrain.h`) along the segment at the elevation grid spacing (`Engine/Source/Frame/GridCoord.h`), blocked when any sample rises above the shooter's plane height. It reads only the cell's own static data, the simulation sampling path `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation` requires, and needs no new baked data. Its cost grows with segment length.
- Un-inflated threshold contour: bake each terrain-collision plane's threshold contour with zero clearance and run the same segment test against it. This adds a polygon set per plane to the bake, the static data, and the wire, so adopting it changes `kiNavDataVersion` and `kiProtocolVersion`.
- Coarse-then-exact hybrid: keep today's tier-0 test, and run the elevation-grid march only when it reports blocked. This holds only if the inflated, simplified polygons always contain the terrain above the threshold; the contour simplification must be checked for corners it cuts inside terrain.

## What to measure

- Agreement with the current test: on recorded engagements, count per call site the queries where an alternative and the current test disagree, and in which direction.
- Per-tick cost: server time spent in sight queries in a massive battle, for the current test and each alternative, on the same replay. `query_profile`'s `NavQuery` row times only `NavQueryDirection` (`Projects/BrokenEngineSandbox/Source/Profile/AGENTS.md`), so a line-of-sight timer is a prerequisite.
- Determinism: sight decides CRC'd targeting and firing, so each alternative must pass replay determinism (`.agents/skills/agent-harness/references/replay.md` `### Replay determinism acceptance`).

## Questions

- Which shot height should the march test: the shooter's plane height, or the height along the line from shooter to target, which matters when the two sit on different planes?
- At what sample spacing does the march stop missing thin terrain features, and what does that spacing cost per query?
- Do the disagreements change engagement outcomes enough to justify the cost?

This becomes a Plan only after the user chooses an option.
