# Plan03 readiness runtime evidence

## Identity and setup

Owned fresh Local-data Sandbox Debug run; HEAD c1aa4847 plus reviewed pending readiness fix.
Claim metadata: Temp/plan03-ready-runtime/claim.json; launch-server.json and launch-client.json.
Input hashes: input-hashes.json; fresh AppData root recorded by launch scripts and commands.
lock-before.json and lock-after.json retain the advancing heartbeat; isolation checks found no client.
All following unqualified artifact names are under Temp/plan03-ready-runtime/.
All commands/replies with UTC, endpoint, request, exit and response: commands.jsonl.
Server load returned paused true/resetToFresh false; server-loaded.json records tick400/clientCount1.
Client settled tick415, two battleships/three omnis/four blasters, no fighters/missiles.
Graphics set_log_level returned effective Debug (graphics-debug.json).

## Matched foundation shots

Row3 BLOCKED pending corrected reference Lighting=false close/wide captures and offline comparison.
close.png and wide.png are new native1920x1080 images; screenshots.json retains exact results.
Each shot has scene/ui/profile before-and-after JSON plus the successful pinned Sun Angle input.
Field of view15/perspective and Disabled completion: ui-controls.json, mode-disabled.json,
disabled-ui.json (Capture disabled:true), commands.jsonl; no radio-checked inference.
Close eye=[69.48355865478516,-160.26315307617188,261.8525085449219], lod0.
Wide eye=[69.48355102539062,-160.26315307617188,1130.2447509765625], lod1.
Both camera basis[0,0], paused415, exact full poses/projected areas/units in corresponding scene JSON.
Cell gates required every subscribed coordinate confirmed/rendered with no pending full state.
No comparison against historical invalid Lighting=true reference images was made.
No pixel inspection was performed: no corrected reference or completed fixture image exists.

## Readiness retest and capture

Point0: Fixture reference, Close, Point,100 control,256 samples,event0; actual sourceCount1.
point0-ui-ready.json, point0-scene-ready.json, point0-profile-ready.json retain pre-request proof.
Request after all subscribed cells ready; no arbitrary residency warmup was added.
point0-progress.jsonl retains every paired scene/profile sample; point0-timeline-summary.json is compact.
First receivers sample elapsed3.032s:0/129600 tiles,87instances,6858595triangles.
Restoration increases remained receivers/0tiles, correlating client.log Restored island mesh lines.
First tracing sample at21:03:47.127416Z/258.241s:120tiles, completed-frame diagnostics156instances.
Later tracing diagnostics161instances/13174622triangles; these counters lag completed framebuffers.
Last successful query21:10:12.2007616Z/643.315s:46920/129600tiles, still tracing/error empty.
No Published scene changed during capture was observed, but the export did not complete.
Process failure HARNESS-F-004 is settled by process-check-latest.json and commands.jsonl tail.
client.log retains Exit main loop and orderly destruction; no cause is inferred and no retry occurred.
capture-inventory.json records no fixture export files; no raw pixel checks could run.
Scratch pixels.ps1 and capture-next.ps1 were prepared but never executed.

## Criterion results

Row3 BLOCKED: corrected Lighting=false reference/offline matched comparison remains outstanding.
Row4 FAIL: HARNESS-F-004 interrupted active point visibility capture; skin pair/offscreen check unexercised.
Row5 BLOCKED: HARNESS-F-004 prevented wide capture and terrain pixel inspection.
Row6 FAIL: HARNESS-F-004 interrupted active point radiometry capture; no three-pixel raw evidence exists.
Row7 FAIL: HARNESS-F-004 prevented active capture export; wide900s deadline was never exercised.
Row8 PASS for whole attempted run: log-scan.json/client.log contain zero required Vulkan/debug patterns.
Row9 runtime logs PASS: log-scan.json records zero desync/CRC/debug patterns for both whole logs.
Row9 overall BLOCKED pending manager-owned merge-aware static proof and completion of fixture coverage.
Raw required frame diff retained row9-frame-diff.txt; no tracked files were edited by this worker.

## Terrain timer

Required1080p comparison-enabled Close fixture island view, paused415; point0-scene-ready.json proves view.
Last eligible Lighting Terrain: current962302us/average983944us/max1018556us, summary JSON.
Earlier eligible observation at197.876s: current752728us/average665062us/max754854us.
These are far above15ms order; retain LightingTerrainPreparationMemoryCost.md and its link.
No4K general performance claim, memory experiment or source performance change was made.

## Release

release.json status pass, survivingPids empty; lock-released.json held:false.
Both exact launch PIDs are absent; no foreign process was stopped.
Whole logs and all evidence were retained. Run ended after terminal process failure, below90min budget.
