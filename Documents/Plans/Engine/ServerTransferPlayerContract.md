<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T21:14:34.737Z","dependsOn":[]} -->
# Resolve the server transfer/broadcast Player collection dependency

## Context

The approved D15 boundary requires Engine-owned source and interfaces to avoid
Player-specific headers, Player symbols or collections, Player constants, and
Player semantics. Generic game-provided Frame, Game, session, message, HUD,
and profile headers may retain Player internals when Engine treats them
opaquely.

Two Engine-owned server network translation units still take the Player-specific
header directly:

- `Engine/Source/Network/Server/ServerTransferManager.cpp:9` includes
  `Frame/Collections/Players/Players.h`. It reads
  `postRender.pPlayers->iCount` for the destination-liveness test at
  `ServerTransferManager.cpp:33`, binds
  `game::PlayersPostRender& rDestPlayers` at `:234`, scans
  `rDestPlayers.pGlobalPlayerIds` at `:240`, writes
  `rDestPlayers.pClientGuids` at `:268` and `:285`, and logs
  `pPlayers->iCount` at `:206` and `:221`.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:7` includes the same
  header. It binds `const game::PlayersPostRender& rPlayers` at `:273` and
  scans `rPlayers.pGlobalPlayerIds` / `rPlayers.puiIds` at `:277-279` to turn a
  global player id into a frame-local player uuid.

Neither file references `game::kfPlayerRadius` or `game::kfPushMargin`, so
`Documents/Plans/Engine/NavigationClearanceContract.md` — which owns exactly
those two constants in `Engine/Source/Frame/NavBuild.cpp` and
`Engine/Source/Main.cpp` — does not cover these includes, and neither does
`Documents/Plans/Engine/PusherAnchorContract.md`, which owns the
`PushersUpdate.cpp` include.

Authority conflict to resolve before editing. `Engine/Source/Network/Server/AGENTS.md`
currently records a deliberate exception for these two classes: "Both classes
name `game::StatusChange` and read game Frame state — a deliberate exception to
the engine/game contract, because the payload crossing a cell boundary is
game-defined while the machinery moving it is not." The approved D15 boundary
outranks that documentation under the repository authority order, but the
documented exception is the reason the current code is shaped this way, so this
Plan exists to settle the conflict rather than to assume one side.

## Design

Before editing, resolve the conflict above with the user and select exactly one
outcome:

1. Confirm and narrow the documented exception. The `game::StatusChange`
   payload dependency stays, but the Player-collection dependency is written
   down explicitly in `Engine/Source/Network/Server/AGENTS.md` as part of the
   same deliberate ownership, and no source changes. Selecting this completes
   the Plan with a documentation-only change.
2. Replace the Player-collection reads with a bounded generic contract: the
   game supplies the destination-liveness answer and the global-id to
   frame-local-uuid lookup through the existing `game::NetworkSessionContract`
   or `game::gpServerSession` seam, and the Engine files drop the
   Player-specific include.

Whichever outcome is selected, the current observable behavior is preserved
exactly: the liveness rule (a destination is live when it has a committed
player or an active subscriber), the `kTransferPlayer` exemption from the
liveness gate, deterministic transfer type ordering, the post-transfer
`sharedCrc` recomputation on the destination Frame, the emitted log field sets
and text, allocation-suppression behavior, and main-thread affinity. If the
selected outcome would require changing any of those, return the work for
re-planning instead of expanding scope.

## Critical files

- `Engine/Source/Network/Server/ServerTransferManager.cpp:9,33,206,221,234-286`
  — the Player include, liveness test, logs, and destination player scan.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:7,273-281` — the Player
  include and the global-id to player-uuid scan.
- `Engine/Source/Network/Server/AGENTS.md` — the recorded deliberate exception
  that either narrows (outcome 1) or is updated to match (outcome 2).
- `Projects/BrokenEngineSandbox/Source/Network/NetworkSessionContract.h` and
  `Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.h` — only
  if outcome 2 needs a declaration or a supplying call site.

## In scope

- Selecting one of the two outcomes above with the user, and recording it
  before any source edit.
- The exact regions named in `## Critical files`, and only if outcome 2 is
  selected: replacing the Player-collection reads and removing the
  Player-specific include from those two Engine translation units.
- Updating the `Engine/Source/Network/Server/AGENTS.md` exception text to match
  the selected outcome.

## Out of scope

- The change-list transport shape, codec envelope, transfer harvest
  restructuring, and publication assembly, all owned by
  `Documents/Plans/Engine/ChangeListTransportContract.md`, whose
  `## Out of scope` already excludes Player dependency removal.
- The four game-side transfer producers, owned by
  `Documents/Plans/Engine/TransferProducerShapeDecision.md`.
- NavBuild clearance/threshold inputs, the pusher anchor, and profile counter
  anchoring; those belong to the other D15 Plans.
- Player gameplay, fleet policy, owned-entity registry semantics, the wire
  format, and the protocol version.
- Compatibility wrappers, callback/registry abstractions, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3: these paths sit on the cross-cell transfer and
per-cell publication route that feeds the wire payload and the deterministic
`sharedCrc` the CRC check covers, and they span independently owned Engine and
game subsystems. Preserve PostRender bit determinism, transfer type ordering,
the post-transfer destination CRC recomputation, adjacency and liveness rules,
publication order, log content, allocation-tracking behavior, and main-thread
affinity. No new wire type, protocol-version bump, or `Frame::kiVersion` change
is authorized here.

## Coordination

No directional prerequisite is required. If
`Documents/Plans/Engine/ChangeListTransportContract.md` is implemented first,
it may move or renumber the regions cited above; re-derive the citations from
current source before editing rather than trusting the line numbers. That Plan
still cites these two files at their former
`Projects/BrokenEngineSandbox/Source/Network/Server/` paths, which no longer
exist.

## Acceptance criteria

- The selected outcome is recorded before any source edit.
- If outcome 1 is selected, no tracked C++ file changes and
  `Engine/Source/Network/Server/AGENTS.md` names the Player-collection
  dependency explicitly.
- If outcome 2 is selected, a scoped search finds no
  `Frame/Collections/Players/Players.h` include and no `PlayersPostRender`
  member access in either file, while game-owned player handling stays intact.
- Client and server both compile.
- A harness run with a cross-cell player transfer and an update-player request
  shows the same liveness decisions, destination CRC results, publication
  output, and no new transfer, CRC, or desync errors.

## Notes

This Plan records only the server transfer/broadcast Player-collection seam. It
was raised as an out-of-scope residual during the
`NavigationClearanceContract.md` session, which proved these two includes
uncovered by every tracked D15 sibling Plan.
