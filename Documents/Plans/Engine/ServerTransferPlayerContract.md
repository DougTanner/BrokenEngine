<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-20T21:14:34.737Z","dependsOn":["Documents/Plans/Engine/FrameRegistry.md"]} -->
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

Authority conflict, settled. `Engine/Source/Network/Server/AGENTS.md` currently
records a deliberate exception for these two classes: "Both classes name
`game::StatusChange` and read game Frame state — a deliberate exception to the
engine/game contract, because the payload crossing a cell boundary is
game-defined while the machinery moving it is not." The approved D15 boundary
outranks that documentation under the repository authority order, and the user
resolved the conflict in D15's favour on 2026-08-21, so the Player-collection
reads are removed and the exception text is updated to match.

## Design

Remove the Player-collection dependency from the two Engine translation units.
The engine-facing seam is the `FrameRegistry` ownership layer and its scalar
wrappers, added by the dependency `Documents/Plans/Engine/FrameRegistry.md` — not
bespoke game free functions and not a new session-contract method. The three
needs this Plan covers through that view are the destination-liveness count, the
global-id to frame-local-uuid lookup, and the arrival client-GUID bind via
`AssignRegistryClientGuid`. Both files drop the Player-specific include, and
`Engine/Source/Network/Server/AGENTS.md` is updated so its recorded exception
covers only the remaining `game::StatusChange` payload dependency.

(Keeping the documented exception as-is and merely narrowing it in documentation
was the alternative considered; the user rejected it on 2026-08-21.)

The current observable behavior is preserved exactly: the liveness rule (a
destination is live when it has a committed player or an active subscriber), the
`kTransferPlayer` exemption from the liveness gate, deterministic transfer type
ordering, the post-transfer `sharedCrc` recomputation on the destination Frame,
the emitted log field sets and text (byte-identical), allocation-suppression
behavior, and main-thread affinity. No version bump happens here beyond the one
the dependency Plan already makes. If the work would require changing any of
those, return it for re-planning instead of expanding scope.

## Critical files

- `Engine/Source/Network/Server/ServerTransferManager.cpp:9,33,206,221,234-286`
  — the Player include, liveness test, logs, and destination player scan.
- `Engine/Source/Network/Server/ServerBroadcaster.cpp:7,273-281` — the Player
  include and the global-id to player-uuid scan.
- `Engine/Source/Network/Server/AGENTS.md` — the recorded deliberate exception,
  updated to cover only the `game::StatusChange` payload dependency.
- `Engine/Source/Frame/FrameRegistry.h` — the ownership layer and scalar
  wrappers this Plan consumes, supplied by the dependency Plan.
- `Projects/BrokenEngineSandbox/Source/Frame/Frame.h` — `Frame::OwnershipLayer`,
  the game-side builder of that layer, supplied by the dependency Plan.

## In scope

- The exact regions named in `## Critical files`: replacing the
  Player-collection reads with the `FrameRegistry` ownership layer and its
  scalar wrappers, and removing the Player-specific include from those two
  Engine translation units.
- Updating the `Engine/Source/Network/Server/AGENTS.md` exception text to match.

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
- Compatibility wrappers, callback abstractions, any registry surface beyond the
  ownership layer and scalar wrappers the dependency Plan supplies, and unit
  tests.

## Risk tier and invariants

Change Workflow Tier 3: these paths sit on the cross-cell transfer and
per-cell publication route that feeds the wire payload and the deterministic
`sharedCrc` the CRC check covers, and they span independently owned Engine and
game subsystems. Preserve PostRender bit determinism, transfer type ordering,
the post-transfer destination CRC recomputation, adjacency and liveness rules,
publication order, log content, allocation-tracking behavior, and main-thread
affinity. No new wire type, protocol-version bump, or `Frame::kiVersion` change
is authorized here.

## Coordination

`Documents/Plans/Engine/FrameRegistry.md` is the directional prerequisite and is
carried in this Plan's metadata: it adds the ownership layer, the scalar
wrappers, and `Frame::OwnershipLayer` that this Plan consumes, and it moves the
surrounding code, so re-derive every line citation from current source at claim
time. If
`Documents/Plans/Engine/ChangeListTransportContract.md` is implemented first,
it may move or renumber the regions cited above; re-derive the citations from
current source before editing rather than trusting the line numbers. That Plan
still cites these two files at their former
`Projects/BrokenEngineSandbox/Source/Network/Server/` paths, which no longer
exist.

## Acceptance criteria

- A scoped search finds no `Frame/Collections/Players/Players.h` include and no
  `PlayersPostRender` member access in either file, while game-owned player
  handling stays intact.
- `Engine/Source/Network/Server/AGENTS.md` no longer claims a Player-collection
  dependency for these two classes.
- Client and server both compile.
- A harness run with a cross-cell player transfer and an update-player request
  shows the same liveness decisions, destination CRC results, publication
  output, and no new transfer, CRC, or desync errors.

## Notes

This Plan records only the server transfer/broadcast Player-collection seam. It
was raised as an out-of-scope residual during the
`NavigationClearanceContract.md` session, which proved these two includes
uncovered by every tracked D15 sibling Plan.
