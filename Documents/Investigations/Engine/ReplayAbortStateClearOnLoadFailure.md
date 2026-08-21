# Replay load-failure abort clears only part of the aborted state

Findings record. Not decision-complete: the correct fix depends on answers this
document does not have, so it stays here until the decision exists.

## Observed gap

`engine::Replay::SaveLoadReplay` loads a replay by staging readers, then
replacing live state, then publishing that playback is running. Its exception
handler decides how much state to discard from a single flag:

- `Engine/Source/File/Replay.cpp:705-718` — the `catch` logs
  `SaveLoadReplay aborted: corrupt replay data`, then calls
  `ClearReplayAbortState()` when `mrGameBase.mbReplaying` is already true and
  `ClearReplayTransientState()` otherwise.
- `Replay.cpp:274-289` — `ClearReplayTransientState` discards only replay-owned
  readers and republishes the playback flag. `ClearReplayAbortState` calls that
  and additionally clears
  `game::gpServerSession->mpTransferManager->mTransfers` and
  `game::gpServerSession->mpBroadcaster->mBroadcastStatusChanges`.

Live state is replaced before the flag is published: `game::gpGame->Reset()` and
`engine::AdoptGridSave` run at `Replay.cpp:661-662`, while
`PublishReplayingState()` — the only thing that sets `mbReplaying` — runs at
`Replay.cpp:676`, after the reader-activation loop at `Replay.cpp:665-675`. An
exception thrown inside that window takes the transient branch even though the
pre-load live game has already been torn down, so any staged transfer or
unpublished broadcast status change belonging to the replaced game would survive
against freshly replaced state.

Pre-existing, not a regression: the identical conditional exists at the session
baseline in
`git show 8f4cd35:Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp`
(catch handler, lines 803-813). The `ReplayLifecycleToEngine` session moved it
byte-identically into `Engine/Source/File/Replay.cpp` under a
behavior-identical-extraction contract, so fixing it was out of that session's
scope. The line numbers above are the post-landing tree.

Source: `/repo-code-review` finding during the `ReplayLifecycleToEngine`
session, adjudicated pre-existing and out of scope there. The failure scenario
was reasoned from the code, never reproduced at runtime.

## Why the decision is open

The reviewer's original proposal — call `ClearReplayAbortState()`
unconditionally — is not obviously safe. The same `catch` handler also covers
failures that occur *before* any live state is replaced: the load path throws
`common::CorruptStreamException` at `Replay.cpp:606`, `:613`, `:627`, `:637`,
`:645`, `:650`, and `:655`, all while the live game is still running untouched.
On those paths, clearing `mpTransferManager->mTransfers` and
`mpBroadcaster->mBroadcastStatusChanges` would discard healthy live staging — a
new defect traded for the old one.

So the real design space is how to distinguish "state replacement has begun"
from `mbReplaying`, and that cannot be settled without the answers below.

## Open questions

1. Can an exception actually be thrown between the live-state replacement
   (`Replay.cpp:661-662`) and `PublishReplayingState()` (`Replay.cpp:676`)?
   Reader activation (`ActivateReplayReader`) is the only code in that window.
2. Can `mpTransferManager->mTransfers` or
   `mpBroadcaster->mBroadcastStatusChanges` hold entries at that moment, and are
   surviving entries actually harmful — for example a stale transfer or status
   change applied to, or broadcast for, a coord or identifier that no longer
   means what it did before the reset?
3. If harmful, does clearing them also have to happen for the pre-replacement
   throw sites, or must those keep the live game's staging intact?

## Candidate outcomes

- **No change.** If the window in question 1 is unreachable, or the surviving
  entries in question 2 are provably harmless, record the finding and stop
  rather than hardening defensively.
- **Unconditional full cleanup.** Drop the `mbReplaying` conditional and always
  call `ClearReplayAbortState()`. Simplest, but only correct if clearing the two
  containers is proven harmless for every pre-replacement throw site listed
  above.
- **Replacement-began flag.** Branch on a local flag set immediately after
  `AdoptGridSave` instead of on `mbReplaying`. Precisely matches the condition
  that actually matters, at the cost of one more piece of load-path state.

## Invariants any fix must preserve

- An aborted replay load leaves the process able to continue as a live game:
  current frames, inputs, and the active coord set stay intact
  (`Replay.cpp:285`).
- A failure before any live state is replaced must not discard state the still
  running live game needs.
- `mbReplaying` remains the single engine-owned published answer to whether
  playback is running; no second source of truth for it.
- No determinism/CRC-relevant state is written from the failure path.

## Files involved

- `Engine/Source/File/Replay.cpp` — `SaveLoadReplay` load path and its `catch`
  handler (`:590-718`), `ClearReplayTransientState` and `ClearReplayAbortState`
  (`:274-289`), `PublishReplayingState` (`:269-272`).

## If this earns a Plan

Expected Change Workflow Tier 3: the replay abort path is save/replay-lifecycle
behavior and the cleared containers are server transfer and broadcast staging
that feeds the wire, both excluded from Tier 2. Verification would need the
existing record/stop/playback/abort replay harness scenario plus a server build.
