# Agent Harness Command Reference

Engine-owned commands, identical for every game project: the shared commands both endpoints accept, and the client and server commands the engine dispatches. The selected project's hub, `Projects/<Project>/Documents/AgentHarness.md`, routes that project's own command schemas.

Every field below belongs under request `params`; every returned field belongs under response `result`. Sending a side-specific command to the other executable still returns `unknown command`. The six shared commands below do not capture or drive desktop windows.

A project may take over an engine client or server command: its handler runs before the engine's, validates its own parameters, calls the engine command, and then appends its own fields. Its added parameters and fields are documented in its own command files, beside a link to the engine entry here.

Positions in any command's result are cell-local: a `local` array is meters from the center of the cell, and a `coord` array beside it names that cell whenever a report's rows span cells. A project command may name its pair differently (the selected project's command files). No command reports absolute world meters; derive them from the pair when they are wanted.

The server monitor is an engine-owned GDI window, not a protocol command surface; use the [server-window recipe](server-window.md#server-gdi-monitoring-window-capture-and-click) when that window must be observed or clicked.

## Shared commands

- `ping`: no params. Returns `{"build":"server"|"client","tick":int}`; tick is `-1` before game creation.
- `quit`: no params. Requests clean shutdown; server autosaves. Returns `{}`.
- `get_logs`: `{"count"?:64,"pattern"?:"ECMAScript regex","category"?:"Default|Temp|Audio|Graphics|Loading|NavData|Network|Input|Replay"}`. Pattern scans the whole selected wrapping ring (512 cross-category lines when category is omitted; 128 lines for one category), then returns the last `count` matching lines chronologically as `{"lines":[...]}`. Matching is case-sensitive; use character classes rather than `(?i)`.
- `set_log_level`:
  `{"level":"Verbose|Debug|Info|Warning|Error","category"?:name}`. Returns one
  effective level or a category map. Each category has a compile-time floor, set
  by the `keLogLevel*` constants in `Engine/Source/EngineSwitches.h` (the
  `kpeLogLevels` table in `Common/Log/Log.h`; mechanism:
  [`Common/Log/AGENTS.md`](../../../../Common/Log/AGENTS.md)); a request below
  that floor clamps up to it. An effective level above the requested one
  therefore means the lines below it are compiled out of that build, not
  filtered at runtime, and no runtime level emits them. A run that needs those
  lines lowers that category's floor through a temporary verification edit
  ([`.agents/references/change-workflow.md`](../../../references/change-workflow.md)
  `#### Step 5 — Run targeted pre-review checks`).
- `cell_coordinate_probe`: `{"coord":[q,r]}` and nothing else — a params object
  that is not exactly that one key fails with
  `cell_coordinate_probe accepts only 'coord'`; both endpoints accept it. Any
  pair of integers in the signed-int32 range is valid — the cell needs no
  subscription, no loaded frame, and no adjacency — while an integer outside
  that range fails with
  `cell_coordinate_probe 'coord' values must fit in a signed 32-bit integer` and
  a non-integer value, a non-array `coord`, or an array of any other size fails
  with `cell_coordinate_probe 'coord' must be an array of 2 integers`. Its one
  precondition is loaded island templates; without them it fails with
  `cell_coordinate_probe requires loaded island templates`. Read-only: it
  generates a throwaway placement list and elevation grid for the requested
  cell, reports them, and frees them, touching no live cell, tick, or
  subscription. Returns
  `{"coord":[q,r],"area":{"width":float,"height":900.0},"terrain":{"samplesFinite":true},"placements":{"count":int,"crc":uint64},"elevation":{"crc":uint64}}`,
  where `area` is the cell hexagon's bounding rectangle, about 1,039.23 m wide.
  Run the same coordinate on the client and on the server and require both CRCs
  to be equal; the two endpoints generate the cell independently from the same
  coordinate.
- `crash_report_harness_rig`: no params. Simulates a production crash on demand
  so the agent can observe the real report, upload, and exit. Debug-only: a
  non-`kbDebugInput` build fails with
  `crash_report_harness_rig requires kbDebugInput build` whatever object the
  params are. In a `kbDebugInput` build params must be exactly `{}` — any other
  object fails with `crash_report_harness_rig requires empty params`. Each
  failure is an ordinary command failure that leaves the process running. On
  success it deliberately never responds: the endpoint writes its crash report,
  uploads it when the client is connected and accepted by a server and the
  connection has not already carried a report (waiting up to 5 s for ENet to
  confirm delivery), and exits, so AgentHarness reports
  `no response (timed out or peer closed)` and returns exit code 1 while the
  game process itself exits with code 0.

## Client commands

- `send_raw_packet`: `{"bytes":[int,...]}`; `bytes` is required and no other
  field is accepted. Each byte is an integer in `0..255`, the first being the
  packet type. Requires an accepted connected client. Sends the bytes unchanged
  as one reliable packet to the server over the client's own connection and
  returns `{"size"}`, the byte count sent. The server's ordinary admission and
  corrupt-input checks
  ([Client to Server Contract](../../../../Documents/Architecture/Network.md#client--server-contract))
  decide the outcome, which the server log shows:
  `Server::RecordContractViolation First` at a client's first violation of a
  kind, `Server::RecordContractViolation Disconnecting` at the limit, and, at
  `Network` `Debug` level, `Server::Receive dropped corrupt packet`. For example
  `{"bytes":[0]}` sends a server-only packet type, which the server counts as
  one corrupt-data violation with reason `not client-sendable`.
- `desync_probe`: packet mode `{"desyncReports"?:0..8,"debugFrameRequests"?:0..8}` or mutually exclusive recovery mode `{"triggerRecovery":true}`. Requires an accepted connected client and in-game current state. Returns `{"tick","coord":[q,r],"desyncDebugFrames","stalled","desyncReports","debugFrameRequests","triggerRecovery"}`.
- `set_client_grid_coord`: `{"coord":[q,r]}`. The object accepts only `coord`,
  an array of exactly two integers anywhere in the signed-int32 range; only an
  integer that does not fit in `int32` is rejected. Moves the client's grid cell
  and immediately refreshes its desired subscriptions, so automation can drive
  the cross-cell subscribe and full-state adoption path. Requires an accepted
  connected client with a valid assigned owned entity. Returns
  `{"clientGridCoord":[q,r]}`. Any invalid input returns the normal
  `ok:false`/`error` envelope and changes no state. The command also pins the
  coordinate: while pinned, every other write of the client's cell — the game's
  own writers, such as owned-entity events, and the server-load clear — leaves
  it unchanged, so a server load or replay loop restart keeps it. A repeated
  `set_client_grid_coord` moves the pin; only `release_client_grid_coord` or a
  game reset (main-menu connect or frame change, including connection loss)
  clears it. The pin is never persisted. The server remains the subscription
  authority — a cell other than origin whose `GridDistance` from every
  authorized cell exceeds 1 is accepted locally and rejected on the wire.
- `release_client_grid_coord`: no params (only an empty object is accepted). Requires a running game. Clears the `set_client_grid_coord` pin and returns the current `{"clientGridCoord":[q,r]}`. The viewer stays in that cell until the next game writer, such as an owned-entity event, moves it.
- `screenshot`: `{"path"?,"maxWidth"?:1568,"format"?:"jpg"|"png","quality"?:80}`. A `quality` outside `1..100` is a parameter error. A `maxWidth` value `<= 0` disables downscaling and preserves native framebuffer dimensions. Default path is `%TEMP%\Screenshots\agent_N.<ext>` using the process temp directory. Returns `{"path","width","height"}` — the saved file and its pixel dimensions, never image bytes. Requires `kbScreenshots`.
- `renderdoc_capture`: `{"frames"?:1}`; `frames` is `1..8`. Triggers RenderDoc
  capture(s) of the next presented frame(s) and returns
  `{"paths":[absolute .rdc paths]}`. Requires a client launched with
  `--renderdoc`; otherwise returns `ok:false` naming `--renderdoc`. Temporarily
  restores a minimized client without activation, captures, then re-minimizes;
  an already-visible client fails when swapchain recreation is deferred.
  Occupies the single channel until RenderDoc has serialized every capture. The
  wait is bounded by the deferred-response drain-count liveness timeout
  (`kiDeferredTimeoutDrains` drains, counted once per client render frame), not
  wall-clock, so set `--timeout-ms` at least as large as the expected wall-clock
  capture time. See
  [RenderDoc capture](renderdoc.md).
- `resize`: `{"width","height"}` within `320x180..16384x16384`; rounds each to 8, then returns OS-applied `{"width","height"}`. Rejects while minimized and never changes persisted/fullscreen state.
- `fullscreen`: `{"on":bool}`. Toggles borderless fullscreen/windowed without persisting. Windowed restore uses launch extent, not a later resize. Returns `{"fullscreen","width","height"}`. Rejects while minimized.
- `window_state`: `{"minimized":bool}`. Minimize uses normal minimize; restore does not activate and waits for settled swapchain. Returns `{"minimized","width"?,"height"?}`.
- `audio_resume`: no params. Resumes client audio; an agent client boots with audio suspended, so send this before any scenario that must observe audio. Completes synchronously with the normal envelope; repeating it is accepted.
- `audio_streaming_harness_rig`: Debug-only music streaming controls and
  snapshot, exact schema
  `{"action":"inspect"|"play"|"clear"|"suspend"|"resume"|"clear_history"|"hold_read"|"release_read"|"stall_loaders"|"release_loaders"|"stop_publication"|"resume_publication"|"load_chunk"}`
  plus exactly the fields its action lists; an extra or missing field fails.
  Other builds return an error naming `kbDebugInput`. Every action returns a
  snapshot. `inspect` changes nothing. `play` `{"crc"}` plays that music track
  through the ordinary stream path; the first `play` in a process clears the
  playing music and stops playlist transitions for the rest of the process. It
  fails while audio is suspended or when no voice is allocated. Music CRCs are
  the `kAudioMusic*Crc` constants of the generated `Data/Audio.h`; a snapshot's
  `current.crc` names the playing track. `clear`, `suspend`, and `resume` use
  the ordinary stop-music and audio suspend/resume paths. `clear_history`
  releases the read hold and the loader stall, waits until no Audio read and no
  record write is in flight, then empties `records`, zeroes `retryCount` and the
  partition counters, and starts recording; nothing is recorded before the first
  `clear_history`. `hold_read` `{"crc","offset","length"}` holds the next Audio
  read of exactly that range a loader starts until `release_read`, and fails
  unless the hold is idle; music reads are 16 KiB at 16 KiB-aligned offsets, a
  track's last read shorter. `release_read` releases the hold and replies once
  no read is held. `stall_loaders` stops both loader threads from starting any
  job until `release_loaders`; a running job finishes. `stop_publication` stops
  the current voice from starting new reads until `resume_publication`; reads
  already started still complete. `load_chunk` `{"crc","priority"}` queues an
  ordinary whole-chunk load at `"realtime"` or `"normal"` priority; a chunk
  already requested or loaded is unchanged. A loader drain (device loss,
  teardown) releases the hold and the stall.

  Snapshot fields: `scenarioGeneration` counts completed `clear_history` calls;
  `retryCount` counts the times a voice found all six read entries in use since
  the last one. `holdOwner` is `none|controlled` and `holdState`
  `idle|armed|consumed`; `heldPoolIndex` (null when no read is held),
  `heldGeneration`, and `heldState` describe the held entry. `controls` is
  exactly `holdArmed,loaderStaged,publicationStopped`. `poolEntries` has six
  index-ordered `index,state,generation,crc,offset,length` rows with `state`
  `free|queued|loading|ready`; a Free row has zero CRC/offset/length. `current`
  and `newestFade` contain
  `present,crc,volume,fadingIn,fadingOut,pendingSlots,buffersQueued,underrunning`,
  where `underrunning` means the voice has not submitted its last buffer, no
  buffer is queued, and the next slot is not ready; `olderFades` contains
  `count,pendingSlots,underrunningCount`. `partitions` lists `main`, `loader0`,
  and `loader1` with `head` and `dropped`, capacities 32, 24, and 24; `overflow`
  is true when any partition dropped a record. `gapFree` is true when the copied
  `records` form an unbroken sequence since the last `clear_history` (a dropped
  record takes no sequence), and `coherent` when, in addition, nothing was
  dropped and the copy saw no concurrent change. `records` are ordered by
  `sequence`, each
  `sequence,scenarioGeneration,partition,phase,threadId,threadPriority,poolIndex,crc,offset,length,state,generation,cancelAcknowledged`.
  `phase` is `startup` (a voice was created); `refill_queued`, `refill_loading`,
  `refill_ready`, or `refill_cancelled` for an Audio read, where
  `cancelAcknowledged` is true when the cancelled entry was freed at that point;
  `existing_queued` for a whole-chunk load queued from the main thread; and
  `existing_complete` for a finished whole-chunk or range load.
- `dump_render_target`: `{"name","index"?:0,"channel"?:0,"path"?,"raw"?:false}`. Unknown names list valid names. Single-channel output normalizes to grayscale PNG; four-byte RGBA writes direct PNG; float formats require raw `.bin`. The default base path is `%TEMP%\Screenshots\dump_<name>_N`. Requires `kbScreenshots`.
- `describe_ui`: no params. Returns UI state, `tweaksVisible` (true while the F3
  Tweaks overlay is shown), game flags, framebuffer, windows, and labeled
  items with rect/disabled/checked/inputable/visible fields. Items include
  offscreen and clipped ones with `visible:false` (`### Client command
  behavior`). An item is named by its `window` plus its full `label`; a label
  whose visible text repeats within one window carries a hidden `##<section>`
  suffix, such as `Low##Terrain Shadows`. Menu
  controls created with `MenuUtils::WrapperSlider` and
  `MenuUtils::WrapperPlusMinus` also include optional `value` text for the
  current numeric value, using the control's display format. Other items omit
  the field; checkbox state and radio controls created with
  `MenuUtils::RadioRow` expose their state through `checked`.
- `click`: `{"label","window"?,"describeUiAfter"?:true}`. Resolves the label in
  the last completed UI frame and activates that item by its ImGui ID, as
  ImGui's own navigation does, then settles two frames; the mouse never moves,
  and an offscreen item activates too. A disabled item (`disabled:true` in
  `describe_ui`) is not activated, and the reply reports `enabled:false`.
  Presses buttons, selectables, checkboxes, radio buttons, tab items, and the
  engine menu buttons; a text field becomes active with its text selected,
  ready for `type_text`. Returns found/enabled and optional UI.
  Ambiguous/not-found errors list candidates; pass the full label, including
  any `##` suffix, and `window` to select one.
- `set_slider`: `{"label","window"?,"value":number}`. Activates the slider into
  its text box (ImGui's prefer-input activation, offscreen sliders too), types
  the value, and presses Enter. A target that is not inputable (a button or
  checkbox, for example) fails with `target is not inputable`. A disabled
  target is not activated and nothing is typed; the reply reports
  `enabled:false`. An inputable text field is typed into the same way, and
  Enter submits it. Returns found/enabled. Tweaks overlay sliders are reachable
  only in Debug clients, where `kbDebugInput` lets F3 open the overlay; in
  Profile and Release clients they fail with `no widget matches label`.
- `get_wrapper`: `{"key":string}`. Read-only lookup of a Tweaks-registered wrapper. Returns `value`, `min`, and `max`, each a `"%.6f"` string matching the slider's own display format. `key` is the registrar's slider map key, not the display label where the two differ. Needs no F3 overlay and works in every client build. A missing or non-string `key` fails with `get_wrapper requires string 'key'`; an unregistered key fails with `get_wrapper: no Tweaks slider key '<key>'`.
- `key`: `{"key":name,"holdFrames"?:1}`. `holdFrames` is an integer in `0..1798`
  (default `1`); a value outside that range is a parameter error. The limit
  counts client drains/frames rather than wall-clock time, so duration varies
  with the client drain rate. Supports case-insensitive single letters/digits;
  `ESC`/`ESCAPE`, `SPACE`, `TAB`, `ENTER`/`RETURN`, `UP`/`DOWN`/`LEFT`/`RIGHT`,
  and `F1`–`F24`. Returns `{"ok":true}`. While the client is not rendering
  (minimized or swapchain recreate deferred), only the RawInput overlay takes
  effect; `key` sends no ImGui event.
- `type_text`: `{"text":string}`. Types the UTF-8 text into the active ImGui text field through ImGui character input, then presses and releases Enter on its own frames. An empty string only presses Enter. A missing, non-string, or longer-than-256-byte `text` fails with `type_text requires string 'text'`. Returns `{"ok":true}`.
- `wheel`: `{"notches"?:1}`. Adds mouse-wheel notches for camera zoom only: no
  coordinates, buttons, or ImGui event, so it never scrolls an ImGui window.
  `notches` is an integer in `-17895697..17895697` whose running total with
  every earlier `wheel` command must also stay within that range; any other
  value fails without changing input state. Every notch reaches the camera,
  because the agent client's ImGui mouse never hovers a window. Returns
  `{"ok":true}`.
- `describe_scene`: no params; the engine command reads none, and a project may
  take it over to add parameters and fields. Returns
  `camera{basisCoord,eye,visibleArea,lod}`, `uiState`, `tweaksVisible` (true
  while the F3 Tweaks overlay is shown), `gameFlags`, `tick`, `clientGridCoord`,
  `subscribedCoords`,
  `cellStates[{coord,confirmedTick,confirmedOffset,highWaterValidatedTick,lastFullStateTick,snapshotHead,snapshotCount,lastRenderedTick,serverUpdateCount,lastReplayConfirmedTick,lastReplayServerUpdateCount,stuckFrameCount,pendingFullStateTick:int|null,firstServerUpdateTick:int|null,lastServerUpdateTick:int|null,tailTick}]`,
  and `islands[{coord,local,rotation,footprint?}]`.
  `camera.basisCoord` is the cell the camera's own values are local to: `eye`
  and `visibleArea` are in that cell's meters, while each row's `local` is in
  its own `coord` cell, so compare them only after adding the center offset of
  the axial delta `[dq,dr] = coord - basisCoord`, which is
  `[dq * 779.42, dq * 450 + dr * 900]` meters. An island's `local` is its placement
  center in its cell's meters. `cellStates` has one row per `subscribedCoords`
  entry, whether or not the cell has a snapshot, and reports that cell's
  reconciliation state: `confirmedTick` is -1 until the cell adopts a full
  state, and again after a resync reset, and `tailTick` is the newest snapshot
  tick, or -1 with no snapshot.
- `query_profile`: no params (only an empty object is accepted). Client-only
  profile query; returns `gpuTimers[{index,name,currentUs,averageUs,maxUs}]` for
  every timer row, including zeros, frame-coherent
  `shadowSample{sequence,currentUs}`,
  `clock{offsetTicks,targetBehindTicks,errorTicks}`, integer `fps`, and
  `counters[{index,name,count}]` for every CPU counter, including zero rows
  hidden by the Profile UI. The shadow sample is published after the matching
  framebuffer fence; sequence advances only when a new available Shadow result
  is latched. The clock fields are the same smoothed values the network profile
  overlay's Clock line prints, in sim ticks, sampled at the moment of the query
  with no frame latch; the overlay prints target-behind with a leading minus
  while `targetBehindTicks` is the positive stored value, and all three read
  zero until a client session publishes a clock correction. `fps` is the frame
  count the profile overlay's fps header prints (renders in the last second),
  sampled at the moment of the query.
  Also returns `cpuTimers[{index,name,displayUs,displayAllocations,threads,visible}]`
  for every CPU timer in index order, including hidden zero rows. `index`,
  `displayUs`, `displayAllocations`, and `threads` are integers; `name` preserves
  its exact table text, including indentation, and `visible` is boolean. The
  display values are the stored smoothed microseconds and allocation counts
  used by the CPU overlay; visibility is the formatter's stored decision and
  may remain unchanged while another screen is selected. Timer fields and names
  are copied under the CPU timer mutex at query time. `cpuTimerText` is a string
  copied from the CPU timer area's explicit retained character span, preserving
  whitespace and empty contents exactly. It is empty after the area is cleared
  or while ImGuiManager is temporarily absent during graphics recreation. The
  text reflects the most recent publication to that area; it and the structured
  timer fields have distinct cadences and need not describe the same frame.
- `presentation_continuity_probe`: no params (only an empty object is accepted).
  Read-only client query of the presentation state the most recently published
  render frame sent. Returns
  `{"publishedFrames":int,"basisCoord":[q,r],"water":{"origin":[x,y],"reducedNoiseOrigin":[x,y],"noiseFrequency":float}}`
  plus a `shadow`, `lighting`, and `smoke` object each shaped
  `{"currentArea":[x,y,z,w],"previousArea":[x,y,z,w],"historyResets":int}`;
  `smoke` reports flight plane 0's area pair, the pair wind reads. Take
  every sample with the client restored (`window_state {"minimized":false}`): a
  minimized client defers swapchain recreation, publishes no frame, and leaves
  the whole snapshot standing still. `publishedFrames` counts publications, so
  two samples carrying the same value describe the same rendered frame — re-take
  such a pair instead of reading it as continuity. `water.origin` is the
  camera's cell-local position in the top-level `basisCoord`;
  `reducedNoiseOrigin` is a unitless reduced phase value rather than a position,
  and `noiseFrequency` is the multiplier the noise reduction applies to the
  absolute camera position before reducing modulo 10. Each
  `currentArea`/`previousArea` is a cell-local rectangle in the top-level
  `basisCoord`, in the shared component order `x`=left, `y`=top, `z`=right,
  `w`=bottom. `historyResets` is a monotonic per-owner count of the multi-cell
  camera jumps that discarded that owner's history.

### Client command behavior

Client input/capture commands can defer across frames. The single channel remains occupied until the response; size timeout accordingly and never overlap calls.

Capture commands temporarily restore a minimized client without activation, wait for extent settlement, capture, then re-minimize. An already-visible client fails if swapchain recreation is deferred. `resize`/`fullscreen` reject minimized state; restore first. `click`, `set_slider`, and `type_text` fail while the client is not rendering (minimized or swapchain recreate deferred); restore first. After resize/fullscreen/restore, call `describe_ui` or `screenshot` to remeasure.

The harness drives ImGui UI only by item activation: `click` and `set_slider`
activate the item behind a labeled `describe_ui` entry by its ImGui ID unless
it is disabled, and no command sends an ImGui mouse position, button, or wheel
event. On an agent client ImGui does not clip items, so an item
outside the viewport or its window's visible area — a slid-away
panel's buttons, a row scrolled out of view — is listed with
`visible:false` and activates like a visible one; only items
inside a fully clipped child window are skipped. Side effects: offscreen labels
take part in label resolution, so a label unique on screen can become
ambiguous — for six frames once per Tweaks screen lifetime, the Tweaks slider
audit renders every section into an offscreen `##slider-audit` window,
repeating the sections' tab items and non-slider controls — and `window`
narrows the match; offscreen widgets also run their full behavior and draw
code. Mouse-only widgets (curve-editor control points, ImPlot plot
interactions) and hover effects, such as hover-opened panels, cannot be driven.

The agent client's ImGui mouse stays at ImGui's no-mouse sentinel, and the
RawInput game mouse stays at a frozen boot-time snapshot (under agent
suppression, physical mouse messages never reach DirectXTK Mouse).

## Server commands

- `status`: no params. Returns
  `{"tick","paused","recording","replaying","clientCount","activeCoords":[[q,r],...],"nextGlobalId","harvestedTransferTotal":int}`.
  `harvestedTransferTotal` counts every cell-crossing transfer applied by a
  harvest since the current game state began, cumulatively, and no spawn; it is
  zeroed on load, reset, every load-time client resynchronization, and entering
  replay playback, and does not move while paused or during playback.
- `pause`: `{"paused":bool}`. Returns the applied `paused` value. To advance a
  paused server by exactly one tick, send `timescale {"faster":false}` until it
  returns `denominator` 32, then send `pause {"paused":false}` and
  `pause {"paused":true}` back to back, with no wait or other command between
  them, and confirm that `status.tick` rose by 1. The server takes one agent
  request at the start of each update, before that update's tick wait: the
  unpause lets the tick ending that update run, and the pause stops the next
  tick only if it arrives before the next update starts. That window is one
  update interval, about 1 s at 1/32 but 31 ms at 1/1, shorter than one
  AgentHarness call, so a wait between the two commands, or a faster timescale,
  lets extra ticks run.
- `timescale`: `{"faster":bool}`. Steps the shared timescale and returns `{"numerator","denominator"}`. The slowest step is 1/32; `{"faster":false}` there changes nothing and still succeeds.
- `save`: `{"file"?:"bare filename"}`; defaults to quicksave. `file` (also `load`'s) is an appdata-relative bare filename; empty names, NUL, separators, `..`, `:`, and Windows device basenames are rejected. Every loaded cell is saved with its frame, so pinned cells (`create_cell`) stay pinned. Returns `{"file"}`.
- `load`: `{"file"?:"bare filename","pauseAfterLoad"?:false}`. `pauseAfterLoad`
  must be Boolean when present; invalid parameters fail before loading or
  resetting and do not change pause state. Missing/corrupt/truncated data resets
  fresh while returning success, as does a save written under another game
  frame version, such as an older build's (`resetToFresh:true`).
  Absent/false leaves the normal post-load state unpaused; true atomically
  pauses after a successful load or completed fresh fallback, before frame
  inputs can be constructed. Returns
  `{"file","resetToFresh":bool,"paused":bool}`.
- `reset`: no params. Resets the server to a fresh game, game-owned server state included. Returns `{}`.
- `create_cell`: `{"coord":[q,r]}`; `coord` is the only key. Creates a fresh
  cell at a coordinate that has none, sets its frame value `uiPinned` to 1, and
  returns `{"coord":[q,r]}`. A pinned cell stays in `status.activeCoords` across
  updates, paused or not, with no owned entity or subscription, until a project
  `edit_frame` `frame` write of `uiPinned` 0 releases it; `edit_frame` pins an
  existing cell with a nonzero `uiPinned`. The value lives in the frame, so
  `save` and `load` keep it and a server relaunch restores it through the exit
  autosave; `reset` leaves no pinned cell. A pinned cell is still not a live
  transfer destination: a non-owned transfer into a neighbour that holds no
  owned entity and that no client watches is dropped, pinned or not. Fails
  without `kbDebugInput`, while recording, during playback, and while a
  `replay_record` or `replay_play` transition is pending
  (`create_cell is refused while recording, during playback, or with a
  replay_record or replay_play transition pending`), and on a coordinate that
  already has a cell (`coord already has a cell; pin it with edit_frame`).
- `replay_record`: `{"start":bool}`. Schedules or cancels an idempotent recording transition and returns `{"pending":bool}`. Poll `status.recording` for the effective transition. `{"start":true}` fails while playback is active or pending. Requires `kbDebugInput`.
- `replay_play`: no params. Starts playback or cancels active playback and
  returns `{"pending":true}`. Fails while recording is active or a record start
  or stop is pending. Poll `status.replaying`. Per-tick checks call
  `DifferenceStreamReader::ValidateChecksum`; mismatch logs
  `LogDifferences CRC Client: ...`. Readers retire independently. When the last
  reader reaches its endpoint, the server logs `End replay <tick>, looping` at
  `Default/Debug` and loops; playback never ends by itself. Requires
  `kbDebugInput`.
- `replay_transfer_capture`: no params. Returns
  `{"firstWriterInputTick","writerInputCount","events":[{"recordingEventTick","playbackEventTick"}]}`;
  a project may take it over to add per-event fields.
  `firstWriterInputTick` is `-1` until observed. `events` holds one entry per
  recording event tick in capture order; it holds every captured event of the
  recording and clears at recording start and whenever the harness rig snapshot
  is cleared (reset, and any recording or playback failure, cancellation, or
  abort). It is kept after a successful stop and is carried into playback. An
  entry's `playbackEventTick` is `-1` until playback applies that entry's record
  and then equals its `recordingEventTick`. An event is a post-dispatch record:
  the post-tick batch of cell-crossing transfers and spawns applied after
  dispatch at `E` is recorded at `E` in the post-dispatch channel and
  published/replayed at `E`. During playback, an entry's `playbackEventTick` is
  latched from the exact tick whose post-dispatch record the reader loaded and
  applied. If the `set_log_level` response reports effective Network `Verbose`,
  use `ServerBroadcaster::BuildTickPublication` as runtime publication evidence;
  if the requested level is clamped to `Debug`, that kVerbose line is
  unavailable and the static source/phase trace owns publication-at-`E`. In both
  cases, runtime acceptance uses the event ticks, final state/count,
  checksum/no-desync signals, and completed replay loop; status/query results
  are not historical timing evidence. `writerInputCount` is the total
  normal-writer update ticks since a successful recording start; terminal
  flushes do not increment it. Requires `kbDebugInput`.
- `replay_drop_retained_end_frame`: `{"coord":[q,r]}`. During recording, removes the retained terminal frame from that coordinate's most recently terminalized generation: the last generation when it is terminal, otherwise the generation immediately before the open last generation. Returns `{"coord":[q,r],"dropped":true}`. Requires `kbDebugInput`.
- `replay_inject_persistence_failure`:
  `{"stage":"invalidation"|"grid"|"transfer_capture"|"coordinate_writer"|"fullframes_record"|"metadata"|"inventory"|"final_manifest","coord"?:[q,r]}`.
  `coordinate_writer` and `fullframes_record` require `coord`; every other stage
  rejects it. `invalidation` and `grid` require inactive recording; every other
  stage requires active recording. `transfer_capture` invalidates recording when
  the next accepted post-tick batch (transfers and spawns) reaches replay
  capture, before a writer is created or changed; the live batch still proceeds. `coordinate_writer` and
  `fullframes_record` resolve their target when armed: they select the open last
  generation when present, otherwise the most recently terminalized generation
  with retained state. A later generation append cannot redirect either failure.
  After the selected writer saves, `fullframes_record` atomically truncates its
  nonempty `.fullframes` file by one byte before final artifact hashing; a
  mutation I/O failure deletes that writer's complete sibling set and prevents
  manifest publication. `inventory` is consumed immediately before final
  artifact hashing and manifest publication. Returns
  `{"stage","coord"?:[q,r],"armed":true}`; the response does not expose a
  generation selector. Requires `kbDebugInput`.
- `query_profile`: server-only CPU profile query. `params` must be `{}`; any
  other value fails before any profile read. It returns raw
  `timers[{index,name,currentUs,averageUs,maxUs,allocations,threads}]` and
  `counters[{index,name,count}]`, including zero rows hidden by the Profile UI.
  On a server built with `kbProfiling`, only the `NavQuery` timer row
  additionally returns `sampleSequence`, `sampleUs`, `queryCount`, and
  `aStarCount`. `sampleSequence` starts at `0` and advances once per accepted
  normal one-tick update; `sampleUs` is the accumulated timed
  `NavQueryDirection` duration latched after all active-cell workers join,
  `queryCount` counts timed calls, and `aStarCount` counts calls that entered
  A*. Repeated reads before a newly accepted tick return the same raw sequence
  and values. Paused, burst, non-`1/1` timescale, save, replay, zero-tick, and
  other rejected/no-dispatch paths discard pending raw values. Polling can skip
  raw sequence values; skipped values are unobserved gaps, not rejected-update
  records. The client `query_profile` remains `{}`-only; its response is the
  client entry under `## Client commands`.

Every server `[q,r]` axial grid-coordinate parameter (`coord`) must hold two JSON integer literals, with no fraction or exponent, within the signed 32-bit range; any other component, including a whole-valued `1.0` or `1e0`, fails the command before that coordinate is looked up or used.

Pause/timescale persist on an empty server. They reset only when the last connected client disconnects. A client may join a paused server and render frozen state, but spawning still needs an unpaused tick.

## Durable caveats

- When comparing `query_profile` timings, run and discard a warm-up cohort
  first, take baselines only after values settle, and compare at least five
  cohorts per side, of equal sample count, captured within one process lifetime,
  alternating the sides cohort by cohort when a runtime toggle switches between
  them; a single-condition measurement is one side. Report each side's median
  and range, call a gap inside the cohort-to-cohort range no measurable
  difference, and discard a cohort in which every sample of the measured timer
  row is zero, since the measured work did not run. For the server `NavQuery`
  raw row, retain every observed sequence and gap marker; do not reconstruct
  skipped sequences or use the overlapping `averageUs`/`maxUs` smoother rows as
  cohorts.
- Engine command handlers live under `Engine/Source/Agent`: the shared ones in
  `AgentCommandsShared.cpp`, the client ones in `AgentCommandsClientGeneric.cpp`,
  the server ones in `AgentCommandsServerGeneric.cpp`, and focused command
  modules under `Commands/`. A project's own handlers and its take-overs live
  under the selected project's `Source/Agent`.
