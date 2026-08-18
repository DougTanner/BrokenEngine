<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T18:52:42.454Z","dependsOn":[]} -->
# Move world-grid state and the Frames profile surface to the engine

## Context

The D10 investigation found world-grid state split between game policy and
engine consumers. `Projects/BrokenEngineSandbox/Source/Game.h:144-154` stores
`mClientGridCoord`, `mVisibleNeighbors`, `miVisibleNeighborCount`, and
`mActiveCoords`; `Game.cpp:168-266` computes the visible ring. Client
subscription servicing consumes that ring at
`Network/Client/ClientSessionSubscriptions.cpp:18-40`. Engine dispatch/render
paths consume the active list and focus at `Engine/Source/GameBase.cpp:182-205,
287-296,694-703`, while `Engine/Source/Network/Server/ServerSessionRuntime.cpp:
268-326` clears, appends, and synchronizes it. The Frames profile reads the
same state at `Profile/ProfileManager.cpp:78-160`.

The completed `ActiveSetSkeletonToEngine` landing (`f4356dc`) is prerequisite
history, not a scheduler dependency. Preserve its direct
`GameBase::PrepareActiveSet` policy boundary.

## Pre-implementation decisions and options

The ownership recommendation remains unapproved; record the selected option
before implementation. Fixed policy remains game-owned in every option:
`mVisibleNeighbors`, `miVisibleNeighborCount`, `SetClientGridCoord`
invalidation, `UpdateDesiredCoords`, camera visibility, and game-required
coordinates.

- **Option A (recommended):** store `mActiveCoords` and `mClientGridCoord` in
  `engine::GameBase`; retain game population and cache policy.
- **Option B:** retain game storage and expose only a narrow engine-facing
  access path; prove that no game policy or duplicate storage leaks into the
  engine.
- For Frames, either move formatting into the engine or retain a thin game
  adapter over engine-owned state. Record the choice without adding a virtual
  `ComputeActiveSet` or hook registry.

## Design

Implement the selected boundary without changing membership or population
policy. Preserve the direct required game calls from `PrepareActiveSet` and
the existing `ServerSessionRuntime::ComputeActiveSet` path. The order of
`mActiveCoords` is observable: subscribed cells, game-required cells, then the
origin are appended in that order and the same order reaches
`BuildAndDispatchFrameTicks`, `ServerBroadcaster`, `ServerTransferManager`,
replay refresh, rendering, and the Frames profile. Do not sort or replace this
with unordered iteration.

Frames output stays unchanged: active count/focus header, `P`/`#`/`O` map, and
focused tick/time when a render frame exists. Preserve workbuffer use and
allocation-free formatting.

## Critical files

- `Engine/Source/GameBase.h/.cpp:182-205,287-296,694-755` — storage/access,
  dispatch, render, replay refresh, and active-set boundary.
- `Projects/BrokenEngineSandbox/Source/Game.h:67,144-155` and `Game.cpp:168-287`
  — focus/cache policy and active population.
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:268-326` — ordered
  server construction.
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionSubscriptions.cpp:18-40`
  — visible-ring consumer.
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerBroadcaster.cpp:23-26,151-169`
  and `ServerTransferManager.cpp:50-55` — ordered consumers.
- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h:116-118` and
  `ProfileManager.cpp:78-160` — Frames profile surface.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp` and focus callers
  — save/replay focus transitions.

## In scope

- Select and record one storage/access option and one Frames boundary before
  implementation.
- Hoist or adapt only `mActiveCoords` and `mClientGridCoord` at that boundary.
- Preserve direct game policy calls, visible-ring invalidation, active-set
  membership, save/replay transitions, and all current downstream consumers.
- Preserve deterministic `mActiveCoords` order through construction, dispatch,
  broadcast, transfer, replay, render, and Frames display.
- Move or adapt the Frames formatters without output or allocation changes.

## Out of scope

- Network profile, `NetworkGraphs`, client transport, prediction, desync, clock,
  or reconciliation ownership.
- ServerDisplay, `ServerCellStats`, window-message lifetime, or memory-hash work.
- Renaming `mClientGridCoord`; it remains a server save/replay focus-coordinate
  residual.
- Simulation, frame CRC, replay/wire/save formats, compatibility aliases,
  speculative abstractions, or unit tests.

## Risk tier and invariants

Tier 3 — engine/game ownership integration across client render, server
dispatch, subscriptions, transfer, replay, profile display, and deterministic
ordering.

- Exactly one selected boundary owns active coordinates and focus; no divergent
  duplicate storage.
- Visible-neighbor state and invalidation remain game-owned.
- `PrepareActiveSet` keeps direct required policy calls; no pure-virtual policy.
- `mActiveCoords` ordering and Frames output remain unchanged.
- No simulation, CRC, replay, wire, or save-state layout changes occur.

## Acceptance criteria

1. The implementation records the selected storage and Frames options before
   editing and matches them.
2. Static ownership and caller traces show one active-list/focus path, no stale
   bypass, no policy virtual, and unchanged visible-ring/cache policy.
3. A deterministic trace or scenario proves the current `mActiveCoords` order
   from construction through dispatch, broadcast, transfer, replay, render,
   and Frames consumption.
4. Frames still emits the count/focus header, `P`/`#`/`O` map, and focused
   tick/time without per-frame heap allocation.
5. Client Debug/Release and server Debug builds and the smallest ordering,
   profile, and replay observations pass; no unit tests are added.
6. The `mClientGridCoord` server misnomer/save-replay residual is retained and
   explicitly reported.

## Execution card

- **Tier/triggers:** Tier 3; ownership integration, deterministic ordering,
  client/server consumers, replay, and profile output.
- **Roles:** implementer; plan/correctness/scope/adversarial reviewers;
  builder/harness; cleanup and landing verification roles.
- **Coordination:** consume the landed `ActiveSetSkeletonToEngine` result by
  history; do not recreate it or add a metadata edge to its deleted Plan.
