<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-21T19:04:43.011Z","dependsOn":[]} -->
# Rename ClientSession's desync member to match its ClientDesyncCore type

## Context

`game::ClientSession` owns the engine desync core through
`std::unique_ptr<engine::ClientDesyncCore> mpDesyncManager;`
(`Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h:78`). The
move of client desync policy into the engine renamed the type to
`engine::ClientDesyncCore`, but the member kept the obsolete `Manager` name, and
so did the local aliases that mirror it — for example
`engine::ClientDesyncCore& rDesyncManager = *gpClientSession->mpDesyncManager;`
at `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp:86` and
`:179`.

Two names now denote one concept, which the root `AGENTS.md` "one term per
concept" directive forbids. The mismatch is pre-existing: it is present at the
session baseline `8ea8526b335fc9a3aab3307e58b61802f6983420`, and the session that
recorded this residual moved the Network profile screen and graphs into the
engine without editing `ClientSession.h`, so the rename lies outside that
change's implementation boundary.

## Design

Mechanical rename only. Rename the member to `mpDesyncCore` and rename every
local reference variable that mirrors it (`rDesyncManager` -> `rDesyncCore`).
Change no types, no signatures, no ownership, no call order, and no behavior.

The authoritative site list at implementation time is whatever
`git grep -n "DesyncManager"` returns across `Engine/`, `Projects/`, and
`Documents/`; the files below are the sites present when this Plan was recorded.
The member is public and dereferenced from engine code, so the rename must be
applied in one pass across both layers or the build breaks.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.h` — the
  declaration at line 78, under the `// Managers` comment
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp` —
  construction and the per-frame dereferences (lines 36, 140, 155, 195, 196,
  241, 251)
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp` — the
  dereferences and the two `rDesyncManager` local aliases (lines 86, 89, 90,
  179, 193, 442, 462, 468)
- `Engine/Source/GameBase.cpp` — the stall gate at line 218
- `Engine/Source/Profile/ProfileNetworkScreen.cpp` — the desync readout (lines
  218, 222)

## In scope

- Rename the `ClientSession` member `mpDesyncManager` to `mpDesyncCore` at its
  declaration and at every dereference across the files above
- Rename local reference variables that mirror the old member name
  (`rDesyncManager`) to match
- Update any comment or AGENTS.md prose that names the member by its old name,
  if such a reference exists at implementation time

## Out of scope

- `engine::ClientDesyncCore` itself: its file names, type name, members, and
  behavior
- Any other `Manager`-suffixed name in the codebase, including `ClientSession`'s
  sibling members and the `// Managers` grouping comment
- Ownership, lifetime, header layering, or client/server affinity changes
- Any behavior change whatsoever

## Risk tier and invariants

Tier 1 (mechanical, behavior-preserving rename with no public signature or
invariant exposure; the member's type, layout, and access stay identical). No
determinism/CRC, serialization, `.pack`, `kiVersion`, replay, wire, threading,
allocation, or shader exposure: the member is a runtime-only `unique_ptr` that
never enters a serialized or CRC-covered structure.

## Acceptance criteria

- `git grep -n "DesyncManager"` returns no hits across the repository
- The client builds cleanly (`/compile` for the BrokenEngineSandbox client);
  the engine dereference sites are compile-checked by the rename itself

## Notes

The member is public and reached through `game::gpClientSession` from engine
translation units, so a partial rename fails to compile rather than silently
diverging.
