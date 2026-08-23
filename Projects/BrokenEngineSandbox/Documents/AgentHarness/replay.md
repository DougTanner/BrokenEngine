# BrokenEngineSandbox Replay Verification

[Back to AgentHarness hub](../AgentHarness.md)

### Replay determinism acceptance

Run replay acceptance only on a `kbDebugInput` build. It must prove transitions and a completed playback loop, not merely the absence of old errors:

1. Set the server `Default` log level to `Debug`. Capture server and client relevant-log baselines with `get_logs {"count":512,"pattern":"[Rr]eplay|CRC|[Cc]hecksum|[Dd]esync"}`. Preserve the ordered arrays so later checks can identify only appended lines.
2. To compare literal per-tick CRC values instead of only the absence of failures, also send `set_log_level {"category":"Replay","level":"Verbose"}` and run a single-coordinate replay: a freshly `reset` server with `status.activeCoords` length 1 and no `replay_transfer_fixture`. Scrape `get_logs {"category":"Replay"}` incrementally through recording and playback — that category ring holds 128 lines — then diff the `Checksum DifferenceStreamWriter Update` sequence against the `Checksum DifferenceStreamReader` sequence tick by tick. More than one coordinate invalidates the diff: the line text carries no coordinate, and emission order across coordinates is not stable between record and playback.
3. Send `pause {"paused":false}` and require `status.paused:false`, `recording:false`, and `replaying:false`.
4. Send `replay_record {"start":true}` and require a later status with `recording:true` and `paused:false`. Let multiple ticks complete.
5. Send `replay_record {"start":false}` and require a later status with `recording:false` and `paused:false`.
6. Send `replay_play`; require `replaying:true` and `paused:false`. Wait for a newly appended server line matching `End replay [0-9]+, looping`. This current runtime marker proves every reader reached its recorded endpoint and the first playback loop completed.
7. Compare post-run relevant logs with both ordered baselines. Require no newly appended replay-persistence/read failure, `LogDifferences CRC Client`, checksum-mismatch, `CONFIRMED DESYNC`, or unresolved-CRC error line. Do not count pre-baseline lines as new evidence, and do not treat speculative reconciliation messages alone as a replay failure.
8. Send `replay_play` again to cancel. Require a later status with `replaying:false`, `recording:false`, and `paused:false`.

If the 512-line relevant-log window cannot retain the baseline through this bounded scenario, rerun with file-offset evidence from the configured per-process logs; never downgrade to “silence = pass.”

### Replay transfer-capture fixture

Run this on the Debug server only (`kbDebugInput` is required). Before capturing the fixture baseline, send `set_log_level {"category":"Network","level":"Verbose"}` and inspect the returned `result.effective` level. When it is `Verbose`, use the Network-Verbose publication log as runtime evidence; on the Debug build it is clamped to `Debug`, so the kVerbose `BuildTickPublication` line is unavailable and its absence must not block semantic replay acceptance. In the clamped case, use the static source/phase trace for publication-at-`E`, with replay capture ticks, final state/count, checksum/no-desync, and the completed loop as runtime signals. Keep the effective setting for the baselines and the entire A–G matrix. It replaces the unavailable historical tick-39222 artifact with deterministic transfers. Keep a newly appended server-log slice for the whole fixture and reject any new `LogDifferences CRC Client`, checksum mismatch, `CONFIRMED DESYNC`, replay-reader error, or `SaveLoadReplay aborted` line.

`replay_transfer_fixture` is accepted during active recording, or only while the server is paused with an unconsumed `replay_record {"start":true}` transition. It rejects playback, ordinary non-recording use, an unpaused pending start, cancelled starts, invalid source frames, and invalid coordinates. Its optional Boolean `pauseAfterWriterInput` arms count `1` for a pending start. During active recording it snapshots the current writer-input count `N` and arms `N + 1`: after command drain, that update's `SyncReplayTick` records writer input `N + 1` for event tick `E`, then the same update's `FinalizeFrameTick` harvests the fixture at `E` and records it in that coord's post-dispatch channel at `E`, so the whole event is complete before the pause takes effect. Playback applies and publishes the transfer after dispatch at `E`, while the destination first enters the simulation dispatch set at `E + 1`. The `player` arm mints a synthetic player with an empty client GUID, so this fixture proves transfer state, ordering, CRC, and replay-reader behavior only; it cannot prove persistent ownership or GUID relinking. The result echoes the applied Boolean.

`replay_transfer_capture` exposes `firstWriterInputTick` and `writerInputCount` in addition to the event fields. Both tick fields and the event counts latch as described in the `replay_transfer_capture` command entry, so one snapshot describes one event only while recording; during playback `recordingEventTick` and the counts still describe the last recorded event while `playbackEventTick` sweeps the replayed event ticks, so a playback snapshot can pair fields from two different events: for a single-event recording at `E`, require `recordingEventTick == playbackEventTick == E`, and for a multi-event recording evaluate across polls instead — take a capture poll as each event is recorded and require its matching count to be `1` with the other counts zero, then during playback require the set of distinct observed `playbackEventTick` values to equal the set of recorded event ticks. `playbackEventTick` is a direct observation: the reader loads the post-dispatch record stored at exactly `E` and latches the tick it applied it on. If the setup command returned effective Network level `Verbose`, use a newly appended `ServerBroadcaster::BuildTickPublication Coord: (...) Tick: E StatusChanges: ...` line as runtime publication evidence. If the requested `Verbose` level was clamped to `Debug`, that kVerbose line is unavailable and must not be required; the static source/phase trace owns publication-at-`E` (post-dispatch record load and transfer staging at `E` before dispatch, normal dispatch at `E`, transfer apply/publication at `E` after that dispatch, then the destination's first dispatch at `E + 1`). In either case, a status or query response cannot prove historical timing; runtime acceptance uses the capture event ticks, final state/count, checksum/no-desync signals, and the completed replay loop. The count is one per advancing simulation tick with one or more nonterminal writer updates, never one per writer or coordinate; terminal flushes do not increment it.

For the focused active-recording pause scenario, begin with `writerInputCount == N > 1`, then queue a player fixture with `pauseAfterWriterInput:true`. Require one automatic pause at count `N + 1`, `recordingEventTick == E`, `playbackEventTick == E` after playback, and exactly one synthetic player transfer. Resume, allow a later writer input, and prove it does not pause again. Use the natural persistent-GUID scenario below for ownership and GUID assertions; do not attribute those assertions to this fixture.

Run the following A–G matrix. Keep a newly appended server-log slice for the entire matrix and reject any new `LogDifferences CRC Client`, checksum mismatch, `CONFIRMED DESYNC`, replay-reader error, or `SaveLoadReplay aborted` line.

A. Send `reset`, `pause {"paused":true}`, then `replay_record {"start":true}`. While still paused, queue `replay_transfer_fixture {"type":"player","source":[0,0],"destination":[1,0],"pauseAfterWriterInput":true}` and require its echoed Boolean. Unpause. Tick `E` starts recording and harvests the fixture; after the `E + 1` writer input, require automatic `status.paused:true`, `recordingEventTick == E`, `firstWriterInputTick == E + 1`, `writerInputCount == 1`, `playerCount == 1`, and every other transfer count zero. Treat the player as synthetic state only; no ownership or GUID conclusion is allowed.
B. While automatically paused after A, send `pause {"paused":false}` and require the server to resume recording. Keep recording for at least `engine::kiTickRate` advancing ticks beyond `recordingEventTick` (require `status.tick >= recordingEventTick + engine::kiTickRate`) before sending `replay_record {"start":false}`. Poll until `status.recording:false` before sending `replay_play`, then poll `status.replaying:true`.
C. During that playback, poll `replay_transfer_capture` until `playbackEventTick == recordingEventTick == E`. Immediately, before terminal reload, query `query_frame` and `query_players {"coord":[1,0]}`; require exactly one synthetic player, the matching destination player count, and the capture result's expected event tick and final transfer count. If the setup response reported effective Network `Verbose`, require the newly appended `ServerBroadcaster::BuildTickPublication` line for destination `[1,0]` at `Tick: E`; if it reported clamped `Debug`, do not fail on the missing line and use the static source/phase trace to establish publication at `E` and first simulation dispatch at `E + 1`. Do not infer either historical point from `status` or `query_players`. After those immediate observations, separately wait for a newly appended `End replay <tick>, looping` marker and scan the appended log slice for CRC/read/`CONFIRMED DESYNC` errors. This proves delayed activation, replayed transfer publication, and checksum validation; the marker confirms one completed loop while playback remains active.
D. Record a multi-event variant in this order: use the player fixture to create `[1,0]`, then queue one each of `spaceship`, `blaster`, and `missile` from `[0,0]` to `[1,0]`. As each event is recorded, poll `replay_transfer_capture` and require its matching count to be `1` and the other counts zero; during playback, require the set of distinct observed `playbackEventTick` values to equal the set of the four recorded event ticks. When effective Network `Verbose` is available, also require the matching `ServerBroadcaster::BuildTickPublication` line at the event tick; when it is clamped to `Debug`, use the static source/phase trace and do not require that line. Require the matching `query_collection` count at `[1,0]` after playback, no checksum/CRC/`CONFIRMED DESYNC` error, and the completed replay loop. The Blaster query is the deterministic tick-39222 regression signal; this matrix makes no ownership claim.
E. D ends in active playback and injection is rejected during replay, so first cancel that playback by sending `replay_play` again and polling `status.replaying:false`, then start a fresh recording over the still-live `[1,0]` D created: send `replay_record {"start":true}`, poll `recording:true`, and keep recording through advancing ticks so the retirement below lands mid-recording. Retire `[1,0]` during that fresh recording by reading its player `uuid`, sending `inject_status_changes {"changes":[{"coord":[1,0],"type":"DestroyPlayer","playerUuid":<uuid>}]}`, and waiting for it to leave `status.activeCoords`. Stop, poll `recording:false`, then issue `replay_play`. The destination must retire at its recorded terminal boundary without missing/duplicate collections, `Replay reader terminal data was not fully consumed`, or `advanced beyond terminal`; require an `End replay <tick>, looping` marker.
F. Abort a pending start: `reset`, pause, request recording, queue the player fixture, then send `replay_record {"start":false}` before unpausing. After unpausing, require no `[1,0]` destination, no transferred entity, and no recording event in `replay_transfer_capture`.
G. Abort an injected start failure: `reset`, pause, arm `replay_inject_persistence_failure {"stage":"invalidation"}`, request recording, and queue the player fixture. Unpause. Require no `[1,0]` destination, no transferred entity, and no recording event in `replay_transfer_capture`.

### Natural persistent-GUID ownership and cell crossing

The A–G matrix deliberately uses a synthetic player with an empty client GUID.
Run this separate scenario when acceptance needs ownership or GUID relinking.
It uses a real connected client, the ordinary fleet spawn path, and a natural
`UpdateFleet` cell crossing; the fixture above is not a substitute.

1. Start the server and client with a fresh per-scenario AppData root and
   complete the handshake. If this is a separate launch from the A–G matrix,
   repeat `set_log_level {"category":"Network","level":"Verbose"}` before
   the baseline; otherwise inherit the fixture's setting. Inspect the returned
   `result.effective`: when
   it is `Verbose`, publication logs are available; when it is clamped to
   `Debug`, the kVerbose publication line is unavailable and its absence is not
   a failure. Keep the client connected for the entire recording and playback.
2. Use `describe_ui`, then click `[+]##Fleet` and wait for the fleet count to
   increase. Click `[+]##Player` and wait for the new fleet member plus the
   ordinary `ServerSession::SendPlayerState State: Spawned` line. Record the
   player's `uuid` and `globalId` from
   `query_players {"coord":[0,0]}`; this player came through the client-owned
   spawn path and carries the client's persistent GUID.
3. Start `replay_record {"start":true}`. Confirm the source subscription, and
   capture the current `describe_scene.subscribedCoords` plus newly appended
   `Server::ClientSubscribe` lines so the destination subscription change can be
   distinguished from the later transfer.
4. Queue the ordinary status path with
   `inject_status_changes {"changes":[{"coord":[0,0],"type":"UpdateFleet","playerUuid":<uuid>,"fleetWantedCoord":[1,0],"isFlagship":true}]}`.
   Wait for the same `globalId` to appear at `[1,0]` in `query_players`; this is
   a present-state check, not historical tick evidence.
5. Require the newly appended
   `ServerSession::SendPlayerState State: ChangedFrame ... GlobalPlayer:<globalId> Grid: (1,0)`
   line and the destination subscription authorization (`Server::ClientSubscribe`
   for `[1,0]` or `[1,0]` present in the client's `describe_scene.subscribedCoords`).
   Confirm the destination row still has the same `globalId`; the ChangedFrame
   plus subscription evidence proves the real client's ownership relink.
6. Stop recording, start playback, and require the normal replay loop plus no
   new CRC, checksum, reader, or confirmed-desync errors. For any captured natural
   transfer at event `E`, use the Network-Verbose
   `ServerBroadcaster::BuildTickPublication` line at `E` when the effective level
   is `Verbose`; when it is clamped to `Debug`, use the static source/phase trace
   — post-dispatch record load and transfer staging at `E` before dispatch, the
   active-set dispatch for `E`, transfer apply and publication at `E` after that
   dispatch, first destination dispatch at `E + 1` — without requiring the absent line. In either case, use replay capture event ticks,
   final state/count, checksum/no-desync, and the completed loop as runtime
   signals; do not claim that a status or query response observed either
   historical event.

#### Replay manifest v3 integrity matrix

This is a stopped-server AppData test, not an agent command. Produce the valid multi-coordinate fixture above and prove one loop. Stop/release the server, back up the complete `F7.replay.manifest`, `.grid`, `.meta`, and every coord sibling from `<absolute app-data root>\Broken Engine Sandbox Server`; restore the backup before each change and relaunch. The v3 manifest is fixed-width little-endian: version, initial tick, activation count and `(activationTick,x,y)` records; a `hasFullFrames` byte; inventory count and ordered `(kind,coordKey,byteCount,sha256[32])` entries; then the 32-byte generation digest. Kinds are grid/meta/header/frames/checksums/fullframes = 0..5. The SHA-256 preimage is, in order, the four little-endian length bytes `2B 00 00 00`, the 43 bytes of `broken-engine/replay-manifest-generation/v3` without a NUL, then the exact manifest semantic payload bytes from version through inventory.

Every scenario iteration is one loop: release, stage the files on disk while the server is stopped, re-claim, relaunch, observe. Each re-claim mints a fresh owner token and invalidates the previous one, so a released token is never reused. Retain the current claim response's `owner` as the only valid owner and substitute that literal token in every socket command, `Wait-HarnessPing.ps1`, `Wait-IslandSceneReady.ps1`, and release call; never use an owner from an earlier claim. A stale token hard-stops on owner mismatch and reads like a scenario failure. The claim invocation is itself the wait for a foreign holder, so add no polling of your own around the re-claim.

Under that same backup-and-restore discipline, staging a byte-modified artifact the reader still accepts is a manifest repair in a fixed order. First self-check the repair tool: recompute the generation digest of the untouched manifest by the preimage above and require it to equal the stored trailing 32 bytes, because otherwise a repair bug is indistinguishable from the corrupt-data rejection under test. The fixed widths are int64 for version, initial tick, activation count and inventory count, 16 bytes per activation record (int64 `activationTick`, int32 `x`, int32 `y`), one byte for `hasFullFrames`, and 49 bytes per inventory entry (uint8 kind, uint64 `coordKey`, int64 `byteCount`, 32 raw SHA-256 bytes), so inventory entry `i` begins at byte `24 + 16 * activationCount + 9 + 49 * i`. Entries are sorted ascending by `(kind,coordKey)`, and `ToKey` packs `x` in the high 32 bits and `y` in the low, so `[0,0]` is key `0`; kinds 0..5 name `F7.replay.grid`, `F7.replay.meta`, `F7.replay.<coordKey>`, `F7.replay.<coordKey>.frames`, `F7.replay.<coordKey>.checksums`, and `F7.replay.<coordKey>.fullframes`. Then modify the artifact file, and rewrite only that artifact's entry in place at its existing offset, leaving inventory order untouched: `byteCount` is the modified file's size in bytes, and its digest is a plain SHA-256 over the file's raw bytes with no domain prefix — unlike the generation digest, which does carry the `2B 00 00 00` and 43-byte label prefix above. Recompute the generation digest over the amended payload from version through inventory last, and overwrite the trailing 32 bytes with it.

For every rejection case, pause immediately after relaunch, record `status.activeCoords` and the live ID state, send `replay_play`, and require `status.replaying:false` with unchanged live state (no adoption). Corruption cases require a new `SaveLoadReplay aborted: corrupt replay data:` line.

H1. Change version to `2`: reject with a new `Replay manifest version ...` line; restore v3 unchanged and require a loop as the control.
H2. Change one byte or byte count of each inventory kind; then separately remove, replace with a directory, or replace with a reparse point each named artifact. Reject before grid/meta/stream parsing.
H3. Change the generation digest and reject. For the binary coordinate-identity case, change a kind-2 coord-header entry from `[1,0]` to unrecorded `[2,0]` (little-endian `ToKey` bytes `00 00 00 00 02 00 00 00`), preserve inventory ordering, and recompute the generation digest; require `ReplayManifest inventory identity` to corrupt-abort before adoption. Separately test invalid fullframes flag, unknown kind, reordered inventory, duplicate inventory entry, missing/surplus entry, negative count/size, and trailing data. Reject each.
H4. On Debug, kind-5 `.fullframes` holds complete frame snapshots distinct from the kind-3 `.frames` input-difference stream; require one kind-5 fullframes manifest entry for each recorded coord's kind-2 header, and require every named ordinary fullframes file to match its manifest size/hash identity. A clean replay control must prove the debug reader consumes the snapshots without CRC, desync, or read errors. On non-Debug, static inspection proves `kbReplayFullFrames == false` and v3 has no kind-5 entry. Do not enable runtime Release diagnostics.
H5. While recording with no special coord requirement, arm `replay_inject_persistence_failure {"stage":"inventory"}`, stop, then call `replay_play`: it must fail with no valid manifest/adoption. This verifies manifest-last publication.
H6. These procedures do not claim authentication or protection against files changed after preflight.

## Durable caveats

- Injection during replay fails. Injection while clients wait for spawn also fails immediately so it cannot corrupt snapshot-diff assignment; it is never queued for later. Injection while paused remains accepted with `deferred:true` and applies on the next unpaused tick.
