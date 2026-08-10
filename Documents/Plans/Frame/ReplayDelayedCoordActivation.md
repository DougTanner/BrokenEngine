<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-09T23:27:42.520Z","dependsOn":[]} -->
# Replay delayed destination activation before fixed-tick dispatch

## Context

The runtime acceptance for the claimed `Documents/Plans/Frame/ReduceCollectionHeader.md` Plan used the replay transfer fixture's A–E matrix. A and B passed. C required the delayed destination `[1,0]` to be absent through the recording event tick, active at the writer-input tick, and to contain exactly one replayed player with `playbackEventTick == writerInputTick`; C failed, so D and E did not run. The user explicitly revised the header Plan's acceptance to exclude this unrelated replay failure and preserve it as follow-up work.

The failure is reproducible from current source:

- `GameSaveLoad::ActivateReplayReader` (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:349-360`) creates the delayed destination frame, installs its initial input, and registers its reader, but does not add the coordinate to `game::gpGame->mActiveCoords`.
- `GameBase::ServerUpdate` (`Engine/Source/GameBase.cpp:181-227`) prepares the replay active set once, caches that vector for the fixed-tick loop, and dispatches every tick from the cached view. With the observed `FullTicks:2/3` timing, activation occurs at tick 8 and the transfer difference at tick 9 while dispatch still contains only `[0,0]`; the destination frame swaps without simulation and is then retired/reloaded at tick 10.
- `GameSaveLoad::SyncReplayTick` sets `iPlaybackEventTick` only while applying the transfer difference (`Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp:1081-1085`), while replay reload restores the capture metadata without that field (`:770-790`), making the marker transient across the loop.

The replay manifest and coordinate stream contain the delayed activation and tick-9 difference, and the acceptance logs show no replay-read, checksum, CRC, or desynchronization error. The defect is therefore the unresolved activation/dispatch lifecycle, not corrupt replay data.

## Design

Repair the replay activation boundary without changing the recorded replay format. After `SyncReplayTick` changes the live reader set, the replay active-coordinate view must be refreshed before `BuildAndDispatchFrameTicks` for that same fixed tick; `ServerUpdate` must not reuse a replay-only active view across multiple `FullTicks` iterations. The activated coordinate must have its prepared current/next frames and input before dispatch, must execute `RunFrameTick` exactly once at each recorded tick, and must remain active until its terminal reader tick is consumed. Normal non-replay active-set preparation and the existing five-phase `RunFrameTick` order remain unchanged.

Carry the successful `iPlaybackEventTick` capture through the in-memory replay reload restoration so `replay_transfer_capture` can observe the tick-9 event after the loop boundary; a later transfer event still overwrites the marker. Do not add a replay-file field or alter manifest, difference-stream, checksum, transfer-payload, `Frame::kiVersion`, or CRC encoding.

## Critical files

- `Engine/Source/GameBase.cpp` — `GameBase::ServerUpdate` and the replay path's active-coordinate input to `BuildAndDispatchFrameTicks`.
- `Projects/BrokenEngineSandbox/Source/Save/GameSaveLoad.cpp` — `ActivateReplayReader`, `SyncReplayTick`, and replay-load capture-metadata restoration.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness.md` — read-only replay transfer fixture and determinism acceptance contract.

## In scope

- Refreshing replay active-coordinate membership at the `SyncReplayTick` to fixed-tick dispatch boundary, including delayed-reader activation during a multi-tick `ServerUpdate`.
- Preserving the in-memory playback transfer marker through replay reload and relatching it on the next observed transfer.
- The existing replay transfer fixture's delayed player activation, multi-event transfer, terminal retirement, and replay-loop behavior needed to prove the above.

## Out of scope

- `Documents/Plans/Frame/ReduceCollectionHeader.md`, its `Collection.h` relocation, `CollectionLifecycle.h`, and project/filter membership changes.
- Replay manifest, grid, metadata, difference-stream, checksum, transfer-payload, wire, or `Frame::kiVersion` format changes; CRC algorithm or field-order changes.
- Transfer semantics, entity gameplay behavior, client reconciliation replay, normal non-replay subscription active-set policy, or unrelated save/load persistence.
- Changes to `RunFrameTick` phase order, simulation math, replay input ordering, or terminal reader ownership beyond the activation/dispatch lifecycle.
- Unit tests or unrelated harness commands.

## Risk tier and invariants

Change Workflow Tier 3 (invariant/integration). Triggers: save/replay determinism and fixed-tick dispatch across the Engine and game subsystems. Invariants:

- Replay formats and CRC encoding remain unchanged.
- Each active replay coordinate dispatches exactly once per recorded tick.
- Terminal readers retire only after their terminal reader tick is consumed.
- Normal non-replay active-set preparation and `RunFrameTick` phase ordering remain unchanged.

## Acceptance criteria

- `/agent-harness` runs the documented replay transfer fixture matrix. During playback, the destination is absent through event tick `E`, active at `E + 1`, contains exactly one player, and reports `playbackEventTick == writerInputTick`; the reproduction covers a batched `FullTicks > 1` update (including the observed 2/3 case) or equivalent evidence that activation precedes same-update dispatch.
- The multi-event variant replays one player, spaceship, blaster, and missile at the destination with matching capture counts and destination collection queries, without missing or duplicate entities.
- After destination-player destruction, its recorded terminal boundary retires cleanly and the replay reaches `End replay <tick>, looping`; no replay-reader terminal, advanced-beyond-terminal, checksum-mismatch, `LogDifferences CRC Client`, `CONFIRMED DESYNC`, or `SaveLoadReplay aborted` line is newly logged.
- The general replay determinism acceptance still passes with matching per-tick CRC, and client and server builds complete through `/compile`.

## Coordination

This Plan has no directional dependencies. It is independent of the originating Collection relocation because it changes only replay/save and fixed-tick dispatch paths; no reciprocal coordination edit is required.

## Notes

Origin: runtime acceptance of `Documents/Plans/Frame/ReduceCollectionHeader.md` on 2026-08-09; the user selected the separate-follow-up decision after C failed. This is ordinary pre-existing/out-of-scope debt, not tooling friction. The session baseline `388ed4033795f5ec14912fe6a887aeea2ab4e6bb` has no changes in `GameSaveLoad.cpp` or `GameBase.cpp`; the current session changes are limited to the approved Collection relocation and its client/server project/filter entries.
