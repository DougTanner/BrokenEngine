# Foundation zoom smoke evidence

## Run identity and commands

2026-10-29 UTC; worktree 0cb8b7b1-9695-4b87-ab78-8bf06eceacad; target BrokenEngineSandbox Debug Local.
Build: Temp/AgentBuildEnvelopes/compile-20261029T220641317Z-14916.md ## Normal/minimum fixture camera builds.
Claim owner 5d07a714-3a6a-439c-ae75-c5d592302992, session foundation-zoom-smoke, claimedAt 22:13:17.899Z.
Claim data: own worktree Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/Output/Data.
Claim harness: own worktree Tools/AgentHarness/Platforms/VisualStudio2026/Output/AgentHarness.exe.
Isolation enumerations found no foreign client/viewer. Server PID33596 start22:13:26.3744829Z; client PID28260 start22:13:37.1019200Z.
Fresh AppData root Temp/foundation-zoom-smoke/AppData did not exist before setup.
Copied original ClientGuid.bin and patched lighting-strip-baseline-t400 from Temp/lighting-matched-reference/inputs.
Save 2547bytes SHA256 42DD5DA00A924D39B95D985ACD09D9F104972BBD0946DBEF2FFCB1154E0AAF55.
Exact commands: setup.ps1 and check.ps1 under Temp/foundation-zoom-smoke. Every response is retained alongside those scripts.
Heartbeat advanced from22:13:17.899Z to22:13:38.787Z (lock-before.json/lock-after.json).

## Criterion results

6/6 passed: save load, Lowest preset, Default preset/event stability, viewport, logs, release.
Save: load.json resetToFresh=false, paused=true; status.json tick400/clientCount1/activeCoords[[0,0]].
Client readbacks tick415, clientGridCoord/basisCoord[0,0], lod0.
Lowest: lowest-click.json found/enabled true; lowest-event0.json successful. lowest-scene.json camera.eye[0,0,261.8525085449219], visibleArea[-64.53076934814453,37.75167465209961,64.38672637939453,-45.3996467590332].
Default: default-click.json found/enabled true; default-scene.json camera.eye[0,0,517.7050170898438], visibleArea[-127.58120727539062,74.63734436035156,127.29643249511719,-89.75782775878906].
Default event119: default-event119.json successful; default-event119-scene.json camera byte-equivalent to event0.
Expected eyes approximately261.85254/517.70509 from gameplay150/300 framing and default FoV15deg; observed float error<0.0001m.
Source corroboration: LightingFixture.cpp MakeCamera uses default FoV, Lowest flight-plane target, eye directly above target and up[0,1,0]; CameraBase.cpp comparison binding consumes pose eye/target/up/FoV/shared clips/framing. These unexposed orientation/framing fields are source corroboration, not claimed query fields.
UI labels Lowest zoom/Default zoom present; Close/Wide absent in ui-lighting.json. Radio checked=false is not used as evidence.
profile.json lighting sources100/instances41/traceOnlyInstances2/triangles2211562; capture.status not-run, no error, zero tiles.

## Images

lowest-image.json: lowest.png, width1920 height1080.
default-image.json: default.png, width1920 height1080.
Both full-resolution PNGs under Temp/foundation-zoom-smoke were loaded for the camera-framing pixel check. UI shows selected Lowest then Default; terrain/water and fixture solids visibly enlarge at Lowest. Gray fixture solids and water remain visible; no lighting-quality, radiometry, whole-scene visibility or reference export verdict is claimed.
No Capture activation, full reference export, benchmark, 15-minute test or exhaustive event run was performed.

## Logs and owned release

Whole client.log/server.log: zero matches for DEBUG_BREAK at, CheckVk failed, VK_ERROR_DEVICE_LOST, DebugUtilsCallback, SendDesyncReport, RecoverFromDesync, LogDifferences; broad Vulkan/error/CRC/desync search found only Vulkan SDK/layer informational lines.
release.json status pass/code ok at22:14:38Z, crashFindings[], survivingPids[].
Both quit exitCode0/acceptedtrue. Server33596 already-absent; client28260 exited-after-quit; stopAttemptedfalse for each. lockRelease attemptedtrue/exitCode0.
Claim-to-release approximately81s; runtime pair approximately61s.
No source/tracked-document edits; pre-existing pending changes retained.
