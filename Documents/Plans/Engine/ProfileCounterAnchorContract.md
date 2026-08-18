<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T21:12:43.245Z","dependsOn":[]} -->
# Replace the Player-named profile counter anchor

## Context

The approved D15 boundary requires Engine-owned source and interfaces to avoid
Player-specific headers, Player symbols or collections, Player constants, and
Player semantics. Generic game-provided Frame, Game, session, message, HUD,
and profile headers may retain Player internals when Engine treats them
opaquely.

The profile counter anchor leak is evidenced by
`Engine/Source/Profile/ProfileManagerBase.cpp:6`, which includes the game
profile header, and `ProfileManagerBase.cpp:13-15`, where the engine pins the
first game CPU timer and the first game CPU counter with static assertions; the
counter assertion names `game::kCpuCounterPlayers`. The game enum and its
ordered names are declared at
`Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h:6-17,49-61`,
and `ProfileManager.cpp:34-35` passes those arrays and counts into the base.
The current contiguous index contract drives the base accessors and profile
row numbering, so removing the Player-named anchor must not renumber or hide
the existing counters.

## Design

Before editing, choose one bounded generic counter contract:

1. Replace the Player-named assertion with a generic first-counter/index
   contract supplied by the game profile setup, retaining the existing
   contiguous engine-to-game index space.
2. Use a count-only profile contract that proves the game counter block begins
   at `kEngineCpuCounterCount` without naming a first game counter, while
   preserving the existing timer anchor, row numbering, names, and accessors.

The selected contract must keep the timer access path and all current profile
labels/indices. Do not hide a Player requirement behind a callback, move game
profile rows into Engine, or introduce a second counter registry.

## Critical files

- `Engine/Source/Profile/ProfileManagerBase.cpp:1-18` — the include and timer/
  counter anchor assertions.
- `Engine/Source/Profile/ProfileManagerBase.h:334-439` — constructor, counts,
  arrays, and generic counter accessors.
- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h:6-17,49-61,89-111`
  — game enum, ordered names, and storage; update only the selected anchor
  seam if required.
- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp:34-35` —
  game-to-engine profile construction.
- `Engine/Source/Profile/ProfileScreens.cpp:63-82,287-300` — counter/timer
  row consumers used to prove numbering and access remain unchanged.

## In scope

- Replace the `kCpuCounterPlayers`-named engine anchor with the selected
  generic first-counter or count-only contract.
- Remove the Player-specific semantic requirement from the covered Engine
  profile contract while preserving the generic timer anchor and contiguous
  counter indexing.
- Update only the direct game profile construction/declaration sites required
  by the selected contract.
- Preserve profile row order, labels, timer/counter access, client/server
  compilation, allocation behavior, and existing profile output.

## Out of scope

- FrameCollections include ownership, pusher anchoring, or navigation
  clearance/threshold inputs; those belong to the other three D15 Plans.
- Network profile/NetworkGraphs ownership, Frames profile extraction,
  ServerDisplay, profile content, counter names, or gameplay metrics.
- Removing Player-named declarations from game-owned profile headers when
  Engine consumes them only through the selected generic contract.
- Targets or target acquisition. The approved
  `Documents/Features/Frame/GenericSpatialQueries.md` feature owns that D3
  deletion.
- Compatibility aliases, callback/registry abstractions, speculative profile
  systems, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3: the seam crosses Engine/game profile
construction and shared client/server index contracts. Preserve the contiguous
engine-to-game counter and timer indices, row labels/order, accessors, output,
threading/locking, allocation behavior, and client/server affinity. After the
change no covered Engine profile anchor may name Player semantics.

## Coordination

This Plan is independent of the other three D15 seam Plans and owns only the
profile counter anchor. The approved `GenericSpatialQueries.md` feature is the
authority for Targets removal and is not a prerequisite or scope here.

## Acceptance criteria

- The selected generic counter contract is recorded before source edits and
  no covered Engine assertion or interface names `kCpuCounterPlayers` or
  another Player-specific anchor.
- Client and server compile, and profile rows retain their current names,
  order, counts, timer access, and displayed values.
- A second game can construct the generic profile base without a Player-named
  counter while preserving contiguous index validation and no new allocation.
- No callback registry, duplicate counter store, gameplay/profile-content move,
  compatibility alias, or unit test is introduced.

## Notes

This Plan records only the profile counter anchor from the approved D15
boundary. The other three seams are independently executable Plans.
