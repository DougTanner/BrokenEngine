# Ships - Shared Ship Code

What the three ship collections — Fighters, Omnis, Battleships — share: the vocabulary (`ShipTypes.h`), the per-row behaviours each type's PostRender script calls (`ShipBehaviours`), the cross-type queries (`ShipQueries`), and the ship status changes (`ShipStatusChanges`). This folder holds no collection; each type's own `AGENTS.md` documents its script.

## Ship Invariants

- A row's CRC'd `ShipController` selects its script branch and its model. Fleet-controlled rows run the fleet navigation below and are the owned entities (`Frame::OwnedEntityCount`); random-spawn rows, which the frame's random-spawn waves create, carry global ID 0.
- Stable global IDs provide lookup and transfer identity; flagship status is the `ShipOrders::kIsFlagship` bit, never a privileged SOA index. A fleet row follows its own flagship, found by `pFlagshipGlobalIds` in any of the three types.
- In PostRender Update a ship script reads other ship rows only from the previous frame, so the tuple order never hands it a row this tick has not updated yet; Spawn-phase weapon targeting reads the current frame, after every ship's Update. Changing a read's source frame shifts CRC-visible behavior by one tick.
- Every ship type's `pNavigationDelays` stays within [0, 60] seconds, the range `IsNavigationDelayInRange` tests and every navigation-delay admission shares. Each type's PostRender `PostRead` rejects a non-finite or out-of-range navigation delay and an unknown controller on every Frame read.

## Fleet Navigation Invariants

- Navigation covers roaming, cross-cell movement, island destinations, and flagship following. Mode transitions depend only on shared state.
- Flagship following has no frame-local exit when the flagship row is gone;
  recovery comes from the server's flagship reassignment landing in the next
  tick's Update phase, not from navigation. Two ticks after the death (D+2), the
  promoted flagship's reassigned flag, carried mode 5 (following the flagship),
  and same-cell wanted coord recover it to mode 4 (navigating to an island
  destination) and clear the stale destination; this preserves mode 4's schedule
  of three random draws and leaves the fleet's 60-second rally timer unchanged.
- Path queries run on a deterministic cadence staggered by tick and stable global ID, with immediate recomputation for mode, destination, or direction changes.
- The cadence also breaks off-schedule when a navigation polygon contains either
  the lookahead point along the carried steering bearing or the current
  position. Both tests are needed: containment ahead does not imply containment
  at the position, and a ship already driven inside sits there as its steady
  state. Key this on nav-polygon containment, not a terrain-elevation probe —
  the polygon is that elevation contour inflated outward, so a ship stranded in
  the band between them measures clear terrain and never re-paths.
- Random draws, arrival checks, and mode transitions remain outside the
  pathfinding throttle. Modes 4 and 5 must consume the same number of random
  draws on any tick they can flip between, so mode 5 mirrors mode 4's three
  entry draws (island pick, footprint X, footprint Y) on every tick it runs. In
  a cell with no islands neither mode draws: mode 5 skips the mirror and mode 4
  navigates to the cell center instead of an island. Only the draw count matters
  — each `common::Random` advances the shared random stream once no matter what
  bound it is given — so mode 5's mirror draws are load-bearing, not dead code,
  even though it throws their results away.
- Spawning always consumes its cell-change random draw.
- Preserve cached steering and client debug waypoints across ticks where pathfinding is skipped.
