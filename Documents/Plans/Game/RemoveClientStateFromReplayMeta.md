<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:04:57.650Z","dependsOn":[]} -->
# Remove Always-Zero Client State From Replay Metadata

## Context

Found by the architecture review phase of a deep analysis of
`Projects/BrokenEngineSandbox/Source` (that directory only, 2026-11-14). Paths
are under `Projects/BrokenEngineSandbox/Source/` unless they start at the
repository root. Line numbers are from commit `022915b6`; locate by symbol if
they moved. The finding was confirmed by code reading; nothing was compiled or
run.

`ReplayMeta` (`Game.h:32-39`) is the fixed-size value at the head of the replay
metadata file. Besides `clientGridCoordinate` it carries
`iClientOwnedEntityIdentifierValue`, `fPreviousClientArmor`, and `puiPadding`.
Both fields are always written as zero, and the code that reads them back never
does anything:

- The only writer and reader of the file are whole-file `BT_SERVER`:
  `WriteReplayMetadata`, `ReadReplayMetadata`, and `AdoptReplayMetadata` in
  `Save/GameSaveLoad.cpp` (guard at line 3; lines 23-28 and 81), called from
  `Engine/Source/File/Replay.cpp` (guard at line 3). No client code reads
  `ReplayMeta`.
- `WriteReplayMetadata` fills `iClientOwnedEntityIdentifierValue` from
  `Game::ClientOwnedEntityIdentifier()` (`Save/GameSaveLoad.cpp:26`). That
  function's body is `BT_CLIENT`-only (`Game.cpp:52-63`), so on the server it
  returns `{}`.
- It fills `fPreviousClientArmor` from `Game::mfPreviousClientArmor`
  (`Save/GameSaveLoad.cpp:27`). That member becomes non-zero only in
  `Game::UpdateCameraShake` (`Game.cpp:120`) and
  `Network/Client/ClientReconciler.cpp:138`, both client-only.
- So in `Game::RestoreReplayMetadata` (`Game.cpp:319-328`) the
  `AddClientOwnedEntity` branch (lines 322-326) never runs and line 327 always
  stores zero.

The same dead path is the only reason the client owned-entity state compiles
into the server: `Game::mClientOwnedEntities`, `mfPreviousClientArmor`
(`Game.h:123-125`), and `ClientOwnedEntityIdentifier`,
`AddClientOwnedEntity`, `RemoveClientOwnedEntity`, `ClientOwnedEntity`
(`Game.h:87-89`, `:91`; `Game.cpp:50-95`) are unguarded. A search of both
source trees found every other use of these symbols in client-only code:
`FleetSelection.cpp`, `Graphics/Camera.cpp`, `Network/Client/`,
`Ui/Screens/HudScreen.cpp`, and `Ui/Screens/MapScreen.cpp`. Root `AGENTS.md`
`## Client/Server Targets` asks for client-only code to be guarded.

Unmet condition: no server translation unit compiles owned-entity state, and
`ReplayMeta` holds no field that is constant in its only writer.

## Design

The author's recommendation, in one stage:

1. `ReplayMeta` (`Game.h`): delete `iClientOwnedEntityIdentifierValue`,
   `fPreviousClientArmor`, and `puiPadding`; keep `clientGridCoordinate`. Raise
   `ReplayMeta::kiVersion` by one from its value at claim (7 when this Plan
   was written), so older replay metadata files are rejected by the version
   header. No backward compatibility.
2. `WriteReplayMetadata` (`Save/GameSaveLoad.cpp:23-28`): drop the two
   initializers for the deleted fields.
3. `Game::RestoreReplayMetadata` (`Game.cpp:319-328`): delete lines 322-327,
   leaving the `SetClientGridCoordinate` call.
4. Guard the owned-entity state under `BT_CLIENT`:
   - `Game.h`: the declarations of `ClientOwnedEntityIdentifier`,
     `AddClientOwnedEntity`, `RemoveClientOwnedEntity`, `ClientOwnedEntity`,
     and the members `mClientOwnedEntities`, `mfPreviousClientArmor`.
     `RestoreReplayMetadata` (`Game.h:90`) sits among them and stays outside
     the guard.
   - `Game.cpp`: one `BT_CLIENT` block around lines 50-95; the inner
     `#if`/`#endif` inside `ClientOwnedEntityIdentifier` (lines 52 and 63)
     goes.
   - `Game::ResetGameState` (`Game.cpp:174-185`): move the one `clear()` call
     (lines 176-177) and the `mfPreviousClientArmor` reset (line 184) into the
     existing `BT_CLIENT` block at line 178. The server body is then empty;
     the function keeps its declaration because
     `Engine/Source/GameBase.cpp:1196` calls it in both builds.

Exposure: the replay metadata file format changes (`ReplayMeta` layout, and
therefore the offset of the replay fleet records that follow it), covered by
the `ReplayMeta::kiVersion` bump. No wire, `Frame`, CRC, grid save, or
`.pack` change. Affinity changes for the listed symbols only.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Game.h`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`

## In scope

- `Game.h`: `ReplayMeta` (its three deleted members and `kiVersion`); the
  `BT_CLIENT` guard around the six declarations `## Design` item 4 names.
- `Game.cpp`: `ClientOwnedEntityIdentifier`, `AddClientOwnedEntity`,
  `RemoveClientOwnedEntity`, `ClientOwnedEntity` (guard only, plus the inner
  guard removal); `ResetGameState`; `RestoreReplayMetadata`.
- `Save/GameSaveLoad.cpp`: the `ReplayMeta` initializer in
  `WriteReplayMetadata`.
- Documentation sync that `/update-claude-docs` finds necessary.

## Out of scope

- `clientGridCoordinate`, the replay fleet records, the fleet checksums, and
  every other part of the replay metadata file.
- Moving `ReplayMeta` or `ReplayStagedMeta` out of `Game.h`, guarding
  `ReplayMeta` or `RestoreReplayMetadata` under `BT_SERVER`, and include
  cleanup in `Game.h`.
- `ConfirmedClientState::fPreviousClientArmor` and the reconciler's handling
  of it (`Network/Client/`).
- Any behavior change in `Game::UpdateCameraShake`.
- Backward compatibility with older replay files.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: the
change alters a save/replay file layout (`ReplayMeta`) and bumps its version.

## Acceptance criteria

1. `/compile` builds the BrokenEngineSandbox client and server. The guard was
   not compiled when this Plan was written, so a server use the search missed
   shows up here.
2. Code read: `ReplayMeta` holds only `clientGridCoordinate`, and
   `ReplayMeta::kiVersion` is above its value at claim.
3. Code read: the six symbols `## Design` item 4 names are declared and
   defined only under `BT_CLIENT`.
4. Harness (`/agent-harness`): the replay determinism acceptance in
   `.agents/skills/agent-harness/references/replay.md`
   `### Replay determinism acceptance` shows no frame CRC or fleet checksum
   mismatch. The supplied evidence records no harness setup; the executing
   session sets it up from that reference.

## Notes

- Not checked when this Plan was written: the ModelViewer build (it has its
  own `Game.h` and does not compile these files) and the private Coretide
  project, which is absent from session worktrees.
- Observations only, not work items: after this change
  `Game::RestoreReplayMetadata` is one `SetClientGridCoordinate` call, and
  `Game.h:10` includes `Network/Server/ServerFleetSerialization.h` only for
  `ReplayFleetRecord` in `ReplayStagedMeta`.

## Coordination

- `Documents/Plans/Game/FleetOwnedSquadronState.md` also bumps
  `ReplayMeta::kiVersion` and edits `Save/GameSaveLoad.cpp` (a different
  function). Each Plan raises the version once from the value it finds at
  claim; whichever lands second bumps again and does not reuse the first
  one's bump.
