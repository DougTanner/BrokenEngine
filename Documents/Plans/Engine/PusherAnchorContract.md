<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T21:12:34.978Z","dependsOn":[]} -->
# Replace the pusher Player anchor

## Context

The approved D15 boundary requires Engine-owned source and interfaces to avoid
Player-specific headers, Player symbols or collections, Player constants, and
Player semantics. Generic game-provided Frame, Game, session, message, HUD,
and profile headers may retain Player internals when Engine treats them
opaquely.

The pusher anchor leak is evidenced by
`Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:3`, which includes
`Frame/Collections/Players/Players.h`, and
`PushersUpdate.cpp:45-55`, where `SetupZones` centers the 400 m spatial arena
on `rFrame.interpolate.pPlayers->pVecPositions[0]` and falls back to zero when
the collection is empty. `Engine/Source/Frame/FrameBase.cpp:234-235` calls
`SetupZones` after collection updates; the resulting thread-local zones are
used by `ApplyPush` in the Player and Spaceship navigation paths. The current
zone policy and empty-anchor fallback are therefore observable and must not
drift while the Player dependency is removed.

## Design

Before editing, choose one bounded game-neutral anchor contract:

1. Expose a required generic anchor value or function on the Frame contract,
   with the game supplying the current anchor and the existing zero fallback.
2. Change the existing `SetupZones` call to accept a required world-anchor
   value supplied by the game at that call site, preserving the same fallback
   and zone policy without storing Player identity in Engine.

The selected contract must not invent a Player surrogate or hide a Player
requirement behind a callback registry. Preserve the existing arena size,
zone size, zone limits, thread-local ownership, phase timing, and out-of-arena
behavior.

## Critical files

- `Engine/Source/Frame/Collections/Pushers/PushersUpdate.cpp:1-3,45-115` —
  the Player include, anchor, zone build, and query state.
- `Engine/Source/Frame/Collections/Pushers/Pushers.h:55-62` — the
  `SetupZones`/`ApplyPush` contract.
- `Engine/Source/Frame/FrameBase.cpp:232-235` — the existing post-update call.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp:507`
  and `SpaceshipsNavigation.cpp:127` — zone consumers used for behavior
  evidence; their Player/Spaceship gameplay remains game-owned.
- The generic game Frame contract only where the selected anchor requires a
  declaration or caller value.

## In scope

- Replace the `pPlayers[0]` pusher-zone anchor with the selected required
  game-neutral contract.
- Preserve the empty-anchor fallback, arena recentering, zone indexing and
  registration, thread-local lifetime, update/PreCollision ordering, and
  `ApplyPush` results.
- Remove the direct Player-specific include and names from the covered Engine
  pusher path, updating only the exact caller/contract sites required.
- Client and server compile and deterministic pusher-zone behavior evidence.

## Out of scope

- FrameCollections include ownership, NavBuild clearance/threshold inputs, or
  ProfileManagerBase counter anchoring; those belong to the other three D15
  Plans.
- Pusher constants, force falloff, collection layout, Player/Spaceship
  navigation policy, collision semantics, or gameplay changes.
- Targets or target acquisition. The approved
  `Documents/Features/Frame/GenericSpatialQueries.md` feature owns that D3
  deletion.
- Player compatibility wrappers, callback/registry abstractions, speculative
  generic entity systems, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3: pusher zones feed deterministic Player and
Spaceship navigation and are rebuilt in a shared Frame phase on per-thread
state. Preserve the 400 m arena, 8 m zones, 512-entry cap, zone membership
and query rules, zero/empty-anchor behavior, thread affinity, phase order,
floating-point results, and allocation behavior. No Engine-owned pusher path
may directly include or dereference Player symbols after the change.

## Coordination

This Plan is independent of the other three D15 seam Plans and owns only the
pusher anchor. The approved `GenericSpatialQueries.md` feature remains the
authority for Targets removal and is not a prerequisite or scope here.

## Acceptance criteria

- The selected anchor contract is recorded before source edits and a scoped
  search finds no covered Engine pusher include, Player collection dereference,
  or Player-specific semantic requirement.
- Client and server compile, and a deterministic zone/force trace shows the
  same arena origin, membership, empty fallback, and `ApplyPush` behavior.
- Setup timing, thread-local zone ownership, capacity/overflow behavior, and
  Player/Spaceship navigation outcomes remain unchanged.
- A second game can provide the anchor without a Player collection, with no
  surrogate, callback registry, or new allocation in the frame loop.

## Notes

This Plan records only the pusher anchor from the approved D15 boundary. The
other three seams are independently executable Plans.
