# Lighting strip row 3 baseline: replay steps

All paths are relative to the worktree root `C:\Users\dougt\.claude\worktrees\BrokenEngine\0cb8b7b1-9695-4b87-ab78-8bf06eceacad`.
Build: Debug x64 BrokenEngineSandbox client + server, unedited tree (HEAD `bc69eea5`), data
`Projects\BrokenEngineSandbox\Platforms\VisualStudio2026\Output\Data`.

## Replay inputs

- Server save (exact sim state at tick 400, written by `Setup.ps1`):
  `Temp\lighting-strip-baseline\AppData\Broken Engine Sandbox Server\lighting-strip-baseline-t400` (2547 bytes).
- Client identity that owns the saved fleet (battleship globalId 1):
  `Temp\lighting-strip-baseline\AppData\Broken Engine Sandbox\ClientGuid.bin`.
- Drivers: `Temp\lighting-strip-baseline\Capture.ps1` (load, zoom, timed screenshots, query_profile),
  `Temp\lighting-strip-baseline\Compare.ps1` (per-pixel diff), `Temp\lighting-strip-baseline\Setup.ps1`
  (only to regenerate the save; it is not needed for the after-run).

## After-run steps

1. Use two app-data roots that do not exist yet, for example `Temp\lighting-strip-after\AppData` (server)
   and `Temp\lighting-strip-after\AppData-client` (client). Copy the save into
   `<server root>\Broken Engine Sandbox Server\lighting-strip-baseline-t400` and `ClientGuid.bin` into
   `<client root>\Broken Engine Sandbox\ClientGuid.bin`. Do not copy any other client file
   (no `ClientState.bin`, no `TweaksSettings`), so the camera and sun start from defaults.
2. Claim and launch the server (worker steps 5, 21) with `-AppDataDirectory <server root>`; send server
   `load {"file":"lighting-strip-baseline-t400","pauseAfterLoad":true}` only after the client connects
   (Capture.ps1 does it). Pause resets when the last client disconnects, so never relaunch the client
   after the load without reloading.
3. Launch the client with `-AppDataDirectory <client root>`; retain its launch result `startTimeUtc`.
4. Immediately: client `window_state {"minimized":false}`, then `resize {"width":1920,"height":1080}`
   (reply must be 1920x1080).
5. Immediately run
   `pwsh -NoProfile -File Temp\lighting-strip-baseline\Capture.ps1 -OutDir <after dir> -Owner '<owner>' -Prefix after -ClientStartUtc '<client startTimeUtc>'`
   (defaults: `-SaveFile lighting-strip-baseline-t400 -CloseAtSeconds 60 -WideAtSeconds 75`). It:
   loads the save paused (requires `resetToFresh:false`, server tick 400); waits until client tick,
   camera eye and lastRenderedTick are stable; `wheel {"notches":40}` (clamps to eye height 156);
   at client start + 60 s takes `screenshot` png maxWidth 0 (`after-close.png`); `wheel {"notches":-60}`
   (eye height 665.117, lod 1); at start + 75 s takes `after-wide.png`; then `query_profile {}`.
6. Compare: `pwsh -NoProfile -File Temp\lighting-strip-baseline\Compare.ps1 -A Temp\lighting-strip-baseline\baseline-close.png -B <after dir>\after-close.png`, same for wide.

## Recorded camera and sim state

| Shot | Client time | Client tick / confirmed / lastRendered | Eye (cell [0,0] m) | Visible area (l,t,r,b) | LOD |
| --- | --- | --- | --- | --- | --- |
| close | start + 60 s (12:28:18.18Z) | 415 / 400 / 413 | 69.48, -160.26, 156 | 3.96, -121.97, 135.25, -206.65 | 0 |
| wide | start + 75 s (12:28:33.17Z) | 415 / 400 / 413 | 69.48, -160.26, 665.117 | -209.92, 4.24, 348.62, -356.02 | 1 |

Client start `2026-10-29T12:27:18.1704020Z`. Server paused at tick 400 throughout both shots; the client
predicts to tick 415 (ceiling = 400 + 16 lead - targetBehind 4 + 3 slack) and renders 413.
Scene at both shots: counts battleships 2, omnis 3 (random alignment, shields), blasters 4, fighters 0,
missiles 0; focused fleet battleship globalId 1. The two injected explosions (tick 400) have expired by
render tick 413, so no explosion or smoke is on screen. UI state `kNone`, Tweaks overlay hidden,
lighting comparison not enabled (`query_profile` `lighting.capture.status` `not-run`, `captureRequested` 0).

## Setup that produced the save

Server `reset`, `pause true` (tick 1), then one `inject_payload` batch on coord [0,0]: at tick 200 three
`SpawnOmni` `alignment:"random"`, `eController:"RandomSpawn"` at [20,-60], [60,-170], [110,-60] and three
`SpawnFighter` `alignment:"fleet"`, `eController:"RandomSpawn"` at [30,-140], [50,-90], [90,-150]; at tick
400 two `SpawnExplosion` `iTypeIndex:0`, `lifecycleFlags:4`, `iTrailCount:6`, `iParticleCount:16` at
[50,-25] and [95,-60]. Unpause at 1/1 to tick 358, pause, timescale to 1/32, exact single steps to 400,
timescale back to 1/1, `save {"file":"lighting-strip-baseline-t400"}`. A fresh reset is not tick-for-tick
reproducible (two setup runs put battleship 1 at different positions at tick 400), so the after-run must
use the save, not this setup.

## Sun angle caveat

`CameraBase` advances `mfSunAngle` every client frame (0.01 rad/s by day, 0.075 rad/s by night, 356 s
cycle) even while the server is paused; `ResetForSession` sets it to 1.8 at session start. Untimed captures
of the same paused state differed completely (`repeat-close.png`/`repeat-wide.png` vs an earlier capture:
100% of pixels differ, mean channel delta 143). The timed schedule above fixes the capture points relative
to client start, but its repeatability (start-to-session-start offset varies by a second or two) was not
measured before the run budget ended.
