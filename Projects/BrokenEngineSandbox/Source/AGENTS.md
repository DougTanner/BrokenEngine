# BrokenEngineSandbox Source - Game Layer

## Overview

BrokenEngineSandbox is a tech demo that shows off and stress-tests engine features — not a game: there is no human player and no win condition. AI-driven spaceship fleets (a flagship plus its wingmen) fight continuously spawning enemies over the island ocean, so a client's owned entities are ships the AI drives, never ones a person steers. All game code lives in the `game` namespace.

Game implementation built on Engine and Common. `Game` owns the per-side sessions, each cell's frame contents, shared fleet data, client selection/settings, and the game half of server persistence. The awake-cell list itself and on-demand cell creation are engine-owned (`../../../Engine/Source/AGENTS.md`).

## Ownership

- `Fleet` is the shared data model and uses persistent identifiers assigned by the server. `FleetSelection` is client-only focus and navigation UI state; it also owns the HUD fleet panel's network-pending controls, so clearing it on game reset or server load resets pending marks the server can no longer resolve.
- `ClientSession` and `ServerSession` are game-policy wrappers that compose engine-owned session runtimes. See `Network/AGENTS.md`.
- Server save, load, reset, and autosave, like replay recording and playback, are engine-owned (`../../../Engine/Source/File/AGENTS.md`); the game half of those contracts and of the grid save lives in `Save/AGENTS.md`.
- The server monitoring window is engine-owned (`../../../Engine/Source/Server/AGENTS.md`); the game reaches it only through the per-cell population snapshot in `Frame/AGENTS.md` and the profile counters it publishes for its own reasons.
- Display input is engine-owned (`../../../Engine/Source/Input/AGENTS.md`). The game's only display-input entry point is `Game::ProcessGameMenuInput`, which `GameBase::ProcessInput` calls last, after engine menu policy: it polls its own keys through the borrowed `engine::InputPoll` and owns the game network transport for the generic save/replay/time/pause flags. Everything it does is `kbDebugInput`-only today.
- Agent commands are game-dispatched, but transport, synthetic input, UI snapshots, the build-agnostic shared handlers, and the client-generic handlers are engine-owned; the dispatcher tries the engine shared handlers before its side-specific ones, and under `BT_CLIENT` tries the engine client-generic handler before its own client handlers. `Game` teardown clears any deferred agent response first, while its captured runtime dependencies are still live. See `Agent/AGENTS.md`.

## Architecture

- Client subscriptions cover the current cell plus visible neighbors within the authorized 3x3 ring; the engine client session runtime computes them (`../../../Engine/Source/Network/Client/AGENTS.md`). The client's cell is stored on `GameBase`, but game policy chooses it; its write path is engine-owned (`../../../Engine/Source/Network/Client/AGENTS.md` `## Other`).
- Cross-cell transfers are serialized `StatusChange`s carrying spawn state; the machinery that moves them between cells is engine-owned (`../../../Engine/Source/Network/Server/AGENTS.md`). The client removes them from `FrameInput` before normal Spawn processing; server replay records them in the difference stream's post-dispatch channel, never inside `FrameInput`.
- Omni/enemy alignments are copied into each new `FramePostRender`; frame code reads the snapshot, not `gpGame`.
- The game-owned persisted client setting `ClientState.bin` is versioned POD
  written through the engine versioned-file helpers. `ClientState.bin` loads once at startup, the focused
  fleet and ship into an in-memory mirror that focus changes refresh and the
  zoom straight into the camera; orderly client shutdown writes that mirror plus
  the camera's zoom target once, while abnormal exits do not guarantee
  current-session changes are saved and may leave the previous orderly-exit
  snapshot on disk. Bump the owning version on layout change. Tweaks, game,
  sound, and graphics settings persistence is engine-owned
  (`../../../Engine/Source/Ui/AGENTS.md`).
- Save/replay compatibility is gated by `Frame::kiVersion`; `Version.h::kiGameVersion` is informational.
- `Pch.h` defines only the game's own compile-time switches and includes the engine's (`../../../Engine/Source/AGENTS.md` `## Hub Conventions`).
- The game PCH force-includes shared aggregation/layout headers. Consumer TUs do not repeat those includes; DataPacker has a separate PCH.

## Subsystems

- `Agent/AGENTS.md` - Harness command dispatch, queries, and network diagnostic probes
- `Frame/AGENTS.md` - Game simulation and collections
- `Graphics/AGENTS.md` - Camera controller
- `Network/AGENTS.md` - Game protocol and sessions
- `Profile/AGENTS.md` - Game profiling counters
- `Save/AGENTS.md` - The game half of the server save/load and replay contracts
- `Ui/AGENTS.md` - HUD and settings

## See Also

- `../../../Engine/Source/AGENTS.md`
- `../../../Documents/FloatingPointDeterminism.txt`
