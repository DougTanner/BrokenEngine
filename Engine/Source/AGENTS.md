# Engine/Source - Core Runtime

## Overview

Shared C++23 runtime for the client and server. Managers are constructed in dependency order and destroyed in reverse; fixed-rate simulation feeds a free-running interpolated renderer.

Update Frame Update Pipeline (`../../Documents/Architecture/FrameUpdatePipeline.md`) when main-loop or frame-phase order changes.

## Hub Conventions

- Engine/game contract: Engine code may consume game types, hooks, globals, and compile-time symbols that the game is required to provide. An engine-owned shared/public member, enum value, friend, or signature must not name a game-only concept unless an existing leaf document records deliberate ownership. If one would, stop and surface the ownership decision.
- Aggregation: `Engine.h` is the engine aggregation header and is included by the game PCH. Preserve its documented include order and existing affinity spans. New single-build headers and source files still carry their own whole-file `BT_CLIENT` or `BT_SERVER` guard. An engine header that must include a game header stays out of the aggregation and is included directly by its consumers, so game types never reach the shared PCH; the leaf document owning such a header records the deliberate game-type ownership. `GameBase.h` is the one aggregated header that includes a game header anyway, recorded here because it has no detail document: it stores the per-coord tick input by value, so that game type needs a complete definition. Do not read it as license to add another.
- Keep implementation internals out of the aggregation: heavy private state in a header `Engine.h` aggregates belongs behind a `std::unique_ptr` and a forward declaration, its definition header included only by the owner's TUs and never by `Engine.h` — otherwise every edit to it invalidates the PCH and rebuilds both projects. Does not apply to collection SOA storage, where layout is CRC-load-bearing and an indirection is forbidden. Precedents: `FileManager`/`PackChunks`, `AudioManager`/`StaticVoices`+`StreamingVoices`. Weigh the churn: worth it when the internals' header changes often, not when the only gain is removing one stable header at the cost of rerouting every member access through an indirection.
- Allocation: Startup and teardown may allocate; main-loop allocation follows Memory (`Memory/AGENTS.md`).
- Determinism: FMA3 is disabled and SSE4.1 is required for cross-CPU CRC matching. Simulation threads must retain the required floating-point environment.
- Reuse counter: a counter incremented each time a slot, file, or connection is reused, so stale references can be detected. It appears in code as `epoch` in some subsystems and `generation` in others.

## Startup and Main Loop

- `LaunchOptions` owns command-line parsing. `--loopback-only` is independent of `--agent-port`; `--data-directory` and `--app-data-directory` must each resolve to an existing absolute directory; port values are validated before conversion. The `--log-file` sink opens in `wWinMain` immediately after a successful parse, so `FileManager`-constructor diagnostics reach the file while parse rejections do not.
- Agent launches stay minimized and suppress physical client input while preserving the close/Alt+F4 escape path. Synthetic input and command transport live in Agent (`Agent/AGENTS.md`).
- A startup failure that ends the process logs at `kError` and puts up its modal dialog only when `AgentLaunched()` (`LaunchOptions.h`) is false, wherever in the runtime the failure is detected.
- Client sound settings load before `Game` construction. A checked `Mute in background` setting suspends audio on focus loss and focus gain always resumes it; agent launches suspend audio before `Game` construction regardless of the setting so harness clients boot silent.
- Effective fullscreen resolves in one fixed order: the agent runtime override wins, then `--windowed WxH` forces windowed, then the saved `gFullscreen` preference applies. None of the three writes the saved preference, so an agent command or launch flag never rewrites what the user chose.
- Client startup waits for terrain elevation and priority textures before renderer construction, then primes each framebuffer before showing the window.
- `TextureUploadManager` and `FileManager` outlive `MainThread` so crash handling and teardown can use them. Device loss recreates `Graphics` in place.
- Client COM uses `RO_INIT_MULTITHREADED`. Process and worker priorities, worker counts, and the single-instance policy are set in `Main.cpp`.

## Frame Ownership

- Client ring indices use `SnapshotIndex`; server ticks swap dual frame buffers. ID assignment is server-authoritative.
- Client rendering interpolates only populated coord rings and never extrapolates past committed state. `ResetClientState()` is the reset point for per-coord client counters.
- The engine owns the awake-cell list for an update — the coords simulated, published, and rendered — and creates a cell's frame storage and static data on demand; it also owns the per-coord tick input storage the deterministic dispatch reads. The game decides which cells the client keeps awake and fills each frame's and each input's contents. Membership in that list does not mean a client is watching the cell, so never derive liveness from it.
- Whether replay playback is running is a single engine-owned flag that the replay owner republishes (`File/AGENTS.md`). Read that flag rather than inspecting replay reader state.
- Client simulation advances only through reconcile and is capped by the limit on how fast the clock-adjustment loop may correct. Server update owns network pre-tick, save/load/replay, simulation, broadcast, resend, and autosave ordering. Before an armed debug replay capture reaches its pause target, it consumes at most one accumulated tick and returns the remainder to `TimeStep`; when the target pauses the update, use the normal accumulator-clearing semantics.
- Per-coord simulation dispatch uses pre-resolved `ActiveFrameRef` entries and thread-local state. Engine session runtimes own reusable network mechanics and phase order; game wrappers own gameplay policy and persistence. See game Source (`../../Projects/BrokenEngineSandbox/Source/AGENTS.md`).

## Crash Reporting

The exception path writes from fixed buffers because it is reachable during heap corruption. Do not add allocator-dependent formatting or path construction there, and do not log from it. Nothing on that path may allocate or follow a pointer the heap owns, so `--app-data-directory` reaches crash handling through a fixed buffer populated during startup rather than through the launch options' heap-owned path storage; reading an inline scalar of the statically allocated launch options, such as the agent port that selects the unprompted non-modal branch, stays within the rule. The override is used only when the complete report path fits the fixed path buffer; otherwise the existing per-user report location remains. Both report file paths — the Desktop one and the per-user one — are resolved once at startup into fixed buffers, after launch options are parsed, by `ResolveCrashReportPaths`, which also creates the per-user report directory. The exception path only picks one of those buffers, and falls back to a fixed working-directory-relative file name when the picked buffer is empty because its startup resolution failed.

The background DxDiag reader appends to its report string without a lock, and the agent crash-report fixture can enter the exception path while that reader is still running. The report always writes the DxDiag begin/end markers but includes the collected text only once the reader publishes completion.

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
