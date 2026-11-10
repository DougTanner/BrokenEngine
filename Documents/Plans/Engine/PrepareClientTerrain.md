<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-10T23:38:19.545Z","dependsOn":["Documents/Plans/Engine/ElevationGridRowSpans.md"]} -->
# Prepare client terrain before live cell activation

## Context

The user requested asynchronous client terrain preparation before cell activation, with elevation generated
locally. RunFrameTick currently allocates and builds the first elevation grid and the client render placement
cache before sim phases (`Engine/Source/Frame/FrameBase.cpp:299`). ClientSessionRuntime creates local
placeholders for the current and visible neighbour coords and immediately adds them to the active set
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:677`). Online cells receive static data first, then a
full state after reliable delivery (`Engine/Source/Network/Server/ServerSessionRuntime.cpp:203`).
`Engine/Source/Network/Client/Client.cpp:99` clears the receive vectors each poll, and
`Engine/Source/Network/Client/Client.cpp:456` marks the wire slot active after accepting a full state. Runtime
adoption drains full states and deltas in the same poll
(`Engine/Source/Network/Client/ClientSessionRuntime.cpp:258`). Static receipt currently replaces CellStaticData
immediately (`Engine/Source/Network/Client/ClientSessionBase.cpp:57`).

## Design

The design below is the plan author's recommendation for Tier-3 review and user approval after claim. The proposed boundary covers live client cells and keeps the existing synchronous menu, server replay bootstrap and debug island cycling paths.

Use one bounded, ordinary-priority client preparation worker owned by ClientSessionRuntime and initialized with
common::ThreadLocal::Entry. Wake it for queued work and drain completed results on the main thread without
waiting for completion. The existing Multithreading dispatcher joins before returning and disallows nested
dispatch, and PersistentWorker runs at time-critical priority and mandates a paired wait
(`Common/Threading/AGENTS.md:5`). Follow the ordinary jthread initialization and stop/join precedent in
`Engine/Source/Server/ServerMonitorHost.cpp:62`. Bound pending work by currently needed local cells and
subscribed or requested coordinates, coalesce current work per coordinate and prefer the current camera/player
cell over neighbours. A cancelled in-flight job may finish but cannot publish. Stop and join at teardown before
IslandTerrain or packed assets can be released.

Main owns desired sets, placement generation, every Cell, residency requests and packet adoption. Generate local
placements on main because GenerateIslandChain memoizes regions (Engine/Source/Frame/AGENTS.md, Terrain and
Navigation). Use the received authoritative placement list for online preparation. Give the worker only copied
placement inputs, a unique preparation identity and access to immutable elevation template inputs. Build into
job-owned vectors using the same shared builder and strict-FP setup for nonempty placement lists. For an empty
list, skip BuildElevationGrid, which always allocates a grid (`Engine/Source/Frame/IslandTerrain.cpp:445`).
Leave elevationGrid empty to preserve the tick path's empty-list behavior (`Engine/Source/Frame/FrameBase.cpp:301`)
and the collision path's null-grid bypass (`Engine/Source/Frame/TerrainCollisionUtils.cpp:343`).
WaitForElevationMaps finishes populating
the heightmap pointers before live startup (`Engine/Source/Main.cpp:308`,
`Engine/Source/Frame/IslandTerrain.cpp:198`). Prove the residency path
(`Engine/Source/Frame/IslandTerrainResidency.cpp:124`) only mutates independent members from those the worker
reads. Build the render placement query cache from the same prepared placements. Keep texture acquisition, nav
ownership and all render API work on main.

Maintain main-thread pending state separate from published mCells. Keep successful transport full-state adoption
and ACK state changes at the same receive/drain boundary. Transfer accepted static/full/delta records out of the
poll vectors in the same frame into the matching staged state. Capture the assigned slot and epoch and load
generation from the receive records, and use a distinct local preparation identity across reset/replacement even
when coordinates repeat. Preserve initial full-state and active resync distinctions, tick filtering, delta
deduplication, ACK floors and existing bounded-buffer overflow/resync behavior. Keep clock-arrival sampling at
the transport arrival boundary and first-confirmed-state adoption at the publication boundary. A completed
resync on unchanged static data reuses the prepared grid and needs no worker. If authoritative placement bytes
match an already prepared or running local result in the same lifetime, reuse only the derived vectors after
matching the ordered placement bits and current preparation identity, while adopting authoritative navigation
data. Otherwise supersede local work and prepare authoritative input.

Commit a ready cell on main at the normal drain/active-set boundary after revalidating desire or subscription,
local preparation identity, and online slot/epoch/load generation. Commit static data and initial snapshot
together before reconciliation, tick-reference collection, or renderer visibility. Keep old complete local
placeholders visible while a replacement is pending where the existing camera policy can use them. Pending or
cancelled cells cannot enter mCells, render projection, reconciliation or the active list. An empty cell uses
explicit readiness independent of elevationGrid contents. Calculate desired coords independently of readiness
so neither online subscription requests nor local preparation stall. Continue selecting an already usable camera cell or existing
no-camera branch (`Engine/Source/GameBase.cpp:774`) while the intended cell is preparing. Preserve hydration
against the current complete frame and the existing first-full-state clock initialization. Transfer buffered
updates to the published cell in order by tick and let normal reconciliation consume them. Do not enqueue
repeated preparation for ready cells.

Include the first connected origin cell. ClientSession::OnConnectionAccepted clears kMainMenu before calling
GameBase::CreateNewFrame, which currently publishes an origin snapshot and generates its
placements (`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:169`,
`Engine/Source/GameBase.cpp:1070`). Stage that origin through the same readiness gate, with the existing session
reset sequenced so it neither discards the new preparation nor activates the origin early. Keep synchronous menu creation.

Discard pending world work on disconnect/reconnect, server load and game session reset. Invalidate a pending
coord on unsubscribe, cancellation, superseding input or slot reuse. Handle local camera churn without retaining
unwanted job outputs. Preserve the current synchronous paths for server replay bootstrap
(`Engine/Source/File/Replay.cpp:352`), the initial menu cell and debug island cycling
(`Engine/Source/GameBase.cpp:1063`), and server setup. Clients connected to a replaying server use ordinary
asynchronous subscriptions. Document this scope and assert in Debug that live cells never reach lazy terrain
construction in RunFrameTick.

## Critical files

- Engine/Source/Network/Client/ClientSessionRuntime.cpp and .h own desired and active sets, transport draining and preparation.
- Engine/Source/Network/Client/ClientSessionBase.cpp and Client.cpp own static receipt, transport admission and reset.
- Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp owns the first connected transition in OnConnectionAccepted.
- Engine/Source/GameBase.cpp owns the live CreateNewFrame origin path, session reset and publication consumers. Engine/Source/Frame/FrameBase.cpp owns the old tick-time fallback.
- Engine/Source/Frame/IslandTerrain.cpp and CellStaticData.cpp own derived terrain and render data.

## In scope

- ClientSessionRuntime lifecycle and preparation ownership, ComputeActiveSet, UpdateDesiredCoordinates, PollAndDrain, ApplyReceivedFullStates, ApplyReceivedUpdates and subscription reset/retirement paths. Carry matching receive identity through the client receive records in Client.cpp and Client.h.
- ClientSessionBase::ApplyReceivedStaticData and reset integration, plus GameBase live activation/readiness and the lazy-build gate in FrameBase::RunFrameTick where required.
- ClientSession::OnConnectionAccepted, the live client branch of GameBase::CreateNewFrame and GameBase::ResetSession integration for staging the first connected origin.
- Worker-owned derived terrain and render data and relevant ownership documentation. Use existing project membership validation only if a new source file is necessary.

## Out of scope

- Server scheduling including synchronous server replay bootstrap, transport packet formats, game state or save/replay formats, menu bootstrap scheduling, navigation baking, and unit tests.

## Risk and invariants

Tier 3. Threading crosses client networking, simulation and render. The worker never owns or mutates live cells or renderer state. Publication requires a complete matched static input and derived result, and an initial frame wherever the cell is simulation-active. Existing slot admission, ACK/resync, CRC and deterministic terrain behavior must hold.

## Verification

- Before the first edit, capture a Debug replay and live baseline covering crossing, static/full-state adoption
  and activation. Under controlled worker delay after the change, exercise offline camera pin and neighbour
  arrival, first online join from the menu including paused first-full-state clock seed, first deltas arriving
  while preparation is pending, authoritative resync during preparation and after activation, unsubscribe/re-subscribe and slot
  reuse, load and reconnect while jobs are outstanding, same-coord replacement, island-free cells, and teardown.
  Observe no pending cell in the active set/render consumers, no accepted update lost, no stale completion
  adopted, and no render wait or lazy live grid build, including for the first connected origin. Verify that an
  island-free cell reaches readiness with an empty elevationGrid. Use the existing harness and temporary trace
  points when an observation has no existing command. Remove temporary diagnostics before landing.
- Debug client and server finish a recorded replay loop across terrain and a cell transition with the existing checksums and no desync. Verify that the connected client prepares replayed cells through ordinary subscriptions. Compare main-menu and synchronous server replay bootstrap behavior to the baseline.
- Compare a live prepared cell grid CRC with the shared builder on the same nonempty placements. Use `cell_coordinate_probe` for the generated reference where the live placement CRC matches. Use temporary diagnostic reads for the live grid or when the live placement list differs.
- Profile matched repeated camera activation scenarios before and after with the same packed data and warmed
  runs. Record worker runtime, request-to-publication latency and main render/tick timings. Use existing
  profiling where it exposes the phase, with temporary timing only for missing observations. Show that the live
  grid build leaves the render-frame dispatch, that the main-frame spike decreases beyond run variability and
  that publication never waits for the worker. Require those outcomes before completing the scheduling change.
  Use BES client/server Debug and Profile x64 builds with compile and agent-harness.

## Execution card

### What does this plan do?

It builds live client cell terrain on a background worker. It publishes a cell only when the terrain, and any required authoritative initial frame, are ready for simulation and rendering.

### Why this is good for the codebase

The live client currently includes first-time terrain work in the tick dispatch the render frame waits for. Preparing immutable inputs independently removes that work from the dispatch while retaining the client's authority and subscription ordering.

- Goal: Remove live cell elevation preparation from the client tick/render path.
- Out of scope: Wire and deterministic data changes, menu bootstrap scheduling, server scheduling including synchronous replay bootstrap, and unit tests.
- Tier trigger: Tier 3, new thread and activation/transport integration (`Engine/Source/Network/Client/ClientSessionRuntime.cpp:258`).
- Interfaces and invariants: Worker reads immutable template input and owns results, main adopts complete matching output and preserves accepted frames and deltas, no live lazy build or render waits, identical terrain bits and replay CRC.
- Acceptance checks: Debug replay and Profile camera-cohort baseline before the first edit. Delayed worker scenario matrix including first connected origin and empty-grid readiness, exact terrain bits, clean Debug replay and Profile render-frame improvement outside cohort variation or revert.
- Roles: Plan alternatives, simplicity review, plan audit, grill, implementation and propagation, C++ correctness/style/comment/over-engineering reviews, adversarial review, compile and agent harness, and acceptance verification.

