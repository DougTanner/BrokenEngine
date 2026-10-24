<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-23T17:37:37.987Z","dependsOn":[]} -->
# Hexagonal simulation cells (Option 2: direction-index reshape)

Authored against `5205e34e7ae8836763ab04708456412195765f00`. Tier 3. The approach is the one the user chose
from a `/plan-alternatives` comparison ("Option 2"); the plan already carries the accepted `/plan-audit` and
`/plan-simplicity-review` findings from its planning session. No decision is open. Each entry under
`## Settled decisions` says whether the user chose it (with the user's words quoted) or the author recommends it
(with its rationale); that section overrides any conflicting text.

## Context

Today each simulation cell is a 900 m square (`Engine/Source/Frame/GridCoord.h:44-45`) with 8 neighbours: four
share an edge and four touch only at a corner, which `ComputeTransferDelta` turns into diagonal transfers
(`Engine/Source/Frame/FrameUtils.h:169-175`). Regular hexagon cells give 6 equal edge neighbours, remove every
corner-only crossing, and make encirclement and supply-line play in Coretide more natural: every adjacent cell is
reached across a full edge and is the same distance away. The new cell is a flat-top hexagon exactly as tall as
today's square (900 m). The user's fixed direction: "full re-write no backwards compatibility" — no square-cell
path survives, and every format whose meaning changes bumps its owning version so old saves, replays, peers, and
derived data are rejected (root `AGENTS.md` `## Directives`).

## Settled decisions

- **Option 2 — direction-index reshape (chosen by the user).** `GridCoord` keeps two `int32_t` fields
  (`GridCoord.h:8-9`), now holding axial lattice coordinates (q, r), so the offset between two cell centres is the
  linear map dq·A + dr·B with no row parity. One `constexpr` table in `common::` lists the six axial neighbour
  deltas in the fixed order (1,0), (1,−1), (0,−1), (−1,0), (−1,1), (0,1) — clockwise, with n_{k+1} − n_k = n_{k+2}
  — with their centre offsets n_k; that table alone defines the cell (the Voronoi region of the lattice: edges
  dot(p, n_k) against |n_k|²/2), its neighbours, edges, transfers, and ship headings. The inside test, segment exit
  trace (smallest t over the six half-planes), transfer direction (the half-plane crossed), multi-cell point
  location (repeated exit-direction steps), `MakeRenderBasis` and the water/wave absolute offsets, int64 grid
  distance, and the rectangle/hex overlap all evaluate that table. The four-lane `vecArea` / `CellBounds` /
  `common::InsideArea` cell convention goes away; containment call sites take only a position.
  `game::TransferRequest` drops `iDeltaX`/`iDeltaY` (`Projects/BrokenEngineSandbox/Source/Frame/Frame.h:76-81`)
  and carries one `int8_t` direction index 0..5, so a diagonal or two-cell transfer is unrepresentable.
  `TransferRequest` is never serialized (its only users are the producers, `FrameBase.cpp:141-157`,
  `ServerTransferManager.cpp:50-66`, and `ReconcileReplayClientState.cpp:47-55`), so this is not a wire change by
  itself. Ship navigation direction uses the same 0..5 index. Elevation, collision, and pusher grids stay
  rectangular over the hexagon's bounding rectangle. Protocol, frame, frame-input, and replay-metadata versions are
  bumped once. Islands may be placed anywhere inside the hex; every cell's island layout changes, accepted.
- **D1 — Orientation: flat-top (settled by the user).** "Flat top probably works better for rectangular
  monitors". Edges face north and south; the six neighbours are NE, SE, S, SW, NW, N, so the table order above is
  index 0 NE (1,0), 1 SE (1,−1), 2 S (0,−1), 3 SW (−1,0), 4 NW (−1,1), 5 N (0,1). There is no east or west edge
  neighbour, so the harness east-edge coast example
  (`Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:302-311`) becomes a vertex crossing and
  is rewritten (S4). The east/west sun march (`Engine/Data/Shaders/Shadow/AGENTS.md:14`) is a world direction and
  is unaffected. No orientation parameter exists anywhere.
- **D2 — Size: today's height (settled by the user).** "match the current height of a square with the height of
  the hex". Flat-top height √3·R = 900 m, so apothem a = 450 m (today's centre-to-edge distance), circumradius
  R = 2a/√3 = 900/√3 ≈ 519.62 m, width 2R ≈ 1,039.23 m, area (√3/2)·900² ≈ 701,481 m² (~87% of today's
  810,000 m²). Lattice basis A = (a√3, a) ≈ (779.42, 450), B = (0, 2a) = (0, 900); every |n_k| = 900 m, so every
  edge midpoint is 450 m from the centre. Bounding rectangle 2R × 900 m, half extents (R, a) ≈ (519.62, 450).
  Author's recommendation: make the engine constant the apothem `kfCellApothem = 450.0f` and have
  `MakeHexDirections` take the apothem, so B, both N/S offsets, and every offset's y component are exact in
  float and the height is exactly 900; only the four slanted offsets' x component (a√3) and R round. The zoom
  ceilings (release 600 m, dev 2,000 m, `Engine/Source/Graphics/CameraBase.h:87-91`) stay. The Huge anchor island
  (400 × 400 m footprint, `Engine/Source/Frame/IslandTerrain.cpp:15-18`) always fits the margin-inset hex: its
  rotated support along any normal is at most 200·√2 ≈ 282.9 m, under the 440 m inset apothem (450 m minus the
  10 m `kfCellEdgeMarginMeters`, `Engine/Source/Frame/IslandChainPlacement.cpp:18`), so `ASSERT(bAnchorPlaced)`
  (`IslandChainPlacement.cpp:257`) holds. At the default 300 m eye height (`CameraBase.h:80`) the visible
  rectangle's half extents are about 120 × 67 m at 16:9 (vertical FOV `gFieldOfView/aspect`,
  `Engine/Source/Graphics/CameraBase.cpp:395`, default 45 deg at `Engine/Source/Ui/WrapperBase.cpp:7`) — small
  against the 450 m apothem and the 259.8 m half edge, which A5 relies on.
- **D3 — Edge and vertex tie rule (author's recommendation: today's rule in its axial analogue; any consistent
  three-consecutive-edge split behaves identically in play, and this one keeps the existing rule).** Today a cell
  owns its west and south edges (`Engine/Source/Frame/FrameUtils.h:166`, `Engine/Source/Frame/AGENTS.md:45-48`),
  i.e. of two neighbouring cells the one with the larger coordinate owns their shared edge. Recommended: of two
  neighbouring cells, the one with the larger coordinate — larger r, or equal r and larger q — owns their shared
  edge. So each cell's half-plane test is closed (<=) on its edges toward table indices 1, 2, 3 (SE, S, SW — the
  three south-facing edges) and strict (<) on indices 4, 5, 0. Exact-arithmetic proof: edge k of C is edge k+3 of
  C+n_k, and {1,2,3} and {4,5,0} partition the indices, so every edge has one owner. With w_j the vertex between
  edges j and j+1, a cell owns exactly w_1 and w_2 — the two ends of its S edge — and no other vertex; w_j of C is
  w_{j+2} of C+n_j and w_{j+4} of C+n_{j+1}, an all-even or all-odd index set holding exactly one of {1, 2}, so
  every 3-cell vertex has exactly one owner. `CellExitDirection` picks the crossed half-plane with the largest
  dot(p, n_k) (the nearest neighbour centre), exact ties to the first maximum in table order, the same tie rule
  as D8: an exact float tie needs a position exactly on the edge between two neighbours, and either pick shares
  that boundary, so the D6 nudge that follows in both callers moves the position inside the picked cell,
  deterministically on both endpoints, and no ownership special case is needed. The exit trace uses the inside
  test's closed/strict split and hits exactly when `!IsInsideCell(end)`. Floating-point agreement across the two
  cells' frames is D6.
- **D4 — Field names (settled by the user).** "Option 1 rename": `GridCoord::iX`/`iY` become `iQ`/`iR` in every
  file of the rename set under `## In scope` (`git grep -I -l '\.i[XY]\b'` lists 49 files outside `Documents/Plans`, 48 of them `GridCoord`
  users; `DataPacker/Source/ExportJobs/Island/ProcessBakedRegion.cpp` matches only its own `CropRect` and is not
  renamed). Wire size and field order are unchanged.
- **D5 — Bounding-rectangle storage (settled by the user).** "ok, sounds good, just be careful not to put things
  in that 'wasted space' because that is actually owned by the neighour hexes". Stated as the **corner invariant**
  (Approach 4) with its enforcement points.
- **D6 — Transfer landing precision: bounded nudge (settled by the user).** "Option 1, should not be visible to a
  user casually". Centre offsets with √3 round in float, so `p - n_k` can round a position within an ulp of the
  edge onto the wrong side of the destination edge (today's exact multiple of 900 m cannot,
  `Engine/Source/Frame/FrameBase.cpp:150-153`). After the subtraction, while the position fails the destination
  `IsInsideCell` and fewer than `kiLandingNudgeSteps` (recommended 4, because the subtraction errs by at most
  half an ulp per component) steps have run, replace x and y each by `std::nextafter(component, 0.0f)` — one ulp
  toward the destination centre, the local origin, at most ~6.1e-5 m inside the hex — then `ASSERT` the position is
  inside. Deterministic in both builds, no visible displacement. The same nudge runs after each subtraction in the
  `ResolveLocalPosition` loop (Approach 4), there without the `ASSERT`, because a still-outside position is the
  loop's ordinary multi-cell case: it stops a rounding residual from stepping back and forth across one edge or
  around one vertex.
- **D7 — Client subscription (settled by the user).** "currently the client subscribes to only the cells it
  needs, we will keep that, and make no 'guarantees' just best effort". Today's visible-neighbour filter
  (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:675-733`) is kept, scanning the 6 table neighbours
  instead of the 8 square ones. A cell beyond the ring that would be on screen is simply not subscribed, as today
  (`ClientSessionRuntime.cpp:703-704`). Hex size and camera zoom are tuning only: no visibility guarantee, assert,
  zoom clamp, or acceptance check ties them to the ring.
- **D8 — Fleet heading toward a wanted cell: integer most-direct direction (settled by the user).** "Should be 1,
  so multiple fleets with the same origin/destination choose the same path". Replaces today's `Random(3)` pick
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:227-244`): convert the int64
  axial delta (dq, dr) from the current to the wanted coordinate to cube coordinates (dq, −dq−dr, dr) and pick the
  table direction with the largest integer cube dot product (proportional to the Euclidean dot of the centre
  offsets), exact ties to the first maximum in table order; no float and no random draw (the fleet-override draw at
  `ShipBehaviours.cpp:229` disappears; the Ships draw-count invariants,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md:58-67`, constrain only today's modes 4/5,
  renumbered 6/7, and spawning). Determinism property: the picked direction is a pure function of the current and
  wanted coordinates, so ships with the same origin and destination take the same route (no held direction, D11).
  A wanted coordinate can be more than one cell away (an Omni or Battleship spawned at the origin cell's spawn point
  carries the fleet's current wanted coordinate,
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:115,123-129`; harness rig,
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationHarnessRigs.cpp:274`); the pick handles any
  distance, and int64 holds the cube terms (each below 2^34). The fleet controller draws uniformly among the 6
  neighbours (`Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp:86` becomes
  `Random(5)`; bounded integer draws include the upper bound, `Common/Math/AGENTS.md` `## Random Streams`).
- **D9 — Rectangle/hex overlap (the user's conditional choice; the author's assessment that the condition
  holds).** The user: "Write a new rectangle/hex overlap function on DirectXMath — but only if it ends up more
  efficient/faster than the generic one". `common::ConvexHullsOverlap` (`Common/Math/ConvexHull.h:52-96`) does an
  AABB check and then up to 10 axes × 10 vertex projections in scalar float; a dedicated
  axis-aligned-rectangle/hex test needs only the AABB check plus the 2 hex edge normals not parallel to x or y,
  each one dot product against the rectangle's half-extent support (`XMVectorAbs`, `XMVector2Dot`). It is
  therefore cheaper by construction, and the author recommends writing it (Approach 1). It returns
  false for edge touching, matching today's strict rectangle test (`ClientSessionRuntime.cpp:725-726`).
  Client-only caller, off the CRC path.
- **D10 — Server monitor cell map stays square (settled by the user).** "'Option 1: Redraw the monitor's cell map as
  hexes.' -> as a followup plan". The server display's square grid indexed by `GridCoord`
  (`Engine/Source/Server/ServerDisplay.cpp:228-240`) is not redrawn here; with axial coordinates it shows cells
  sheared (a (1,−1) neighbour drawn as a diagonal, the non-neighbour (1,1) drawn as one). This change only renames
  its fields (D4); the hex redraw is a separate follow-up Plan that depends on this one.
- **D11 — No held fleet direction (settled by the user).** "Option 1". Today a ship keeps a held direction 0..3
  while it still moves toward the wanted coordinate (`ShipBehaviours.cpp:213-227`); that hold check is deleted.
  Each evaluation of the fleet branch (`ShipBehaviours.cpp:207`: the wanted coordinate differs from the cell's and
  the pending countdown has ended) computes the D8 pick; only a pick that differs from `riNavigationDirection`
  writes the direction, clears the island destination, and logs the fleet override (`ShipBehaviours.cpp:244-245`).
  A pick equal to the current direction changes nothing, so the direction is stable within a cell: the pick
  depends only on the cell's and the wanted coordinates, and nothing else in the function rewrites a 0..5
  direction while they differ (`FollowFlagship`, `ShipBehaviours.cpp:249-252`, runs only when they match; the
  roaming timer, `ShipBehaviours.cpp:255-261`, only at -1). Every cell entry resets
  navigation to spawn defaults
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp:518-522`, likewise
  `Battleships.cpp:513` and `Omnis.cpp:292`), so the next cell picks afresh. The route therefore depends only on
  origin and destination.
- **No third-party hex library (author's recommendation, answering the user's question about one).**
  `ThirdParty/` holds no hex-grid library, and the needed math is about 100 lines in `common::` on DirectXMath,
  so a dependency would replace a few small functions with foreign types and conventions that are not built for
  the bit-determinism rules. Write it with the Red Blob Games hex-grid formulas as a reference only, importing no
  code; no multi-cell A* (YAGNI).
- **Geometry helper placement (settled by the user).** "Some helper functions (in common::) might be useful for
  things like fast rectangle/hex overlap testing would be good, using as much Direct3DMath existing as possible".
  The pure hex geometry lives in `common::` beside the existing DirectXMath helpers in `Common/Math/MathUtils.h` /
  `.cpp` (`MathUtils.h:225-227`), uses DirectXMath types, `XM_CALLCONV`, and existing DirectXMath operations
  wherever one fits; each helper has a caller in this conversion; helpers on the simulation/CRC path use no
  DirectXMath `...Est` approximation. Author's recommendation: engine-specific types and constants (`GridCoord`,
  the cell apothem) stay engine-side, because `common::` must not depend on Engine (`Common/AGENTS.md`
  `## Overview`): the `common::` table is produced by a `constexpr` function of the apothem, and the engine binds
  it once to its cell constant (Approach 1).
- **Navigation-direction field width (repository fact, settled from code).** The field is bits 8-10, "3 bits,
  stored as value+1: 0-6 representing -1 to 5" (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipTypes.h:88-91`,
  getter and setter `ShipTypes.h:99-109`). Today's values: -1 roaming (`ShipBehaviours.cpp:255`),
  0-3 the four neighbours (`ShipBehaviours.cpp:217-223,439-451`), 4 island tour (`ShipBehaviours.cpp:198,260,371`),
  5 follow flagship (`ShipBehaviours.cpp:190,193,350`). Hex needs -1, six directions, island tour, and follow
  flagship: 9 values, while 3 bits hold 8. Bit `0x0800` is unused (`ShipTypes.h:79-96` lists every `ShipFlags`
  bit; the waypoint index starts at bit 12, `ShipTypes.h:93-95`; the only raw masks of the field in the game source
  are the getter's `& 0x7` at `ShipTypes.h:101` and the setter's `0x0700` at `ShipTypes.h:107`). The field becomes
  bits 8-11 (setter mask `0x0F00`, getter mask `& 0xF`, add `kNavigationDirectionBit3 = 0x0800`), stored
  value+1: -1 roaming, 0-5 hex directions, 6 island tour, 7 follow flagship.
- **D12 — Per-spawn safe search in cell (0,0) (settled by the user; how it is found is the author's
  recommendation).** Every Omni and Battleship spawn uses one fixed point, (120, −80)
  (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp:81-84,125`), chosen in `db9cbe539`
  "outside every island rectangle of cell (0,0) by more than kfBattleshipRadius" for the square layout, which this
  change replaces. The user: "'spawns every player fleet at a fixed point, (120, −80)' don't do this, need to
  determine spawn point after Islands have been randomly placed", and then: "should also avoid spawning where other
  ships already are in 0,0 need some sort of 'safe search'". For the case where no spot is clear, main presented
  "Option 1: Wait and retry — the fleet doesn't spawn this tick; the server tries again next tick, logging once;
  reuses the retry pattern fighter launches already use" and "Option 2: spawn at the least-crowded spot"; the user
  replied "Option 1". The two constants are deleted. Author's recommendation, each part with its rationale:
  - Island candidates, built once: `ServerClientManager` gains a constructor that runs
    `GenerateIslandChain(engine::kOriginCoordinate, ...)` (`Engine/Source/Frame/IslandChainPlacement.h:40`) into a
    local vector and stores, in a new member `std::vector<XMFLOAT2> mFleetSpawnCandidates`, every search candidate
    (order below) that lies inside the inset hex and clear of every island footprint, then logs the candidate count
    and the first candidate — the spawn point of an empty origin cell — once at `kInfo`; `ResetState` leaves the
    list alone. Islands are static: the layout is a pure function of the build's generator and island pack, the same
    one every fresh game gives the origin cell (`Engine/Source/GameBase.cpp:1023`), so the list never goes stale, and
    the per-spawn search below repeats no island test. Startup may allocate (`Engine/Source/AGENTS.md`
    `## Hub Conventions`). `IslandTerrain` is built before `Game` on the server (`Engine/Source/Main.cpp:200,213`),
    and `CellCoordinateProbe.cpp:53` already generates a layout on its own. The live origin placements in `mCells` are
    not read: a grid save supplies them from a file whose trust check bounds only template, centre, and rotation
    (`Engine/Source/File/GridSave.cpp:119-133`), so a crafted save could leave the list empty and reach the
    `ASSERT` below, against the grid-save trust policy (root `AGENTS.md` `## Key Patterns`).
  - Candidate order and island test: square rings of step `kfFleetSpawnSearchStepMeters` = 10 m around the cell
    centre: ring 0 is the centre, and ring k is every (i·10, j·10) with max(|i|, |j|) = k, in j-ascending then
    i-ascending order, up to the first ring whose every point lies past the circumradius (k = 52). A candidate is
    kept only if `common::IsInsideHex(p, engine::kCellDirections, kfBattleshipPusherRadius)` holds (a 6.75 m inset,
    `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h:53`, the radius plus a margin), so
    the point also passes `IsInsideCell` and the `QueueSpawn` assert (`ServerTransferManager.cpp:258`) and is off
    the corners, and only if it is more than `kfBattleshipRadius` (4.5 m, `Battleships.h:27`) outside every
    placement's footprint rectangle — the existing comment's rule: with (c, s) from
    `common::DeterministicSinCos(-fRotation)` (the convention at `Engine/Source/Frame/IslandTerrain.cpp:416-417`),
    the offset from the placement centre rotated into the island frame has |x| > `fQuadFootprintX`/2 + r or
    |y| > `fQuadFootprintY`/2 + r. No existing helper does this test: `SamplePlacementHeightmap` is file-static in
    `IslandTerrain.cpp` and samples heights, and `ConvexHullsOverlap` compares two hulls. So the test is a few lines
    in a file-local `ComputeFleetSpawnCandidates` in `ServerClientManager.cpp`, and no helper is added. Every
    spawning Omni or Battleship is treated as a Battleship, the largest ship (`kfBattleshipRadius` 4.5 m against
    `kfOmniRadius` 1.1 m, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h:33`), so one list and one clearance rule serve both types.
  - Per-spawn ship test: each Omni or Battleship spawn in `SpawnFleetMember` takes the first entry of
    `mFleetSpawnCandidates` whose horizontal (x, y) distance is greater than the clearance from every Fighters,
    Omnis, and Battleships row of cell (0,0) and from every point an earlier Omni or Battleship spawn of the same
    `TickRespawnTimers` pass took. One pass is one advancing update, and all its spawns share one `iSpawnTick`, so
    the taken points are a local list `TickRespawnTimers` declares before its owner, fleet, and member walk
    (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:138-142`) and passes by reference
    through `SpawnFleetMember` to `FindFleetSpawnPoint`; the Fighter case ignores it. It reads `gpGame->mCells.at(engine::kOriginCoordinate).pCurrent`, the committed frame of the last
    finished tick, through `ForEachShipType`
    (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.h:13-26`) and each type's Interpolate
    `pVecPositions` — the same frame and column the Fighter launch reads (`ServerClientManager.cpp:148,155,158`) and
    the wave spawn's fleet-distance test reads (`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:265-279`). That
    frame is committed at this point: `SpawnFleetMember` runs from `ServerFleetManager::TickRespawnTimers`, called
    from `BuildAdvancingFrameInputs`
    (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:149-162`), called by
    `ServerBroadcaster::BuildFrameInputs` (`Engine/Source/Network/Server/ServerBroadcaster.cpp:29-32`) from
    `PrepareActiveSet` (`Engine/Source/GameBase.cpp:1071-1072`) before the update's tick loop
    (`GameBase.cpp:416-423`), and the active set always holds the origin (`Engine/Source/Network/Server/AGENTS.md`
    `## Session Invariants`). Every row counts, whatever its controller, flight plane, or lifecycle: pushers ignore
    planes (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md` `## Ship Invariants`), and a
    dying row still sits there. Spawns
    queued in an earlier update are already rows of that frame: the origin is always active, a spawn targets the
    next tick (`ServerClientManager.cpp:94-95`) and is drained on it (`ServerTransferManager.cpp:100-126`), and an
    advancing update runs at least one tick (`GameBase.cpp:423-474`). Spawns queued earlier in the same `TickRespawnTimers`
    pass are not, hence the local list of taken points. A Fighter launched in the same pass needs no entry: it starts
    `kfFighterLaunchOffset` = 3 m ahead of its Battleship (`ServerClientManager.cpp:89`), a row of the frame whose
    13.5 m clearance covers the Fighter's 3 m offset plus the 9 m pusher-radius sum of a Battleship and a Fighter. The test is a file-local `FindFleetSpawnPoint` in
    `ServerClientManager.cpp`, returning no point when no entry is clear; no spatial index (its worst case,
    candidates × rows, arises only in a crowded cell).
  - Clearance: one number, more than 2 × `kfBattleshipPusherRadius` = 13.5 m, against every Fighters, Omnis, and
    Battleships row and every point taken earlier in the `TickRespawnTimers` pass, so the `ForEachShipType` visitor needs no
    type-to-radius lookup. Rationale: it is never smaller than `kfBattleshipPusherRadius` plus the other ship's
    pusher radius (`kfFighterPusherRadius` 2.25 m, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h:55`;
    `kfOmniPusherRadius` ≈3.67 m, `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.h:58`;
    `kfBattleshipPusherRadius` 6.75 m), and it needs no new constant; each pusher acts only inside its own radius
    (`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:154-155`), so neither ship pushes the other at the
    spawn; and the smallest margin over the two ships' radius sum, 4.5 m against a Battleship, exceeds the one tick
    another ship moves between the read and the spawn row's creation — at most 1.25 m, a Fighter at `kfFighterMaxSpeed` 40 m/s
    (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h:31`) over one 1/32 s tick (`Engine/Source/Frame/TimeStep.h:6`).
  - No clear spot — wait and retry: the spawn is skipped by `return` before `QueueFleetSpawn`, the path a
    skipped Fighter launch takes (`ServerClientManager.cpp:135-164`). `QueueFleetSpawn` therefore never clears
    the member's `kIsDead` (`ServerClientManager.cpp:119-120`), its countdown stays expired, and
    `TickRespawnTimers` calls `SpawnFleetMember` for it again on the next advancing update
    (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetManager.cpp:133-155`), normally
    the next tick (`GameBase.cpp:404-406`). Logging once per blocked episode: a new member `bool
    mbFleetSpawnBlocked` — a failed search logs one `kWarning` only when it is false and sets it, and a successful
    search clears it; `ResetState` leaves it alone.
  - Remaining `ASSERT`: the constructor asserts `mFleetSpawnCandidates` is not empty, the islands-only case. The
    footprint sizes alone do not rule it out, because footprint rectangles may overlap and the cap's summed footprint
    area (1 Huge at 160,000 m², 2 Large at ≤ 80,000, 3 Medium below 48,000, 101 Small below 16,000 each;
    `IslandTerrain.cpp:14-20`) is ~2.1 km², over the 0.70 km² hex. The assert is justified because its only input
    is a constant of the build: A9 observes the list's first point, and a generator or pack change that left none
    fires the assert at every server start. The ship-blocked case has no `ASSERT`: it depends on live state and
    waits instead.
  - Server only, no determinism requirement: the position travels in the spawn `StatusChange` (`vecPosition` is a
    `SpawnInfo` wire column, `Battleships.h:284`) queued through `QueueSpawn`. Clients apply that change. Server
    replay playback does not run `BuildFrameInputs` or any server manager (`GameBase.cpp:1064-1068`) and takes
    spawns only from the recorded post-dispatch channel (`Engine/Source/File/Replay.cpp:867-891`), so no client or
    playback re-runs the search. It is deterministic anyway — no random draw, a pure function of the cached list,
    the committed origin frame, and the points taken earlier in the `TickRespawnTimers` pass — so it consumes no CRC'd stream. The spawn
    heading stays +x.

## Open decisions

None.

## Approach

One table in `common::` defines the hexagon; thin engine wrappers bind it to the cell apothem and `GridCoord`;
every consumer routes through them; rectangular storage grids span the hex's bounding rectangle under the corner
invariant; the compatibility gates break once.

1. **Cell geometry (Common + Frame).** In `Common/Math/MathUtils.h` / `.cpp`, already aggregated by
   `Common/Common.h:21`, so no file, include, or project membership is added. Nothing here reaches Engine
   (`Common/AGENTS.md` `## Overview`). Helpers, all flat-top (no orientation parameter), each with its callers in
   this change:
   - `constexpr` `MakeHexDirections(fApothem)` returning the six entries in the settled axial order, each the axial
     delta (dq, dr) and the centre offset n_k as `XMFLOAT2`, plus the lattice basis A = (a√3, a), B = (0, 2a) and
     the bounding-rectangle half extents (2a/√3, a); `std::numbers::sqrt3_v<float>` is the one irrational
     constant. Callers: every helper below, the client subscription loop, ship navigation, the fleet controller,
     both transfer consumers, and `Server.cpp` adjacency.
   - `IsInsideHex(position, directions, fInset)` — the six half-planes `dot(p, n_k)` against
     `|n_k|²/2 - fInset·|n_k|`, closed on indices 1-3 and strict on 4, 5, 0 (D3), with `XMVector2Dot` and lane
     compares in all-lanes-pass polarity: inside only when every lane's compare is true, so a NaN lane fails and
     NaN and ±inf positions are outside (every axis has normals of both signs, so an infinite component makes
     some lane +inf or NaN). The grid save trust check relies on this
     (`Engine/Source/File/GridSave.cpp:119-120,127-128`). Callers: every containment site through `IsInsideCell`
     (`fInset` 0), `TryTouchPlace` and `PlaceAnchor` (margin inset), the grid save check, the elevation
     splat's corner skip, and the fleet spawn search (Approach 11, `kfBattleshipPusherRadius` inset).
   - The segment exit trace, keeping today's contract (`FrameUtils.h:92-98`): a start that fails the inside test
     returns t = 0; otherwise it hits exactly when the end fails the inside test, at the smallest
     t_k = (h_k − dot(s, n_k)) / dot(e − s, n_k) over the half-planes the end fails (dot(e − s, n_k) > 0 on each in
     exact arithmetic); a start on a closed edge moving outward gets t = 0 from the same formula, today's
     non-inward-boundary case. Caller: Collision's exit trace.
   - `HexExitDirection(position, directions)` — for a position outside the hex, the crossed half-plane with the
     largest `dot(p, n_k)`, exact ties to the first maximum in table order (D3). Callers: `PrepareTransferRequest` and the
     `ResolveLocalPosition` loop.
   - Centre offset of an axial delta, dq·A + dr·B, as an `XMVECTOR` with W 0 and in double from the float basis
     promoted to double, so the absolute water/wave camera position moves by the same offset the camera shift
     applies. Callers: `MakeRenderBasis`, ship destinations (n_k), and the two double absolute camera positions.
   - int64 axial grid distance `(|dq| + |dr| + |dq + dr|) / 2`. Callers: `Server.cpp` adjacency and the camera and
     retained-area step tests.
   - `RectangleOverlapsHex(f4Rectangle, vecHexCentre, directions)` (D9) — the rectangle in the
     `(minX, maxY, maxX, minY)` packing `AxisAlignedBoundingBoxIntersectsArea` reads
     (`Common/Math/MathUtils.cpp:139-147`): a strict AABB test against the hex bounding rectangle at the centre,
     then for the two table normals with neither component zero, separated when
     `abs(dot(d, n_k)) >= |n_k|²/2 + dot(rectangle half extents, abs(n_k))` with d the rectangle centre minus the
     hex centre, `abs(n_k)` the per-component absolute value (`XMVectorAbs`), and `|n_k|` the length as elsewhere
     (`XMVector2Dot`); edge touching returns false. Caller: the client visible-neighbour
     filter.
   Engine-side, `GridCoord.h` replaces `kfCellWidth`/`kfCellHeight` and the four `kfBaseArea*` constants
   (`GridCoord.h:44-54`) with `kfCellApothem = 450.0f` (D2), `kCellDirections = common::MakeHexDirections(kfCellApothem)`,
   and the bounding half extents read from it, and updates the elevation comment (`GridCoord.h:47-48`).
   `FrameUtils.h` deletes `CellBounds`, `ComputeCellBounds`, the rectangular `TracePointToCellExit`,
   `LocalCellArea`, `IsOutOfBounds`, and `ComputeTransferDelta` (`FrameUtils.h:54-60,69-137,161-175`), keeps
   `struct SegmentHit` (`FrameUtils.h:62-67`), and adds wrappers
   that only bind `kCellDirections` or `GridCoord`: `IsInsideCell(position)`, `TracePointToCellExit(start, end)`
   still returning `SegmentHit` (shared with the terrain trace, `Engine/Source/Frame/TerrainCollisionUtils.h:11`),
   `CellExitDirection(position)`, `GridDistance(a, b)`, the centre offset between two `GridCoord`s (float and
   double), and the D6 landing nudge. `TryAddGridCoordinate` (`FrameUtils.h:139-157`) keeps its signature and
   no-wrap contract; every new neighbour site passes a table entry's delta through it. `common::InsideArea`
   (`Common/Math/MathUtils.cpp:149-160`, `MathUtils.h:227`) loses its last caller and is deleted.
2. **Transfers.** `game::TransferRequest` (`Frame.h:76-81`) replaces `iDeltaX`/`iDeltaY` with `int8_t iDirection`.
   `PrepareTransferRequest` (`FrameBase.cpp:141-157`, declaration `FrameBase.h:245-247`) drops its `CellBounds`
   parameter, writes `iDirection = CellExitDirection(position)`, subtracts `kCellDirections[iDirection]`'s centre
   offset, and applies the D6 nudge and `ASSERT`. The five producers (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp:317,350-352`,
   `Battleships/Battleships.cpp:307,341-343`, `Blasters/Blasters.cpp:125,144-146`, `Missiles/Missiles.cpp:280,312-314`,
   `Omnis/OmnisNavigation.cpp:16,56-58`) drop the bounds local and log `Direction` instead of `Delta`. The span
   checks at `ServerTransferManager.cpp:52-57` and `ReconcileReplayClientState.cpp:47-51` are deleted with no
   replacement range check: `CellExitDirection` is the only producer of `iDirection` and returns a table index by
   construction, so both consumers index `kCellDirections[rRequest.iDirection]` directly, with no ASSERT added, and
   pass its delta to the existing `TryAddGridCoordinate` check (`ServerTransferManager.cpp:59-66`,
   `ReconcileReplayClientState.cpp:54-58`), whose logs print the direction.
3. **Containment consumers.** Every `common::InsideArea(..., LocalCellArea())` and `IsOutOfBounds` call becomes
   `IsInsideCell(position)`: `Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:30`,
   `ServerTransferManager.cpp:258`, `Fighters.cpp:479`, `Fighters/FightersCombat.cpp:93,133`, `Battleships.cpp:471`,
   `Battleships/BattleshipsCombat.cpp:94,139`, `Blasters.cpp:92`, `Missiles.cpp:337`, `Omnis.cpp:237`,
   `Omnis/OmnisCombat.cpp:149,185,381`, `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:256`,
   `ServerSimulationHarnessRigs.cpp:243`, and the Fighter launch-pose test (`ServerClientManager.cpp:161`). Collision's exit trace (`Engine/Source/Frame/Collision.cpp:142`) calls the
   hex trace. `Collision::Collide` (`Collision.h:102`, `Collision.cpp:366,372`, caller `FrameBase.cpp:301`) /
   `Collision::SetupZones` (`Collision.h:116`, `Collision.cpp:292-305`) and `PushersInterpolate::SetupZones` (`Engine/Source/Frame/Collections/Pushers/Pushers.h:56`,
   `PushersUpdate.cpp:13,43-54`, caller `FrameBase.cpp:234`) drop the `vecArea` parameter and span the bounding
   rectangle from the `GridCoord.h` half extents (8 × 8 collision zones of ~129.9 × 112.5 m, 50 × 50 pusher zones
   of ~20.8 × 18 m); the wave spawn grid (`Frame.cpp:285-302`, 20 × 20) reads the same extents.
4. **Islands, terrain, and the corner invariant.** The corner triangles of the bounding rectangle outside the hex
   belong to the neighbours: they hold no placement, no simulation reads anything stored there, and nothing this
   cell owns lives there. Enforcement points:
   - Placement: `GenerateIslandChain` (`Engine/Source/Frame/IslandChainPlacement.cpp:227-317`) keeps its streams,
     seeds, draw order, and the 107 cap; `context.fHalfWidth`/`fHalfHeight` (`:231-237`) become the bounding half
     extents (≈519.62, 450), and the anchor target stays one third of the bounding rectangle from its south-west
     corner (`:251-252`), ≈(−173.2, −150) before jitter. `PlaceAnchor` (`:139-164`) computes, per table direction,
     the limit `apothem - margin - s_k` (440 m − s_k) with s_k the rotated footprint rectangle's support along that
     edge normal, returns false if any limit is negative (never for the Huge footprint, D2), and scales an outside
     target toward the centre until it meets every limit. `TryTouchPlace` (`:207-213`) rejects a hull with any
     vertex outside `IsInsideHex` at the margin inset (a convex hull lies in the convex hex iff its vertices do).
     The header comments `IslandChainPlacement.h:14` ("within [-450,+450]") `:19` ("truncated by the cell edge"),
     and `:34-35` ("SW third of the cell", "leave the cell") state the hex extents.
   - Elevation: `BlendPlacementIntoGrid` (`IslandTerrain.cpp:409-463`) skips every texel whose centre fails
     `IsInsideCell`, so corner samples keep the sea floor `BuildElevationGrid` assigns first (`IslandTerrain.cpp:469`).
     This is needed because a touch-placed footprint can extend past its hull (only the hull AABB is limited,
     `IslandChainPlacement.cpp:207-213`; "rectangles may overlap underwater, hulls may not",
     `Common/Math/ConvexHull.h:8-9`). A simulation read at a position just outside the hex therefore returns sea
     floor, as an out-of-grid read does today (`IslandTerrain.cpp:505-508`).
   - Spawns: every `Spawn(SpawnInfo)` and the wave spawn grid test `IsInsideCell` (Approach 3), so no entity this
     cell owns is created in a corner.
   - Zone grids: collision, pusher, and wave grids span the corners as storage only; they hold only this cell's
     own rows, and collision events after a row's hex exit time are cut off by the exit trace (`Collision.cpp:142-152`).
   - Grid save trust check (`Engine/Source/File/GridSave.cpp:119-131`): the placement centre range test
     (`:127-128`) becomes `IsInsideCell`, which every generated centre passes because placements stay inside the
     inset hex; a centre in a corner, NaN, or ±inf aborts the load; the comment at `:119-120` says so.
   Elevation storage keeps `kiElevationGridDimension` 1,024 per axis (4 MiB unchanged, `GridCoord.h:47-49`); the
   pitch follows the bounding rectangle per axis, ≈1.015 m in x (1,039.23/1,024) and 0.879 m in y (900/1,024,
   today's value) — the pitch constants at `IslandTerrain.cpp:22-23`, the pitch and origin constants at
   `TerrainCollisionUtils.cpp:12-15,158-161`, and the grid origin uses of the deleted `kfBaseAreaMinimumX/Y` at
   `IslandTerrain.cpp:432-435,448,452,501-502`, which read the bounding-rectangle minimum (the negated
   `GridCoord.h` half extents) instead; and the grid comments in `GridCoord.h:47-48` ("~0.88-unit spacing",
   "sub-meter"), `IslandTerrain.cpp:408` (texel 0,0 at the south-west corner, `kfBaseAreaMinimumX/Y`),
   `IslandTerrain.h:143` ("[-450,+450] meters"), and `Engine/Source/Frame/CellStaticData.h:46`.
   `ResolveLocalPosition` (`IslandTerrain.cpp:233-255`) loops while `!IsInsideCell(p)`: k = `CellExitDirection(p)`,
   step the coordinate by table delta k with `TryAddGridCoordinate` (returning false when it fails, as today),
   subtract n_k, and run the D6 nudge without its `ASSERT`. In exact arithmetic each step strictly lowers |p|
   (dot(p, n_k) > |n_k|²/2 gives |p − n_k|² < |p|²), so the loop ends; the nudge removes the float residual near a
   shared edge or vertex.
5. **Client subscription, camera, render.** The 3x3 loop (`ClientSessionRuntime.cpp:675-733`) walks the six table
   entries through `TryAddGridCoordinate` and keeps the visible-neighbour filter, testing `mf4LargeVisibleArea`
   with `RectangleOverlapsHex` against the neighbour hex at its offset from the camera basis (D9), so both sides
   stay in the camera basis frame (`ClientSessionRuntime.cpp:680-683`). `mpVisibleNeighbors[8]` becomes `[6]`
   (`ClientSessionRuntime.h:108`) and `kiMaximumDesiredCoordinates` 9 becomes 7 (`ClientSessionRuntime.cpp:509`);
   `kiCoordinateSlots` stays 16 (`Engine/Source/Network/NetworkProtocol.h:86`), only its "9 visible" comment
   changes. `MakeRenderBasis` (`Engine/Source/Graphics/CameraBase.h:12-27`) uses the centre offset and drops
   `constexpr`; its comment and the `RenderBasis` comment (`GridCoord.h:57-61`) change "exact in float" to
   "deterministic"; `RenderBasis` stays client-only and out of CRC. The step tests in
   `CameraBase::ShiftToRenderedCell` (`Engine/Source/Graphics/CameraBase.cpp:88-110`) and
   `RetainedAreaBasis::Advance` (`Engine/Source/Graphics/Render/Render.h:56-76`) become `GridDistance <= 1`, and the
   `RetainedAreaBasis` comment (`Render.h:57-58`, "the one-cell case the 3x3 subscription allows") names the
   edge-neighbour step instead. The
   double absolute camera positions (`Engine/Source/Graphics/Render/MainUniforms.cpp:433-434`,
   `Engine/Source/Graphics/Render/WaterUniforms.cpp:109-110`) use the double centre offset. The debug cell-edge
   draw (`MainUniforms.cpp:41-58`) draws the six hex edges. Island rendering, particles, audio, and shaders
   consume only the basis offset (`Engine/Source/Graphics/Islands.cpp:268,349,373`; no shader encodes cell size).
6. **Server trust check.** Subscription adjacency (`Engine/Source/Network/Server/Server.cpp:738-746`) becomes
   int64 `GridDistance <= 1` against each authorized coordinate, keeping the 64-bit hostile-input guard.
7. **Ship navigation (game).** The 4-bit field from `## Settled decisions`: `ShipTypes.h:88-109` setter
   mask `0x0700` → `0x0F00` (`:107`), getter mask `& 0x7` → `& 0xF` (`:101`), `kNavigationDirectionBit3 = 0x0800`,
   and the comment. `ShipBehaviours.cpp` literals follow: 5 → 7 at `:190,193,350,546-547,714,724,727,738`; 4 → 6 at `:198,260,371`; the held-direction
   check at `:213-227` is deleted (D11); the choice at `:227-246` becomes the D8 pick, which writes the direction,
   clears the island destination, and logs only when it differs from the current direction (D11); the neighbour
   destinations at `:438-455` become `vecCellCenter + n_k` from the table. Spawn defaults 4 → 6 at
   `Fighters.cpp:522`, `Battleships.cpp:513`, `Omnis.cpp:292`. `FleetNavigationController.cpp` replaces
   `NavigationDirectionOffset` (`:13-25`) with the table, the "cardinal mode" skip `0..3` → `0..5` (`:81`), and
   draws `Random(5)` (`:86`).
8. **Compatibility gates.** Bump each of these four by one from its value at claim time (its value at this Plan's
   baseline in parentheses): `kiProtocolVersion` (36, `NetworkProtocol.h:83`); the `Frame::kiVersion` base term
   (140, `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:92`, gating grid saves at `GridSave.cpp:17,75` and
   replay streams; `ShipFlags` bit meaning, coordinate meaning, and the fleet random draw count change);
   `FrameInput::kiVersion` (27, `Projects/BrokenEngineSandbox/Source/Frame/FrameInput.h:10`;
   `fleetWantedCoordinate` changes meaning, `Projects/BrokenEngineSandbox/Source/Frame/StatusChange.h:123`); and
   `ReplayMeta::kiVersion` (2, `Projects/BrokenEngineSandbox/Source/Game.h:31`, holds `clientGridCoordinate`).
   `kiNavDataVersion` (`Engine/Source/Frame/NavBuild.h:15`) is not bumped: navigation data's format and cell-local
   meaning do not change and it is never persisted. Island packs and DataPacker sources are untouched.
9. **Agent and docs.** `cell_coordinate_probe` (`Engine/Source/Agent/Commands/CellCoordinateProbe.cpp:63-65`)
   reports the bounding rectangle's width and height from the `GridCoord.h` extents; agent coordinate JSON stays a
   2-integer array, now axial. The prose under `## In scope` is updated to the hex rules, including the
   `kClientSubscribe` row's "3×3 adjacency" (`Documents/Architecture/Network.md:298`), which becomes grid distance
   at most 1, and the east-edge coast example, rewritten for a flat-top edge.
10. **Field rename (D4).** Every `.iX`/`.iY` on a `GridCoord` in the 48 files listed under `## In scope` becomes
    `.iQ`/`.iR`, including designated initializers and `GridCoord.h`'s own members; no other type's fields change.
11. **Fleet spawn safe search (game, D12).** `ServerClientManager.cpp` deletes `kfFleetSpawnOffsetX`/`Y` and
    their comment (`:81-84`) and adds the file-local `ComputeFleetSpawnCandidates()` (the D12 island candidates in
    search order) and `FindFleetSpawnPoint` (the D12 per-spawn ship test over those candidates, the origin's
    current frame, and the points taken earlier in the `TickRespawnTimers` pass, with the one 2 ×
    `kfBattleshipPusherRadius` clearance), plus the constructor that fills
    `mFleetSpawnCandidates` once, asserts it is not empty, and logs its count and first point;
    `ServerClientManager.h` (`:10-27`) declares the constructor, `std::vector<XMFLOAT2> mFleetSpawnCandidates`, and
    `bool mbFleetSpawnBlocked`, and adds the taken-points list, by reference, as a parameter of `SpawnFleetMember`
    (`:15`). `TickRespawnTimers` (`ServerFleetManager.cpp:133-160`) declares that local list before its owner,
    fleet, and member walk (`:138`), its growth covered by the existing allocation suppression (`:135-136`), whose
    `Heap:` comment it extends to name the list, and passes it to the `SpawnFleetMember` call (`:155`). In `SpawnFleetMember`
    (`ServerClientManager.cpp:92-188`) the Omni and Battleship cases (`:182-183`) call `FindFleetSpawnPoint` with
    that list; with no point they log once per blocked episode and `return`, as the Fighter launch does
    (`:135-164`), and the Fighter case ignores the list; with one they clear `mbFleetSpawnBlocked`, append the point
    to the list, and call `QueueSpawnPointSpawn` (`:123-130`), which sets x and y from that point and keeps the
    flight-plane height, the +x heading, the grace period, and the origin cell.

## In scope

Slices: S1 Common and engine Frame/File geometry (first); then in parallel S2 Engine Network, Graphics, Agent,
Profile, Server, and `GameBase.cpp`; S3 the game project; S4 docs. Each file below belongs to exactly one slice.

- S1 `Common/Math/MathUtils.h` / `.cpp` — the `common::` hex helpers in Approach 1; delete `InsideArea`.
- S1 `Engine/Source/Frame/GridCoord.h` — cell constants, `kCellDirections`, elevation and `RenderBasis` comments,
  field names (Approaches 1, 5, 10).
- S1 `Engine/Source/Frame/FrameUtils.h` — the engine wrappers and D6 nudge; delete the rectangular helpers and
  `LocalCellArea` (Approach 1).
- S1 `Engine/Source/Frame/FrameBase.cpp` / `FrameBase.h` — `PrepareTransferRequest`; the `SetupZones` and
  `Collide` calls at `FrameBase.cpp:234,301` (Approaches 2, 3).
- S1 `Engine/Source/Frame/Collision.cpp` / `Collision.h` — exit trace call, `Collide`/`SetupZones` signatures
  (Approach 3).
- S1 `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp` / `Pushers.h`;
  `Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp` (Approach 3).
- S1 `Engine/Source/Frame/IslandChainPlacement.cpp` / `.h` — containment limits and cell-extent comments
  (Approach 4).
- S1 `Engine/Source/Frame/IslandTerrain.cpp` / `.h`, `Engine/Source/Frame/TerrainCollisionUtils.cpp`,
  `Engine/Source/Frame/CellStaticData.h` — pitch/origin, the corner skip in `BlendPlacementIntoGrid`,
  `ResolveLocalPosition`, grid comments (Approach 4).
- S1 `Engine/Source/File/GridSave.cpp` — placement centre trust check (Approach 4); S1 `Engine/Source/File/Replay.cpp`
  — rename only.
- S2 `Engine/Source/Network/Client/ClientSessionRuntime.cpp` / `.h` — 6-neighbour visible filter, array sizes
  (Approach 5).
- S2 `Engine/Source/Network/Server/Server.cpp` — subscription adjacency (Approach 6);
  `Engine/Source/Network/Server/ServerTransferManager.cpp` — span check removal, table destination, spawn assert
  (Approaches 2, 3); `Engine/Source/Network/NetworkProtocol.h` — protocol bump and slot comment (Approaches 5, 8).
- S2 `Engine/Source/Graphics/CameraBase.h` / `.cpp`, `Engine/Source/Graphics/Render/Render.h` (`Advance` step
  test and `RetainedAreaBasis` comment),
  `Engine/Source/Graphics/Render/MainUniforms.cpp`, `Engine/Source/Graphics/Render/WaterUniforms.cpp` (Approach 5).
- S2 `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp` (Approach 9).
- S2 `Engine/Source/Server/ServerDisplay.cpp` — rename only (D4, D10).
- S3 game project `Projects/BrokenEngineSandbox/Source`: `Frame/Frame.cpp` (Approaches 3, 8), `Frame/Frame.h`
  (`TransferRequest`, Approach 2), `Frame/FrameInput.h` and `Game.h` (Approach 8); collections
  `Battleships/Battleships.cpp`, `Battleships/BattleshipsCombat.cpp`, `Blasters/Blasters.cpp`,
  `Fighters/Fighters.cpp`, `Fighters/FightersCombat.cpp`, `Missiles/Missiles.cpp`, `Omnis/Omnis.cpp`,
  `Omnis/OmnisCombat.cpp`, `Omnis/OmnisNavigation.cpp` (Approaches 2, 3, 7); `Ships/ShipBehaviours.cpp` (D8 pick,
  held-direction check deleted per D11), `Ships/ShipTypes.h`, `Network/Server/FleetNavigationController.cpp`
  (Approach 7); `Network/Client/ReconcileReplayClientState.cpp` (Approach 2);
  `Agent/Commands/ServerSimulationHarnessRigs.cpp` (Approach 3); `Network/Server/ServerClientManager.cpp` (launch
  test, Approach 3; spawn constants, `ComputeFleetSpawnCandidates`, `FindFleetSpawnPoint`, constructor, the Omni and
  Battleship cases of `SpawnFleetMember`, `QueueSpawnPointSpawn`, Approach 11),
  `Network/Server/ServerClientManager.h` (constructor, `mFleetSpawnCandidates`, `mbFleetSpawnBlocked`, and the
  taken-points parameter of `SpawnFleetMember`, Approach 11), and `Network/Server/ServerFleetManager.cpp` (the
  taken-points list in `TickRespawnTimers` and its `SpawnFleetMember` call, Approach 11).
- Rename only (Approach 10), beyond the files above: S2 `Engine/Source/Agent/AgentCommandsClientGeneric.cpp`,
  `Engine/Source/Agent/AgentCommandsServerGeneric.cpp`, `Engine/Source/Agent/AgentCommandsShared.cpp`,
  `Engine/Source/Agent/Commands/ClientDesyncProbe.cpp`, `Engine/Source/Agent/Commands/FrameEdit.h`,
  `Engine/Source/Agent/Commands/ReplayHarnessRigs.cpp`, `Engine/Source/GameBase.cpp`,
  `Engine/Source/Network/Client/Client.cpp`, `Engine/Source/Network/Client/ClientDesyncCore.cpp`,
  `Engine/Source/Network/Client/ClientSubscriptions.cpp`, `Engine/Source/Network/Client/ReconcileReplay.cpp`,
  `Engine/Source/Network/Client/ReconcileReplayCrc.cpp`, `Engine/Source/Network/Client/ReconcileReplayTick.cpp`,
  `Engine/Source/Network/NetworkCursor.h`, `Engine/Source/Network/Server/ServerBroadcaster.cpp`,
  `Engine/Source/Network/Server/ServerBufferedFrames.cpp`, `Engine/Source/Network/Server/ServerSessionBase.cpp`,
  `Engine/Source/Profile/ProfileScreens.cpp`; S3 `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipStatusChanges.cpp`,
  `Projects/BrokenEngineSandbox/Source/Graphics/Camera.cpp`, `Projects/BrokenEngineSandbox/Source/Network/Client/ClientReconciler.cpp`,
  `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp`,
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerFleetSerialization.cpp`,
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp`,
  `Projects/BrokenEngineSandbox/Source/Ui/Screens/HudScreen.cpp`. Together with the renamed sites in the files
  listed above (S1 `GridCoord.h`, `FrameUtils.h`, `Replay.cpp`; S2 `CellCoordinateProbe.cpp`, the five Graphics
  files, `ClientSessionRuntime.cpp`, `Server.cpp`, `ServerTransferManager.cpp`, `ServerDisplay.cpp`; S3
  `Battleships.cpp`, `Blasters.cpp`, `Fighters.cpp`, `Missiles.cpp`, `Omnis.cpp`, `OmnisNavigation.cpp`,
  `ShipBehaviours.cpp`, `ReconcileReplayClientState.cpp`, `FleetNavigationController.cpp`,
  `ServerClientManager.cpp`), these are the 48
  `GridCoord` files `git grep -I -l '\.i[XY]\b'` lists.
- S4 prose stating square-cell rules (Approach 9): `Engine/Source/Frame/AGENTS.md` (lines 34, 45-57, 99-108),
  `Engine/Source/Frame/Collections/Pushers/AGENTS.md` (line 9), `Engine/Source/Network/Server/AGENTS.md`
  (adjacency bullets), `Engine/Source/Graphics/AGENTS.md` (`## World and Camera Contracts` rebasing bullet),
  `Engine/Source/File/AGENTS.md` (`## Grid Saves` placement bullet), `Projects/BrokenEngineSandbox/Source/AGENTS.md:21`,
  `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md:57`,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md:58-67` (modes 4/5 renumbered 6/7),
  `Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md:35` (the "one fixed spawn point" becomes the
  per-spawn safe search of D12: the first candidate clear of cell (0,0)'s generated islands and of its ships, or a
  retry on the next advancing update),
  `Documents/Architecture/Network.md:298` (`kClientSubscribe` adjacency) and `:347-358`,
  `.agents/skills/agent-harness/references/command-reference.md` (45, 93, 214),
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:302-311`,
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/replay.md` (216, 329-341),
  `Projects/BrokenEngineSandbox/Documents/AgentHarness/verification.md:18`.

## Out of scope

- Island templates, Gaea bakes, DataPacker sources, and island packs: placement rotation stays arbitrary and
  nothing in `DataPacker/Source` depends on cell shape (`ProcessBakedRegion.cpp`'s `CropRect` fields are not
  renamed).
- `Common/Math/ConvexHull.h` (unchanged; island placement keeps using it).
- Shaders under `Engine/Data/Shaders` and `Projects/BrokenEngineSandbox/Data/Shaders`: none encodes cell size or
  edges; the EWNS lighting and east/west shadow conventions are world directions, not cell edges.
- Navigation build and query (`Engine/Source/Frame/NavBuild.cpp`, `NavCellData.cpp`, `NavQuery.cpp`) and
  `kiNavDataVersion`: placement-derived, cell-local, shape-independent.
- Island render residency, particles, audio rebasing, and every collection render body: they consume the
  `RenderBasis` offset unchanged.
- `kiCoordinateSlots`, `kiMaxActivePlacements`, `kiMaximumIslandsPerCell`, `kiElevationGridDimension`, the
  collision, pusher, and wave zone counts, `kfCellEdgeMarginMeters`, the placement RNG streams and their seeds,
  `SeedFromGridCoordinate`, the camera zoom ceilings and default eye height, and the sticky-subscription mechanism.
- A second subscription ring, any subscription beyond the 6 edge neighbours, and any visibility guarantee, assert,
  or zoom clamp tying hex size or camera zoom to the ring.
- A hex-shaped (row-trimmed) elevation layout, and any new `PostRead` range check on the navigation-direction field.
- A range check or ASSERT on `TransferRequest::iDirection` at its consumers.
- Any held-direction rule for fleet navigation (D11).
- Any `common::` geometry helper without a caller in this conversion, an orientation parameter on the helpers, a
  third-party hex library, and multi-cell pathfinding.
- The server monitor's cell map (`Engine/Source/Server/ServerDisplay.cpp:228-240`): it stays a square grid of the
  axial coordinates, sheared until the separate hex-redraw follow-up Plan lands (D10).
- Tools/AgentHarness, WorktreeCli, ModelViewer behaviour (ModelViewer and DataPacker only rebuild against the
  changed `Common`/`Engine` sources).
- Any compatibility, migration, or conversion path for square-cell saves, replays, or peers.
- Fleet spawn safe search (D12): a general spawn-search helper or framework, a spatial index for the search,
  spawning in a cell other than (0,0) when it is crowded, a least-crowded or any other fallback spot instead of
  wait-and-retry, a reserved point per player or fleet or any spacing beyond the D12 clearance, a per-type spawn
  radius for Omnis, a changed spawn heading, reading the live or loaded origin placements, a load-time check of
  saved placements against the candidates, treating agent-injected spawns still queued as obstacles, and any change
  to the Fighter launch pose or its retry.

## Execution card

### What does this plan do?
Replaces the 900 m square simulation cell with a flat-top regular hexagon of the same 900 m height across the
engine and the BrokenEngineSandbox game. Grid coordinates become axial, and one table of six neighbour directions
in `common::`, built on DirectXMath, decides containment, cell exit, transfer direction, adjacency, ship headings,
and the subscription overlap test; transfers and ship navigation carry a direction index 0..5, so a diagonal or
two-cell move cannot be expressed, and fleets pick their heading toward a wanted cell by integer arithmetic with no
random draw and no held direction. Storage grids keep a rectangle over the hex, and its corners, which belong to
the neighbours, hold nothing this cell owns. The fixed fleet spawn point is replaced by a per-spawn safe search in
cell (0,0): each Omni or Battleship spawn takes the first candidate clear of the cell's generated islands and of
every ship already there, and waits for a later tick when none is clear. Protocol, frame, frame-input, and replay-metadata versions are bumped
so every square-cell save, replay, and peer is rejected.

### Why this is good for the codebase
The user wants Coretide's world to have six equal edge neighbours with no corner-only crossings, for encirclement
and supply-line play. Today square geometry is spread across rectangular tests at more than 20 sites; after this
change one table defines the cell shape and its tie rule, so client and server agree on ownership by construction
and an invalid transfer cannot be represented. Players get the new map shape and fleets that share an origin and
destination share a route; developers get one place to change cell geometry.

- Goal: every simulated cell is a flat-top regular hexagon 900 m tall (apothem 450 m, circumradius ≈519.62 m)
  addressed by axial `GridCoord` (`iQ`, `iR`) with 6 edge neighbours; ownership, exit, transfer, adjacency, and
  headings follow one `common::` direction table with the settled D3 tie rule (the larger coordinate owns a shared
  edge); `TransferRequest` and ship navigation carry a 0..5 direction index; a rebased transfer position lands
  inside its destination through the bounded one-ulp nudge (D6); the fleet heading toward a wanted cell is the
  integer most-direct direction with no random draw and no held direction (D8, D11); the client keeps today's
  best-effort visible-neighbour filter over the 6 neighbours with a dedicated rectangle/hex overlap test; the
  bounding-rectangle corners hold nothing this cell owns; each Omni and Battleship spawn takes the first candidate of
  a fixed search over cell (0,0) that lies inside the hex, clear of every island footprint of its generated layout,
  and clear of every ship row in its current frame and every point taken earlier in the same `TickRespawnTimers` pass, or is retried on
  the next advancing update with one log per blocked episode (D12); no square-cell code or compatibility path
  remains.
- Out of scope: island templates/packs and DataPacker sources, `ConvexHull.h`, shaders, navigation build/query and
  its version, render residency, slot, zone, and elevation-grid counts, placement RNG, camera zoom limits, any
  second subscription ring or visibility guarantee, a hex-shaped elevation layout, a transfer-direction range
  check, a held-direction rule, any helper without a caller, a third-party hex library, the server monitor's hex
  redraw (follow-up Plan), any compatibility path, and for the spawn search a general search helper or spatial index,
  spawning in another cell, a least-crowded fallback, reserved per-fleet points, per-type Omni radius, a changed
  heading, use of loaded placements, queued agent spawns as obstacles, or a Fighter launch change.
- Tier trigger: Tier 3 — CRC'd positions and transfer conversion (`Engine/Source/Frame/FrameUtils.h:161-175`,
  `Engine/Source/Frame/FrameBase.cpp:150-153`); CRC'd `ShipFlags` layout and fleet random draws
  (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipTypes.h:88-109`,
  `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:229`); wire/protocol and
  save/replay compatibility (`Engine/Source/Network/NetworkProtocol.h:83`, `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:92`,
  `Engine/Source/File/GridSave.cpp:127`); trust boundaries (`Engine/Source/Network/Server/Server.cpp:738-746`,
  `GridSave.cpp:119-131`); spans Frame, Network, File, Graphics, Agent, Server, Common, and the game project.
- Interfaces and invariants: an entity's position lies inside its own hex under the D3 rule, and `Spawn` still
  refuses or asserts on an outside position; `PrepareTransferRequest` stays the single destination-frame
  conversion, writes the direction index `CellExitDirection` returns, and its output lies inside the destination,
  nudged by at most 4 one-ulp steps toward the destination centre and asserted (D6); transfer destinations are
  table neighbours and subscription destinations are at grid distance 0 or 1; `TryAddGridCoordinate` keeps its
  no-wrap contract and forms every neighbour and destination coordinate; the cell shape is one `constexpr` table
  stored nowhere, so no save or payload carries it; the `common::` helpers take the apothem or table as input,
  never reach Engine, use DirectXMath types and `XM_CALLCONV`, each has a caller, and the simulation-path ones
  (inside test, exit trace, exit direction, centre offset, grid distance) use no `...Est` approximation; the inside
  test treats NaN and ±inf as outside; corner invariant — the bounding-rectangle corners hold no placement, no
  non-sea-floor elevation sample, and no entity this cell spawns, and zone grids there hold only this cell's rows;
  island placement keeps stream purposes, draw order, and the 107 cap, and the Huge anchor always places; the
  elevation grid stays bit-identical client/server and is read only by its own cell in tick code; the
  navigation-direction field is bits 8-11 stored value+1 (-1 roaming, 0-5 directions, 6 island tour, 7 follow
  flagship) and modes 6/7 keep today's equal draw counts; the fleet heading pick is integer-only, draws no random
  number, is a function of the current and wanted coordinates only, and is written only when it differs from the
  current direction, so same origin and destination give the same route (D11); the client subscribes only to its
  cell and overlapped edge neighbours, best effort; `RenderBasis` stays client-only and out of CRC; shared
  PostRender state stays bit-deterministic; the fleet spawn candidate list is built once per server process from
  `GenerateIslandChain(kOriginCoordinate)` — candidates inside the hex inset by `kfBattleshipPusherRadius` and more
  than `kfBattleshipRadius` outside every footprint rectangle of that layout, asserted non-empty — and each Omni or
  Battleship spawn takes its first entry whose horizontal distance from every Fighters, Omnis, and Battleships row
  of the origin's current frame and from every point taken earlier in the `TickRespawnTimers` pass exceeds 2 ×
  `kfBattleshipPusherRadius`; with none clear the spawn is skipped and
  the member stays dead, so it is tried again on the next advancing update, with one `kWarning` per blocked episode and no `ASSERT`; the search
  draws no random number and runs only on the server (its point travels in the spawn `StatusChange`, and replay
  playback reads the recorded spawn); the bumped versions reject every older save, replay, and connection.
- Acceptance checks:
  - A1 Build: `/compile` BrokenEngineSandbox client and server in Debug and Release x64, ModelViewer Debug x64
    (compiles `MathUtils.cpp` and `IslandTerrain.cpp`), and DataPacker Release x64 (compiles `MathUtils.cpp`,
    `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj:15`) succeed with no new warnings; the static-check
    runner passes.
  - A2 Geometry probe: `cell_coordinate_probe` on origin and on all 6 table neighbours of origin, on both
    endpoints, returns `area.width` ≈1,039.23 and `area.height` 900 (the hex bounding rectangle), `samplesFinite`
    true, and identical `placements.crc` and `elevation.crc` client vs server; the probe on origin on the unedited
    tree, `before the first edit`, reports 900/900.
  - A3 Edge transfer: for each of the 6 directions, an edge-coast `SpawnOmni` (`commands-server.md` recipe, started
    just inside the edge midpoint 450 m from the centre along that direction and moving along it) arrives at the
    neighbour the table names, found by `query_collection` on that coord, with no DEBUG_BREAK and no assert; on the
    unedited tree, `before the first edit`, the (1,−1) and (−1,1) coasts — whose starts near (389.7, −225) and
    (−389.7, 225) lie inside today's 900 m square — arrive at (1,0) and (−1,0), not the table's coord, because the
    square has no such edge neighbour.
  - A4 Tie rule (code read): `IsInsideHex` is closed on exactly table indices 1-3 and strict on 4, 5, 0 in
    all-lanes-pass polarity; `HexExitDirection` returns the largest crossed dot with exact ties to the first
    maximum in table order; the exit trace returns a hit exactly when the end fails the inside test; settled
    against the D3 proof in `## Settled decisions`.
  - A5 Subscription ring: with the client following an owned omni, `describe_scene.subscribedCoords` never holds a
    coord at grid distance > 1 from `clientGridCoord` (sticky entries aside) and holds at most 7 non-sticky; at the
    default eye height (`Engine/Source/Graphics/CameraBase.h:80`) with the omni held still near its cell centre it
    holds only `clientGridCoord`, and held still just inside one edge midpoint it adds exactly the neighbour across
    that edge; `set_client_grid_coord` to (1,1) relative to the owned cell, grid distance 2 but adjacent under the
    old rule, logs `Server::ClientSubscribe Rejected (not adjacent)`; the unedited tree, `before the first edit`,
    accepts it.
  - A6 Determinism: a Debug replay record/playback across at least 3 hex-edge transfers and one fleet cell change
    completes with every recorded tick CRC matching (`.agents/skills/agent-harness/references/replay.md`), and a
    10-minute live client/server run with fleets crossing cells logs no CRC mismatch or resync, while periodic
    `query_collection` samples of ship rows (`fighters`, `battleships`, `omnis`) decode `flags` bits 8-11 (stored
    value+1) to an observed heading set containing every direction 0-5 plus modes 6 and 7.
  - A7 Rejection of old data: a grid save written by the unedited Debug server `before the first edit` fails to
    load on the edited server with the version-mismatch log (`GridSave.cpp:79`); a replay recorded by the unedited
    build `before the first edit` is rejected at playback; protocol rejection of an old client is settled by code
    read of the bumped `kiProtocolVersion` gate (no second-build harness path exists).
  - A8 Code read: the grid save's corner-centre, NaN, and ±inf rejection (`GridSave.cpp:119-131`); the D6 nudge
    (at most 4 one-ulp steps toward the centre, then `ASSERT`) in `PrepareTransferRequest` and the same nudge
    without `ASSERT` in the `ResolveLocalPosition` loop; the D8 pick — integer cube dot, first maximum in table
    order, no float, no `common::Random` call, a function of the current and wanted coordinates only, with no
    held-direction check left and the direction, island destination, and log written only when the pick differs
    from the current direction (D11); `RectangleOverlapsHex` near a vertex and its false result for edge touching;
    the corner skip in `BlendPlacementIntoGrid` and the inset-hex limits in `PlaceAnchor` and `TryTouchPlace`;
    `GenerateIslandChain` keeps its placement streams, seeds, draw order, and the 107 cap; every new neighbour or
    destination site goes through `TryAddGridCoordinate`; the cell shape is stored in no save or payload;
    `RenderBasis` stays client-only and out of CRC; the 4-bit navigation field with its `0x0F00` setter mask and
    `& 0xF` getter mask; `ComputeFleetSpawnCandidates` — no `common::Random` call, a fixed candidate order, its
    only input the `GenerateIslandChain(kOriginCoordinate)` output, `DeterministicSinCos` for the footprint
    rotation, the `kfBattleshipPusherRadius` hex inset and the strict `kfBattleshipRadius` footprint clearance, run
    once from the constructor and not from `ResetState`, with the non-empty `ASSERT` there; `FindFleetSpawnPoint` —
    no `common::Random` call, candidates walked in list order, reading every Fighters, Omnis, and Battleships row
    of `mCells.at(kOriginCoordinate).pCurrent` (Interpolate `pVecPositions`, x and y only) and the points taken
    earlier in the same `TickRespawnTimers` pass, with the one strict D12 2 × `kfBattleshipPusherRadius` clearance for every row and taken point and no `ASSERT`;
    the blocked path's `return` before `QueueFleetSpawn` (so the member stays dead and `TickRespawnTimers`
    calls `SpawnFleetMember` for it again) and its single `kWarning` per `mbFleetSpawnBlocked` episode; and no fixed spawn constant left;
    and no
    `...Est` call in the simulation-path `common::` helpers — paths or properties no harness command reaches;
    settled by code read plus the build.
  - A9 Fleet spawn safe search: on a fresh Debug server, the startup log's first fleet spawn candidate P0 lies
    inside the hex inset by 6.75 m and more than 4.5 m outside every footprint rectangle of cell (0,0), checked
    against the (0,0) `islands` (`local`, `rotation`, `footprint`) that `describe_scene` reports on a client
    subscribed to (0,0); after the first client connects to the empty origin, `query_collection` `battleships` on
    (0,0) shows its fleet Battleship at P0 on its spawn tick, or no farther from it than the Battleship can travel
    since; then, with the server sent `pause {"paused":true}` while that Battleship is still less than 13.5 m from P0
    (its `local` read after the pause), a second client launched as the harness's additional simultaneous client
    connects (its fleet is created while paused and its Battleship spawns on the first advancing update, whose search
    reads the paused frame), and after `pause {"paused":false}` its fleet Battleship appears on (0,0) at a `local`
    other than P0 that, allowing the travel since its spawn tick as in the first check, is farther than the D12
    clearance of 13.5 m from every ship row of (0,0) read
    while paused, the first Battleship included (a paused-read distance of 13.5 m or more from that Battleship to
    P0 is a setup miss: repeat); a second server launch logs the
    same P0 and candidate count. The blocked path (no clear candidate) is settled by A8's code read, because no
    harness command fills the whole hex with ships.
- Roles: required — the `/next-plan` preparation `implementer`, `reviewer` `/plan-audit`, fresh `reviewer`
  `/plan-simplicity-review`, `implementer` `/external-grill-plan` evidence, main `/verify-external-claims` via
  `locator` when raised, `implementer` `/implement-plan` slices (S1 first; then in parallel S2, S3, S4, file
  ownership per `## In scope`), `implementer` `/update-affected-code`, `mechanic` `/code-style-review` and
  `/comment-review`, `builder` `/compile` (pre-edit Debug build for the `before the first edit` runs, then every
  `Build required`), fresh `reviewer` `/over-engineering-review` finder plus `implementer` validator, fresh
  `reviewer` `/repo-code-review`, fresh `reviewer` `/coherence-review` for the docs, `reviewer`
  `/adversarial-review`, `implementer` `/update-claude-docs`, fresh `reviewer` `/progressive-disclosure-review`,
  `/agent-harness` runs for A2, A3, A5, A6, A7, A9, `implementer` `/finalize-changes` at the landing gate;
  conditional — `/glsl-review` only if a shader changes, `/update-vcxproj` only if a file is added or removed,
  `/resolve-findings` per accepted finding, `/external-diagnose-bug` on a failed retest.

## Verification

1. Before the first edit, on the unedited tree in Debug (client and server): write one grid save and record one
   short replay, keeping both for A7; run the A2 probe on origin (900/900); run the A3 (1,−1) and (−1,1) edge
   coasts and record the arrival coords ((1,0) and (−1,0) expected); run the A5 (1,1) `set_client_grid_coord` and
   record its acceptance.
2. Build: `/compile` client and server Debug and Release x64, ModelViewer Debug x64, and DataPacker Release x64;
   run `pwsh -NoProfile -File .agents/scripts/Invoke-StaticChecks.ps1` as the Change Workflow states.
3. Harness (`/agent-harness`, Debug): A2 probe on origin and its 6 neighbours on both endpoints (≈1,039.23 × 900);
   A3 six edge coasts via `inject_payload`, each started just inside its 450 m edge midpoint and confirmed with
   `query_collection` and the server log; A5 `describe_scene.subscribedCoords` while following an omni, at the cell
   centre and near an edge midpoint, plus the (1,1) `set_client_grid_coord` rejection; A9 the logged first spawn
   candidate P0 against the (0,0) `describe_scene` islands, the first fleet Battleship's `query_collection`
   position at P0 after a connect, the second client's fleet Battleship, connected while the server is paused with
   the first Battleship near P0, at a different point clear of every paused-read ship row, and the same P0 and
   candidate count on a second server launch.
4. Replay determinism and CRC (A6): record across hex-edge transfers and a fleet cell change, play back, require
   every tick CRC to match; then a 10-minute live run with no CRC mismatch or resync logged, sampling ship rows'
   `flags` until headings 0-5 and modes 6 and 7 have all been observed.
5. Old-data rejection (A7): load the step-1 save and play the step-1 replay on the edited build; both are rejected
   by their version gates.
6. Code read (A4, A8): each listed path or property, including the D11 single-write fleet pick, the D12
   candidate list's inputs and non-empty `ASSERT`, and the per-spawn search's inputs, clearance, and
   wait-and-retry path with its single log per blocked episode.
