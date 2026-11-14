<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:42:34.618Z","dependsOn":[]} -->
# Remove no-effect statements and duplicate state from Replay

## Context

Found by the in-function phase of an analysis of `Engine/Source/File` (the
directory, not its subdirectories), verified against commit `7dae9a27`. All
seven items are pre-existing debt in `Engine/Source/File/Replay.cpp` and
`Replay.h`, not an unmet acceptance criterion of any change. Each removes
code that cannot change behavior; line numbers are for `Replay.cpp` unless a
file is named.

Shared proof used by items 1 and 2: the playback flag and the reader sets
cannot disagree outside one statement. `mbReplaying` is assigned only at
lines 277, 360, 546, 716, and 829, always from the two containers. Every
reader-set write is followed by one of those publishes: clear 275-276
(publish 277); emplace 359 (360); pending push 543 (546); erase 827 (829, or
an abort through 791, 285, 277); pending erase 834 (an abort return, or 846
then 360). `GameBase::ResetSession` reaches `ResetStreams`
(`Engine/Source/GameBase.cpp:1188`), which publishes through line 294. The
only outside user, `GameBase.cpp:1035-1036`, is a read.

1. Lines 714-716 copy `ClearReplayTransientState` (275-277) at record start,
   after `ASSERT(!(mbReplaying || kLoadReplay))` at line 698. A false flag
   means both containers are empty, so the three statements change nothing.
   `Engine/Source/File/AGENTS.md` `## Replay Streams` says those `ASSERT`s
   "are not recovery".
2. Three clears of already-empty state: line 404 (the guard at 398 returned
   for any non-empty set); line 898 (inside `empty() && empty()` at 895,
   after 829 published); the `else` at 578-581 (a false flag in the catch
   means both sets are empty).
3. Line 823 clears `rFrameInput.statusChanges` on the `mFrameInputs` entry
   that line 825 erases; nothing between reads it.
4. Lines 547-549 repeat `AdoptGridSave`'s tick adoption
   (`Engine/Source/File/GridSave.cpp:168-170`, same `ASSERT`). Line 501
   throws unless `stagedGrid.iTick == iInitialTick`, `ResetSession` runs at
   529 before the adoption at 530, and 531-546 never write the counter.
5. `miReplayInitialTick` (`Replay.h:61`) is a member used as a local: writes
   at 295, 304, 656, 726, 732; reads only at 735 and 747, both in the block
   that assigned it in the same call.
6. `StagedReplayReader::record` duplicates `PendingReplayReader` fields:
   `ReplayManifestRecord` is exactly `{iActivationTick, coordinate}` (52-56),
   and 434-435 copy both into `pendingReader`. `ActivateReplayReader`
   (349-361) takes a coordinate parameter both callers (539, 845-846) fill
   from the pending reader's own coordinate. Low priority.
7. Line 666 writes
   `recordedRecords.empty() ? 0 : recordedRecords.front().iActivationTick`.
   The block requires `!mReplayWriters.empty()` (587), both insert sites push
   a generation straight after `try_emplace` (332-340, 744-748), and no code
   removes one, so `recordedRecords` (594-601) is never empty there. The
   reader already calls `front()` unguarded at 484.

Unmet criterion: no handling for a state no writer produces
(`.agents/references/cpp-conventions.md`, first bullet), and no state held
twice.

## Design

Recommended, each located by symbol rather than line:

1. Delete lines 714-716.
2. Delete the `ClearReplayTransientState()` calls at 404 and 898, and reduce
   574-581 to `if (game::gpGame->mbReplaying) { ClearReplayAbortState(); }`.
   The same change rewords the playback-flag bullet of
   `Engine/Source/File/AGENTS.md` `## Replay Streams`, which says to
   republish "including the clears on abort, retirement, and loop
   transition": after 404 and 898 go, no clear exists on loop transition
   (the last retirement at 829 publishes false). `AGENTS.md` outranks code,
   so the two change together. If the executing session cannot reword that
   bullet, it keeps line 898 instead and says so in its report.
3. Delete line 823.
4. Delete lines 547-549.
5. Replace `miReplayInitialTick` with a local in the record-start block
   (726-749); delete `Replay.h:61` and the writes at 295, 304, and 656.
6. Stage `std::vector<PendingReplayReader>` instead of the wrapper struct,
   drop the `coordinate` parameter of `ActivateReplayReader`
   (`Replay.h:65`, `Replay.cpp:349`), and read `rPendingReader.coordinate`.
7. Line 666 becomes
   `.iInitialTick = recordedRecords.front().iActivationTick`.

Do item 6 last; if it grows beyond the lines named, leave it out and report
it rather than widening the change.

## Critical files

- `Engine/Source/File/Replay.cpp`
- `Engine/Source/File/Replay.h`
- `Engine/Source/File/AGENTS.md`

## In scope

- `Replay::SyncReplayTick`: the three reader-clear statements at record
  start, the `statusChanges.clear()` before the retirement erase, the
  loop-transition clear, and `miReplayInitialTick`.
- `Replay::SaveLoadReplay`: the clear after the early-return guard, the
  catch block's `else`, the repeated tick adoption, the staged reader
  vector, and the manifest `iInitialTick` initializer.
- `Replay::ActivateReplayReader`: its coordinate parameter.
- `Replay::ResetStreams` and `Replay::InvalidateReplayRecording`: their
  `miReplayInitialTick` writes.
- `Replay.h`: `miReplayInitialTick` and the `ActivateReplayReader`
  declaration.
- The playback-flag bullet of `Engine/Source/File/AGENTS.md`
  `## Replay Streams`.

## Out of scope

- `ClearReplayTransientState` and `ClearReplayAbortState` themselves, and
  every other publish of `mbReplaying`.
- The replay file names, the manifest format and
  `kiReplayManifestVersion`, and the stream bytes.
- `AdoptGridSave`, `ReplayHarnessRigs`, and `GameBase`.
- Moving code between `SaveLoadReplay` and `SyncReplayTick`.

## Risk tier

Tier 2 — scoped behavior. Trigger: it edits the record-start, playback-load,
retirement, and loop paths of one subsystem's runtime, a debug-only
developer tool, and one sentence of its leaf document. No replay stream or
manifest byte, save compatibility, determinism input, or threading changes:
every deleted statement is proven to have no effect in a reachable state.

## Acceptance criteria

- `mbReplaying` is assigned at four sites, none at record start;
  `miReplayInitialTick` and `StagedReplayReader::record` no longer exist
  (the latter unless item 6 was left out and reported).
- The `## Replay Streams` playback-flag bullet names no clear that the code
  no longer has.
- The BrokenEngineSandbox client and server build without errors or new
  warnings.
- Live, through `/agent-harness`, the replay determinism check: record a
  replay on a server with simulated activity, stop it, play it back through
  at least one full loop, and confirm no checksum mismatch or playback abort
  is logged; then start a second recording after playback ends. The analysis
  recorded no harness setup for this; the executing session uses the
  skill's replay commands.

## Coordination

No mandatory nondirectional coordination.
