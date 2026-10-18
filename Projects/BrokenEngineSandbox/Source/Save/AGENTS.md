# Save - Server Persistence

## Overview

This subsystem supplies the game half of the engine-owned server save/load, grid save, and replay contracts (`../../../../Engine/Source/File/AGENTS.md` `## Grid Saves` and `## Replay Streams`). Files are versioned by `game::Frame::kiVersion`; navigation and elevation are derived and are not persisted.

## Save and Load

- `GameSaveState.h` declares the game half the engine grid save calls by name — the staged value and its four entry points; fleet serialization defines them (`../Network/Server/AGENTS.md`).

## Replay Contract

Recording, playback, the manifest, and every stream lifetime rule are engine-owned (`../../../../Engine/Source/File/AGENTS.md`), the same deferral the grid file uses. This subsystem owns the game half of that contract.

- Replay metadata is game-owned: written after the writer files complete, and read into an isolated staged value the engine carries without inspecting and applies only once the whole replay generation validates.
- After a successful save load, a fresh reset, replay adoption, a replay playback abort, or an agent frame edit with a connected client, one game entry point runs the engine session load reset, which rebuilds client ownership and resynchronizes connected clients. It is the sole caller of that load reset, so every one of these paths shares identical ordering.
- Replay capture diagnostics use the game Agent's transfer-count contract (`../Agent/AGENTS.md`) and the engine Agent's replay-timed observations (`../../../../Engine/Source/Agent/AGENTS.md`).

## Affinity

The subsystem is whole-file `BT_SERVER`.

## See Also

- `../../../../Engine/Source/File/AGENTS.md`
- Frame serialization: `../Frame/AGENTS.md`
