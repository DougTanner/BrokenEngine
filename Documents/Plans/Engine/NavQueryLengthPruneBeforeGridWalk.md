<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-13T23:07:21.482Z","dependsOn":[]} -->
# A* tests an edge's length before walking the grid for it

## Context

`AStarPath` (`Engine/Source/Frame/NavQuery.cpp`) offers edges to its local
lambda `TryNeighbor`, which returns early when the neighbour is closed, then
when the cost so far plus the edge length is not below the neighbour's
`pGCost`, and only then pays for `RectangleCrossingPenalty`.

Edges to and from rectangle corners have no baked adjacency, so the local
lambda `TryVisibleNeighbors` tests them against the obstacle polygons at query
time. It does so in the other order: for every open node in its range it calls
`SegmentBlockedByObstacle` — a slab clip plus a grid walk that runs
`SegmentsIntersect` on every edge in every grid cell the segment crosses — and
only an unblocked node reaches `TryNeighbor` and its length test. The grid walk
of every node the length test then rejects is wasted.

How often that happens: `TryVisibleNeighbors` runs over the corner range for
the start node and for every expanded static vertex, and over all nodes
(`0` to `iTotalNodes`) for every expanded corner. Expanding the start node
gives every node it can see a `pGCost`. From then on, most offers to an
already reached node cost at least as much as what it holds and fail the length
test, yet each still pays a full grid walk first. The corner range is empty
when the query carries no rectangles, so only queries with rectangles are
affected; the game passes rectangles on its ship routing path
(`NavRectangleBoard::Select` into `engine::NavQueryDirection`,
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`).
No timing was measured; the saving is the skipped calls.

Why the reorder cannot change a result: an edge is accepted only when all of
these hold — the neighbour is open, the segment is unblocked, the length test
passes, and the cost with penalties is below `pGCost`. `SegmentBlockedByObstacle`
takes its points by value and `NavData` by const reference, writes nothing, and
keeps no state. Nothing is written between the tests of one edge, so each test
reads the same values in either order, and the length is the same float
expression evaluated once. An edge rejected by the length test writes nothing
today either; the reorder only skips the grid walk whose answer that rejection
already discards. The same edges are accepted in the same loop order with the
same costs, so the heap, its tie-breaks, the parents, and the returned waypoint
are bit-identical.

Origin: a refactor candidate logged in
`Documents/Investigations/Engine/RefactorCandidates.md`, validated against
commit `213d2577`.

## Design

Recommended: move the obstacle test into `TryNeighbor`, after the length test
and before `RectangleCrossingPenalty`, behind a second `bool` parameter that
says whether this edge needs it. When the parameter is set, the graph is not
empty (`iVertexCount != 0`), and
`SegmentBlockedByObstacle(f2Current, f2Neighbor, pVertices, rNavData)` is true,
`TryNeighbor` returns. `TryVisibleNeighbors` becomes a plain loop that calls
`TryNeighbor` with the parameter set for every index in its range; its own
closed test goes, because `TryNeighbor` already returns first on a closed
neighbour. Every other `TryNeighbor` call passes the parameter clear and
behaves as today. Whether the parameter is defaulted or passed at each call is
the implementer's trivial choice.

This is recommended over repeating the length expression inside
`TryVisibleNeighbors` ahead of the grid walk, because that copy would have to
match `TryNeighbor`'s expression exactly, and a later change to one alone would
silently drop edges inside the deterministic simulation.

The obstacle test stays ahead of the penalty, as it is today, so a blocked edge
still pays no rectangle clips.

Update the comment above `TryVisibleNeighbors`, which says closed nodes go first
to skip the grid walk, and the comment at the penalty line, so both describe
the new order.

## Critical files

- `Engine/Source/Frame/NavQuery.cpp`

## In scope

- In `AStarPath`: the `TryNeighbor` lambda's parameter list and the new
  obstacle test between its length test and its penalty.
- In `AStarPath`: the `TryVisibleNeighbors` lambda's loop body and the comment
  above it.
- In `AStarPath`: the `TryNeighbor` call sites, only as far as the new
  parameter requires.

## Out of scope

- `SegmentBlockedByObstacle`, `SegmentsIntersect`, `PointInAnyPolygon`,
  `RectangleCrossingPenalty`, `Distance`, and `AStarHeap`.
- The order nodes are offered in, the cost and heuristic expressions, and the
  heap tie-break.
- The eager `pStartVisible` loop, the lazy end-visibility test, and the corner
  `PointInAnyPolygon` pass at the top of `AStarPath`.
- A* scratch memory (`AStarMemory`, `ComputeAStarMemoryLayout`,
  `BindAStarMemory`, and the workbuffer reservations in `NavQueryDirection` and
  `NavQueryEdgeReachable`); other Plans own those.
- `NavQuery.h`, `NavBuildInternal.h`, the navigation build, and every caller.
- Navigation and frame versions: no data, format, or result changes.

## Risk tier and invariants

Tier 2 under `.agents/references/risk-tiers.md`: scoped runtime behavior of one
subsystem — the work a navigation query does — with no signature, data, or
result change. It is not Tier 1 because the code runs inside the deterministic
simulation, so the determinism invariant is exposed to a mistake. It is not
Tier 3 because the change must not alter what the simulation computes; a
result that differs in any bit is a defect in the change, never an accepted
outcome.

Invariants to hold:

- Bit-identical query results and CRC (`Documents/FloatingPointDeterminism.txt`,
  `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation`): the same edges
  accepted, in the same order, at the same costs.
- An empty graph has no grid, so `SegmentBlockedByObstacle` is never called
  when `iVertexCount` is zero.
- The build path and the runtime query keep sharing the one
  `SegmentBlockedByObstacle` definition.
- No allocation and no new state: per-cell ticks run in parallel.

## Acceptance criteria

- In `AStarPath`, no `SegmentBlockedByObstacle` call for a rectangle-corner
  edge runs before that edge's closed test and length test.
- The length expression and the `>=` comparison in `TryNeighbor` are unchanged
  and appear once.
- The client and server projects build through `/compile`.
- A replay determinism check through `/agent-harness` passes on a session in
  which ships route around islands and around each other, so queries carry
  rectangles. The originating evidence records no setup for such a session; the
  executing session builds one.

## Notes

- The reorder is exact, not approximate: the proof is in `## Context`. A
  reviewer should check it against the code rather than rely on the replay
  check alone, since a replay only covers the queries that session happened to
  make.
- The profile row `kCpuTimerPostRenderUpdateNavigationQuery` times these
  queries if a before and after figure is wanted; none is required.
