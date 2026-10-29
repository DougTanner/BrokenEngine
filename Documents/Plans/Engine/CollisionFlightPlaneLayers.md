<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T12:55:37.306Z","dependsOn":[]} -->
# Separate Collision Broadphase by Flight-Plane Layer

## Context

**Important: the edge case to watch is always a ship, blaster, or missile
entering a flight-plane layer while transiting between planes.** The user's
direction was: "If any other areas could benefit from this 'layer
intependence' add them as a followup plan, with an important note that the
edge case to watch will always be ships entering a layer by transiting between
them". Here a transiting object is a Fighter or Omni climbing or descending
between planes, an Omni blaster fired on a pitch between planes, or a falling
missile. Each of these crosses a halfway height (28 m between the 6 m and 50 m
planes, 75 m between the 50 m and 100 m planes) during a tick. Every design
step, review, and acceptance check below has to cover that case.

The pusher change that lands together with this Plan made pushers
layer-independent. Every tick each ship is assigned to exactly one flight-plane
layer, the flight plane nearest its height, with the exact midpoints 28 m and
75 m going to the lower layer. `PushersInterpolate::SetupZones` and `ApplyPush`
(`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`) then keep one
zone grid per layer, so a ship pushes only ships in its own layer. That
nearest-plane assignment is called the pusher layer rule below. This Plan
reuses it and does not define a second one. "Flight-plane layer" here is a
different thing from `engine::CollisionLayer`, which is one registering
collection.

Current code shows the same kind of cross-plane candidate work in entity collision:

- `Engine/Source/Frame/Collision.h:8-9` and `Collision.cpp:298-362` (`Collision::SetupZones`): each colliding layer pair gets one 8 by 8 zone grid in XY over the cell's bounding rectangle. It has no height dimension, so every object of both layers goes into every zone its XY footprint touches.
- `Collision.cpp:608-675` (`CollideLayerPair`) and `:523-606` (`TestAndCollectPair`): every A object is tested against every B object in its zones. Each test checks alignment and then runs a 3D swept-sphere test (`SweptSphereTest`, `:255-296`) or a 3D end-distance test against the combined radius.
- The pairs that reach this test are blasters (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/BlastersUpdate.cpp:185-198`) and missiles (`Missiles/MissilesUpdate.cpp:310-323`), both swept, against the Fighter, Omni, and Battleship layers (`Frame/HealthDamage.h:39-53`). The game documents that collision ignores planes (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md` `## Ship Invariants`, last bullet).
- Collision radii are small compared with the gaps between planes. Blasters and
  missiles are 0.5 m (`BlastersUpdate.cpp:27`, `HealthDamage.h:18`). Ships are
  1.5 m for Fighters (`Fighters/Fighters.h:27`), 1.1 m for Omnis
  (`Omnis/Omnis.h:33`), and 4.5 m for Battleships
  (`Battleships/Battleships.h:27`). So the largest combined radius is 5 m,
  while planes are 44 m or more apart
  (`Projects/BrokenEngineSandbox/Source/Frame/FlightPlaneTable.h:9-11`). A pair
  settled on different planes can never pass the 3D test, yet it is still
  tested in full whenever the two share an XY zone.
- Objects that transit between planes in normal play: Fighters climb at 25 m/s and Omnis at 12 m/s (`Fighters.h:34`, `Omnis.h:45`). Battleships never leave plane 0 (`Battleships.h:35`). Omni blasters travel at 150 m/s (`Omnis.h:28`) on a pitch toward the target (`Omnis/OmnisCombat.cpp:245-247`). Falling missiles descend through the planes.

Candidates are collected and then sorted into one global total order before anything is committed (`Collision.cpp:389-399`). So leaving out a pair that cannot pass the 3D test changes no result, as long as no pair that can pass it is left out. `Documents/Investigations/Engine/SameCollectionCollision.md` records the same XY-only broadphase in the context of same-layer collision. It is not a Plan and does not own this work. No live executable Plan covers it.

## Design

The author recommends making the collision broadphase layer-independent in the same way as the pusher zone grids, without dropping any hit. This rule matters for the edge case above: an object that is transiting, or close enough to a halfway height to reach the next layer, goes into every layer it can reach, never just one.

1. Layer assignment: call the pusher layer rule's existing function. If it is
   private to the pusher source file when this Plan is claimed, move it into
   `engine::FlightPlanes` (`Engine/Source/Frame/FlightPlanes.h/.cpp`) so that
   pushers and collision share one definition, and do not copy it. Every
   flight-plane layer covers one connected band of heights, and the rule never
   decreases as height increases. So the layers that a height interval
   `[lo, hi]` overlaps are exactly the layers of `lo` through the layer of
   `hi`. Both facts need plane heights that never decrease with index, since a
   height equally near two planes takes the lower index; the `FlightPlanes`
   constructor asserts them strictly ascending today
   (`Engine/Source/Frame/FlightPlanes.cpp:17-22`). The unimplemented Feature
   `Documents/Features/Frame/HighLayerHeightTweaks.md` would break them only if
   it let High drop strictly below Low or Medium.
2. Registration: give `LayerPairZones` one zone grid per flight-plane layer,
   sized from `engine::FlightPlanes::Count()` and capped at
   `kiMaxFlightPlanes`. The `LayerPairZones` constructor stops sizing the index
   vectors. Instead, `InsertObjectIntoZones` checks whether the grid it is given
   is still unsized and, if so, first sizes both index vectors of all 64 of that
   grid's zone pairs to `kiCollisionZonePreallocate`: first-use sizing, like
   the pusher zone storage's. So a grid's index vectors are committed only when
   the thread first registers an object into it, a grid it never registers into
   commits only its 64 zone-pair headers (about 4 KiB, from
   `sLayerPairZones.Resize`), and the overflow check there never sees an
   unsized vector. For each
   object, take the Z range of the same positions
   `CalculateObjectZoneRange` already uses: start and end for a swept pair, the
   end alone otherwise. Widen that range by the object's own
   `CollisionLayer::fRadius` plus a fixed 1 m allowance for floating-point
   rounding, and register the object in every flight-plane layer the widened
   range overlaps. This is safe because when two objects collide at some
   moment, their heights then differ by at most the sum of their radii. Their
   own-radius intervals around those heights therefore overlap at a shared
   height, and both objects are registered in the layer of that height. An
   object settled on a plane covers one layer. An object that is transiting, or
   within its radius plus 1 m of a halfway height, covers two or more.
3. Query: `CollideLayerPair` loops over the flight-plane layers that each A object is registered in, and over its XY zones in each of them. The existing per-A generation counter (`sTestedBGeneration`) already stops a B object in several zones from being tested twice. It also stops a B object registered in several layers from being tested twice, so no new deduplication is needed.
4. Leave unchanged: `SweptSphereTest`, the end-distance test, the alignment filter, the maximum-time cutoffs, terrain and cell-exit outcomes, the candidate sort, `CommitCandidate`, and result storage. No collection calls `AddLayer` differently, and no game file changes.

Exposure:

- Determinism and CRC: collision results feed CRC-checked damage and destroy state. If the change is correct, it leaves the set of accepted candidates unchanged, so frames and CRCs stay bit-identical. It changes no layout, serialization, wire format, or version: no `kiVersion` or protocol bump.
- Threading: zone storage stays `thread_local`, one copy per Dispatch worker
  and reconcile thread. Today each `LayerPairZones` constructor commits both
  index vectors of all 64 zone pairs (`Engine/Source/Frame/Collision.cpp:86-96`,
  `Common/StableVector.h:68-71`), 2 MiB per layer pair, and a thread's first
  `SetupZones` builds `kiCollisionLayerPairPreallocate` (16) of them
  (`Collision.cpp:300-302`): 2 x 2,048 x 8 B x 64 x 16 = 32 MiB committed per
  simulation thread whether or not a pair is used. Building every per-layer
  grid that way would make it about 96 MiB at 3 layers. With Design step 2's
  first-use sizing and no overflow, a thread commits 2 MiB of index vectors
  only for each (layer pair, flight-plane layer) grid it registers an object
  into, bounded by 16 x `Count()` x 2 MiB (96 MiB at 3 layers), plus about
  4 KiB of zone-pair headers per grid, touched or not. Overflow grows past
  that bound as it does today: layer pairs past 16 (`Collision.cpp:340-344`)
  and a zone's index vector up to its 4x reserve
  (`Collision.cpp:74-75,226-234`).

## Critical files

- `Engine/Source/Frame/Collision.cpp`: `LayerPairZones`, `CalculateObjectZoneRange`, `InsertObjectIntoZones`, `InsertLayerObjectsIntoZones`, `SetupZones`, `CollideLayerPair`.
- `Engine/Source/Frame/Collision.h`: the private declarations of those functions.
- `Engine/Source/Frame/FlightPlanes.h/.cpp`: only if the pusher layer rule has to move here so both users share it.
- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp`: only to call the moved function, if step 1 moves it.
- `Engine/Source/Frame/AGENTS.md` `## Architecture`, the collision bullet: record the flight-plane layer broadphase and the transit rule.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/AGENTS.md` `## Ship Invariants`, last bullet: the statement that collision ignores planes must still hold. Planes are used only to skip work, and hits are unchanged.

## In scope

- Adding a flight-plane layer dimension to `LayerPairZones` and its reset in `SetupZones`.
- Registering objects in layers by their radius-widened Z range, following Design step 2.
- Looping over layers in `CollideLayerPair`.
- Sharing the pusher layer rule as described in Design step 1.
- Updating the two documentation bullets above.
- Temporary measurement instrumentation that counts pair tests, removed before landing.

## Out of scope

- Changing collision radii, the 3D tests, candidate ordering, alignment filtering, the maximum-time and terrain/cell-exit outcomes, or `CollisionLayer`'s public fields.
- Same-layer collision, and ship-to-ship collision or ram damage.
- `engine::AreaDamage`, targeting and registry queries, navigation, and the pusher behavior itself.
- Changing flight-plane heights, climb rates, or Omni blaster pitch.
- Defining a second layer rule.
- Unit tests.

## Acceptance criteria

- Bit-identical outcomes: a server replay recorded before the change, covering a mixed fleet on all three planes, verifies after the change with every CRC matching (`/agent-harness` replay determinism check). The originating session recorded no harness setup or replay for this area. The implementer records its own ordered commands.
- Transit cases covered: the replay or an added live scenario includes Fighters and Omnis climbing and descending through 28 m and 75 m, Omni blasters fired between planes, and missiles falling through planes. CRCs match in each case, and in each case hits on transiting ships are recorded both before and after the change.
- Benefit measured: temporary counters, removed before landing, record pair tests per tick before and after on the same replay. The `kCpuTimerPostRenderCollide` timer is compared too. The landing summary states the measured reduction. If pair tests do not drop for a mixed-plane fleet, stop and report to the user before landing.
- No layout, version, serialization, or wire change, and no instrumentation left behind.

## Notes

Change Workflow Tier 3. Trigger: determinism/CRC. The collision outcome feeds CRC-checked simulation state, and a wrong layer registration would silently drop hits and cause a desync. That calls for `/plan-audit`, `/external-grill-plan`, and `/adversarial-review`, with transit across the halfway heights as the main hypothesis to disprove.

This Plan reuses the pusher layer rule that lands with its creating change. It has no `dependsOn` edge: it reaches the primary branch in the same landing as that rule. If the rule is missing from the primary tip at claim time, stop and report it instead of defining a replacement. No mandatory coordination with a live Plan was found.
