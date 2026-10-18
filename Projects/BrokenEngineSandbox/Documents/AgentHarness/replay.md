# BrokenEngineSandbox Replay Verification

[Back to AgentHarness hub](../AgentHarness.md)

### Replay determinism acceptance

Run replay acceptance only on a `kbDebugInput` build. It must prove transitions and a completed playback loop, not merely the absence of old errors:

1. Set the server `Default` log level to `Debug`. Capture server and client relevant-log baselines with `get_logs {"count":512,"pattern":"[Rr]eplay|CRC|[Cc]hecksum|[Dd]esync"}`. Preserve the ordered arrays so later checks can identify only appended lines.
2. To compare literal per-tick CRC values instead of only the absence of
   failures, also send `set_log_level {"category":"Replay","level":"Verbose"}`
   and run a single-coordinate replay: a freshly `reset` server with
   `status.activeCoords` length 1, no `inject_payload` spawn entry, and no cell
   crossing. Scrape
   `get_logs {"category":"Replay"}` incrementally through recording and playback
   — that category ring holds 128 lines — then diff the
   `Checksum DifferenceStreamWriter Update` sequence against the
   `Checksum DifferenceStreamReader` sequence tick by tick. More than one
   coordinate invalidates the diff: the line text carries no coordinate, and
   emission order across coordinates is not stable between record and playback.
3. Send `pause {"paused":false}` and require `status.paused:false`, `recording:false`, and `replaying:false`.
4. Send `replay_record {"start":true}` and require a later status with `recording:true` and `paused:false`. Let multiple ticks complete.
5. Send `replay_record {"start":false}` and require a later status with `recording:false` and `paused:false`.
6. Send `replay_play`; require `replaying:true` and `paused:false`. Wait for a newly appended server line matching `End replay [0-9]+, looping`. This current runtime marker proves every reader reached its recorded endpoint and the first playback loop completed.
7. Compare post-run relevant logs with both ordered baselines. Require no newly appended replay-persistence/read failure, `LogDifferences CRC Client`, checksum-mismatch, `CONFIRMED DESYNC`, or unresolved-CRC error line. Do not count pre-baseline lines as new evidence, and do not treat speculative reconciliation messages alone as a replay failure.
8. Send `replay_play` again to cancel. Require a later status with `replaying:false`, `recording:false`, and `paused:false`.

If the 512-line relevant-log window cannot retain the baseline through this bounded scenario, rerun with file-offset evidence from the configured per-process logs; never downgrade to “silence = pass.”

### Replay transfer-capture harness rig

Run this on the Debug server only (`kbDebugInput` is required). Before capturing
the harness rig baseline, send
`set_log_level {"category":"Network","level":"Verbose"}` and inspect the
returned `result.effective` level. When it is `Verbose`, use the Network-Verbose
publication log as runtime evidence; when it is clamped to `Debug`, the kVerbose
`BuildTickPublication` line is unavailable and its absence must not block
semantic replay acceptance. In the clamped case, use the static source/phase
trace for publication-at-`E`, with replay capture ticks, final state/count,
checksum/no-desync, and the completed loop as runtime signals. Keep the
effective setting for the baselines and the entire A–G matrix. It replaces the
unavailable historical tick-39222 artifact with deterministic spawns and
transfers. Keep a
newly appended server-log slice for the whole harness rig and reject any new
`LogDifferences CRC Client`, checksum mismatch, `CONFIRMED DESYNC`,
replay-reader error, or `SaveLoadReplay aborted` line.

The harness rig is an `inject_payload` batch of spawn entries (`SpawnOmni`,
`SpawnSpaceship`, `SpawnBlaster`, `SpawnMissile`, or `SpawnExplosion`) at an
active cell with no `tick`, so each is applied right after the next tick and is
that tick's post-dispatch event, counted under its collection. A case that
needs a newly created destination instead uses the edge-coast `SpawnOmni` of
the `inject_payload` entry in [commands-server.md](commands-server.md), whose
cell crossing is the destination's event: the crossing harvested at `E` is
published at `E`, while the destination first enters the simulation dispatch
set at `E + 1`. `pauseAfterWriterInput:true` is accepted during active
recording, or only while the server is paused with an unconsumed
`replay_record {"start":true}` transition; it rejects an unpaused pending
start, cancelled starts, and a batch with no spawn entry or with a `tick`
entry, and playback rejects every injection. It arms count `1` for a pending
start. During active recording it snapshots the current writer-input count `N`
and arms `N + 1`: after command drain, that update's `SyncReplayTick` records
writer input `N + 1` for event tick `E`, then the same update's
`FinalizeFrameTick` harvests the post-tick batch at `E`, applies the spawn, and
records it in that coord's post-dispatch channel at `E`, so the whole event is
complete before the pause takes effect. Playback applies and publishes the
spawn after dispatch at `E`. A `SpawnOmni` entry mints a synthetic omni with an
empty client GUID, so this harness rig proves spawn and transfer state,
ordering, CRC, and replay-reader behavior only; it cannot prove persistent
ownership or GUID relinking. The result echoes the applied Boolean.

`replay_transfer_capture` exposes `firstWriterInputTick` and `writerInputCount`
beside an `events` array holding one entry per recording event tick in capture
order, as described in the `replay_transfer_capture` command entry. A
single-event recording at `E` has exactly one entry with
`recordingEventTick == E`, and once playback applies that entry's record its
`playbackEventTick == E`. Check a multi-event recording with a single read after
recording stops: require one entry per recorded event in capture order, and
during playback require every entry's `playbackEventTick` to equal its
`recordingEventTick`. If the setup command returned effective Network level
`Verbose`, use a newly appended
`ServerBroadcaster::BuildTickPublication Coord: (...) Tick: E StatusChanges: ...`
line as runtime publication evidence. If the requested `Verbose` level was
clamped to `Debug`, that kVerbose line is unavailable and must not be required;
the static source/phase trace owns publication-at-`E` (post-dispatch record load
and batch staging at `E` before dispatch, normal dispatch at `E`, batch
apply/publication at `E` after that dispatch, then, for a newly created
destination, its first dispatch at `E + 1`). In either case, a status or query response cannot prove
historical timing; runtime acceptance uses the capture event ticks, final
state/count, checksum/no-desync signals, and the completed replay loop.
`writerInputCount` is one per advancing simulation tick with one or more
nonterminal writer updates, never one per writer or coordinate; terminal flushes
do not increment it.

For the focused active-recording pause scenario, begin with
`writerInputCount == N > 1`, then send a `SpawnOmni` entry with
`pauseAfterWriterInput:true`. Require one automatic pause at count `N + 1`,
exactly one `events` entry with `recordingEventTick == E` whose
`playbackEventTick == E` after playback, and exactly one synthetic omni
spawn. Resume, allow a later writer input, and prove it does not pause again.
Use the natural persistent-GUID scenario below for ownership and GUID
assertions; do not attribute those assertions to this harness rig.

Run the following A–G matrix. Keep a newly appended server-log slice for the entire matrix and reject any new `LogDifferences CRC Client`, checksum mismatch, `CONFIRMED DESYNC`, replay-reader error, or `SaveLoadReplay aborted` line.
Run it with no client connected: a client's fleet spawns join the post-tick
batch at the origin `[0,0]` and count under `omniCount`, which breaks the
per-case counts below.

A. Send `reset`, `pause {"paused":true}`, then `replay_record {"start":true}`.
While still paused, send
`inject_payload {"entries":[{"coord":[0,0],"type":"SpawnOmni"}],"pauseAfterWriterInput":true}`
and require its echoed Boolean. Unpause. Tick `E` starts recording and applies
the spawn after its dispatch; after the `E + 1` writer input, require automatic
`status.paused:true`, `firstWriterInputTick == E + 1`, `writerInputCount == 1`,
and exactly one `events` entry with `recordingEventTick == E`,
`omniCount == 1`, and every other count zero. Treat the omni as synthetic state
only; no ownership or GUID conclusion is allowed.
B. While automatically paused after A, send `pause {"paused":false}` and require the server to resume recording. Keep recording for at least `engine::kiTickRate` advancing ticks beyond the `recordingEventTick` of that recording's `events` entry selected by `recordingEventTick == E` (require `status.tick >= recordingEventTick + engine::kiTickRate`) before sending `replay_record {"start":false}`. Poll until `status.recording:false` before sending `replay_play`, then poll `status.replaying:true`.
C. During that playback, poll `replay_transfer_capture` until that recording's
`events` entry selected by `recordingEventTick == E` has
`playbackEventTick == E`. Immediately, before terminal reload, query
`query_frame` and `query_omnis {"coord":[0,0]}`; require exactly one synthetic
omni, the matching omni count, and that entry's expected event tick and final
count. If the setup response reported effective Network `Verbose`, require the
newly appended `ServerBroadcaster::BuildTickPublication` line for `[0,0]` at
`Tick: E`; if it reported clamped `Debug`, do not fail on the missing line and
use the static source/phase trace to establish publication at `E`. Do not infer
that historical point from `status` or `query_omnis`. After those immediate
observations, separately wait for a newly appended `End replay <tick>, looping`
marker and scan the appended log slice for CRC/read/`CONFIRMED DESYNC` errors.
This proves replayed spawn publication and checksum validation; the marker
confirms one completed loop while playback remains active.
D. First choose the D blaster's `vecPosition` in `[0,0]` and confirm it
terrain-clear in a probe that records nothing, because the command makes no
terrain check and a blaster destroys itself on terrain contact. C ends in active
playback, so cancel it by sending `replay_play` again and polling
`status.replaying:false`. Send `reset`, read `status.tick`, and send a
`SpawnBlaster` entry into `[0,0]` at the chosen `vecPosition` with `tick:T` for
some `T` above it, so `T` is the tick it is applied after. Pause once
`status.tick >= T + 1`, then require a
`query_collection {"coord":[0,0],"collection":"blasters"}` row whose `local` x
and y equal `vecPosition + vecVelocity * (status.tick - T) / 32` within 0.05 m
(`vecVelocity` defaults to `[1,0]`); if none matches, choose another position
and repeat. Send `reset` again, so no probe blaster is present when D records,
and unpause. Then record a multi-event variant in this order: send the
edge-coast `SpawnOmni` entry into `[0,0]` and wait until its crossing creates
`[1,0]` (`[1,0]` appears in `status.activeCoords`), then send one each of
`SpawnSpaceship`, `SpawnBlaster` at the probed `vecPosition`, and
`SpawnMissile` into `[0,0]`. Send each entry only after the previous event has
been applied, confirmed by an advanced `status.tick`; entries released for the
same tick join one batch and merge into a single capture entry. After recording
stops, one `replay_transfer_capture` read must return five `events` entries in
capture order: the omni spawn and then its crossing, each with `omniCount` `1`,
then one each with `spaceshipCount`, `blasterCount`, and `missileCount` `1`,
every other count zero; during playback, poll `replay_transfer_capture` until
every entry's `playbackEventTick` equals its `recordingEventTick`. When
effective Network `Verbose` is available, also require the matching
`ServerBroadcaster::BuildTickPublication` line at each event tick; when it is
clamped to `Debug`, use the static source/phase trace and do not require that
line. The crossing proves delayed activation: `[1,0]` is published at its event
tick and first dispatched one tick later. Require the matching `query_frame`
counts at `[0,0]` and the crossed omni at `[1,0]` after playback, no
checksum/CRC/`CONFIRMED DESYNC` error, and the completed replay loop. The
Blaster query is the deterministic tick-39222 regression signal; this matrix
makes no ownership claim.
E. D ends in active playback and injection is rejected during replay, so first
cancel that playback by sending `replay_play` again and polling
`status.replaying:false`, then start a fresh recording over the still-live
`[1,0]` D created: send `replay_record {"start":true}`, poll `recording:true`,
and keep recording through advancing ticks so the retirement below lands
mid-recording. Retire `[1,0]` during that fresh recording by reading its omni
`uuid`, sending
`inject_payload {"entries":[{"coord":[1,0],"type":"DestroyOmni","iOmniUuid":<uuid>}]}`,
and waiting for it to leave `status.activeCoords`. Stop, poll `recording:false`,
then issue `replay_play`. The destination must retire at its recorded terminal
boundary without missing/duplicate collections,
`Replay reader terminal data was not fully consumed`, or
`advanced beyond terminal`; require an `End replay <tick>, looping` marker.
F. Abort a pending start: `reset`, pause, request recording, send the `SpawnOmni` entry of A, then send `replay_record {"start":false}` before unpausing. After unpausing, require `status.recording:false`, no recording event in `replay_transfer_capture`, and the spawned omni live at `[0,0]` (its minted global id in `query_omnis`), because cancelling a start drops no queued entry.
G. Abort an injected start failure: `reset`, pause, arm `replay_inject_persistence_failure {"stage":"invalidation"}`, request recording, and send the `SpawnOmni` entry of A. Unpause. Require `status.recording:false`, no recording event in `replay_transfer_capture`, and the spawned omni live at `[0,0]`, because a recording-start failure drops no queued entry.

### Natural persistent-GUID ownership and cell crossing

The A–G matrix deliberately uses a synthetic omni with an empty client GUID.
Run this separate scenario for ownership, GUID relinking, repeated coordinate
generations, and active-subscription recovery. Recording must park the viewer
away from a coordinate long enough to retire it and create a later generation.
Playback must do the opposite: subscribe the viewer to A before playback and
leave that slot continuously subscribed through A's recorded absent interval.
Parking during playback bypasses the restarted-stream behavior this scenario
must prove.

1. Start the server and client with a fresh per-scenario AppData root and
   complete the handshake. Set Network logging to `Verbose` and inspect
   `result.effective`. Also send the server
   `set_log_level {"category":"Default","level":"Debug"}` so the step-8
   `ReadGrid` and `End replay` markers and the step-10
   `ServerSessionBase::ResetForLoad` line emit. Then capture ordered
   client/server replay, CRC, checksum, desync, `ChangedCell`, subscribe, and
   publication log baselines. When Network is clamped to `Debug`, record that
   result and do not require unavailable kVerbose publication lines. Keep the
   client connected through recording and playback.
2. Use `describe_ui`, click `[+]##Fleet`, then click `[+]##Omni`. Wait for the
   fleet member and the ordinary `ServerSessionBase::SendOwnedEntityState State: Spawned`
   line. Read `status.activeCoords`, query each current coordinate with
   `query_omnis`, and require exactly one Omni overall. Record its
   `globalId` and current `uuid`, then wait, injecting nothing, for its first
   `ServerSessionBase::SendOwnedEntityState State: ChangedCell ... GlobalId: <globalId>`
   line; call that line's cell W. The server fleet timer moves a new flagship
   once within 60 s of spawn, and afterwards only while the flagship sits in W.
   Pick the unit step s along a nonzero axis of W that points away from origin,
   and set A=W+s and B=W+2s. Send the W→A injection under step 3 within 50 s of
   the W line (the crossing itself need not complete in that window), wait for
   its OwnedEntity `ChangedCell` line naming A, and only then start step 4. Never
   route the Omni back into W. A and B are never origin, because the server
   keeps origin active every tick and it cannot retire. If, after the W line and
   through step 7, any OwnedEntity `ChangedCell` line names a cell the harness did
   not request, restart from a fresh server. This discovers the runtime state
   instead of assuming where the Omni spawned or settled; the proven setup
   used A=`[2,0]`, B=`[3,0]`, and `globalId:1`, which fits W=`[1,0]`.
3. Immediately before every crossing, query the source coordinate again for that `globalId` and use its current per-cell `uuid` in `inject_payload {"entries":[{"coord":<source>,"type":"UpdateFleet","iOmniUuid":<fresh-uuid>,"fleetWantedCoordinate":<destination>,"bIsFlagship":true}]}`. An Omni receives a new cell-local UUID after crossing, so a previously captured UUID is not reusable.
4. Start recording and poll `status.recording:true`. Confirm A is subscribed, and keep the current client `describe_scene.subscribedCoords` plus newly appended `Server::ClientSubscribe` lines as the subscription baseline. The exact initial activation tick `A1` comes from the stopped manifest in step 11; a later status poll does not establish it.
5. Move A→B using the fresh UUID rule. Require the same `globalId` at B and
   exactly one new
   `ServerSessionBase::SendOwnedEntityState State: ChangedCell ... GlobalId: <globalId> Grid: (<B>)`
   line; its log tick is `E1`. During recording only, send the client
   `set_client_grid_coord` once to a parking coordinate P whose 3x3 ring
   contains neither A, B, nor origin; the command pins the viewer at P, so
   auto-follow cannot return it to the Omni, and the pin stays there through
   step 7. Wait until `describe_scene.subscribedCoords` omits A, the server logs
   `Server::ClientUnsubscribe` for A, and A leaves `status.activeCoords`. This
   closes generation `(A,A1)`. From here through step 7 send no other
   `set_client_grid_coord` or `release_client_grid_coord` command, and require
   no new `Server::ClientSubscribe` line for A or B, so every later activation
   of A comes from an Omni transfer.
6. Move B→A using B's newly queried UUID. Require the same `globalId` at A and
   exactly one destination-specific OwnedEntity `ChangedCell` line; its tick is `E2`,
   not the tick from a later unrelated transfer batch. With the viewer still
   pinned at P, wait only until B leaves `status.activeCoords`. Auto-follow may
   have subscribed B after `E1` and before the step-5 park, so a
   `Server::ClientUnsubscribe` line for B may or may not appear; any such
   subscription ends within the 2 s sticky subscription window after the step-5
   park. A's second activation is `E2`.
7. Repeat the complete A→B→A cycle with fresh source UUIDs. Derive `E3` and `E4` only from the corresponding OwnedEntity `ChangedCell` lines, require exactly one line per intended destination, and require the same `globalId` after every crossing. Keep the viewer pinned at P throughout; after each crossing wait until the departed cell leaves `status.activeCoords`. A's third activation is `E4`, with `E2 < E4`; other transfer types and ticks do not identify these events.
8. Stop recording and poll `status.recording:false`. Before `replay_play`, send
   `set_client_grid_coord` to A and wait until `describe_scene.subscribedCoords`
   and the server subscribe log prove A occupies an active subscription slot.
   That command pins the viewer at A; the pin, not auto-follow, holds A through
   playback and across loop restarts. From this point until step 10's release
   check, send no parking, `set_client_grid_coord`, or unsubscribe command.
   Baseline the OwnedEntity `ChangedCell` lines for `globalId`, then retain the
   server-log slice from `ReadGrid F7.replay.grid` through the first new
   `End replay <tick>, looping`. Require exactly four destination-specific
   OwnedEntity lines at `(E1,B)`, `(E2,A)`, `(E3,B)`, and `(E4,A)`. Earlier capture
   snapshots and the `replay_transfer_capture` events are not the event oracle:
   an entry carries the counts of its tick, not the destination or the identity
   of what moved.
9. When Network `Verbose` is effective, require publication at each `E`. When it
   is clamped, use the static Replay/GameBase phase trace: at a return tick
   Replay checksum-validates and removes the old terminal reader before
   activating the new generation, loads the post-dispatch record for `E`,
   dispatches only the already-active set, then applies and publishes the
   transfer at `E`; `RefreshReplayActiveSet` admits the returned coordinate for
   its first dispatch at `E + 1`. Status and query responses prove present state
   only.
10. During playback, the step-8 pin holds A. Decide the hold from server logs,
    not `describe_scene.subscribedCoords`, which always lists the client's own
    cell. For each of at least two observed playback loops, require exactly one
    `Server::ClientSubscribe Client: <n> Coord: (<A>) Slot: <s>` line after that
    loop's bare `ServerSessionBase::ResetForLoad` line and before its
    `(E1,B)` OwnedEntity `ChangedCell` line, and no `Server::ClientSubscribe` or
    `Server::ClientUnsubscribe` line for A from that `E1` line through its
    `(E4,A)` line. Between one loop's `E1` and `E2` lines, take one sample:
    `describe_scene.clientGridCoord` must equal A while
    `query_omnis {"coord":<B>}` places the Omni's `globalId` at B. At each
    restarted A activation (`E2` and `E4`; the original failure's first restart
    was `7338`, 155 ticks after the prior generation ended at `7183` and beyond
    the 128-tick receive window), require a new server `SendCoordFullState` line
    for A at that activation tick, corresponding client full-state receipt, and
    later resumed A traffic. Require `clientCount:1`, the same final Omni
    `globalId` and ownership at A, a completed clean loop, no client gap
    disconnect, and no new CRC, checksum, replay-reader, or `CONFIRMED DESYNC`
    errors. These are the runtime observations; the internal hold and
    ACK-negative guarantees are proved independently from the server source:
    restart detection precedes delta serialization, update and resend paths
    remain suppressed while held, only an accepted ACK reaching the current
    pending full-state tick releases the hold, a newer restart supersedes that
    target, wrong-epoch, stale-target, and below-target ACKs cannot release it,
    same-tick resync does not duplicate the full-state send, slot free/load
    reset clears the state, and periodic ACKs provide liveness. After those
    observations and before a later replayed A→B OwnedEntity `ChangedCell` line, send
    the client `release_client_grid_coord {}` and require `ok:true`; once that
    crossing's line appears, poll `describe_scene` until `clientGridCoord`
    equals B, before the next replayed B→A line.
11. Cancel playback, release the processes, and inspect the stopped v5 artifacts. The manifest must contain three records for A with activation ticks `A1`, `E2`, and `E4`, and each must have its own `F7.replay.<A-key>.<activationTick>` header plus `.frames`, `.checksums`, and Debug `.fullframes` siblings. Require each record's sibling set to be present and the earlier siblings' SHA-256 values to remain distinct from the third generation's.

#### Command-only malformed fullframes route

On Debug, create an active recording with the target coord present, then arm
`replay_inject_persistence_failure {"stage":"fullframes_record","coord":[x,y]}`
and stop recording. Require the new `Injected malformed replay fullframes` Debug
line to name the selected coord and report before/after byte sizes differing by
exactly one. Poll until recording stops, send `replay_play`, and require a valid
manifest and adoption, the existing `Full frames file read failed, discarding`
warning for the selected reader, authoritative checksum validation without
CRC/desync errors, and an `End replay <tick>, looping` marker. This command
mutates the saved optional diagnostic stream before manifest hashing; it leaves
other coords untouched and does not require stopped-server file editing. A
mutation open, read, empty-file, or atomic-rewrite failure instead deletes the
selected writer's sibling set and suppresses the manifest.

### Replay capture invalidation and stable writer target

Use fresh recordings for these two failure cases and preserve a stopped-server copy of every generated replay sibling before the next case.

1. For capture invalidation, start recording, arm
   `replay_inject_persistence_failure {"stage":"transfer_capture"}`, and queue
   one valid `SpawnOmni` entry or wait for a natural Omni transfer. Require the
   live spawn or transfer to complete at its destination, exactly one recording-invalidation
   error line, `status.recording:false`, and a cleared `replay_transfer_capture`
   result (an empty `events` array and `-1` `firstWriterInputTick`).
   Stop/release the server and require no valid v5 manifest; after relaunch,
   `replay_play` must not adopt or alter the live state. This case proves Replay
   failure does not veto an already accepted live post-tick batch.
2. For the stable writer target, discover the sole real Omni by querying every
   coordinate in `status.activeCoords`; record its actual non-origin A and
   `globalId`, choose adjacent B, and never assume origin or a fixed A. Start
   recording, arm
   `replay_inject_persistence_failure {"stage":"coordinate_writer","coord":<A>}`,
   refresh the Omni UUID immediately before moving A→B, then park the viewer
   beyond A until A is absent from both `describe_scene.subscribedCoords` and
   `status.activeCoords`. Refresh the same Omni's UUID at B immediately before
   returning B→A. Stop recording and require the failure to name the originally
   armed activation, no valid manifest publication, the armed generation's
   siblings absent, and the later A generation's qualified siblings present
   independently. The proven setup used A=`[2,0]`, B=`[3,0]`, `globalId:1`,
   armed activation `14187`, and later activation `34085`; these values are
   evidence, not setup constants.

### Live adjacent-generation control

Use the existing commands to create a valid `savedEnd + 1` pair; do not edit
replay files or add a harness rig API. Discover a real Omni cell S from
`status.activeCoords` and choose an empty A that shares an edge with S, never a
diagonal neighbor. A plain spawn entry
`{"coord":<S>,"type":"SpawnOmni"}` with `pauseAfterWriterInput:true` arms a
pause; its omni only marks the event. Pause, move the client to A until A is
active, request recording, send that plain entry, unpause exactly once, and
wait for the automatic pause after the next writer input, so recording is
active. Still paused, send an edge-coast spawn into S whose margin is one tick:
`inject_payload {"entries":[{"coord":<S>,"type":"SpawnOmni","vecPosition":<p>,"vecVelocity":<v>,"transferLockTimer":1}],"pauseAfterWriterInput":true}`,
with `p` 449.5 m from S's center toward A on the S→A axis and 0 on the other
axis, and `v` 33.33 m/s toward A (for A east of S, `[449.5,0]` and
`[33.33,0]`). Unpause exactly once and wait for the automatic pause: during
active recording it follows the writer input of the spawn's event tick, so the
omni is applied in S right after that tick and has not moved. While still
paused at completed tick T, repeatedly move the client away until A is
unsubscribed and absent from `status.activeCoords`; the clock must remain T.
Still paused, send the plain entry again, then manually unpause: tick `T + 1`
carries the coasting omni across into A, so the next writer input captures the
return at `T + 1` and pauses again. While still paused after that writer input, send
`replay_record {"start":false}`, then send `pause {"paused":false}` and poll
until `status.recording:false`; only then send `replay_play`, play it through a
completed loop, and inspect the stopped manifest. Require A's earlier generation
to end at T, its next activation to equal `T + 1`, a valid root digest over the
qualified siblings, and no CRC, read, or desync error.

### Replay stop-before-dispatch decisions

These checks combine harness observations with a static phase trace because the public commands do not expose a dispatch counter.

1. For playback abort, record exactly one cell crossing into a newly created
   destination at tick `E`, made with the edge-coast `SpawnOmni` of the
   `inject_payload` entry, and no other post-dispatch event for that
   destination, stop
   recording, release the server, and preserve the complete valid v5 artifact
   set. From the manifest records, compute the destination's exact
   `(coordKey,activationTick)` kind-2 header file and authenticate the untouched
   manifest and artifacts with the root-digest self-check before editing. In
   that header require exactly one occurrence of the byte sequence consisting of
   post-dispatch count `1` as little-endian int64, `E` as little-endian int64,
   status count `1` as little-endian int64, and the `TransferOmni` byte `05`,
   followed by that record's serialized `OmnisPostRender::SpawnInfo` image
   running to the end of the file, so the sequence opens the header's trailing
   post-dispatch section and its only record. Change that status count to `0`
   and truncate the file immediately after it; the record then parses as an
   empty batch, which the post-dispatch check rejects with the same semantic
   error as a tick-input entry. Recompute the manifest root digest using the v5 repair
   procedure under Replay manifest v5 integrity and generation matrix below.
   After relaunch, capture ordered-log counts for `ReadGrid F7.replay.grid`,
   `GameBase::ResetSession()`, and `Replay post-dispatch record is not a
   post-tick batch`, plus a status tick, before sending `replay_play`. The
   malformed event may abort in the same update that adopts the replay, so do
   not require an observable `replaying:true` state. Require exactly one new
   semantic-error line, `status.replaying:false`, later status ticks advancing
   normally, exactly one newly appended grid-read and reset for the initial
   adoption before the error, and no second grid-read or reset after it. Runtime
   therefore proves the malformed record reached `AbortReplay`, playback
   cleared, normal ticking resumed, and no second reload occurred. The static
   Replay/GameBase trace independently proves the remaining per-iteration facts
   the command surface cannot expose: `kStopBeforeDispatch`, no load flag,
   pre-tick tick/time rollback, normal active-set rebuild, and the loop
   break before dispatch.
2. For normal loop completion, play an unmodified v5 recording through its final
   terminal reader. Require `End replay <tick>, looping`, continued
   `status.replaying:true`, and the next loop's initial state on a later
   fixed-tick iteration. The static trace must show terminal validation/removal
   before the empty-reader decision, `kLoadReplay` armed with
   `kStopBeforeDispatch`, `SaveLoadReplay` reloading, and the loop break before
   dispatch; the current iteration must not dispatch the reloaded initial state.

#### Replay manifest v5 integrity and generation matrix

This is a stopped-server AppData test, not an agent command. Produce the valid
repeated-generation harness rig above and prove one loop. Stop/release the
server, back up the complete `F7.replay.manifest`, `.grid`, `.meta`, and every
qualified coordinate sibling from
`<absolute app-data root>\Broken Engine Sandbox Server`; restore the backup
before each change and relaunch. The v5 manifest is fixed-width little-endian:
version, initial tick, activation count and `(activationTick,x,y)` records; a
`hasFullFrames` byte; then the 32-byte generation digest. It stores no
inventory: the artifact set and order are computed from the records. The
recording has one grid and one meta artifact, and each record has a header,
`.frames`, and `.checksums` artifact, plus `.fullframes` when `hasFullFrames` is
`1`. Kinds are grid/meta/header/frames/checksums/fullframes = 0..5. Artifacts
are ordered grid, meta, then each record in stored manifest order with its
header, `.frames`, `.checksums`, and `.fullframes` artifacts in that order.
`ToKey` packs `x` in the high 32 bits and `y` in the low, so `[0,0]` is key `0`.
Coordinate names are `F7.replay.<coordKey>.<activationTick>` plus the existing
suffix. The SHA-256 preimage is, in order, the four little-endian length bytes
`2B 00 00 00`, the 43 bytes of `broken-engine/replay-manifest-generation/v5`
without a NUL, the exact stored manifest payload bytes from version through
`hasFullFrames`, then each artifact's raw 32-byte SHA-256 in that order, with no
byte count.

Every scenario iteration is one loop: release, stage the files on disk while the
server is stopped, re-claim, relaunch, observe. Each re-claim mints a fresh
owner token and invalidates the previous one, so a released token is never
reused. Retain the current claim response's `owner` as the only valid owner and
substitute that literal token in every socket command, `Wait-HarnessPing.ps1`,
`Wait-IslandSceneReady.ps1`, and release call; never use an owner from an
earlier claim. A stale token hard-stops on owner mismatch and reads like a
scenario failure. The claim invocation is itself the wait for a foreign holder,
so add no polling of your own around the re-claim.

Under that same backup-and-restore discipline, staging a byte-modified artifact
the reader still accepts is a manifest repair in a fixed order. First self-check
the repair tool: recompute the root digest over the untouched manifest and
artifacts by the preimage above and require it to equal the stored trailing 32
bytes, because otherwise a repair bug is indistinguishable from the corrupt-data
rejection under test. The fixed widths are int64 for version, initial tick, and
activation count, 16 bytes per activation record (int64 `activationTick`, int32
`x`, int32 `y`), and one byte for `hasFullFrames`, so the digest begins at byte
`24 + 16 * activationCount + 1`. Then modify the artifact file, recompute the
root digest over the stored payload plus every artifact's current plain SHA-256
over raw bytes (no domain prefix, no byte count), and replace the trailing 32
bytes.

For every rejection case, pause immediately after relaunch, record `status.activeCoords` and the live ID state, send `replay_play`, and require `status.replaying:false` with unchanged live state (no adoption). Corruption cases require a new `SaveLoadReplay aborted: corrupt replay data:` line.

H1. Change version to `4`: reject with a new `Replay manifest version ...` line; restore v5 unchanged and require a loop as the control.
H2. Change one byte or the length of one artifact of each kind and require root-digest rejection; then separately remove, replace with a directory, or replace with a reparse point each named artifact. Reject before grid/meta/stream parsing.
H3. Change the generation digest and reject. Separately test invalid fullframes flag, negative activation count, and trailing data. Reject each before adoption.
H4. Preserve a repeated coordinate but duplicate one complete manifest record `(activationTick,x,y)`, raise the activation count by one, and recompute the root digest. Require `ReplayManifest overlapping coordinate generations` rejection before adoption. A repeated coordinate with distinct activation ticks remains the clean control.
H5. Use the unmodified recording from the live adjacent-generation recipe above
as the accepted `savedEnd + 1` control; do not manufacture that control by
rewriting a retained replay. For the rejection case, raise the earlier
generation's saved end in a backed-up copy of that recording, whose coordinate A
has an earlier generation ending at `T` and a next activation `T + 1`. In the
earlier generation's kind-2 header, named from its manifest record, the two
16-byte version headers precede the saved-start frame, whose tick fills bytes
32-39; the saved-end frame follows the int64 difference count and opens with its
own tick. Require exactly one occurrence of `T` as a little-endian int64 from
byte 40 on, and change it to `T + 1`. Append one 8-byte checksum of any value to
that generation's `.checksums`, because the reader requires exactly one checksum
per tick from start through saved end. Recompute the root digest by the repair
procedure above, and require a new
`SaveLoadReplay aborted: corrupt replay data:` line naming
`ReplayManifest overlapping coordinate generations` before adoption. Leave the
later generation's header unchanged: the reader requires its start tick to equal
its manifest activation, so lowering that tick is rejected as
`ReplayManifest stream bounds` before the overlap check runs.
H6. Change the staged grid so it contains a later-only coordinate or omits a distinct coordinate activated at the initial tick, recompute the root digest, and require rejection before state replacement. A coordinate present at the initial tick may also have later records; the clean repeated-A harness rig must load.
H7. On Debug, kind-5 `.fullframes` holds complete snapshots distinct from the kind-3 `.frames` stream. Require the root digest to fold one kind-5 artifact per complete `(coordKey,activationTick)` header identity. A clean replay proves snapshot consumption without CRC, desync, or read errors. On non-Debug, static inspection proves `kbReplayFullFrames == false` and the v5 root digest folds no kind-5 artifact. Do not enable runtime Release diagnostics.
H8. While recording, arm `replay_inject_persistence_failure {"stage":"inventory"}`, stop, poll until `status.recording:false`, then call `replay_play`: it must fail with no valid manifest/adoption. This verifies manifest-last publication.
H9. These procedures do not claim authentication or protection against files changed after preflight.

## Durable caveats

- Injection during replay fails. Injection while paused remains accepted with `deferred:true` and applies on the next unpaused tick, or on its `tick` when that is later.
