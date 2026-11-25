<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-24T21:09:25.469Z","dependsOn":["Documents/Plans/Engine/RemoveFlightPlanes.md","Documents/Plans/Game/FighterGracefulTerrainRise.md"]} -->
# Fighter Auto-Rise Above Battleships and Navigation Blockers

## Context

Fighter auto-rise (`RaiseAboveTerrain`,
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`)
samples only island terrain elevation via
`engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData)`. Navigation
blocker ships — currently Battleships, identified by `IsNavRectangleRow`
(`ShipBehaviours.cpp`, testing `pPushers[iRow].uuid.iValue != 0` and
`gShipHitboxShapes[eType][eController].bNavigationObstacle`) — carry
model-space Z bounds (`gShipHitboxShapes[eType][eController].hitbox.fMinimumZ`
and `fMaximumZ`, populated from the baked `common::ModelOutline` in
`InitializeShipHitboxes`, `ShipHitboxes.cpp`) and posed horizontal boxes
(`BuildShipHitbox`, which adds the ship's Z to produce posed Z bounds), and
`NavRectangleBoard` (`ShipBehaviours.cpp`) builds obstacle rectangles from them
for horizontal routing.

Because Fighter auto-rise ignores navigation blockers, a Fighter flying at the
shared flight height over or near a Battleship does not climb above its hull
and instead relies solely on horizontal routing or pusher separation.

The user requested: "They should also auto-rise above Battleships (or any other
navigation blockers) in the same way they do terrain".

Originating gap: user-requested follow-up kept out of scope of the single-height
ship refactor.

## Design

Extend the Fighter graceful auto-rise and last-resort floor mechanism (from
`Documents/Plans/Game/FighterGracefulTerrainRise.md`) so navigation-blocker
ships contribute to the required vertical clearance in the same way terrain
does:

- **Blocker enumeration (author's recommendation)**: query all live
  `IsNavRectangleRow` ships across every ship type via `ForEachShipType` (never
  hardcoding `ShipType::kBattleship`), reading blocker poses from
  `rPreviousFrame` in `FightersInterpolate::Update` and from
  `rFrame.interpolate` in a short post-ship Fighter clearance pass called from
  `FramePostRender::Update` after `ForEachPostRenderUpdate`, so Battleships and
  any future `bNavigationObstacle` ship models are handled uniformly without
  reordering `GamePostRenderCollections`.
- **Proactive lookahead vs. current-XY hard floor (author's recommendation)**:
  compute each overlapping blocker's required clearance height from the
  model-space Z bounds stored in `gShipHitboxShapes[...].hitbox` (populated by
  `InitializeShipHitboxes`), never from `BuildShipHitbox`'s posed Z bounds:
  blocker world top (`XMVectorGetZ(blockerPos) + blockerShapeHitbox.fMaximumZ`)
  plus the Fighter's upright hull bottom (`-fighterShapeHitbox.fMinimumZ`) and
  vertical safety margin, so neither ship's Z is applied twice. Use
  `BuildShipHitbox` (or the posed horizontal box) only for XY overlap tests:
  - **Proactive acceleration target floor** (`FightersInterpolate::Update`):
    test candidate `rPreviousFrame` blockers whose posed horizontal box
    (inflated by the Fighter's horizontal clearance) intersects the Fighter's
    current XY or lookahead segment, and take the maximum of the terrain
    lookahead clearance height and those blocker clearance heights. Lookahead
    feeds only proactive vertical acceleration so the Fighter climbs smoothly on
    approach.
  - **Last-resort hard floor clamp** (`FightersInterpolate::Update` after
    vertical integration, `FightersPostRender::Update` for terrain after pusher
    X correction, and a post-ship pass in `FramePostRender::Update` after
    `ForEachPostRenderUpdate` for blockers): test only blockers whose posed
    horizontal box (inflated by the Fighter's horizontal clearance) contains
    the Fighter's current XY, clamping Z to the maximum of the terrain floor at
    the Fighter's current XY and those current-XY blocker clearance heights.
    `FightersInterpolate::Update` tests `rPreviousFrame` blocker poses. Because
    `Fighters` precedes `Omnis` and `Battleships` in
    `GamePostRenderCollections` (`FrameCollections.h:30,32`), running the final
    Fighter blocker clamp in `FramePostRender::Update` (`Frame.cpp`) right after
    `ForEachPostRenderUpdate` (before `ApplyUpdateStatusChanges`) reads every
    blocker's final current-tick pose from `rFrame.interpolate` after both
    `Interpolate::Update` motion/rotation and `PostRender::Update` position
    corrections (terrain-contact bounce and coincident-pusher X shifts, e.g.
    `Battleships.cpp:502-504`), so a blocker moving, turning, or shifting
    across the Fighter's path during the tick still clamps the Fighter above it.
    A blocker detected only in lookahead never triggers the hard clamp.
- **Navigation rectangles and pusher vertical clearance**:
  - **`NavRectangleBoard`**: skip building blocker rectangles when `eSelfType`
    is `ShipType::kFighter` (leaving `NavRectangleBoard::Select` returning an
    empty span for Fighters) so Fighter path queries (`ComputeNavigation`,
    `NextRouteWaypoint`, `FleeRouteWaypoint`, `ComputeReturnDestination`,
    `AttackOrStartVeerOff`) fly directly over blockers instead of routing around
    them horizontally, even when both route endpoints lie outside the blocker's
    box. Omnis and Battleships keep their existing rectangle routing unchanged.
  - **Pusher exclusion**: tag every `bNavigationObstacle` ship's pusher with a
    new `engine::PusherFlags::kTypeObstacle` bit alongside `kTypeDefault` when
    syncing it, and exclude `PusherFlags::kTypeObstacle` in Fighter
    `ApplyPusherPush` queries. Because a blocker's 3D pusher sphere derives from
    its horizontal hull length and extends well above `blockerZ + fMaximumZ`,
    excluding it for Fighters prevents lateral pusher deflection and
    coincident-XY X snaps during overflight while keeping Fighter-to-Fighter,
    Fighter-to-Omni, and Omni/Battleship-to-Battleship separation unchanged.
- **Determinism, read source, and version gating**: read blocker poses from
  `rPreviousFrame` in `FightersInterpolate::Update` and from
  `rFrame.interpolate` (every ship's final post-`PostRender::Update` pose,
  paired with `rPreviousFrame.postRender` controller and liveness) in the
  post-ship Fighter blocker-clearance pass called from `FramePostRender::Update`
  after `ForEachPostRenderUpdate`, documenting that explicit exception alongside
  the existing current-frame read exceptions in `Ships/AGENTS.md` and
  `Frame/AGENTS.md`. Keep all math `/fp:strict` and deterministic across client
  and server. Bump `PushersInterpolate::kiVersion`
  (`Engine/Source/Frame/Collections/Pushers/Pushers.h`) for the `PusherFlags`
  and CRC-visible Fighter trajectory changes; `PushersInterpolate::kiVersion`
  already contributes to `Frame::kiVersion`, so no separate `Frame::kiVersion`
  base increment is needed.

## Critical files

- `Engine/Source/Frame/Collections/Pushers/Pushers.h`
- `Engine/Source/Frame/Collections/Pushers/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipHitboxes.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md`

## In scope

- Extending Fighter auto-rise lookahead (proactive acceleration only) and the
  current-XY last-resort floor clamp in `ShipBehaviours.h/.cpp`, `Fighters.cpp`,
  and a post-ship pass in `FramePostRender::Update` (`Frame.cpp`, after
  `ForEachPostRenderUpdate`) to include all live `IsNavRectangleRow` navigation
  blockers using their posed horizontal boxes and model-space
  `gShipHitboxShapes` Z bounds (`fMaximumZ` and Fighter `fMinimumZ`), reading
  `rPreviousFrame` poses in `FightersInterpolate::Update` and final
  post-`PostRender::Update` `rFrame.interpolate` poses in the post-ship clamp.
- Skipping `NavRectangleBoard` blocker rectangles for `ShipType::kFighter` in
  `ShipBehaviours.cpp`.
- Adding `engine::PusherFlags::kTypeObstacle` in `Pushers.h`, tagging
  `bNavigationObstacle` ship pushers with it (in `Battleships.cpp` / ship pusher
  sync), excluding `PusherFlags::kTypeObstacle` in Fighter `ApplyPusherPush`
  queries, and bumping `PushersInterpolate::kiVersion`.
- Updating `Pushers/AGENTS.md`, `Frame/AGENTS.md`, `Fighters/AGENTS.md`, and
  `Ships/AGENTS.md` to document blocker auto-rise, Fighter rectangle omission,
  `PusherFlags::kTypeObstacle`, and the post-ship `rFrame.interpolate`
  blocker-pose read exception.

## Out of scope

- Implementing the base graceful acceleration and ease-out vertical motion
  model (covered by `Documents/Plans/Game/FighterGracefulTerrainRise.md`).
- Making Omnis or Battleships auto-rise over blockers or terrain.
- Adding `bNavigationObstacle` to new ship models or changing DataPacker
  outline/hitbox baking.
- Changing weapon line of sight (`HasShipLineOfSight`), collision layers, or
  Battleship missile targeting.

## Acceptance criteria

- `BrokenEngineSandbox` (client) and `BrokenEngineSandboxServer` build cleanly.
- Live `/agent-harness` verification (no prior observing run recorded; setup:
  spawn a Battleship and a Fighter flying on a route whose start and destination
  both lie outside the Battleship's inflated box and whose straight line crosses
  the Battleship's hull, stepping ticks and querying both ships' positions):
  - The Fighter flies over the Battleship without routing around it horizontally
    or suffering lateral pusher deflection, proactively accelerates upward on
    approach (without snapping up before reaching the blocker), clears above the
    Battleship's top (`blocker Z + model-space fMaximumZ` plus clearance)
    without hull clipping, and eases back down to `EffectiveShipFlightHeight`
    after passing.
  - Lateral-crossing case: spawn a Battleship moving or turning laterally across
    a Fighter's path so the Fighter enters the Battleship's current-tick
    inflated box while remaining outside its previous-tick box; verify the final
    clearance clamp raises the Fighter above the Battleship's hull top on that
    same tick without clipping.
  - Post-correction crossing case: spawn a Battleship that receives a
    `PostRender::Update` position correction (terrain-contact bounce or
    coincident-pusher X shift in `BattleshipsPostRender::Update`) moving its
    inflated horizontal box over a Fighter placed outside both the previous-tick
    and pre-correction blocker boxes with Z below the required clearance before
    the final pass; verify the post-ship clearance clamp raises the Fighter
    above the Battleship's hull top on that same tick without clipping.
  - A dying Battleship (whose pusher is released, so `IsNavRectangleRow` is
    false) does not trigger Fighter auto-rise.
- Client/server replay determinism and CRC parity hold across the overflight.

## Notes

- Risk tier: Tier 3 — changes deterministic simulation state and CRC-checked
  Fighter trajectories with cross-collection reads of navigation blockers.
