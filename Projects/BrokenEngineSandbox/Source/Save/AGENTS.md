# Save - Server Persistence

## Overview

`GameSaveLoad` orchestrates server save, load, reset, and autosave, and this subsystem supplies the game half of the engine-owned grid save and replay contracts. Files are versioned by `game::Frame::kiVersion`; navigation and elevation are derived and are not persisted.

## Save and Load

- Grid file framing, validation, staging, and adoption are engine-owned (`../../../../Engine/Source/File/AGENTS.md`); this subsystem owns only when a save, load, reset, or autosave runs and what happens around it. Return the actual commit result to callers.
- Successful loads preserve the saved simulation clock and resynchronize clients to that state. Fresh-game paths reset server fleet management explicitly.
- Server load and reset cancel pending raw profile samples and unpublished event arms before mutating or restoring frame state; this diagnostic cleanup does not alter the persisted payload.

## Replay Contract

Recording, playback, the manifest, and every stream lifetime rule are engine-owned (`../../../../Engine/Source/File/AGENTS.md`), the same deferral the grid file uses. This subsystem owns the game half of that contract.

- Replay metadata is game-owned: written after the writer files complete, and read into an isolated staged value the engine carries without inspecting and applies only once the whole replay generation validates.
- When the engine invalidates its replay streams, the game clears its replay-only transfer fixture and leaves the live game running.
- After a successful save load, a fresh reset, or replay adoption, one game entry point relinks and resynchronizes connected clients. It is the sole caller of that relink, so all three paths share identical ordering.
- Counting captured transfers by type is game policy, because only game code knows the `StatusChange` payload types. The engine carries the resulting per-type counts as an opaque member of its capture record.

## Affinity

The subsystem is whole-file `BT_SERVER`.

## See Also

- `../../../../Engine/Source/File/AGENTS.md`
- Frame serialization: `../Frame/AGENTS.md`
