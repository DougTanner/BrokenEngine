<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:54:20.172Z","dependsOn":[]} -->
# Halt on a damaged grid save and call the load reset directly

## Context

Two pieces of debt meet in `ServerSaveLoad::ServerLoad`
(`Engine/Source/File/ServerSaveLoad.cpp`): it survives a damaged grid save
through code that exists only for that, and it calls the load reset through a
one-statement game wrapper.

### Damaged grid save

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

### OnStateReplaced pass-through

Originating gap: the architecture review (cohesion lens) of the
`/external-deep-analysis` run over
`Projects/BrokenEngineSandbox/Source/Save`. The wrapper predates that run.

`game::OnStateReplaced()` (declared at
`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h:15`, defined at
`Save/GameSaveLoad.cpp:89-92`) has a one-statement body:
`game::gpServerSession->ResetForLoad();`. `Documents/C++StyleGuide.txt` rule 49
forbids a function whose implementation is only one pass-through call.

Verified against commit `4cf9d607`; the line numbers in `GameSaveLoad.cpp` and
under `Engine/Source/File` were corrected for later changes:

- `OnStateReplaced` has five callers:
  `Engine/Source/File/ServerSaveLoad.cpp:43` (`ServerLoad`) and `:61`
  (`ServerReset`), `Engine/Source/File/Replay.cpp:288`
  (`Replay::ClearReplayAbortState`) and `:552` (replay adoption, right after
  `game::AdoptReplayMetadata`), and
  `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp:213`.
- `engine::ServerSessionBase::ResetForLoad` (public, declared at
  `Engine/Source/Network/Server/ServerSessionBase.h:24`, defined at
  `ServerSessionBase.cpp:285`) has no caller other than `OnStateReplaced`.
- Game policy already enters the load reset from inside it:
  `ResetForLoad` calls `game::gpServerSession->OnResetForLoad()`
  (`ServerSessionBase.cpp:319`). The wrapper adds no game policy of its own.
- Four of the five call sites can already reach the target: the two engine
  files use `game::gpServerSession->` on adjacent lines. `ServerFrameEdit.cpp`
  no longer includes `Game.h`, so it needs
  `#include "Network/Server/ServerSession.h"`, which declares `gpServerSession`.
- Three documents describe the wrapper. `Save/AGENTS.md` `## Replay Contract`,
  the bullet starting "After a successful save load" (lines 47-51), says one
  game entry point runs the engine session load reset and is its sole caller.
  `Engine/Source/File/AGENTS.md:171-172` says abort "also runs the game
  state-replacement hook".
  `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md:72-73` says connected
  clients "resynchronize through the Save entry point (`../Save/AGENTS.md`)".
  No document names `OnStateReplaced` itself.

`Engine/Source/AGENTS.md` `## Hub Conventions`, engine/game contract bullet,
protects the engine calling game code by name. It does not bar this change:
the replacement call goes through `game::gpServerSession`, which is still game
code reached by name.

Unmet criteria: a current-format grid save file whose contents fail to load
halts the server; no code wipes or rebuilds live state to survive one; the
`OnStateReplaced` pass-through is gone and its five sites call the load reset
directly; the documents say so.

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

Before step 1, check each other game project present in the primary checkout
for a `ResetGameState` that reads cell or save state, and for a caller of
`ServerLoad` or `Autoload` that relies on `false` meaning a damaged file; if
either exists, stop and report it rather than reordering.

1. `ServerSaveLoad::ServerLoad`, one combined rewrite: read into a local
   `StagedGridSave`. When the read returns `false`, `ASSERT` that
   `bHeaderValidated` is clear, then return `false` with the grid and the
   game save state unchanged. On success copy `clientGridCoordinate` out,
   call `ResetSession()`, then `AdoptGridSave(std::move(staged))`, then
   `SetClientGridCoordinate` with the copied coordinate. Delete the tick save
   and restore at lines 38 and 40-41. Keep `LatchRawCpuTimers(false)` before
   frame state changes. Keep the tail in place, with its load-reset call
   written as `game::gpServerSession->ResetForLoad();` in place of
   `game::OnStateReplaced();` (line 43), followed by the unchanged
   `ComputeActiveSet` call. Add no file probe.
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
6. The other four `OnStateReplaced` call sites, each at the same position:
   - `ServerSaveLoad.cpp:61` (`ServerReset`), `Replay.cpp:288` and `:552`:
     replace `game::OnStateReplaced();` with
     `game::gpServerSession->ResetForLoad();`.
   - `ServerFrameEdit.cpp:213`: replace `OnStateReplaced();` with
     `gpServerSession->ResetForLoad();` and add
     `#include "Network/Server/ServerSession.h"`. Remove the
     `Save/GameSaveLoad.h` include at line 15 if nothing else in the file then
     uses that header; at the baseline the call is the file's only use of it.
7. `Save/GameSaveLoad.cpp`: delete the `OnStateReplaced` definition.
   `Save/GameSaveLoad.h`: delete the `OnStateReplaced` declaration, and end
   the header comment at lines 10-11 after the per-tick fleet checksums,
   dropping "and client resynchronization", which described only the deleted
   function.
8. Rewrite the five grid-save document passages named in `## Context` so each
   says a current-format grid save whose contents fail to load halts the
   server, and none says such a file is rejected, fails the load, or resets
   to a fresh game. Each passage that names the survived cases names exactly
   two: a missing file and a save of another format. The `load` entry of the
   harness command reference keeps the reset for those two only. Grid-save
   validation itself stays documented: the checks still run, and the halt is
   how the server answers a current-format file that fails them.
9. Reword the three load-reset document sentences:
   - `Save/AGENTS.md` `## Replay Contract`: reword the "After a successful
     save load" bullet so it stays true. Suggested content: each of those
     paths calls the engine session load reset
     (`engine::ServerSessionBase::ResetForLoad`), which rebuilds client
     ownership and resynchronizes connected clients; the ordering is inside
     that one function, so every path shares it.
   - `Engine/Source/File/AGENTS.md:171-172`: reword "Abort also runs the game
     state-replacement hook (...)" to say abort also runs the engine session
     load reset, citing `Engine/Source/Network/Server/AGENTS.md` (from that
     file, `../Network/Server/AGENTS.md`) instead of the game Save document.
     That document owns the `ServerSessionBase::ResetForLoad` contract: its
     opening paragraph lists the load reset among what `ServerSessionBase`
     owns.
   - `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md:72-73`: reword
     "Connected clients resynchronize through the Save entry point
     (`../Save/AGENTS.md`)." to say connected clients resynchronize through
     the engine session load reset, keeping the `../Save/AGENTS.md` citation,
     whose reworded bullet still lists the agent frame edit path.

The load-reset ordering lives inside `ResetForLoad`, and each site keeps its
call at the same position, so replacing the wrapper changes no ordering.

## Critical files

- `Engine/Source/File/GridSave.cpp`
- `Engine/Source/File/GridSave.h`
- `Engine/Source/File/ServerSaveLoad.cpp`
- `Engine/Source/File/Replay.cpp`
- `Engine/Source/File/AGENTS.md`
- `Engine/Source/Network/Server/ServerSessionBase.cpp`
- `Engine/Source/Agent/AgentCommandsServerGeneric.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`
- `AGENTS.md`
- `.agents/references/cpp-conventions.md`
- `.agents/skills/repo-code-review/references/checks.md`
- `.agents/skills/agent-harness/references/command-reference.md`

## In scope

Grid-save load:

- The `ReadGridSave` overload taking `GridCoord&`: deleted.
- `ServerSaveLoad::ServerLoad` and `ServerSaveLoad::Autoload`.
- The comments and the log text named in `## Design` steps 4 and 5.
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

Load-reset pass-through:

- `game::OnStateReplaced`: declaration and definition, deleted.
- The five `OnStateReplaced` call statements named in `## Design` steps 1
  and 6.
- The `Save/GameSaveLoad.h` include in `ServerFrameEdit.cpp`, only if unused
  after the change.
- A `#include "Network/Server/ServerSession.h"` line added to
  `ServerFrameEdit.cpp`.
- The header comment above `WriteReplayMetadata` in `Save/GameSaveLoad.h`:
  its "and client resynchronization" tail only.
- `Save/AGENTS.md` `## Replay Contract`: the "After a successful save load"
  bullet only.
- `Engine/Source/File/AGENTS.md`: the "Abort also runs the game
  state-replacement hook" sentence.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`: the "Connected
  clients resynchronize through the Save entry point" sentence only.

## Out of scope

- The staged `ReadGridSave` overload's signature, return value, and
  validation checks; `StagedGridSave::bHeaderValidated` and its write;
  `WriteGridSave`, `AdoptGridSave`, and the save file format.
  `game::Frame::kiVersion` does not change.
- Replay playback's load path in `Replay.cpp`, including what it does when
  the replay grid fails to read; only its two `OnStateReplaced` call
  statements change.
- What a missing file and a save of another format do: `Autoload` still
  returns `false`, and the `ServerLoad` callers still reset to a fresh game.
- A file probe in `ServerLoad`, and any check that tells a damaged header
  from another format.
- `ServerReset` apart from its `OnStateReplaced` call statement and the
  comment at `ServerSaveLoad.cpp:58-59`; `ServerSave`, `Autosave`, and the
  save file names.
- The network half of the trust policy: client-to-server records keep the
  bad-value rule.
- The game `ResetGameState`, `AdoptSaveState`, `ResetSaveState`,
  `ReadSaveState`, and `ValidateSaveState`.
- `ServerSessionBase::ResetForLoad` and `ServerSession::OnResetForLoad`:
  body, signature, and ordering unchanged.
- The `ComputeActiveSet` calls, and every statement around the `Replay.cpp`
  and `ServerFrameEdit.cpp` call sites.
- Every other function in `Save/GameSaveLoad.cpp` and `.h`, and renaming those
  files.
- The block comment above `WriteReplayMetadata` in `Save/GameSaveLoad.cpp`.
- Every other bullet of `Save/AGENTS.md` `## Replay Contract`.
- Replay file formats and versions: nothing serialized changes.

## Risk tier

Tier 3 — invariant/integration. Triggers: trust boundary and save/replay — it
changes how the server answers a damaged grid save from surviving it to
halting, which a connected client's load request can reach, changes the
order in which the server adopts a grid save into live deterministic state,
and spans File, Network Server, Agent, and the root trust policy; and it
removes a `game::` function the engine calls by name, across engine File
(`ServerSaveLoad.cpp`, `Replay.cpp`), game Save, and game Agent. Invariants
to hold: a successful load keeps the saved simulation clock and
next-global-ID counter and resynchronizes clients; pending raw profile
samples are discarded before frame state changes; a missing file or a save
of another format leaves the grid and the game save state unchanged in
`ServerSaveLoad`, and never halts; the load reset runs at the same five
points, with its ordering unchanged inside `ResetForLoad`.

## Acceptance criteria

- Only the staged `ReadGridSave` overload exists; no code under
  `Engine/Source/File` mutates `mCells` or game save state on a failed read.
- `ServerLoad` and `Autoload` each `ASSERT`, on a failed staged read, that
  `bHeaderValidated` is clear, and each returns `false` only when the file
  failed the version header check, which covers a missing file, or, in
  `Autoload`, when its probe found no file. The halt and the other-format
  path are verified by code reading; no hand-edited save is made for them.
- None of the five grid-save document passages says a current-format grid
  save whose contents fail to load is rejected, fails the load, or resets to
  a fresh game; each says it halts the server, and none says a missing file
  or a save of another format halts. The change's report names the three
  documents the user's statement overrode.
- A search for `OnStateReplaced` across the repository, excluding
  `Documents/Plans/`, returns nothing.
- Code read: `ResetForLoad` is called at exactly the five sites
  `## Context` lists, each where `OnStateReplaced` was called.
- Code read: the three reworded load-reset document sentences (`## Design`
  step 9) name no game entry point or hook for the load reset and do not call
  anything its sole caller.
- `/compile` builds the BrokenEngineSandbox client and server without errors
  or new warnings.
- Live, through `/agent-harness`, on a server with a connected client: save,
  advance the simulation, then load. The server tick returns to the saved
  tick, the client resynchronizes, and no desync or CRC mismatch is logged
  afterwards. Then `load` a file name that does not exist: the result
  carries `resetToFresh:true` and the server keeps running. Restart the
  server and confirm the autosave loads. The analysis recorded no harness
  setup for this; the executing session chooses the commands.

## Notes

- The pass-through removal's value is small: one function and one
  declaration removed. The cost to weigh is that the five sites lose the name
  "state replaced" and name the load reset instead.
- The analysis that raised the pass-through found no second
  `OnStateReplaced` implementation in the private project of the primary
  checkout. That project is absent from session worktrees, so the executing
  session cannot build it; if it has gained one by claim time, that copy is
  deleted with its own project's change, not here.

## Coordination

`Documents/Plans/Game/ReplayMetaClientStateAndFighterAdmission.md` and
`Documents/Plans/Game/FleetOwnedSquadronState.md` edit other functions of
`Save/GameSaveLoad.cpp`, and the second also edits other bullets of
`Save/AGENTS.md` `## Replay Contract`. The regions do not overlap and no
order is required.
