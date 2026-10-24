# Agent Harness Replay Verification

The replay checks every game project shares. The selected project's hub,
`Projects/<Project>/Documents/AgentHarness.md`, routes that project's own replay
recipes and the launch values the checks below use.

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

#### Replay manifest v6 integrity and generation matrix

This is a stopped-server AppData test, not an agent command. Produce a valid
repeated-generation harness rig from the selected project's replay recipes and
prove one loop. Stop/release the
server, back up the complete `F7.replay.manifest`, `.grid`, `.meta`, and every
qualified coordinate sibling from
`<absolute app-data root>\<server game name>` (the hub's `## Launch values`);
restore the backup
before each change and relaunch. The v6 manifest is fixed-width little-endian:
version, initial tick, next global ID, activation count and
`(activationTick,x,y)` records; a
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
`2B 00 00 00`, the 43 bytes of `broken-engine/replay-manifest-generation/v6`
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
scenario failure. The claim waits for a foreign holder as worker step 5
states, so add no polling of your own around the re-claim.

Under that same backup-and-restore discipline, staging a byte-modified artifact
the reader still accepts is a manifest repair in a fixed order. First self-check
the repair tool: recompute the root digest over the untouched manifest and
artifacts by the preimage above and require it to equal the stored trailing 32
bytes, because otherwise a repair bug is indistinguishable from the corrupt-data
rejection under test. The fixed widths are int64 for version, initial tick, next
global ID, and activation count, 16 bytes per activation record (int64
`activationTick`, int32 `x`, int32 `y`), and one byte for `hasFullFrames`, so the
digest begins at byte `32 + 16 * activationCount + 1`. Then modify the artifact file, recompute the
root digest over the stored payload plus every artifact's current plain SHA-256
over raw bytes (no domain prefix, no byte count), and replace the trailing 32
bytes.

For every rejection case, pause immediately after relaunch, record `status.activeCoords` and the live ID state, send `replay_play`, and require `status.replaying:false` with unchanged live state (no adoption). Corruption cases require a new `SaveLoadReplay aborted: corrupt replay data:` line.

H1. Change version to `5`: reject with a new `Replay manifest version ...` line; restore v6 unchanged and require a loop as the control.
H2. Change one byte or the length of one artifact of each kind and require root-digest rejection; then separately remove, replace with a directory, or replace with a reparse point each named artifact. Reject before grid/meta/stream parsing.
H3. Change the generation digest and reject. Separately test invalid fullframes flag, negative activation count, and trailing data. Reject each before adoption.
H4. Preserve a repeated coordinate but duplicate one complete manifest record `(activationTick,x,y)`, raise the activation count by one, and recompute the root digest. Require `ReplayManifest overlapping coordinate generations` rejection before adoption. A repeated coordinate with distinct activation ticks remains the clean control.
H5. Use the unmodified recording from the selected project's live
adjacent-generation recipe as the accepted `savedEnd + 1` control; do not
manufacture that control by
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
H7. On Debug, kind-5 `.fullframes` holds complete snapshots distinct from the kind-3 `.frames` stream. Require the root digest to fold one kind-5 artifact per complete `(coordKey,activationTick)` header identity. A clean replay proves snapshot consumption without CRC, desync, or read errors. On non-Debug, static inspection proves `kbReplayFullFrames == false` and the v6 root digest folds no kind-5 artifact. Do not enable runtime Release diagnostics.
H8. While recording, arm `replay_inject_persistence_failure {"stage":"inventory"}`, stop, poll until `status.recording:false`, then call `replay_play`: it must fail with no valid manifest/adoption. This verifies manifest-last publication.
H9. These procedures do not claim authentication or protection against files changed after preflight.
