# Engine/Source - Core Runtime

## Overview

Managers are constructed in dependency order and destroyed in reverse; fixed-rate simulation feeds a free-running interpolated renderer.

Update `../../Documents/Architecture/FrameUpdatePipeline.md` when main-loop order, frame-phase order, or phase participation changes.

## Hub Conventions

- IMPORTANT: Engine/game contract — the engine uses game code by name, and that
  is the intended design. Every game project provides the types, functions,
  globals, constants, and compile-time symbols the engine names, and engine
  code calls them directly — for example through `game::gpGame`, a `game::`
  free function or constant, or a cast to the one concrete game type. This is
  not a layering violation: never flag it in review, never propose removing it,
  and never add a virtual function, callback, function pointer, or
  back-reference only to reach game code. The one limit is the engine's own
  interface, not its use of game code: an engine-owned shared/public member,
  enum value, friend, or signature must not name a game-only concept unless an
  existing leaf document records deliberate ownership. If one would, stop and
  surface the ownership decision.
- Game references: an engine document names a game document as plain text, never a link — the game project's own `<project-relative path>`, plus the `## Heading` when needed — because each game project carries its own copy.
- Aggregation: `Engine.h`, the engine aggregation header, is included by the
  game PCH; preserve its documented include order and affinity spans. New
  single-build headers and source files carry their own whole-file `BT_CLIENT`
  or `BT_SERVER` guard. An engine header that must include a game header stays
  out of `Engine.h`, is included directly by its consumers, and its leaf
  document records the game-type ownership. `GameBase.h`, such a header without
  a leaf document, is recorded here: it stores two game types by value, so
  each needs a complete definition — the per-coord tick input from
  `Frame/FrameInput.h` and, client only, `game::FrameInterpolate` from
  `Frame/Frame.h`. Each game's `Pch.h` includes it, then the generic agent
  command headers that name its types, right after `Engine.h`; that placement
  is a recorded exception, not license to add another.
- Switches: `EngineSwitches.h` holds the engine-read compile-time switches with one value for every program and the `keLogLevel*` compile floors; each program's `Pch.h` (each game's and ModelViewer's) includes it after `Log/LogTypes.h` and before `Common.h`. A switch whose value differs by program, such as `kbProfileScreens`, is defined in each `Pch.h` before those includes.
- Internals: heavy private state in a header `Engine.h` aggregates belongs
  behind a `std::unique_ptr` and a forward declaration, its definition header
  included only by the owner's TUs, because every edit to an aggregated header
  rebuilds the PCH of both projects. Not for collection SOA storage, whose
  layout is CRC-load-bearing and forbids indirection. Precedent:
  `FileManager`/`PackChunks`. Worth it when the internals' header changes
  often, not to drop one stable header at the cost of an indirection on every
  member access.
- Allocation: Startup and teardown may allocate; main-loop allocation follows Memory.
- Determinism: FMA3 is disabled and SSE4.1 is required for cross-CPU CRC matching. Simulation threads must retain the required floating-point environment.
- Reuse counter: incremented each time a slot, file, or connection is reused so stale references can be detected; named `epoch` or `generation` depending on the subsystem.

## Startup and Main Loop

- The server uses the console subsystem and disables QuickEdit. Its hidden window
  receives WM_ENDSESSION only; Ctrl+C, Ctrl+Break and console close request quit
  and wait for the main thread's final autosave and monitor worker shutdown.
- `LaunchOptions` owns command-line parsing: `--loopback-only` is independent
  of `--agent-port`; `--data-directory` and `--app-data-directory` must each
  resolve to an existing absolute directory; ports are validated before
  conversion. `ProcessMain` opens the `--log-file` sink right after a
  successful parse, so `FileManager`-constructor diagnostics reach the file but
  parse rejections do not. `LaunchOptions` also owns `ShowStartupError`, the
  client and ModelViewer startup-error dialog, never shown in an agent launch.
- Agent launches stay minimized and suppress physical client input, except IME result characters on a focused agent window (`Input/AGENTS.md` `## Focus and Agent Input`), while preserving the close/Alt+F4 escape path. Synthetic input and command transport live in Agent.
- `FileManager` directory-setup failures during construction and required-asset failures at eager-load completion log their full diagnostic at `kError`; a normal client launch then shows that detail once, an agent launch shows no modal, and both return nonzero through ordinary cleanup without a crash report. Other exceptions are rethrown to an attached debugger, otherwise crash-reported.
- Client sound settings load before `Game` construction. A checked `Mute in background` suspends audio on focus loss; focus gain always resumes it. Agent launches suspend audio before `Game` construction regardless.
- Effective fullscreen resolves in one fixed order: the agent runtime override, then `--windowed WxH` forces windowed, then the saved `gFullscreen` preference. None of the three writes the saved preference.
- Client startup waits for terrain elevation before renderer construction and for priority textures after it, before the first render, then renders once per framebuffer before showing the window; an iteration is skipped, not retried, while a swapchain recreate is pending.
- `wWinMain` installs the main thread's `ThreadLocal` through `ThreadLocal::Entry` around `ProcessMain`, so it spans launch-option parsing, crash handling, and teardown.
- `TextureUploadManager` and `FileManager` outlive `MainThread` so crash handling and teardown can use them. Device loss recreates `Graphics` in place.
- Client COM uses `RO_INIT_MULTITHREADED`. Process and worker priorities, worker counts, and the single-instance policy are set in `Main.cpp`.

## Frame Ownership

- Client ring indices use `SnapshotIndex`; server ticks swap dual frame buffers. ID assignment is server-authoritative.
- Client rendering interpolates only populated coord rings and never extrapolates past committed state. `ResetClientState()` is the reset point for per-coord client counters.
- The engine owns the awake-cell list for an update — the coords simulated, published, and rendered — creates a cell's frame storage and static data on demand, and owns the per-coord tick input storage the deterministic dispatch reads. The client and server session runtimes compute the list; the game fills each frame's and input's contents. List membership does not mean a client is watching the cell; never derive liveness from it.
- Whether replay playback is running is one engine-owned flag the replay owner in File republishes; read it rather than replay reader state.
- Client simulation advances only through reconcile, capped by how fast the clock-adjustment loop may correct. Server update owns network pre-tick, save/load/replay, simulation, broadcast, resend, and autosave ordering. Before an armed debug replay capture reaches its pause target, it consumes at most one accumulated tick and returns the remainder to `TimeStep`; when the target pauses the update, use the normal accumulator-clearing semantics.
- Per-coord simulation dispatch uses pre-resolved `ActiveFrameReference` entries and thread-local state. Engine session bases own the reusable session mechanics and the runtimes own transport, pacing, and phase order; the game sessions, which inherit the bases, own gameplay policy and the game half of the save state. See the game project's own `Source/AGENTS.md`.

## Crash Reporting

A crash runs in a fixed order: `HandleException` writes the report, a connected
client uploads it or a server posts it, the main thread unwinds, and only then
does `PromptCrashReportDesktopCopy` handle the Desktop copy. The write and the
prompt are reachable during heap corruption, so they use fixed buffers and must
not log, allocate, use allocator-dependent formatting or path construction, or
follow a heap-owned pointer. `--app-data-directory` therefore reaches crash
handling through a fixed buffer populated at startup, not the launch options'
heap-owned path; reading an inline scalar of the statically allocated launch
options, such as the agent port, is allowed.
`ResolveCrashReportPaths` resolves the Desktop and per-user report paths once at
startup, after launch options are parsed, into fixed buffers, and creates the
per-user report directory; the override applies only when the complete report
path fits the per-user buffer, otherwise the per-user location remains.
`HandleException` always writes the per-user path, falling back to a fixed
working-directory-relative file name when resolution left it empty; nothing is
copied when the Desktop buffer is empty. The server never asks and always
copies to the Desktop path, even in an agent launch; the client's prompt never
shows in an agent launch, and Yes copies.

Reports leave the machine, so `common::CrashFileWriter` replaces the user
profile path, in the UTF-8 and ANSI forms `ResolveCrashReportPaths` also
stores, with `%USERPROFILE%` in every byte it writes; callers pass whole
strings so a path never splits across writes. `ParseCrashReportHeader`, the
header's one parser, rejects a missing or non-current crash-report version, so
bump the version on any header or report layout change.
The native single-instance mutex permits one client per game name in the Windows
session, independent of app-data overrides, worktrees, and build configurations.
The Sandbox client and ModelViewer share the `Broken Engine Sandbox` key, so
only one runs at a time; ModelViewer acquires it after allocator thread-local
setup and before constructing `TextureUploadManager` or `FileManager`, and holds
it until those managers are destroyed. The server uses a distinct game name and
runs alongside either.
`ProcessMain` renames the previous run's non-empty report to the pending file
beside it, right after the single-instance check and before any other thread
starts, only when both paths resolved from the same base. From then on
`HandleException` writes only the live path, and only the client's
`PollPendingCrashReport` or the server's startup pass reads the pending file,
so no writer and reader share a file.

Only the main-loop catch, when no debugger is attached, and the agent
crash-report harness rig call `TryUploadCrashReport` (client) or
`TryPostCrashReport` (server), on the main thread and only after their own
`HandleException` call returned `true`, so neither reads a report another
thread's SIGABRT handler is still writing. Both may allocate and log and never
propagate an exception. The upload sends the first `kiMaximumCrashReportSize`
bytes only when the client is connected and accepted and the connection has
carried no report, a limit owned by the `kClientCrashReport` row of
[Client to Server Contract](../../Documents/Architecture/Network.md#client--server-contract).
`ProcessMain`'s catches and the SIGABRT handler write and prompt but never
upload or post; the next start's rename makes that report pending.

A report counts as sent once it ends with a fixed trailer, appended only after
ENet confirms the server received the whole report; `HandleException` truncates
the file it writes, so every new report starts unsent. Once per accepted
connection, `PollPendingCrashReport`, called at the top of
`ClientSessionRuntime::PollAndDrain`, sends the pending file, never the live
path, when it is non-empty and unsent; one failing `ParseCrashReportHeader`
stays unsent because the server rejects it as corrupt.

The server posts reports as issues in the private `DougTanner/Coretide` GitHub
repository, shared by every game, only when its app-data folder holds
`GitHubCrashReportToken.txt`. `CrashReportPoster`'s startup pass posts the
pending file and every stored client report in the app-data folder, retrying
failures at the next start; each report `Server::ClientCrashReport` stores gets
its own background post, so the store cap bounds posts too. A report counts as
posted once it ends with a separate posted trailer, appended only after GitHub
answers 201, or 400 or 422, which reject the request itself, so a report GitHub
never accepts does not block later ones; posted reports are skipped. A crash
between that answer and the append reposts the report, because the issues API
has no idempotency key. Shutdown waits for unfinished posts.

`kbDxDiag` is off in every build, leaving the DxDiag section empty, because DxDiag text carries machine-identifying values the profile-path replacement does not remove. When on, the background DxDiag reader appends to its report string without a lock while the agent crash-report harness rig can enter the exception path, so the report always writes the begin/end markers but includes the text only once the reader publishes completion.

## Subsystems

- Agent (`Agent/AGENTS.md`) - Harness command transport, shared, client-generic, and server-generic command handlers, synthetic input, and UI snapshots
- Audio (`Audio/AGENTS.md`) - XAudio2 spatial audio
- File (`File/AGENTS.md`) - Packed assets, versioned files, grid saves, and replay record/playback
- Frame (`Frame/AGENTS.md`) - Base frame state, terrain, navigation, and collections
- Graphics (`Graphics/AGENTS.md`) - Vulkan renderer
- Input (`Input/AGENTS.md`) - Client input
- Memory (`Memory/AGENTS.md`) - Allocator and tracking
- Network (`Network/AGENTS.md`) - ENet transport and discovery
- Profile (`Profile/AGENTS.md`) - CPU/GPU profiling
- Server (`Server/AGENTS.md`) - Server Monitor host: statistics aggregation and the monitor channel
- Ui (`Ui/AGENTS.md`) - Runtime settings and screens
