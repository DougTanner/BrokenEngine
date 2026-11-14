<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:54:20.172Z","dependsOn":[]} -->
# A damaged grid save halts the server

## Context

Found by the architecture phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. It is
pre-existing debt, not an unmet acceptance criterion of any change.

Today a grid save that fails to load is survived, through code that exists
only for that:

- The staged `ReadGridSave` overload
  (`Engine/Source/File/GridSave.cpp:69-161`) writes only its `StagedGridSave`
  out-parameter and returns `false` on a version mismatch, a caught
  `std::exception` from a corrupt or truncated stream, or a failed stream.
- The wrapper overload (`GridSave.cpp:50-67`, declared `GridSave.h:22`) calls
  it, and when the read fails after the header passed it clears
  `game::gpGame->mCells` and calls `game::ResetSaveState()` (lines 55-60),
  leaving `miNextGlobalId` and `miTickCounter` untouched.
  `StagedGridSave::bHeaderValidated` (`GridSave.h:16`) exists only for that
  wipe: written once (`GridSave.cpp:80`), read once (line 55).
- Both callers of `ServerSaveLoad::ServerLoad` call `ServerReset()` on
  `false`, so a failed load becomes a fresh game
  (`Engine/Source/Network/Server/ServerSessionBase.cpp:132-138`,
  `Engine/Source/Agent/AgentCommandsServerGeneric.cpp:153-158`); the agent
  `load` command reports it as `resetToFresh`. The comments at
  `ServerSessionBase.cpp:134-136` and `AgentCommandsServerGeneric.cpp:155-156`
  describe the wrapper's wipe.
- `ServerSaveLoad::Autoload` returns `false` on a failed read
  (`Engine/Source/File/ServerSaveLoad.cpp:129-132`), and its caller then
  starts a new game (`Projects/BrokenEngineSandbox/Source/Game.cpp:30-33`).

The user decided that a failed save file load is an `ASSERT`, and, asked
which failures that covers, chose damaged files only. This is the authoring
session's record of that decision:

- A file of the current format whose contents fail to read or validate halts
  the server with `ASSERT`, instead of continuing with a fresh game. `ASSERT`
  is active in every build (`Common/ErrorUtils.h:20`).
- A save of another format — one that fails the version header check — is
  treated like a missing file and never halts.
- A missing file is not a failed load.
- The debug replay tool's own grid read keeps returning failure.

This Plan reads "treated like a missing file" as the governing words of that
choice: a save of another format does exactly what a missing file does today
on each load path, and neither case changes behavior. At startup that is a
fresh game. On a load a client or the agent requests, it is today's `false`
path: `ServerReset()` to a fresh game
(`Engine/Source/Network/Server/ServerSessionBase.cpp:132-138`). A client
quickload of a missing file or of a save of another format therefore still
resets the running game to a fresh game, as it does today. Leaving the
running game untouched on such a load is out of scope.

The version header check (`GridSave.cpp:75-79`,
`ReadAndValidateVersionHeader` at `Engine/Source/File/FileManager.h:230-236`)
is therefore the one rejection that remains survived. A missing file already
takes it: it opens as a closed stream and fails that check. So does a file
damaged inside the header itself, which cannot be told from another format.

That decision contradicts three documents for the damaged-file case, and the
user's statement was trusted over them (root `AGENTS.md`
`### Diagnosis Discipline`), so this Plan rewrites them:

- `Engine/Source/File/AGENTS.md` `## Grid Saves`: "The file is a trust
  boundary: `ReadGridSave` bounds, cross-checks, and rejects bad adopted
  values".
- Root `AGENTS.md` `## Key Patterns`, the trust-policy bullet: the server
  validates every grid save, "answering a bad value under the bad-value
  rule".
- `.agents/references/cpp-conventions.md`: the bad-value rule ("reject the
  input — drop the packet or fail the load") and the bullet above it, which
  lists grid saves among the inputs the trust-policy bullet covers.

Two more documents describe the surviving behavior and become false for a
damaged file:
`.agents/skills/repo-code-review/references/checks.md` ("Invalid persisted
grid data follows `engine::ReadGridSave` ... clear partial grid/fleet state,
log, and return `false`") and the `load` entry of
`.agents/skills/agent-harness/references/command-reference.md`
("Missing/corrupt/truncated data resets fresh while returning success, as
does a save written under another game frame version").

What each load path does, by case:

- `Autoload` probes for `ServerAutosave.save` before reading
  (`ServerSaveLoad.cpp:123-126`) and returns `false` when it is absent; a
  first start has no autosave, and the caller starts a new game. An autosave
  of another format also returns `false` and starts a new game, as today, so
  a frame version bump does not stop the server from starting. A damaged
  current-format autosave halts the server at start.
- `ServerLoad` has no probe and needs none. A missing file (a quickload
  before any quicksave, or an agent `load` naming a file that does not
  exist) and a save of another format both fail the header check and return
  `false`, and both callers reset to a fresh game, as today. A damaged
  current-format file halts the server.
- A connected client reaches `ServerLoad`: its `kClientLoadRequest`
  debug-control request runs `ServerLoad("ServerQuicksave.save")`
  (`Engine/Source/Network/Server/ServerSessionBase.cpp:129-132`). With no
  quicksave, or one of another format, the server resets to a fresh game, as
  today. With a damaged current-format quicksave, that client's request
  halts the server.

The wrapper also forces a workaround in `ServerLoad`
(`ServerSaveLoad.cpp:38-41`): because the wrapper has already adopted, the
function saves `miTickCounter`, calls `ResetSession` (which zeroes it,
`Engine/Source/GameBase.cpp:1185`), and restores it. Replay playback uses the
other order — stage, `ResetSession`, then `AdoptGridSave`
(`Engine/Source/File/Replay.cpp:491-530`). `ResetSession`
(`GameBase.cpp:1181-1214`) and the game `ResetGameState`
(`Projects/BrokenEngineSandbox/Source/Game.cpp:163-173`) read no cell or save
state, so reset-then-adopt gives `ServerLoad` the same result.

Unmet criteria: a current-format grid save file whose contents fail to load
halts the server; no code wipes or rebuilds live state to survive one; the
documents say so.

## Design

The `ASSERT` goes in `ServerSaveLoad`, after the staged `ReadGridSave`
returns, and that overload's `bool` return and its validation stay as they
are. Replay playback also calls the staged overload and cancels playback on
`false` (`Replay.cpp:492-497`), which the decision keeps; and the bounds
checks inside the read still have to run before the halt, so a corrupt count
cannot overrun memory first.

`StagedGridSave::bHeaderValidated` (`GridSave.h:16`, written at
`GridSave.cpp:80`) already separates the two kinds of `false`: it is clear
when the file failed the version header check, and set when the contents
failed after the header passed. `ServerSaveLoad` reads it in place of the
wrapper.

1. `ServerSaveLoad::ServerLoad`: read into a local `StagedGridSave`. When
   the read returns `false`, `ASSERT` that `bHeaderValidated` is clear, then
   return `false` with the grid and the game save state unchanged. On
   success copy `clientGridCoordinate` out, call `ResetSession()`, then
   `AdoptGridSave(std::move(staged))`, then `SetClientGridCoordinate` with
   the copied coordinate. Delete the tick save and restore at lines 38 and
   40-41. Keep `LatchRawCpuTimers(false)` before frame state changes and the
   `OnStateReplaced` and `ComputeActiveSet` tail. Add no file probe.
2. `ServerSaveLoad::Autoload`: keep the probe; read into a local
   `StagedGridSave`. When the read returns `false`, `ASSERT` that
   `bHeaderValidated` is clear, then return `false`. On success
   `AdoptGridSave`, then `SetClientGridCoordinate`.
3. Delete the wrapper overload (`GridSave.cpp:50-67`, `GridSave.h:22`).
   `bHeaderValidated` and its write stay.
4. Both `ServerLoad` callers keep their `ServerReset()` on `false`, which now
   means only "no file, or a save of another format", and the agent `load`
   result keeps `resetToFresh`. Reword the comments at
   `ServerSessionBase.cpp:134-136` and `AgentCommandsServerGeneric.cpp:155-156`
   and the log text at `ServerSessionBase.cpp:137` so they describe those two
   cases, not a corrupt save or a cleared grid.
5. Reword the comments that describe surviving a bad save: `GridSave.cpp`
   lines 84-86 ("abort the load gracefully ... can't ... crash the server")
   and 94-96 ("leaves the fresh-fallback game minting from a clean base"),
   and `ServerSaveLoad.cpp:58-59` so it does not say `ReadGridSave`
   populates live state.
6. Rewrite the five document passages named in `## Context` so each says a
   current-format grid save whose contents fail to load halts the server,
   and none says such a file is rejected, fails the load, or resets to a
   fresh game. Each passage that names the survived cases names exactly two:
   a missing file and a save of another format. The `load` entry of the
   harness command reference keeps the reset for those two only. Grid-save
   validation itself stays documented: the checks still run, and the halt is
   how the server answers a current-format file that fails them.

Before step 1, check each other game project present in the primary checkout
for a `ResetGameState` that reads cell or save state, and for a caller of
`ServerLoad` or `Autoload` that relies on `false` meaning a damaged file; if
either exists, stop and report it rather than reordering.

## Critical files

- `Engine/Source/File/GridSave.cpp`
- `Engine/Source/File/GridSave.h`
- `Engine/Source/File/ServerSaveLoad.cpp`
- `Engine/Source/Network/Server/ServerSessionBase.cpp`
- `Engine/Source/Agent/AgentCommandsServerGeneric.cpp`
- `Engine/Source/File/AGENTS.md`
- `AGENTS.md`
- `.agents/references/cpp-conventions.md`
- `.agents/skills/repo-code-review/references/checks.md`
- `.agents/skills/agent-harness/references/command-reference.md`

## In scope

- The `ReadGridSave` overload taking `GridCoord&`: deleted.
- `ServerSaveLoad::ServerLoad` and `ServerSaveLoad::Autoload`.
- The comments and the log text named in steps 4 and 5.
- `Engine/Source/File/AGENTS.md` `## Grid Saves`: the trust-boundary
  sentence, and the `ServerSaveLoad` bullet where it describes a load's
  result.
- Root `AGENTS.md` `## Key Patterns`: the grid-save part of the trust-policy
  bullet.
- `.agents/references/cpp-conventions.md`: the grid-save part of the
  bad-value rule and of the bullet above it.
- `.agents/skills/repo-code-review/references/checks.md`: the "Invalid
  persisted grid data" bullet.
- `.agents/skills/agent-harness/references/command-reference.md`: the `load`
  entry.

## Out of scope

- The staged `ReadGridSave` overload's signature, return value, and
  validation checks; `StagedGridSave::bHeaderValidated` and its write;
  `WriteGridSave`, `AdoptGridSave`, and the save file format.
  `game::Frame::kiVersion` does not change.
- Replay playback's load path in `Replay.cpp`, including what it does when
  the replay grid fails to read.
- What a missing file and a save of another format do: `Autoload` still
  returns `false`, and the `ServerLoad` callers still reset to a fresh game.
- A file probe in `ServerLoad`, and any check that tells a damaged header
  from another format.
- `ServerReset`, `ServerSave`, `Autosave`, and the save file names.
- The network half of the trust policy: client-to-server records keep the
  bad-value rule.
- The game `ResetGameState`, `AdoptSaveState`, `ResetSaveState`,
  `ReadSaveState`, and `ValidateSaveState`.

## Risk tier

Tier 3 — invariant/integration. Trigger: trust boundary and save/replay — it
changes how the server answers a damaged grid save from surviving it to
halting, which a connected client's load request can reach, changes the
order in which the server adopts a grid save into live deterministic state,
and spans File, Network Server, Agent, and the root trust policy. Invariants
to hold: a successful load keeps the saved simulation clock and
next-global-ID counter and resynchronizes clients; pending raw profile
samples are discarded before frame state changes; a missing file or a save
of another format leaves the grid and the game save state unchanged in
`ServerSaveLoad`, and never halts.

## Acceptance criteria

- Only the staged `ReadGridSave` overload exists; no code under
  `Engine/Source/File` mutates `mCells` or game save state on a failed read.
- `ServerLoad` and `Autoload` each `ASSERT`, on a failed staged read, that
  `bHeaderValidated` is clear, and each returns `false` only when the file
  failed the version header check, which covers a missing file, or, in
  `Autoload`, when its probe found no file. The halt and the other-format
  path are verified by code reading; no hand-edited save is made for them.
- None of the five document passages says a current-format grid save whose
  contents fail to load is rejected, fails the load, or resets to a fresh
  game; each says it halts the server, and none says a missing file or a
  save of another format halts. The change's report names the three
  documents the user's statement overrode.
- The BrokenEngineSandbox client and server build without errors or new
  warnings.
- Live, through `/agent-harness`, on a server with a connected client: save,
  advance the simulation, then load. The server tick returns to the saved
  tick, the client resynchronizes, and no desync or CRC mismatch is logged
  afterwards. Then `load` a file name that does not exist: the result
  carries `resetToFresh:true` and the server keeps running. Restart the
  server and confirm the autosave loads. The analysis recorded no harness
  setup for this; the executing session chooses the commands.

## Coordination

No mandatory nondirectional coordination.
