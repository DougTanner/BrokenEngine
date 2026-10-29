# Plan03 final runtime evidence

## Result

Status: NEEDS_ACTION; Criterion results: 2/7 passed (rows8/9 runtime log checks).
Rows3/4/7 FAIL; rows5/6 BLOCKED after the required first-capture failure stop.
No source, tracked-document, commit or branch changes; tracked tree remains clean.
Only ignored Temp/plan03-final-runtime/ and this evidence file were written.

## Provenance and release

HEAD c1aa4847eb49a70ebb3df5c143fc7b300913c90b; branch lighting-foundation-work.
Build input: Temp/AgentBuildEnvelopes/compile-20261029T202606996Z-9240.md ## Build envelopes.
Claim and launch used that build's worktree Local Data directory, never older Shared reference data.
claim.json preserves full claim metadata; owner c6337189-9dbf-46ae-83e7-36d151cecb9c.
lock-before.json/lock-after.json prove heartbeat advanced20:32:13.597Z to20:32:45.920Z.
Fresh AppData root did not exist before setup; only patched save/original identity were copied.
input-hashes.json verifies the preserved save/identity SHA256; save header256/2547bytes.
Server launch PID25236 at20:32:27.2694005Z; client PID23792 at20:32:35.1490264Z.
Both launch process checks/pings PASS; isolation enumeration found no foreign client/viewer.
server-loaded.json: resetToFresh:false load, tick400 paused:true clientCount1.
Client settled tick415; counts battleships2/omnis3/blasters4/fighters0/missiles0.
release.json retains full successful release payload, crashFindings[], survivingPids[].
Both quits accepted; server already-absent/client exited-after-quit; stopAttempted:false both.
lockRelease attempted:true/exitCode0; no foreign process stopped; tree-status.txt empty.

## Matched comparison row3

Reference provenance: Temp/lighting-matched-reference/reference-runtime-evidence.md ## Shots.
Reference PNGs: Temp/lighting-matched-reference/runtime-main/{close,wide}.png.
Candidate PNGs: Temp/plan03-final-runtime/{close,wide}.png.
commands.jsonl retains every request/response and UTC; screenshots.json retains path,width,height.
Both candidate shots1920x1080 PNG/maxWidth0; Tweaks hidden, kPaused/uiState kNone.
FOV wrapper15.000000; Orthographic Projection unchecked; explicit sun1.8 before each shot.
Disabled mode input succeeded; UI radio rows expose checked:false even after accepted inputs.
The ordinary disabled state is also consistent with zero fixture sources before fixture activation.
shot-summary.json retains actual pose, visibleArea, LOD, counts, readiness and file bytes.
Close eye[69.4835434,-160.2631531,261.8525085],lod0; time20:33:10.2504448Z.
Wide eye[69.4835510,-160.2631531,1130.2447510],lod1; time20:33:13.5259624Z.
Both shots subscribed only[0,0], confirmed400/snapshot16/rendered413/no pendingFullStateTick.
Projected view matches reference to small floating differences; full before/after files retained.
Compare.ps1 exact preserved comparator: close over8=53.884%; wide over8=43.028%.
Full results: compare-close.txt and compare-wide.txt.
Both candidate/reference pairs were deliberately loaded for row3's required visual inspection.
Same islands, water, units and PAUSED UI; candidate lacks reference blue/pink ship glow and
reference wide red local glows on water and island. This fails the no-missing-local-glow clause.
One permitted retake was not exercised: first fixture capture subsequently failed, requiring stop.
No additional reference-branch run was authorized or performed.

## Fixture gate and capture failure rows4 and7

capture.ps1 invoked -Name point0 with Point/Close/event0/256samples inputs, all accepted/enabled.
Point source count independently read as1. Client remained paused/Tweaks visible/1920x1080.
island-ready.json: Status success/Code ready/Ready true, two stable footprint samples.
point0-scene-ready.json: all6 subscribed cells confirmed400/snapshot16/rendered413/no pending.
Coordinates[0,0],[0,1],[1,0],[-1,1],[1,-1],[-1,0]; global tick415 and kPaused.
point0-profile-ready.json and point0-ui-ready.json retain publication/control context.
Graphics Debug requested and effective Debug (graphics-debug.json).
point0-progress.jsonl pairs full describe_scene/query_profile after every capture sample.
Capture requested about20:34:35.155Z; failed20:35:21.691224Z after46.5356454seconds.
Last tracing20:35:18.0853836Z:9840/129600tiles,87instances,6858595triangles.
Failure9960/129600tiles:90instances,7109498triangles; error Published scene changed during capture.
Every cell remains confirmed400/snapshot16/rendered413/no pending through that transition.
failure-transition.json retains the full last-good/first-failed scene and profile pair.
client.log:121-122 records Restored island mesh CRC13385203107645622817 indices83526,
and CRC11876020618390489856 indices585657. These lines are retained, not a root-cause verdict.
The observed triangle increase250903 equals2*(83526/3)+(585657/3), useful for diagnosis.
No silent retry, guard change, snapshot patch, driver cancellation or source edit was performed.
Capture directory empty; no raw,finished.png,capture.json exports; inventory in capture-inventory.json.
No panel zero/motion, skin animation or offscreen traceOnlyInstances acceptance was settled.
Remaining point119,density100 event0/event119 and wide1000 event0 captures were not attempted.

## Rows5 and6 residual checks

Row5 BLOCKED: no wide1000 finished.png exists because run stopped at first actual capture failure.
Row6 BLOCKED: no complete isolated-point raws exist for the three-pixel radiometry calculation.
Row7 FAIL: attempted capture produced none of its7 required export files; wide15minute check unrun.

## Terrain timing and log checks

At point0 fixture Close comparison enabled with island footprints present,1920x1080:
point0-profile-ready.json Lighting Terrain current494855us/average375152us/max505708us.
final-profile.json current516833us/average502900us/max526908us.
This is plan03's1080p observation, far above15ms order; retain memory-cost document/link unchanged.
Whole launched-process logs scanned after release; log-scan.json retains each pattern and count.
Zero DebugUtilsCallback,CheckVk failed,VK_ERROR_DEVICE_LOST,DEBUG_BREAK at matches in client.
Zero Validation Error,Validation Warning,VUID- matches in client and server.
Zero SendDesyncReport,RecoverFromDesync,LogDifferences CRC,DEBUG_BREAK at matches in either log.
Rows8/9 runtime log checks PASS for the entire attempted scenario; no full completed capture tested.
Row9 static merge-aware examination is manager's previously accepted evidence, not repeated here.
No replay determinism test was requested or run.
