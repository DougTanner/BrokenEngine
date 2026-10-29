# Plan03 remaining runtime evidence

## Scope and identity

Continuation on a66da68abfd942591d92c9d69489e5e240b927e4; no source edits.
Fresh AppData did not exist before launch. Input save SHA256:
42DD5DA00A924D39B95D985ACD09D9F104972BBD0946DBEF2FFCB1154E0AAF55.
Original ClientGuid SHA256:
2C089A51FE21DF3AD3A7D987A4640E319AD84A10753676A8CF6F6BF528648BB0.
Latest build envelope: Temp/AgentBuildEnvelopes/compile-20261029T213155713Z-31876.md
## Foundation restore builds; all four targets passed using foundation Local data.
Claim and launch receipts: Temp/plan03-complete-runtime/claim.json,
launch-server.json, launch-client.json. Lock heartbeat before/after receipts retained.
Fresh save loaded pauseAfterLoad: resetToFresh false, server tick400;
client prediction415, counts battleships2/omnis3/blasters4/fighters0/missiles0.
Viewport resize returned1920x1080; window restored and held visible.
Graphics log level effectiveDebug; island-ready.json Statussuccess Codeready Readytrue.
Every capture checks all subscribed cells confirmed>=0, snapshot>0, rendered>=0,
pendingFullStateTick null, and retains scene/profile timeline.
Row3 was settled by the prior approved fallback, not repeated in this continuation:
Temp/lighting-matched-reference/reference-off-runtime-evidence.md
## Row3 comparison and visual verdict.

## Capture requests and deadlines

Wide1000 density/event0/256 samples requested first; receivers wait included in elapsed time.
See commands.jsonl for exact request UTC and wide1000-progress.jsonl for full timeline.
No prewarm was added after selecting fixture controls.

## Exports and pixels

Stopped by user-directed camera revision; see final sections below.

## Terrain GPU time

Lighting Terrain already measured far above15ms at1080p; retain memory-cost document.
Full timer samples are in each progress row profile.gpuTimers.

## Logs and release

Stopped by user-directed camera revision; see final sections below.

## Supersession and historical deadline

The user replaced the6000m wide camera requirement with lowest/default gameplay zoom.
Manager ordered immediate cancellation/release and no further captures or source/docs edits.
The driver was interrupted, then Disabled##LightingMode click returned found/enabled true.
No capture failure was observed before the user-directed cancellation.
The last buffered observation had already crossed the old deadline:
2026-10-29T21:55:12.4687004Z, elapsed905.6803191s, tracing54960/129600, error empty.
This historical old-criterion time failure is HARNESS-F-004; it is not a crash or scene failure.
Elapsed starts immediately before Capture click; click response UTC21:40:09.0402514Z.
The pre-click start was21:40:06.7883813Z; response-to-observation903.428449s also exceeds900.
No capture.json or seven-file export exists, so metadata wallMs cannot be compared.
Readiness stayed receivers through363.5s and was tracing960tiles by375.3s.
Subsequent tracing progressed approximately102tiles/second with instances186 stable.
Complete timeline: Temp/plan03-complete-runtime/wide1000-progress.jsonl.
Exact deadline snapshot: Temp/plan03-complete-runtime/wide-deadline.json.

## Final criterion disposition

Rows4/5/6 BLOCKED: user-directed camera supersession stopped this run before pixel captures.
Row7 old requirement FAIL: HARNESS-F-004; subsequent camera revision belongs to manager.
No finished.png was loaded or inspected because this request never reached export.
No radiometry or shadow/skin pixel pass is claimed. No offscreen instance proof obtained.
Rows8/9 runtime logs PASS for the entire attempted scenario through release.
Row9 static proof belongs to manager; this run does not claim to have performed it.
Row3 retains its prior approved fallback citation; it was not repeated here.
No source or tracked documentation edits; four pre-existing tracked plan notes remain modified.

## Final terrain GPU time

At905.680s, Lighting Terrain current1168.405ms/average1163.397ms/max1198.686ms.
Enclosing Main render current1179.524ms/average1174.320ms/max1208.763ms; fps1.
1920x1080 wide island view, comparison reference active; memory document must remain.
This is the required1080p criterion, not a4K performance investigation.

## Final logs and owned release

Full client.log/server.log retained under Temp/plan03-complete-runtime/.
rg found zero DebugUtilsCallback, CheckVk failed, VK_ERROR_DEVICE_LOST,
DEBUG_BREAK at, SendDesyncReport, RecoverFromDesync or LogDifferences CRC matches.
Release receipt release.json statuspass/codeok, crashFindings[], survivingPids[].
Both quits exit0/acceptedtrue. Server2332 already-absent after quit;
client42664 exited-after-quit. Both stopAttemptedfalse. Lock release attemptedtrue/exit0.
No remaining owned process, harness driver or heartbeat task.

