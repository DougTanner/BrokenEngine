# Engine/Source - Core Runtime

## Overview

Shared C++23 runtime for the client and server. Managers are constructed in dependency order and destroyed in reverse; fixed-rate simulation feeds a free-running interpolated renderer.

Update Frame Update Pipeline (`../../Documents/Architecture/FrameUpdatePipeline.md`) when main-loop order, frame-phase order, or phase participation changes.

## Hub Conventions

- Engine/game contract: Engine code reaches the game types, functions, globals,
  and compile-time symbols the game is required to provide directly by name —
  for example through `game::gpGame`, a `game::` free function or global, or a
  cast to the one concrete game type — and never adds a virtual function,
  callback, function pointer, or back-reference only to reach game code. An
  engine-owned shared/public member, enum value, friend, or signature must not
  name a game-only concept unless an existing leaf document records deliberate
  ownership. If one would, stop and surface the ownership decision.
- Game references: an engine document names a game document as plain text, never a link — the game project's own `<project-relative path>`, plus the `## Heading` when one is needed — because each game project carries its own copy.
- Aggregation: `Engine.h` is the engine aggregation header and is included by
  the game PCH. Preserve its documented include order and existing affinity
  spans. New single-build headers and source files still carry their own
  whole-file `BT_CLIENT` or `BT_SERVER` guard. An engine header that must
  include a game header stays out of the aggregation and is included directly by
  its consumers, so game types never reach the shared PCH; the leaf document
  owning such a header records the deliberate game-type ownership. `GameBase.h`
  is the one aggregated header that includes game headers anyway, recorded here
  because it has no detail document: it stores two game types by value, so each
  needs a complete definition — the per-coord tick input from
  `Frame/FrameInput.h`, and, client only, the per-coord render interpolation
  `game::FrameInterpolate` from `Frame/Frame.h`. Do not read it as license to
  add another.
- Switches: `EngineSwitches.h` holds the compile-time switches the engine reads and the `keLogLevel*` compile floors; each game's `Pch.h` includes it after `Log/LogTypes.h` and before `Common.h`.
- Keep implementation internals out of the aggregation: heavy private state in a
  header `Engine.h` aggregates belongs behind a `std::unique_ptr` and a forward
  declaration, its definition header included only by the owner's TUs and never
  by `Engine.h` — otherwise every edit to it invalidates the PCH and rebuilds
  both projects. Does not apply to collection SOA storage, where layout is
  CRC-load-bearing and an indirection is forbidden. Precedent:
  `FileManager`/`PackChunks`. Weigh the churn: worth it when the internals'
  header changes often, not when the only gain is removing one stable header at
  the cost of rerouting every member access through an indirection.
- Allocation: Startup and teardown may allocate; main-loop allocation follows Memory (`Memory/AGENTS.md`).
- Determinism: FMA3 is disabled and SSE4.1 is required for cross-CPU CRC matching. Simulation threads must retain the required floating-point environment.
- Reuse counter: a counter incremented each time a slot, file, or connection is reused, so stale references can be detected. It appears in code as `epoch` in some subsystems and `generation` in others.

## Startup and Main Loop

- `LaunchOptions` owns command-line parsing. `--loopback-only` is independent of `--agent-port`; `--data-directory` and `--app-data-directory` must each resolve to an existing absolute directory; port values are validated before conversion. The `--log-file` sink opens in `ProcessMain` immediately after a successful parse, so `FileManager`-constructor diagnostics reach the file while parse rejections do not.
- Agent launches stay minimized and suppress physical client input, except IME result characters on a focused agent window (`Input/AGENTS.md` `## Focus and Agent Input`), while preserving the close/Alt+F4 escape path. Synthetic input and command transport live in Agent (`Agent/AGENTS.md`).
- `FileManager` directory-setup failures during construction and required-asset failures at eager-load completion log their full diagnostic at `kError`. At those two startup boundaries, a normal client launch shows that same detail once, an agent launch shows no modal, and both return nonzero through ordinary cleanup without writing a crash report. Other exceptions are rethrown to an attached debugger and otherwise retain the crash-report path.
- Client sound settings load before `Game` construction. A checked `Mute in background` setting suspends audio on focus loss and focus gain always resumes it; agent launches suspend audio before `Game` construction regardless of the setting so harness clients boot silent.
- Effective fullscreen resolves in one fixed order: the agent runtime override wins, then `--windowed WxH` forces windowed, then the saved `gFullscreen` preference applies. None of the three writes the saved preference, so an agent command or launch flag never rewrites what the user chose.
- Client startup waits for terrain elevation before renderer construction and for priority textures after it, before the first render, then renders once per framebuffer before showing the window; an iteration is skipped, not retried, while a swapchain recreate is pending.
- `wWinMain` installs the main thread's `ThreadLocal` through `ThreadLocal::Entry` around `ProcessMain`, so it spans launch-option parsing, crash handling, and teardown.
- `TextureUploadManager` and `FileManager` outlive `MainThread` so crash handling and teardown can use them. Device loss recreates `Graphics` in place.
- Client COM uses `RO_INIT_MULTITHREADED`. Process and worker priorities, worker counts, and the single-instance policy are set in `Main.cpp`.

## Frame Ownership

- Client ring indices use `SnapshotIndex`; server ticks swap dual frame buffers. ID assignment is server-authoritative.
- Client rendering interpolates only populated coord rings and never extrapolates past committed state. `ResetClientState()` is the reset point for per-coord client counters.
- The engine owns the awake-cell list for an update — the coords simulated, published, and rendered — and creates a cell's frame storage and static data on demand; it also owns the per-coord tick input storage the deterministic dispatch reads. The client and server session runtimes compute that list; the game fills each frame's and each input's contents. Membership in that list does not mean a client is watching the cell, so never derive liveness from it.
- Whether replay playback is running is a single engine-owned flag that the replay owner republishes (`File/AGENTS.md`). Read that flag rather than inspecting replay reader state.
- Client simulation advances only through reconcile and is capped by the limit on how fast the clock-adjustment loop may correct. Server update owns network pre-tick, save/load/replay, simulation, broadcast, resend, and autosave ordering. Before an armed debug replay capture reaches its pause target, it consumes at most one accumulated tick and returns the remainder to `TimeStep`; when the target pauses the update, use the normal accumulator-clearing semantics.
- Per-coord simulation dispatch uses pre-resolved `ActiveFrameReference` entries and thread-local state. Engine session bases own the reusable session mechanics and the runtimes own transport, pacing, and phase order; the game sessions, which inherit the bases, own gameplay policy and the game half of the save state. See game Source (the game project's own `Source/AGENTS.md`).

## Crash Reporting

A crash runs in a fixed order: `HandleException` writes the report, a connected
client uploads it, the main thread unwinds, and only then does
`PromptCrashReportDesktopCopy` handle the Desktop copy. The write and the prompt
use fixed buffers because they are reachable during heap corruption. Do not add
allocator-dependent formatting or path construction there, and do not log from
them. Nothing on them may allocate or follow a pointer the heap owns, so
`--app-data-directory` reaches crash handling through a fixed buffer populated
during startup rather than through the launch options' heap-owned path storage;
reading an inline scalar of the statically allocated launch options, such as the
agent port that suppresses the prompt in a client agent launch, stays within the
rule.
The override is used only when the complete report path fits the fixed path
buffer; otherwise the existing per-user report location remains. Both report
file paths — the Desktop one and the per-user one — are resolved once at startup
into fixed buffers, after launch options are parsed, by
`ResolveCrashReportPaths`, which also creates the per-user report directory.
`HandleException` always writes the per-user path, falling back to a fixed
working-directory-relative file name when that buffer is empty because its
startup resolution failed. Nothing is copied when the Desktop buffer is empty.
The server never asks and always copies the written report to the Desktop path,
even in an agent launch. The client's prompt never shows in an agent launch; Yes
copies the report.

Only the client main-loop catch, when no debugger is attached, and the agent
crash-report harness rig call `TryUploadCrashReport`, on the main thread after
the write. It may allocate and log, never propagates an exception, and sends the
first `kiMaximumCrashReportSize` bytes only when the client is connected and
accepted and the connection has not already carried a report; the per-connection
limit is the `kClientCrashReport` row of
[Client to Server Contract](../../Documents/Architecture/Network.md#client--server-contract).
`ProcessMain`'s catches and the SIGABRT handler write and prompt but never
upload.

A report counts as sent once it ends with a fixed trailer; `HandleException`
truncates the file it writes, so every new report starts unsent. The trailer is
appended only after ENet confirms that the server received the whole report.
Once per accepted connection, `PollPendingCrashReport`, called at the top of
`ClientSessionRuntime::PollAndDrain`, sends the report at the path this
process's `HandleException` writes when it is non-empty and unsent; a report an
earlier session left at a different path is never sent.

The background DxDiag reader appends to its report string without a lock, and the agent crash-report harness rig can enter the exception path while that reader is still running. The report always writes the DxDiag begin/end markers but includes the collected text only once the reader publishes completion.

## Subsystems

- Agent (`Agent/AGENTS.md`) - Harness command transport, shared and client-generic command handlers, synthetic input, and UI snapshots
- Audio (`Audio/AGENTS.md`) - XAudio2 spatial audio
- File (`File/AGENTS.md`) - Packed assets, versioned files, grid saves, and replay record/playback
- Frame (`Frame/AGENTS.md`) - Base frame state, terrain, navigation, and collections
- Graphics (`Graphics/AGENTS.md`) - Vulkan renderer
- Input (`Input/AGENTS.md`) - Client input
- Memory (`Memory/AGENTS.md`) - Allocator and tracking
- Network (`Network/AGENTS.md`) - ENet transport and discovery
- Profile (`Profile/AGENTS.md`) - CPU/GPU profiling
- Server (`Server/AGENTS.md`) - Headless server monitoring window
- Ui (`Ui/AGENTS.md`) - Runtime settings and screens
