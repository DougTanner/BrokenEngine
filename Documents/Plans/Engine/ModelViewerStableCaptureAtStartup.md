<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-29T13:28:31.049Z","dependsOn":[]} -->
# Make the ModelViewer stable capture reachable at default startup

## Context

The documented ModelViewer capture call
(`.agents/skills/agent-harness/references/model-viewer.md` `## Capture`) passes
`-RequireStable`, which makes
`.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1` post F9
until two consecutive captures share a SHA-256 (`Invoke-ViewerCapture`, the
`$hash -ceq $previousHash` test) and otherwise return `capture.unstable` at the
180 s `$CaptureTimeoutSeconds` deadline (`Get-DeadlineOutcome`). At default
startup that condition can never be met, so every acceptance check that runs the
documented call fails with `capture.unstable` whatever the change under test
does.

Observed: a Debug ModelViewer capture run as `## Capture` documents it (fresh
stamped `Temp` app-data path, `-RequireStable`, no `-ModelPath`, so the first
packed scene in the sorted catalog) returned `capture.unstable` with viewer
`exitCode` 0, no crash reports, and no `DEBUG_BREAK at` or error line in
`ModelViewer.log`. The viewer logged 340 completed captures inside the 180 s
window and every written PNG had a distinct SHA-256.

Root cause: at default startup the viewer draws world water whose normal-map
UVs scroll with wall time.

- `Projects/ModelViewer/AGENTS.md` `## Application contracts`: startup uses the
  origin seed with World background enabled; World background is a viewer-owned,
  unsaved control, and no launch option hides it
  (`Projects/ModelViewer/Source/ModelViewerApplication.h:23`,
  `bool mbWorldBackground = true;`; the only writer is the
  `World background` checkbox in `ModelViewerApplication::RenderUi`).
- `ModelViewerApplication::WorldVisible` returns true when flying or when
  `mbWorldBackground` is set, and `engine::RenderFrameMain` in
  `Projects/ModelViewer/Source/ModelViewerApplication.cpp` (`:1135-1137`) calls
  `engine::RenderWaterMain` whenever `WorldVisible()` is true.
- `ModelViewerApplication::Render` (`:752-753`, `:761`) passes the timer's
  elapsed time to `RenderGlobal` and `RenderMainPresentAcquire`.
- `PopulateWaterReducedUv` in `Engine/Source/Graphics/Render/WaterUniforms.cpp`
  (`:83-112`, `:140-149`, called at `:281`) accumulates that time into the
  `fWaterReducedNormalTime*` uniforms.
- `Engine/Data/Shaders/Water/WaterNormalSampling.h` (`:32-34`, `:51-55`) adds
  `speedMult * reducedTime` to every normal-sample UV.
- The default weights and speeds are nonzero
  (`Engine/Source/Ui/WaterWrappersBase.cpp:10-27`, for example
  `gLightingSampledNormalsSpeedOneMinimum` 0.035 and
  `gLightingSampledNormalsWeightOneMinimum` 1.5), so the water's pixels change
  every frame.

`PopulateWaterFoam` and `RenderWaterMain`'s Gerstner wave path also take the
elapsed time. Each would be another time-driven water input on its own.

Pre-existing: `-RequireStable` dates from e349624b (2026-10-24, "Capture
ModelViewer screenshots through the agent harness"), and d84c16e9 (2026-10-26)
added the `RenderWaterMain` call to the viewer. The two contracts have
conflicted since d84c16e9. No caller other than `model-viewer.md` passes
`-RequireStable`, and no live Plan relies on it.

## Design

The fix needs one decision this Plan presents and does not make. The claiming
session presents both options to the user and implements the chosen one only.
The evidence does not clearly favour either option, so the author makes no
recommendation.

- Option A, change the capture contract. Revise `model-viewer.md` `## Capture`
  (and the `-RequireStable` handling in `Invoke-ModelViewerCapture.ps1` if the
  revised contract needs it) so the documented call no longer requires a
  stability that default startup cannot reach. For example, the documented call
  could drop `-RequireStable`, keeping it only for scenes without time-driven
  content. Cost: a default-startup capture loses the settle guarantee
  `-RequireStable` gives, so the first capture may be unsettled. That is the
  case the current text calls "one unsettled capture".
- Option B, give the viewer a startup state with stable pixels. For example,
  add a viewer launch option, parsed beside `--scene-path` in
  `Engine/Source/LaunchOptions.cpp`, that starts with World background disabled.
  The capture script would pass it for a stable capture. `## Application
  contracts` gains the option, and the default startup with World background
  enabled stays unchanged. Cost: a C++ and launch-option change, and a stable
  capture never shows the world. Unverified: nothing has shown that the frame is
  bit-stable with the world hidden. Viewer flight forces the world visible, but
  at startup the viewer is in orbit and the model is in its rest pose. If any
  other time-driven input remains, Option B fails its acceptance criterion.

Exposure. The viewer is a client-only executable and never enters deterministic
game state, so there is no determinism or CRC, wire, replay, `.pack` or
`kiVersion`, or threading exposure. The viewer's World background is unsaved,
so Option B adds no settings or preset format change. Under Option B, LaunchOptions
is shared engine code, so the new option must stay inert in the game client and
server.

Change Workflow tier: Tier 2. Highest trigger: scoped behavior. Option B
changes the ModelViewer's startup behavior and the harness capture tool's
behavior, and touches none of the surfaces Tier 2 excludes. Option A alone is
Tier 1 if it changes documentation only, or Tier 2 if it changes the script.

## Critical files

- `.agents/skills/agent-harness/references/model-viewer.md`
- `.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1`
- `Projects/ModelViewer/AGENTS.md`
- `Projects/ModelViewer/Source/ModelViewerApplication.cpp`
- `Engine/Source/LaunchOptions.h`
- `Engine/Source/LaunchOptions.cpp`

## In scope

- Option A: the `## Capture` text and documented call in `model-viewer.md`,
  and the `-RequireStable` parameter and stability loop in
  `Invoke-ModelViewerCapture.ps1`.
- Option B: one launch option in `LaunchOptions.h`/`.cpp` and its effect on the
  initial `mbWorldBackground` value in `ModelViewerApplication`, the
  `## Application contracts` paragraph that documents viewer launch options, the
  script's viewer argument list, and the `## Capture` text that names the stable
  capture.

## Out of scope

- The water rendering path (`WaterUniforms.cpp`, `WaterNormalSampling.h`,
  `WaterWrappersBase.cpp`) and its time-driven animation.
- The viewer's default startup state: the origin seed with World background
  enabled stays as it is under either option.
- The capture timeout, F9 capture path, close sequence, and result schema of
  the script.
- Agent-harness game-client screenshot paths.

## Acceptance criteria

Live verification runs through `/agent-harness`, `references/model-viewer.md`
`## Capture`, against a freshly built ModelViewer Debug executable. The run
that reproduced the defect was the documented call with `-RequireStable` and
no `-ModelPath`, on a fresh stamped app-data path.

- The `## Capture` call documented after the change, run at that default
  scene, returns code `ok` with `exitCode` 0, empty `crashReports`,
  `windowVisible` false, and no `DEBUG_BREAK at` line in its log.
- Option B only: that result has `stable` true, and the viewer started without
  the new option still begins with World background enabled.
- `model-viewer.md` documents no stability requirement that the documented
  startup cannot meet.

## Notes

- `## Application contracts` makes World background viewer-owned and enabled
  at startup, with no command-line option to hide it. Option B extends that
  contract, and Option A leaves it unchanged.
- Line numbers cite the tree that created this Plan. Use the named symbols if
  they have moved.
