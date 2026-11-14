<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:44:17.784Z","dependsOn":[]} -->
# One function for "build a cell's navigation data if unbuilt"

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(architecture phase), verified by code read against commit `4cf9d607`. Paths
are from the repository root.

The server builds a cell's navigation data lazily, and the build has two
halves that must stay together: call
`BuildCellNavigationData(navigationData, islands)` and set
`CellStaticDataFlags::kNavigationDataBuilt` (an empty result is a valid
build, so only the flag says "built"; see the comment on
`CellStaticData::flags` in `Engine/Source/Frame/CellStaticData.h`). That
pair, with its not-yet-built test and its `ScopedSuppressAllocationTracking`
scope, is written out three times:

- `Engine/Source/Frame/FrameBase.cpp:263-269`, in the frame tick, on the
  per-cell dispatch thread, through a `const CellStaticData&`;
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:132-139`, in
  `ServerSessionRuntime::PreparePausedSubscriptions`, on the main thread;
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp:23-30`,
  in `FlagshipReachesEdge`, on the main thread during the fleet step, through
  a `const CellStaticData&`.

The three blocks differ only in `engine::` qualification and comment text. No
engine function wraps the pair: `BuildCellNavigationData`
(`Engine/Source/Frame/NavBuild.h`) takes the vector and does not set the
flag. A fourth site that forgot the flag would rebuild on every call, and one
that forgot the test would rebuild a built cell.

Not checked: `Projects/Coretide` is absent from session worktrees, so whether
it holds another copy is unknown.

## Design

Recommended: one server-only const member function on `CellStaticData`,
because the function touches only that struct's own `mutable` members
(`navigationData`, `flags`) and two of the three callers hold a
`const CellStaticData&`. A free function beside `BuildCellNavigationData` in
`NavBuild.h` is the alternative; it would need `CellStaticData` there, and
`CellStaticData.h` already includes `NavBuild.h`.

1. `Engine/Source/Frame/CellStaticData.h`: declare, under
   `#if defined(BT_SERVER)`, a const member that builds this cell's
   navigation data unless `kNavigationDataBuilt` is set
   (suggested name `BuildNavigationDataIfUnbuilt`; the name is a trivial
   choice). Update the comment on `flags` that names
   `BuildCellNavigationData` as the server's setter.
2. `Engine/Source/Frame/CellStaticData.cpp`: define it with the body the
   three sites share — the flag test, the `// Heap:` note and
   `ScopedSuppressAllocationTracking` scope, the `BuildCellNavigationData`
   call, and the flag set.
3. Replace each of the three blocks with one call. Each site keeps the part
   of its comment that says why the build happens there (first tick of a
   cell; a paused update serving pending subscriptions; the fleet step
   running ahead of dispatch).

Invariant the function must keep: it reads and writes only its own
`CellStaticData`, with no shared or static state, because the frame-tick
caller runs in parallel across cells. Navigation data is outside the CRC and
the save, so no byte changes.

## Critical files

- `Engine/Source/Frame/CellStaticData.h`
- `Engine/Source/Frame/CellStaticData.cpp`
- `Engine/Source/Frame/FrameBase.cpp`
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp`

## In scope

- The new `CellStaticData` member: declaration and definition, and the
  comment on `CellStaticData::flags`.
- The navigation build block in the frame tick (`FrameBase.cpp`), in
  `ServerSessionRuntime::PreparePausedSubscriptions`, and in
  `FlagshipReachesEdge`.

## Out of scope

- `BuildCellNavigationData` itself and everything it builds.
- The elevation grid build in the frame tick; the Fighter launch admission
  question is
  `Documents/Plans/Game/FighterLaunchTerrainAdmissionNeedsElevationGrid.md`.
- `Engine/Source/Agent/Commands/CellCoordinateProbe.cpp`, which builds into a
  local copy and sets no flag, and the client's `CellStaticData::Read`.
- When or on which thread any site builds.
- `Projects/Coretide`: a session that can read the primary checkout reports
  a copy there as a residual instead of editing it.

## Risk tier

Tier 3 — invariant/integration. Trigger: "a change spanning independently
owned subsystems" (`.agents/references/risk-tiers.md`): it adds an engine
Frame function and changes callers in engine Frame, engine Network Server,
and the game's Network Server, and one caller runs on the per-cell dispatch
threads. No determinism/CRC, wire, serialization, or save/replay byte
changes.

## Acceptance criteria

- Code read: `BuildCellNavigationData` is called with a cell's own
  `navigationData` from one place, the new function, and
  `kNavigationDataBuilt` is set on the server from that place only.
- Code read: the new function uses no static or shared state.
- `/compile` builds the BrokenEngineSandbox server and client.
- The repository static checks pass.
- Harness (`/agent-harness`): a client connects to a fresh server, receives
  the origin's static data, and a fleet flagship picks a new cell (the three
  sites: first tick, subscription service, fleet step). The supplied evidence
  records no harness setup beyond these steps.
