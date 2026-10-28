<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-28T20:01:42.347Z","dependsOn":[]} -->
# Investigate Pusher Work Across Flight Planes

## Context

The user deferred pusher optimization while completing the shadow and Fighter fixes and explicitly requested a tracked follow-up investigation. The unresolved gap is evidence: no measurement establishes the cost of cross-plane candidates or exact-X/Y handling, and no proof establishes when either can safely be skipped. This Plan executes a bounded investigation and records a decision; it does not authorize a simulation optimization.

Current source evidence:

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`, `SetupZones`: a 50 by 50 XY grid registers each source across its radius footprint, without height partitioning. `ApplyPush` visits the query's zone in registration order, filters flags and self, rejects candidates using full 3D squared distance against the source radius, computes full 3D falloff, and applies horizontal force. Exact coincident XY within the sphere gate produces a signed 0.001 m X correction and a finite horizontal force direction.
- `Engine/Source/Frame/Collections/Pushers/Pushers.h:37`: `PushersInterpolate::kiVersion` is 4; the owning version gates push behavior through frame save/replay compatibility.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp:605`, `ApplyPusherPush`: the game wrapper consumes both the impulse and correction from the engine query. Ship updates apply that correction to their local positions.
- `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h:9-11`: heights are 6, 50 and 100 m, with gaps 44, 50 and 94 m. Fighters use high, Omnis medium and Battleships low as their initial target planes; other allowed settled heights and transitions still require analysis. Fighter and Omni climb rates are 25 and 12 m/s in their collection headers.
- Fighter, Omni and Battleship headers define distinct source pusher radii; their collection implementations supply those values to Sync. Plane spacing alone is therefore insufficient evidence for a general exclusion.

`Documents/Investigations/Engine/SameCollectionCollision.md` concerns collision semantics and same-type separation; `ShipForcesInThreeDimensions.md` concerns vertical force behavior. Neither owns this performance investigation, and their historical descriptions are not authority for the current pusher contract. No live executable Plan duplicates this scope.

## Design

The author's recommendation is an evidence-only investigation, because correctness and meaningful cost have not been established. Its deliverable is a findings record at `Documents/Investigations/Engine/PusherPlanePerformance.md` with the following completed analysis and a justified adopt, defer or reject recommendation for each opportunity.

1. Trace every producer and consumer of the shared pusher query, including non-ship sources admitted by the ship flags. Record actual maximum radii by type and any model-derived sphere extents used by the relevant interaction. Distinguish a directed source-radius query from summed collision radii; derive the separation inequality from the operation actually performed, including the boundary where force can be zero but exact-XY correction still applies.
2. Build a settled-plane interaction table from actual allowed plane occupancy and maximum source extents. Prove which directed pairs cannot contribute force or correction at any XY position, or show a counterexample. Trace the registered snapshot's height and plane freshness rather than assuming the current owner position matches it. Do not infer safety from initial target planes alone.
3. Independently examine exact-XY handling. Enumerate same-plane coincident spawns, overlapping destinations, cross-plane coincident XY inside source range, climbing ships, and cell transfer/reconstruction. Being exactly on a plane says nothing by itself about equality of two ships' XY coordinates. Prove reachability or impossibility for every proposed exclusion, preserving finite normalization, separation and deterministic correction ordering.
4. Measure baseline candidate counts, 3D rejections, accepted interactions, exact-XY frequency, and time in `SetupZones`/`ApplyPush` for reproducible mixed fleets: settled separate planes, dense same-plane ships, transitions, coincident spawns and cell transfer. Use `/agent-harness` for live setup and `/analyze-diagsession` if a CPU capture is used. Record workload, hardware/build, commands and repeated measurements; distinguish candidate reduction from total CPU savings. Temporary developer instrumentation may be used only for this investigation and must be removed before its completion. Do not assert a speedup from counts alone.
5. Recommend whether any proved exclusion merits a separately approved implementation Plan. Describe the smallest mechanism justified by measured costs, including registration/query overhead and transition handling; recommend retaining current behavior if proof or benefit is absent. No production optimization is part of this Plan.

## Critical files

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`: `SetupZones`, `ApplyPush`, zone capacity and iteration order.
- `Engine/Source/Frame/Collections/Pushers/Pushers.h`: flags, radii, `ApplyClampedPush`, behavior version.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`: `ApplyPusherPush`, `ClimbTowardsFlightPlane`.
- `Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h`: plane heights.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/{Fighters,Omnis,Battleships}/`: radius definitions, allowed planes, Sync, spawn, movement/correction and transfer paths.

## In scope

Source inspection, interaction bounds, baseline profiling, temporary measurement instrumentation that is removed, and the completed findings record named in Design. Follow only correctness-dependent radius/model producers and pusher consumers needed to settle these two opportunities. The investigation procedure and output are fixed; choices about future production behavior are its results rather than prerequisites to executing it.

## Out of scope

Changing pusher behavior, adding height buckets or new persistent fields, removing exact-XY checks, changing planes or climb rates, changing sphere sizes, adding collision/ram damage, vertical forces, shadow changes, unit tests, and implementing any recommended optimization. Any production change needs its own approved scope and risk classification.

## Acceptance criteria

- The findings record lists every relevant source extent and flag-filtered query path with durable source citations and explicit maximum bounds, and settles each directed settled-plane pair by proof or counterexample.
- It separately settles the safety of skipping exact-XY handling, including same-plane coincident ships; no conclusion relies solely on a ship being settled or a nominal plane gap.
- Reproducible baseline measurements distinguish setup, candidate rejection and correction cost across the five workload categories in Design. The originating session supplied no performance capture or ordered live reproduction commands; the investigation records its own commands and limitations rather than inventing earlier evidence.
- Transitions, coincident spawn positions, registered snapshot timing, cell-local transfer and deterministic ordering are covered explicitly. Each opportunity ends with a supported recommendation, including rejection or deferral when evidence is insufficient.
- Completion leaves no production behavior, layout, version, serialization or wire change and no temporary instrumentation. Runtime optimization is proposed separately only when both safety and benefit have evidence.

## Notes

Change Workflow Tier 1 for the final evidence/documentation-only result, with no production signature or invariant change. Temporary instrumentation must remain observational and be removed. A future optimization touching deterministic simulation, CRC, save/replay compatibility or shared engine/game behavior would require Tier 3 classification and the appropriate behavior/version verification; this Plan does not waive those requirements. Preserve full-3D sphere gating/falloff, planar force, exact-XY correction only within the source sphere gate, query snapshot immutability and cell-local coordinates throughout the investigation.

No directional dependencies or mandatory coordination with a live Plan were found. File and validate this Plan now; perform the investigation only when it is claimed and approved. Finalize its tracked creation with the enclosing authorized session change when that change is landed.
