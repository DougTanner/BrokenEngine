# Agent - Game Command Dispatch

## Overview

Game-owned command handlers expose game simulation, scene, and diagnostic
operations through the engine agent transport
(`../../../../Engine/Source/Agent/AGENTS.md`). Engine-owned shared commands
dispatch first; the game dispatcher then tries its own side-specific handlers
under `BT_CLIENT` or `BT_SERVER`, and only then the engine's side-generic
handler, so a game handler can take over an engine command
(`../../../../Engine/Source/Agent/AGENTS.md` `## Architecture`). This project
takes over `status`
(`AgentCommandsServer.cpp`), `replay_transfer_capture` (the server
simulation-harness-rig module), and `describe_scene` (`AgentScene.cpp`). The
focused server simulation-harness-rig module owns the parsing, result assembly,
and orchestration for `inject_payload`, `set_fleet_flight_plane`, and the
`replay_transfer_capture` counts; the focused server frame-edit module owns
`edit_frame` and `read_frame`.

## Contracts

- JSON parameters fall under the engine's agent-channel exemption (`../../../../Engine/Source/Agent/AGENTS.md` `## Architecture`).
- The engine owns client capture, window state, UI/input automation, the client
  `query_profile` query, the desync probe, the Debug audio streaming command,
  the engine part of `describe_scene`, and moving the client's own grid cell,
  which sets and clears the viewer pin the engine client session runtime owns
  (`../../../../Engine/Source/Agent/AGENTS.md`;
  `../../../../Engine/Source/Network/Client/AGENTS.md`). The game client handler
  owns the `describe_scene` take-over's parameters and its unit, fleet, and count
  fields (`../../Documents/AgentHarness/commands-client.md`).
- The engine owns simulation control, save/load/reset, the server CPU profile
  query, `create_cell`, and the replay commands. Game server handlers own
  status-change injection, frame queries, frame edits and reads, and the
  `status` and `replay_transfer_capture` take-overs. Their focused
  simulation-harness-rig module owns one state slot bound to the live
  `ServerSession`, including the coordinate-keyed status-change queue that
  `inject_payload` destroy and update entries fill; replacing or destroying the
  bound session clears that state. `inject_payload` spawn entries go to the
  engine `ServerTransferManager` spawn queue instead
  (`../../Documents/AgentHarness/commands-server.md`).
- The game Agent owns replay transfer type classification, capture-count storage shape, and the count fields its `replay_transfer_capture` take-over writes. The [engine replay harness rig](../../../../Engine/Source/Agent/AGENTS.md#architecture) owns how those counts are observed and when the classifier is invoked.
  `inject_payload` field keys are the `Fields()` member tokens of each payload (each collection's `SpawnInfo` and the update and destroy payloads in `Frame/StatusChange.h`); a kind's accepted fields change only there.
- `edit_frame` and `read_frame` keys are the `FRAME_COLUMN` tokens of each
  collection's column list in `Commands/ServerFrameEdit.cpp` (Explosions and
  Pushers: the engine `Engine/Source/Agent/Commands/FrameEdit.h`) and of each
  frame struct's `Values()`; `read_frame` reads every one of them, identity and clock
  values included. A server-visible column added to or removed from a collection
  is added to or removed from that collection's list; the list's `static_assert`
  against its pair's `Members()` fails until it is. A server-visible collection
  pair added to any frame tuple fails the server build
  until a `FrameCollection` specialization names it and its column list, the
  only per-collection code; add its name to the `edit_frame` collection names of
  [`commands-server.md`](../../Documents/AgentHarness/commands-server.md). A new
  `Values()` entry becomes writable unless `kRefusedFrameValues` names it; the
  `frame` key list of `commands-server.md` changes with it.
- `edit_frame` writes a cell's current CRC'd frame state directly, outside the
  tick input, so it is refused while recording, during playback, or with a
  pending replay transition, because no replay records the edit. The command
  adds and removes no row itself; the shared compaction ending the next tick's
  Destroy phase removes a row whose kill state it wrote, with the rows it owns,
  so `edit_frame` never writes a handle or runs a removal. `edit_frame` and
  `inject_payload` refuse an edit that would let a fleet Battleship die or
  leave its fleet, because one never does (`../Frame/Collections/Battleships/AGENTS.md`).
  Connected clients resynchronize through the Save entry point
  (`../Save/AGENTS.md`). Command contract:
  `../../Documents/AgentHarness/commands-server.md`.
- `set_fleet_flight_plane` writes fleet state, not tick input, so it is refused
  during playback, because no recording carries the write. Command contract:
  `../../Documents/AgentHarness/commands-server.md`.
- The engine `create_cell` and the game `edit_frame` are the only writers of the engine pinned frame value `uiPinned` (what it keeps loaded: the active-set bullet of `../../../../Engine/Source/Network/Server/AGENTS.md` `## Session Invariants`).
- Commands that wait for a later frame, for the queue where messages wait for the renderer to pick them up, or for an input script use the engine deferred-response path. Ensure every deferred operation can resolve or fail without blocking the single in-flight channel.
- Scene queries are client-only and read render-visible state. Server frame queries read deterministic simulation state and must preserve collection/type validation.
- The frame registry keeps no rows, so nothing about it is enumerable or countable through a query. Its state reaches the harness only as the handle columns the collections themselves own.

## See Also

- `../../../../Engine/Source/Agent/AGENTS.md`
- Game Save: `../Save/AGENTS.md`
- `../../../../Tools/AgentHarness/AGENTS.md`
