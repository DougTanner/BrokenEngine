<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T22:41:01.550Z","dependsOn":[]} -->
# Agent teardown must not wait for loaders after a failed eager load

## Context

Found by a deep analysis of `Engine/Source/Agent` (architecture phase,
simulation and threading lens), verified by code reading against commit
`4fa6d2d7`. It was not run: nothing was built or launched. Line numbers are
from the commit that added this Plan; locate by symbol if they moved.

A Debug client launched with `--agent-port` whose eager asset load fails
blocks forever in teardown instead of returning nonzero:

- `Engine/Source/Main.cpp:197` constructs the agent command server before the
  client first consumes the eager-load future at `Main.cpp:314`
  (`HandleEagerLoadCompletion`, `:79-98`, returns 1 on `std::runtime_error`).
- By then the loader queue is not empty: the `IslandTerrain` constructor
  (`Main.cpp:307`) ends with `RequestChunkLoad(mIslandCrcsSorted, kRealtime)`
  (`Engine/Source/Frame/IslandTerrain.cpp:138`).
- The loader threads are created only by `mLoader.Start()`, the last statement
  of the eager task (`Engine/Source/File/PackChunks.cpp:506`). A task that
  throws (`PackChunks.cpp:466-494`) never reaches it, and the only queue pop
  is in `LoadingThread` (`Engine/Source/File/PackChunkLoader.cpp:195-196`).
- The `return 1` at `Main.cpp:316` destroys the agent command server.
  `~AgentCommandServer` calls `AudioStreamingHarnessRig::Shutdown`
  unconditionally in a `BT_CLIENT && BT_DEBUG` build
  (`Engine/Source/Agent/AgentCommandServer.cpp:99-100`), and `Shutdown` calls
  `WaitForLoadersIdle`
  (`Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp:111`), whose
  predicate needs `mRequestQueue.empty()` (`PackChunkLoader.cpp:138-141`).
  Nothing will ever empty the queue, so the main thread waits forever and the
  process keeps its listener socket and single-instance mutex.

Trigger: a Debug agent launch whose Scene, Model, Shader, or Raw pack
(`PackChunks.cpp:114-117`) is missing, truncated, or fails chunk validation.
Not affected: the Release client, the server, a launch without `--agent-port`,
and the listener-failure return at `Main.cpp:203-208`.

Unmet criterion: `Engine/Source/AGENTS.md` `## Startup and Main Loop` says a
required-asset failure at eager-load completion on an agent launch returns
nonzero through ordinary cleanup.

## Design

Recommended: `AudioStreamingHarnessRig::Shutdown` skips its
`WaitForLoadersIdle` call when the loader threads will never start, and Pack
exposes the one fact needed to tell.

"Not started yet" is not "will never start". If teardown runs while the eager
task is still running (an exception between `Main.cpp:197` and `:314`),
skipping the drain would let a loader start after `Detach`, and a loader's
`AudioStreamingHarnessRig::Record` call (`PackChunkLoader.cpp:243`) loads the
attachment outside the queue lock. The condition therefore covers only a
finished eager task that did not reach `mLoader.Start()`. In the proven path
the future was already consumed at `Main.cpp:314`, and
`PackChunks::mbEagerLoadComplete` (`PackChunks.cpp:504`) is still false.

The author's recommendation, because it reuses state Pack already holds: a
query on `PackChunks` that is true once the eager task has finished without
reaching `mLoader.Start()` (the future is no longer valid and
`mbEagerLoadComplete` is false), read by `Shutdown` before the drain. The
executing session confirms the exact form against `PackChunks` as it then is.
The statements of `Shutdown` around the drain stay as they are.

Risk tier: Tier 3 (the author's classification). Trigger: threading — the
change decides whether main-thread teardown waits for the loader threads, and
a wrong condition detaches the rig while a loader can still record into it.
Exposure: Debug client only; no determinism, wire, serialization, or `.pack`
exposure.

## Critical files

- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`
- `Engine/Source/File/PackChunks.h`, `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/PackChunkLoader.h`, `PackChunkLoader.cpp` (only if the
  query lives on the loader)
- `Engine/Source/File/AGENTS.md`, `Engine/Source/Agent/AGENTS.md` (only if the
  new query changes what they state)

## In scope

- The `WaitForLoadersIdle` call in `AudioStreamingHarnessRig::Shutdown`.
- One query on `PackChunks` or `PackChunkLoader` that reports a finished eager
  task which never started the loaders.

## Out of scope

- `PackChunkLoader::WaitForLoadersIdle` itself and its other caller
  (`Engine/Source/Graphics/Graphics.cpp:733`).
- `PackChunkLoader::Stop`, `~PackChunks`, and the eager task.
- Teardown that begins while the eager task is still running: it keeps
  today's behavior.
- The startup order in `Main.cpp`.

## Acceptance criteria

1. A Debug client launched with `--agent-port` and a missing or truncated
   eager pack exits with a nonzero code instead of hanging. This is settled by
   a live `/agent-harness` launch; the supplied evidence records no setup that
   reproduced the hang, because it was proven by code reading only, so the
   executing session first reproduces it on the unfixed build.
2. A Debug agent launch with intact packs still drains the loaders in
   `Shutdown` and exits cleanly.
3. `/compile` builds the BrokenEngineSandbox client in Debug and Release, and
   ModelViewer.
