<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:15:15.544Z","dependsOn":[]} -->
# Remove the OnStateReplaced pass-through

## Context

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

Originating gap: the architecture review (cohesion lens) of the
`/external-deep-analysis` run over
`Projects/BrokenEngineSandbox/Source/Save`. The wrapper predates that run.

`Engine/Source/AGENTS.md` `## Hub Conventions`, engine/game contract bullet,
protects the engine calling game code by name. It does not bar this change:
the replacement call goes through `game::gpServerSession`, which is still game
code reached by name.

## Design

Recommended: call the load reset at the five sites and delete the wrapper.

1. `ServerSaveLoad.cpp:43` and `:61`, `Replay.cpp:288` and `:552`: replace
   `game::OnStateReplaced();` with `game::gpServerSession->ResetForLoad();`.
2. `ServerFrameEdit.cpp:213`: replace `OnStateReplaced();` with
   `gpServerSession->ResetForLoad();` and add
   `#include "Network/Server/ServerSession.h"`. Remove the
   `Save/GameSaveLoad.h` include at line 15 if nothing else in the file then
   uses that header; at the baseline the call is the file's only use of it.
3. `Save/GameSaveLoad.cpp`: delete the `OnStateReplaced` definition.
4. `Save/GameSaveLoad.h`: delete the `OnStateReplaced` declaration, and end the
   header comment at lines 10-11 after the per-tick fleet checksums, dropping
   "and client resynchronization", which described only the deleted function.
5. `Save/AGENTS.md` `## Replay Contract`: reword the "After a successful save
   load" bullet so it stays true. Suggested content: each of those paths calls
   the engine session load reset (`engine::ServerSessionBase::ResetForLoad`),
   which rebuilds client ownership and resynchronizes connected clients; the
   ordering is inside that one function, so every path shares it.
6. `Engine/Source/File/AGENTS.md:171-172`: reword "Abort also runs the game
   state-replacement hook (...)" to say abort also runs the engine session
   load reset, citing `Engine/Source/Network/Server/AGENTS.md` (from that
   file, `../Network/Server/AGENTS.md`) instead of the game Save document.
   That document owns the `ServerSessionBase::ResetForLoad` contract: its
   opening paragraph lists the load reset among what `ServerSessionBase` owns.
7. `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md:72-73`: reword
   "Connected clients resynchronize through the Save entry point
   (`../Save/AGENTS.md`)." to say connected clients resynchronize through the
   engine session load reset, keeping the `../Save/AGENTS.md` citation, whose
   reworded bullet (item 5) still lists the agent frame edit path.

Behavior is unchanged. The load-reset ordering lives inside `ResetForLoad`, and
each site keeps its call at the same position, so no ordering changes.

## Critical files

- `Engine/Source/File/ServerSaveLoad.cpp`
- `Engine/Source/File/Replay.cpp`
- `Engine/Source/File/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.h`
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`
- `Projects/BrokenEngineSandbox/Source/Save/AGENTS.md`
- `Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerFrameEdit.cpp`
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`

## In scope

- `game::OnStateReplaced`: declaration and definition, deleted.
- The five `OnStateReplaced` call statements named in `## Design` items 1
  and 2.
- The `Save/GameSaveLoad.h` include in `ServerFrameEdit.cpp`, only if unused
  after the change.
- A `#include "Network/Server/ServerSession.h"` line added to
  `ServerFrameEdit.cpp`.
- The header comment above `WriteReplayMetadata` in `Save/GameSaveLoad.h`: its
  "and client resynchronization" tail only.
- `Save/AGENTS.md` `## Replay Contract`: the "After a successful save load"
  bullet only.
- `Engine/Source/File/AGENTS.md`: the "Abort also runs the game
  state-replacement hook" sentence only.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md`: the "Connected
  clients resynchronize through the Save entry point" sentence only.

## Out of scope

- `ServerSessionBase::ResetForLoad` and `ServerSession::OnResetForLoad`: body,
  signature, and ordering unchanged.
- Any statement around the five call sites, including the comments at
  `ServerSaveLoad.cpp:40` and `:58-59` and the `ComputeActiveSet` calls.
- Every other function in `Save/GameSaveLoad.cpp` and `.h`, and renaming those
  files.
- The block comment above `WriteReplayMetadata` in `Save/GameSaveLoad.cpp`.
- Every other bullet of `Save/AGENTS.md` `## Replay Contract`.
- Replay file formats and versions: nothing serialized changes.

## Risk tier

Tier 3 — invariant/integration (the author's classification). Trigger: the
change spans independently owned subsystems — engine File
(`ServerSaveLoad.cpp`, `Replay.cpp`), game Save, and game Agent — and removes a
`game::` function the engine calls by name. No determinism/CRC, wire,
serialization, save/replay compatibility, threading, or trust exposure: the
same function runs at the same five points.

## Acceptance criteria

1. A search for `OnStateReplaced` across the repository, excluding
   `Documents/Plans/`, returns nothing.
2. `/compile` builds the BrokenEngineSandbox server and client.
3. Code read: `ResetForLoad` is called at exactly the five sites
   `## Context` lists, each where `OnStateReplaced` was called.
4. Code read: the three reworded document sentences (`## Design` items 5-7)
   name no game entry point or hook for the load reset and do not call
   anything its sole caller.

## Notes

- Value is small: one function and one declaration removed. The cost to
  weigh is that the five sites lose the name "state replaced" and name the
  load reset instead.
- The analysis that raised this found no second `OnStateReplaced`
  implementation in the private project of the primary checkout. That project
  is absent from session worktrees, so the executing session cannot build it;
  if it has gained one by claim time, that copy is deleted with its own
  project's change, not here.
- `Documents/Plans/Game/RemoveClientStateFromReplayMeta.md` and
  `Documents/Plans/Game/FleetOwnedSquadronState.md` edit other functions of
  `Save/GameSaveLoad.cpp`, and the second also edits other bullets of
  `Save/AGENTS.md` `## Replay Contract`. The regions do not overlap and no
  order is required.
