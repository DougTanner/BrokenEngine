# Plan03 patched-save runtime evidence

## Identity

Worktree: C:/Users/dougt/.claude/worktrees/BrokenEngine/
0cb8b7b1-9695-4b87-ab78-8bf06eceacad. Branch: lighting-foundation-work.
HEAD: a49b61276f424515ff80f746c58ac89f4e73f520.
Diff baseline: cd6d410c064a5a5f905bf68efaf28493ab6fc33a.
Build: Temp/AgentBuildEnvelopes/compile-20261029T192103385Z-5736.md,
## Camera fix Debug x64 builds. Sandbox client/server Debug x64 Local.
Only pre-existing tracked change: user-authorized baseline save header update.
No source, tracked documentation, build, commit, or diagnosis work performed.
All filenames below are within Temp/plan03-patched-runtime unless stated otherwise.

## Claim and setup

Invoke-HarnessClaim.ps1 used assigned absolute RepositoryRoot, session
plan03-patched-runtime, Project BrokenEngineSandbox. Exit0, status pass/code ok.
Owner: 6c23178f-d01f-40c8-a7f3-1cde959c48ae; claimedAt 2026-10-29T19:45:32.815Z.
AgentHarness: Tools/AgentHarness/Platforms/VisualStudio2026/Output/AgentHarness.exe.
Data: Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data in worktree.
prelaunch-lock.json and postserver-lock.json show heartbeat advanced from
19:45:32.815Z to 19:45:51.243Z. No takeover or foreign-process operation occurred.
No client/viewer was running at initial or immediately pre-client isolation checks.
Fresh AppData and AppData-client roots did not exist before creation.
Copied patched authoritative save and original ClientGuid.bin only, no settings/state.
Copied save SHA256: 42DD5DA00A924D39B95D985ACD09D9F104972BBD0946DBEF2FFCB1154E0AAF55.
Source save: Temp/lighting-strip-baseline/AppData/Broken Engine Sandbox Server/
lighting-strip-baseline-t400. Original .version254.bak untouched.
server-launch.json: pass/ok, PID7604, start2026-10-29T19:45:49.7558660Z, tick1.
client-launch.json: pass/ok, PID27304, start2026-10-29T19:45:57.8833939Z, tick0.
Both launches' ping and processCheck passed with findings[].
Explicit logs: server-agent.log and client-agent.log; state: process-check.json.

## Saved state adoption

capture-output.txt records load pauseAfterLoad:true, resetToFresh:false, paused:true.
Server status tick400, clientCount1, activeCoords[[0,0]], nextGlobalId2.
Server frame counts: battleships2, omnis3, blasters4, fighters0, missiles0.
Saved identity owns fleet battleship globalId1. saved-battleships.json records its
server tick400 local[72.4920959473,-158.7055664063,6], uuid1407374883553281.
Other battleship local[93.8143386841,-171.9327850342,6]. saved-omnis.json records
three complete server rows, including identities, positions, armor, shield and velocity.
Client timed shots: tick415, confirmed400, tail415, lastRendered413; counts unchanged.

## Row 3 baseline comparison

restored.json confirms minimized:false; resize.json confirms 1920x1080.
Original Capture.ps1 invoked with launch startTimeUtc and defaults +60/+75 seconds.
after-close.png screenshot result: 1920x1080, +60s at19:46:57.8978050Z.
after-wide.png screenshot result: 1920x1080, +75s at19:47:12.8882984Z.
The two -scene.json files retain exact camera, units, cells and island state.
Close eye[69.483566,-160.263153,261.852509]; required historical height156.
Wide eye[69.483551,-160.263153,1130.244751]; required historical height665.117.
Close bounds[4.908565,-122.482765,134.119308,-205.823242], lod0.
Wide bounds[-209.132843,2.814930,347.309906,-356.088623], lod1.
Compare.ps1: close over8=2073388/2073600 (99.990%); wide2036187 (98.196%).
Exact stdout: compare-close.txt and compare-wide.txt. No retake performed.
Pixel inspection loaded both original baseline and new close/wide images to assess
preserved visuals. Terrain and unit arrangement appear the same; historical close
is dark/gold while new close is daylight. Wide sunlight/shadow directions differ.
No passing local-glow comparison can be asserted under these lighting conditions.
BLOCKED: original timed driver no longer establishes specified camera/sun conditions.
The historical sun-angle caveat permits slight drift, not this proven setup mismatch.
Authority retained: unchanged acceptance; observed current behavior was not substituted.

## Fixture capture failure

Point0.ps1 drives label-addressed Lighting controls after describe_ui; first capture only.
pause-client.json and point0-ui.json show client kPaused, Tweaks visible, 1920x1080.
fixture-mode/close/point/samples.json show enabled/found controls for Fixture reference,
Close, Point, 256. point0-ui.json retains Event index control; default event0.
fixture-scene.json records fixture camera eye[0,-600,900], islands and paused gameFlags.
point0-request.json shows Capture enabled/found. No second request or retry was issued.
point0-progress.jsonl contains every profile sample with UTC and elapsed seconds.
At19:48:33.941Z: tracing240/129600 tiles, instances87, triangles6858595, sources1.
At19:49:39.884Z: tracing15960/129600, instances87, triangles6858595.
At19:49:43.089Z: failed15960/129600, elapsed69.654s, instances89, triangles7143790.
Reported error: Published scene changed during capture.
Point0.ps1 exited1 at its failure branch. No transport or process-check failure occurred.
No complete capture directory, raws, finished.png or capture.json was produced.
CaptureFixture.ps1 and CheckRaw.ps1 were prepared but never executed.
No source cause inferred, runtime workaround attempted, or criterion loosened.

## Log checks

Both full explicit process logs were scanned after release; log-scan.txt retains matches.
Client:23 DebugUtilsCallback messages and23 DEBUG_BREAK at lines; server:0 debug breaks.
First callback client-agent.log:117, VUID-VkWriteDescriptorSet-descriptorCount-10776.
It reports descriptorCount17 at binding7 with VERTEX stage spilling into binding8 FRAGMENT.
First debug break:121; immediately preceding message is that descriptor validation message.
Other observed messages include VUID-VkWriteDescriptorSet-dstArrayElement-00321 and
WARNING-Shader-OutputNotConsumed. Last debug break:237.
No CheckVk failed or VK_ERROR_DEVICE_LOST matched in client log.
Neither log contains SendDesyncReport, RecoverFromDesync or LogDifferences CRC.
No get_logs ring-buffer evidence was used. Whole requested scenario did not complete.

## Terrain GPU timing

final-profile.json has comparison active and islands present in final-scene.json.
Lighting Terrain index18 currentUs675723, averageUs588736, maxUs675723.
These are advisory 1920x1080 observations: current675.723ms, average588.736ms.
Required performance investigation fullscreen4K measurement was not established.
Timing criterion BLOCKED after required capture failure; no eligible timing substituted.
Memory-cost document and handoff link untouched; manager decides disposition.

## Criterion results

Criterion results: 0/7 passed
Row3 BLOCKED: ## Row 3 baseline comparison.
Row4 FAIL: HARNESS-F-001. Panel and skin raws unavailable; traceOnlyInstances remained0.
Row5 BLOCKED: ## Fixture capture failure; wide finished.png unavailable.
Row6 BLOCKED: ## Fixture capture failure; no raws for three-pixel radiometry.
Row7 FAIL: HARNESS-F-001. Required capture did not export; wide capture not attempted.
Row8 FAIL: HARNESS-F-002.
Row9 runtime FAIL: HARNESS-F-002, due to exact no-DEBUG_BREAK criterion.
Rows1/2 and row9 static remain manager-owned; not counted here.

## Findings

HARNESS-F-001 Required ## Fixture capture failure: required point capture failed.
HARNESS-F-002 Required ## Log checks: validation callbacks and debug breaks violate rows8/9.
Both are observations, not diagnoses. No process-check failure was observed.

## Release proof

release.json: status pass, code ok, crashFindings[], processCheckNote null.
Both quits exitCode0, accepted:true, result{}.
Server7604 already-absent, stopAttempted:false.
Client27304 exited-after-quit, stopAttempted:false. survivingPids[].
lockRelease attempted:true, exitCode0, detail empty. final-lock.json: held:false.
No owned resource remains; no foreign resource was modified. All evidence retained.
Final git status matches initial authorized save-header change only.
Claim-to-release duration approximately6minutes; 90-minute overall budget not exhausted.

## Residuals

Resolve observed capture/validation failures through manager's diagnosis workflow.
Reconcile row3 camera/sun preconditions with unchanged acceptance before a valid retest.
Rows3-9 remain incomplete. Terrain timer remains ineligible for requested4K decision.
No wide15-minute trial, panel119, density100 skin pair, or offscreen rig was attempted.
