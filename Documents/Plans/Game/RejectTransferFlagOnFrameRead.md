<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:54:12.417Z","dependsOn":[]} -->
# Reject a saved Transfer flag on game collection rows at the Frame read

## Context

Line numbers are from commit `035db8bf`; locate by symbol if they moved. `BES/`
below is `Projects/BrokenEngineSandbox/`, and `Collections/` is
`BES/Source/Frame/Collections/`.

A grid save can carry `engine::LifecycleFlags::kTransfer` on a row of a game
collection, and no check on the load path rejects it. On the next tick the row
enters the Transfer phase whatever its position, and for a position inside the
cell the engine indexes `kCellDirections.pEntries` with -1. This was traced by
reading the code; it was never run.

Load path, as read:

- `engine::ReadGridSave` reads each cell with `fileStream >> *pFrame`
  (`Engine/Source/File/GridSave.cpp:118`) and then calls
  `game::ValidateSaveState` (`:145`).
- The game `operator>>` (`BES/Source/Frame/Frame.cpp:792-804`) reads both
  halves, then runs the pair checks, `ValidatePusherOwners`, and
  `ValidateFlyingMissiles`.
- `ValidatePusherOwners` rejects a nonzero lifecycle mask only on Pushers rows
  (`Frame.cpp:52-59`); on ship rows it reads only `kDying` (`:42`, `:69`).
  `ValidateFlyingMissiles` reads only `kDying` (`:98`).
- `engine::ValidateAfterRead` tests `puiTypeIndices` and calls `PostRead`
  (`Engine/Source/Frame/Collections/Collection.h:72-95`). None of the six ship
  `PostRead` hooks tests a lifecycle bit or `IsInsideCell`
  (`Collections/Fighters/Fighters.h:74`, `:193-230`;
  `Collections/Omnis/Omnis.h:107`, `:232-265`;
  `Collections/Battleships/Battleships.h:72-95`, `:186-223`). Blasters and
  Missiles declare no `PostRead`.
- No file under `BES/Source/Network` or `BES/Source/Save`, where
  `ValidateSaveState` is defined
  (`BES/Source/Network/Server/ServerFleetSerialization.cpp:389`), names
  `LifecycleFlags`.

Carry into the tick, as read:

- `pLifecycleFlags` is in `PersistentMembers()` of the Fighters, Omnis,
  Battleships, and Missiles PostRender (`Fighters.h:293-296`, `Omnis.h:346`,
  `Battleships.h:281-284`, `Collections/Missiles/Missiles.h:161-163`), and
  `BlastersPostRender` declares none, so its whole `Members()` is copied
  (`Collections/Blasters/Blasters.h:96-100`;
  `Engine/Source/Frame/Collections/AGENTS.md` `## Core Contract`).
- Every write of the column in `Collections/` sets a bit (`Set(...)` or
  `engine::MarkDestroy`); none clears `kTransfer`.
- Each Transfer hook selects rows by the flag alone and calls
  `PrepareTransferRequest`: `Collections/Battleships/Battleships.cpp:243-246`,
  `:274`; `Collections/Fighters/Fighters.cpp:366`, `:395`;
  `Collections/Omnis/OmnisNavigation.cpp:17`, `:51`;
  `Collections/Missiles/Missiles.cpp:269`, `:297`;
  `Collections/Blasters/Blasters.cpp:129`, `:144`.

Out-of-range index, as read:

- `common::HexExitDirection` starts `iExitDirection` at -1 and assigns it only
  for an edge the position fails, so an inside position returns -1
  (`Common/Math/MathUtils.cpp:230-251`).
- `FramePostRenderBase::PrepareTransferRequest` stores that value in
  `rRequest.iDirection` and reads
  `kCellDirections.pEntries[iDirection].f2CenterOffset`
  (`Engine/Source/Frame/FrameBase.cpp:119-125`) before its only `ASSERT`
  (`:126`).
- `ServerTransferManager::CollectTransfers` indexes
  `kCellDirections.pEntries[rRequest.iDirection]` again
  (`Engine/Source/Network/Server/ServerTransferManager.cpp:54`).

Authority: root `AGENTS.md` `## Key Patterns` (the server fully validates every
grid save); the bad-value rule in `.agents/references/cpp-conventions.md`
(reject the input, never clamp or normalize);
`Engine/Source/File/AGENTS.md` `## Grid Saves`; `BES/Source/Frame/AGENTS.md`
`## Invariants` (a transfer producer calls `PrepareTransferRequest` only for a
position that fails `IsInsideCell`). Grid saves are not an exempt developer
tool.

Originating gap: the findings verification (Phase 3) of an
`/external-deep-analysis` run over `BES/Source/Agent`, which found the
`edit_frame` form of this write and noted that the same reader serves grid
saves. It predates that run, which changed no code. Unmet criterion: a grid
save cannot put a row into the Transfer phase with a position inside its cell,
or otherwise reach an out-of-range `kCellDirections.pEntries` index.

Not confirmed, and left to the executing session:

- That a frame the server writes never holds `kTransfer`. As read, every
  `Set(kTransfer)` site is a tick-phase hook, and `engine::CompactCollections`,
  the last step of the game Destroy phase (`Frame.cpp:226-233`), removes every
  `kTransfer` row (`Engine/Source/Frame/FrameUtils.h:371-380`). Whether
  `WriteGridSave` (`Engine/Source/File/ServerSaveLoad.cpp:21`, `:64`;
  `Engine/Source/File/Replay.cpp:719`) and the replay full-frame writer only
  ever write a frame after that step was not read.
- The runtime outcome. No run loaded such a save.

## Design

First confirm the open point above by reading: the tick phase order
(`Documents/Architecture/FrameUpdatePipeline.md`) and every caller that writes
a `game::Frame` to a stream. If a written frame can hold `kTransfer`, stop and
surface it for re-planning: the rule below would then reject a valid save.

Recommended (the author's recommendation): in the game `operator>>`
(`Frame.cpp:792-804`), beside `ValidatePusherOwners`, reject any row of
Fighters, Omnis, Battleships, Blasters, or Missiles whose `pLifecycleFlags` has
`kTransfer` set, by throwing `std::ios_base::failure` in the form the
neighbouring checks use. Test the flag only, not the position.

Rationale:

- A flag-only test rejects a value the simulation never leaves in a written
  frame, so it needs no position rule and cannot disagree with `IsInsideCell`.
- The Frame read is where the other cross-collection row checks live, and
  `ReadGridSave` already turns its throw into a failed load
  (`GridSave.cpp:147-151`).
- No format changes, so no `kiVersion` bump: the rule rejects only bytes no
  server build writes.

Consequence to carry: `edit_frame` commits through the same reader
(`BES/Source/Agent/Commands/ServerFrameEdit.cpp:203-205`), so it will then
refuse a `pLifecycleFlags` write that sets `kTransfer`, returning `ok:false`
with the frame unchanged. That makes two sentences of
`BES/Documents/AgentHarness/commands-server.md` wrong, and this Plan corrects
both: the save-reader check list ("with no lifecycle bit on a Pushers row"),
and the sentence of the unchecked-writes paragraph that starts "A
`pLifecycleFlags` write that sets `LifecycleFlags::kTransfer`" and says the
write is unchecked. The same reader also reads replay full frames
(`Engine/Source/File/DifferenceStream.h:556`); replay streams are exempt but
may keep a check.

Alternative, not recommended: a grid-save-only check, for example in
`game::ValidateSaveState`, would leave `edit_frame` unchanged but puts a frame
row rule in the fleet save-state validator, away from the other row checks.
If the executing session's reading shows the recommended placement cannot
work, it surfaces that for re-planning instead of widening scope.

Risk tier: Tier 2 (the author's classification). Trigger: tightening a check
inside one unit at an existing read boundary, with the format and the trust
unchanged (`.agents/references/risk-tiers.md`). Exposure: no CRC, wire, save
layout, or `kiVersion` change; one agent command gains a refusal; the check
runs on every build-local Frame read, the replay full-frame read included.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/AGENTS.md`
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md`

## In scope

- Investigation: confirm that no writer of a `game::Frame` stream can write a
  row holding `kTransfer`.
- `Frame.cpp`: one file-local check called from the game `operator>>`, or the
  same test added to an existing file-local validator there, covering the five
  collections named in `## Design`.
- `BES/Source/Frame/AGENTS.md` `## Invariants`: the read-boundary bullet that
  lists the Pushers lifecycle rejection gains this one.
- `commands-server.md`, `edit_frame` entry: the save-reader check list and the
  `kTransfer` sentence of the unchecked-writes paragraph.

## Out of scope

- `PrepareTransferRequest`, `HexExitDirection`, `CollectTransfers`, and any
  check, assert, or early return at the index sites.
- Any position rule at the read: a saved position outside its cell, including
  one more than one neighbour step out, which `commands-server.md:105-108`
  records as asserting in `PrepareTransferRequest`.
- `kDestroy` and `kDying` on the read path, and the engine collections'
  lifecycle columns.
- `Frame::ServerRead`, which reads data the client trusts.
- Any format or version change, and any `PostRead` hook.

## Acceptance criteria

1. The game `operator>>` throws `std::ios_base::failure` for a Fighters,
   Omnis, Battleships, Blasters, or Missiles row with `kTransfer` set; the diff
   settles this.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Live, through `/agent-harness`: an `edit_frame` that writes
   `pLifecycleFlags` 1 on a ship row returns `ok:false` and a following
   `read_frame` shows the row unchanged; a save followed by a load of ordinary
   play still succeeds. The supplied evidence records no harness setup for the
   original symptom, which was never reproduced, so this criterion stands in
   for "the recorded symptom no longer reproduces".
4. `commands-server.md` no longer says a `kTransfer` write is unchecked, and
   its save-reader check list names the new rejection.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- `Documents/Plans/Game/RejectNonFiniteOmniArmorAndShield.md` tightens
  `OmnisPostRender::PostRead` at the same boundary; the two edits do not
  overlap and need no order.
- Other Plans edit other passages of `commands-server.md`; line numbers here
  shift once one lands.
