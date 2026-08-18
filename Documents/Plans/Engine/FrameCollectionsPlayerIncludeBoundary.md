<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-18T21:12:30.208Z","dependsOn":[]} -->
# Remove the FrameCollections Player include boundary

## Context

The approved D15 boundary is narrow: Engine-owned source and interfaces must
not directly include Player-specific headers, name or dereference Player
symbols or collections, name Player constants, or require Player semantics.
Generic game-provided Frame, Game, session, message, HUD, and profile headers
may retain Player internals when Engine treats them opaquely.

The remaining Frame compile edge is evidenced by
`Engine/Source/Frame/FrameBase.cpp:4`, which includes the game-owned
`Frame/FrameCollections.h`, and
`Projects/BrokenEngineSandbox/Source/Frame/FrameCollections.h:4`, which
includes `Frame/Collections/Players/Players.h`. The two tuple builders at
`FrameCollections.h:13-20` only dereference the Blasters, Missiles,
Spaceships, and Targets members; the Player include is not used by either
tuple. `FrameBase.cpp:166-175,202-203,232-245` consumes the resulting type
lists for registration and phase dispatch, so the include edge must be
removed or replaced without changing that contract.

## Design

Before editing, choose exactly one bounded seam:

1. Prove that the tuple and per-build Frame contract need no complete Player
   type and remove the unnecessary `Players.h` include.
2. If a complete type is required by the existing Frame contract, introduce a
   neutral collection declaration seam that keeps the current client/server
   tuple and phase contract while avoiding a direct Player-specific include.

The selected seam must preserve the current tuple order, type-list aliases,
registration, phase participation, and compile behavior. Do not add a generic
collection registry, callback, Player wrapper, or Player surrogate.

## Critical files

- `Engine/Source/Frame/FrameBase.cpp:3-7,166-175,202-203,232-245` — the
  Engine consumer and registration/phase paths.
- `Projects/BrokenEngineSandbox/Source/Frame/FrameCollections.h:3-24` — the
  transitive include and tuple/type-list declarations.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` — only if the selected
  neutral declaration seam requires a declaration change.
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/Players.h`
  — evidence of the current dependency; do not move Player gameplay here.

## In scope

- The `FrameCollections.h` transitive `Players.h` compile edge and the
  smallest selected neutral declaration or include change.
- The exact `FrameBase.cpp` include and type-list/phase callers required to
  prove the selected seam.
- Client and server PCH/include-order evidence for the generic Frame contract.
- Preservation of tuple order, collection identity, phase ordering, CRC/wire
  layout, save/replay version behavior, and allocation behavior.

## Out of scope

- The pusher anchor, navigation clearance/threshold, and profile counter
  seams; those belong to `PusherAnchorContract.md`,
  `NavigationClearanceContract.md`, and `ProfileCounterAnchorContract.md`.
- The Targets collection or target acquisition. The approved
  `Documents/Features/Frame/GenericSpatialQueries.md` feature owns that D3
  deletion and its Frame/version contract.
- Moving Players internals, changing gameplay, adding compatibility aliases,
  or introducing callbacks, registries, abstractions, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3: the seam crosses Engine/game compile
ownership and feeds shared Frame type lists, registration, phase dispatch,
CRC, wire, save, and replay contracts. After the change no covered
Engine-owned path may require a direct Player-specific include. The tuple
order, type aliases, phase participation, client/server affinity, and all
serialized/version behavior remain unchanged.

## Coordination

This Plan is independent of the other three D15 seam Plans; none may absorb
its include boundary or alter its tuple contract. The approved
`GenericSpatialQueries.md` feature is the authority for Targets removal, not
an executable prerequisite and not a scope for this Plan.

## Acceptance criteria

- The selected seam is recorded before source edits, and a scoped include/type
  trace proves that Frame collection compilation no longer requires the
  Player-specific header through this path.
- Client and server compile with the documented PCH/include order, including
  a generic second-game Frame contract that omits Player internals.
- Registration, tuple/type-list order, phase calls, CRC/wire layout, and
  save/replay behavior remain unchanged.
- No Player wrapper, callback, registry, surrogate, or unrelated collection
  change is introduced.

## Notes

This Plan records only the FrameCollections compile seam from the approved D15
boundary. The other three seams are independently executable Plans.
