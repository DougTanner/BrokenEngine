<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-24T18:14:51.996Z","dependsOn":[]} -->
# Teach /agent-harness to launch and screenshot the ModelViewer

## Context

`/agent-harness` drives only a game project's server and client through the loopback agent channel
(`.agents/skills/agent-harness/SKILL.md`, `references/worker.md`, `references/launch.md`). The ModelViewer
has no agent transport: with `--agent-port` it stays hidden, suppresses startup dialogs, and "does not start
generic agent transport" (`ModelViewer/AGENTS.md` `## Application contracts`). So no harness recipe can
launch or capture it, and the acceptance checks that compare ModelViewer screenshots have no runnable recipe:
`Documents/Plans/Engine/RemoveModelViewerDefineFeatureSwitches.md` A5 and the two ModelViewer world Plans that
follow it (`Documents/Plans/Engine/ModelViewerModelOwnSpace.md`, `Documents/Plans/Engine/ModelViewerModelOverWorld.md`).

Recorded decision (user choice during planning, agent-recorded): add a small Plan teaching the harness to launch
and screenshot the ModelViewer, and make Phase 1 wait for it (Phase 1 carries this Plan in `dependsOn`).

The viewer already has everything a capture needs, so this Plan adds no C++:

- F9 queues one PNG capture of the presented swapchain image under the viewer's app-data `Screenshots`
  directory, at full framebuffer size, and logs
  `ModelViewer screenshot complete sequence=<n> token=<t> result=<json>` once the PNG is written; the JSON holds
  `path`, `width`, `height`, or `error` (ModelViewer/Source/Main.cpp:64-75, 231-256, 280-289;
  Engine/Source/Graphics/Screenshot.cpp:176-197). A second F9 while one is outstanding logs `busy` and is dropped.
- `ImGui_ImplWin32_WndProcHandler` returns 0 for `WM_KEYDOWN` (ThirdParty/imgui/backends/imgui_impl_win32.cpp:733-775),
  so a posted `WM_KEYDOWN` `VK_F9` reaches the viewer's own handler. Posting to the viewer's window is the
  "send inputs programmatically to the viewer" route `ModelViewer/AGENTS.md` requires; it never activates the
  window, moves the pointer, or injects a desktop-global key.
- With `--agent-port` the viewer never calls `ShowWindow` (Main.cpp:195-208) and renders into the hidden window's
  swapchain, the same path its pre-show startup frames already use (Main.cpp:191-194). `WM_CLOSE` sets the quit
  flag (Main.cpp:89-91), so the viewer exits through its orderly path, saving graphics settings.
- The window class name is `kGameName`, `Broken Engine Model Viewer` (ModelViewer/Source/Game.h:6,
  Main.cpp:151); `--log-file` and `--app-data-directory` are honored (Main.cpp:305-312). Startup loads graphics
  settings and lighting preset W from app data (Main.cpp:140; ModelViewerApplication.cpp:56, 148-169), so a
  fresh app-data directory gives compiled defaults and makes two runs comparable.
- The data root is compiled into the viewer (`ModelViewerDataRoot.h`, `.agents/skills/compile/references/runtime-data-mode.md`),
  so no data directory is passed. Executables: `ModelViewer/Platforms/VisualStudio2026/Output/ModelViewer.Debug.exe`
  (Debug) and `ModelViewer.exe` (Release) (ModelViewer.vcxproj:36-39).
- Model textures stream after the first draw (Engine/Source/Graphics/Objects/ModelPipeline.cpp:183-200), so an
  early capture can show placeholders; a capture is settled once two consecutive captures are identical.
- The viewer binds no port, so a capture does not take the harness lock, which guards the client/server ports
  and slot.

## Design

New script `.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1` (PowerShell 7, the style of
`Invoke-HarnessLaunch.ps1`: `$ErrorActionPreference = 'Stop'`, `Set-StrictMode -Version Latest`, one compact JSON
object on stdout). Parameters:

- `-RepositoryRoot` (mandatory) — the adopted worktree; the executable is resolved beneath it.
- `-AppDataDirectory` (mandatory, absolute) — must not exist; the script creates it, so every run starts from
  compiled defaults.
- `-LogFile` (mandatory, absolute).
- `-Configuration` — `Debug` (default) or `Release`.
- `-RequireStable` — capture until two consecutive captures have identical file bytes (stb's PNG writer is
  deterministic for identical pixels); without it, return after the first capture.
- `-TimeoutSeconds` — default 180, covering launch through the last capture.

Sequence:

1. Resolve the executable; absent → exit 2 `capture.executable-missing`. Existing `-AppDataDirectory` → exit 2
   `capture.app-data-exists`. Create the directory.
2. `Start-Process` the executable with `--agent-port 27103 --log-file <LogFile> --app-data-directory <AppDataDirectory>`
   and working directory `<AppDataDirectory>`. Port 27103 is never bound; the value only selects the viewer's
   agent mode and stays clear of the harness ports.
3. Find the window with Win32 `FindWindowEx` over top-level windows of class `Broken Engine Model Viewer`, keeping
   the one whose `GetWindowThreadProcessId` is the started PID (P/Invoke through `Add-Type`), polling until found.
4. Capture loop: `PostMessage(hwnd, WM_KEYDOWN, VK_F9, 1)` (repeat count 1, previous-state bit 30 clear), then poll
   the log file for the next `ModelViewer screenshot complete sequence=<n>` line and parse its `result=` JSON. An
   `error` member → `capture.failed`. Record `{path, width, height, sha256}` and `IsWindowVisible(hwnd)`. With
   `-RequireStable`, repeat until two consecutive captures share `sha256`. Post the next F9 only after the previous
   completion line.
5. Close: `PostMessage(hwnd, WM_CLOSE, 0, 0)`, wait up to 60 seconds for the exact PID; if it is still alive, stop
   that exact PID with `Stop-Process -Id` and return `close.forced`.
6. If the process exits before step 5, return `process.unexpected-exit` with its exit code and each crash report
   that exists: `<AppDataDirectory>\Broken Engine Model Viewer\Broken-Engine-Model-Viewer-Crash-Report.txt` and
   `<AppDataDirectory>\Crash-Report.txt` (the naming in `references/launch.md` `## Launch`; the directory is
   fresh, so any report is this run's). If the timeout ends first, close as in step 5 and return
   `capture.timeout`, or `capture.unstable` when `-RequireStable` captures completed but never matched.

Output: `schemaVersion` `broken-engine-model-viewer-capture/v1`, `status`, `code`, `message`, `pid`, `exitCode`,
`captures` (every capture in order), `stable` (`true`/`false`, `null` without `-RequireStable`), `windowVisible`
(true if any capture saw the window visible), `logFile`, `crashReports`. Exit 0 `ok`; exit 2 for
`capture.executable-missing`, `capture.app-data-exists`, `capture.timeout`, `capture.unstable`,
`process.unexpected-exit`, `close.forced`; exit 1 for `capture.failed` and `internal.error`.

New reference `.agents/skills/agent-harness/references/model-viewer.md`, `## Capture` section: the build
prerequisite (latest `/compile` ModelViewer result for the configuration), the one-call form
`pwsh -NoProfile -File .agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1 -RepositoryRoot '<absolute adopted worktree>' -AppDataDirectory '<absolute adopted worktree>\Temp\ModelViewerCapture-<UTC stamp>' -LogFile '<absolute adopted worktree>\Temp\ModelViewerCapture-<UTC stamp>.log' [-RequireStable] [-Configuration Release]`
run synchronously in the foreground, how to classify each code (each non-`ok` code is a `HARNESS-F` finding or
a `BLOCKED` criterion under `SKILL.md` `## Handoff`; `capture.executable-missing` routes `/compile` ModelViewer),
that `windowVisible` must be `false`, the `DEBUG_BREAK at` search of `-LogFile` per worker step 42's rule, that
pixel assertions are scripts under `Temp` per the worker `## Rules` image rule, and that no claim, launch script,
process check, or release applies.

Skill wiring:

- `SKILL.md` `## When to use`: the screenshot trigger names the ModelViewer too. `## Inputs`: a ModelViewer
  scenario needs the latest `/compile` ModelViewer result for its configuration and no game project build.
  `## References`: one row for `references/model-viewer.md`, private like `worker.md`.
- `references/worker.md` step 3: a scenario that targets the ModelViewer runs `model-viewer.md` `## Capture` in
  place of steps 3-46, then returns the handoff.

## Critical files

- `.agents/skills/agent-harness/scripts/Invoke-ModelViewerCapture.ps1` (new).
- `.agents/skills/agent-harness/references/model-viewer.md` (new).
- `.agents/skills/agent-harness/SKILL.md`, `.agents/skills/agent-harness/references/worker.md`.

## Risk triggers and invariants

- Tier 2: scoped tool behavior inside one skill package; no C++, shader, data, or project change, and no change to
  the client/server harness path.
- The capture never activates or shows the viewer, never moves the pointer or posts desktop-global input, and
  stops only the exact PID it started.
- The capture holds no harness lock and touches no port, process-check state, or another session's process.

## In scope

- The new script and reference above, complete as specified.
- `SKILL.md` `## When to use` screenshot bullet, `## Inputs` (one sentence), `## References` (one row).
- `references/worker.md` step 3 (one bullet).

## Out of scope

- Any ModelViewer, engine, or AgentHarness C++ change, including an agent transport or new launch option for the
  viewer.
- Driving viewer UI, pointer drags, or keys other than F9 and `WM_CLOSE`.
- Changes to the client/server launch, claim, process-check, or release scripts, or to `/compile`.
- Editing any Plan's acceptance text (Phase 1 already points at this capability).

## Acceptance criteria

- A1 `/external-skill-creator` validate mode on `.agents/skills/agent-harness` returns `PASS`.
- A2 With a ModelViewer Debug build from `/compile`, `/agent-harness` runs the `model-viewer.md` recipe with
  `-RequireStable`: exit 0, `code` `ok`, `stable` true, the last capture's PNG exists with `width` 1280 and
  `height` 800 (the viewer's default extent, Main.cpp:161-165), `windowVisible` false, `exitCode` 0 and the PID
  gone afterwards, and the log has no `DEBUG_BREAK at` line.
- A3 A second run with an existing `-AppDataDirectory` returns exit 2 `capture.app-data-exists` without starting
  a process.
- A4 A script under `Temp` decodes the two A2-recipe runs' final PNGs (two fresh runs, same build) and finds
  identical pixels, proving the recipe reproduces a startup capture.
