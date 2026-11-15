<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:20:59.837Z","dependsOn":[]} -->
# Audio harness rig: teardown after a failed eager load and a named "no pool index"

## Context

Found by a deep analysis of `Engine/Source/Agent`, verified by code reading
against commit `4fa6d2d7`; nothing was built or launched. Line numbers are from
commit `e021a4b3`; locate by symbol if they moved. `Rig.h` and
`Rig.cpp` below are `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h`
and `.cpp`.

### Teardown hangs after a failed eager load

Found in the architecture phase (simulation and threading lens). A Debug
client launched with `--agent-port` whose eager asset load fails blocks
forever in teardown instead of returning nonzero:

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
  `WaitForLoadersIdle` (`Rig.cpp:111`), whose predicate needs
  `mRequestQueue.empty()` (`PackChunkLoader.cpp:138-141`). Nothing will ever
  empty the queue, so the main thread waits forever and the process keeps its
  listener socket and single-instance mutex.

Trigger: a Debug agent launch whose Scene, Model, Shader, or Raw pack
(`PackChunks.cpp:114-117`) is missing, truncated, or fails chunk validation.
Not affected: the Release client, the server, a launch without `--agent-port`,
and the listener-failure return at `Main.cpp:203-208`.

Unmet criterion: `Engine/Source/AGENTS.md` `## Startup and Main Loop` says a
required-asset failure at eager-load completion on an agent launch returns
nonzero through ordinary cleanup.

### The rig's "no pool index" value has no name

The literal `4'294'967'295i64` is written at ten sites: `Rig.h:84`, `:115`,
`:223`; `Rig.cpp:217`, `:503`, `:513`, `:533`;
`Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp:142`, `:192`,
`:347`. Three producers pass `std::numeric_limits<uint32_t>::max()`, the same
number, to `AudioStreamingHarnessRig::Record` for the same meaning
(`Engine/Source/Audio/StreamingVoices.cpp:168`,
`Engine/Source/File/PackChunkLoader.cpp:94`, `:243`).

A change to one of the thirteen sites compiles clean and makes the rig mistake
a held index or report a pool index that does not exist. The damage is limited
to Debug harness output.

The rig's value is its own, not File's "no entry" value
(`PackChunks::kiInvalidAudioReadEntry`, `Engine/Source/File/PackChunks.h:84`,
and the `ChunkReadRequest::miEntryIndex` initializer,
`Engine/Source/File/FileManager.h:131`). The two are equal numbers, and the
File value never reaches the rig: the two `Record` calls that pass
`miEntryIndex` (`Engine/Source/File/PackChunks.cpp:622`, `:711`) run only
after the request is known to hold an entry (`:596`, `:686-689`). File's value
belongs to `Documents/Plans/Engine/AudioReadPathCleanup.md`.

Unmet criterion: the rig's "no pool index" value has one name that its
producers and consumers use.

## Design

1. Teardown. `AudioStreamingHarnessRig::Shutdown` skips its
   `WaitForLoadersIdle` call when the loader threads will never start, and
   Pack exposes the one fact needed to tell.

   "Not started yet" is not "will never start". If teardown runs while the
   eager task is still running (an exception between `Main.cpp:197` and
   `:314`), skipping the drain would let a loader start after `Detach`, and a
   loader's `AudioStreamingHarnessRig::Record` call
   (`PackChunkLoader.cpp:243`) loads the attachment outside the queue lock.
   The condition therefore covers only a finished eager task that did not
   reach `mLoader.Start()`. In the proven path the future was already consumed
   at `Main.cpp:314`, and `PackChunks::mbEagerLoadComplete`
   (`PackChunks.cpp:504`) is still false.

   The recommended form, because it reuses state Pack already holds: a query
   on `PackChunks` that is true once the eager task has finished without
   reaching `mLoader.Start()` (the future is no longer valid and
   `mbEagerLoadComplete` is false), read by `Shutdown` before the drain. The
   executing session confirms the exact form against `PackChunks` as it then
   is. The statements of `Shutdown` around the drain stay as they are.

2. "No pool index" name. One named `constexpr int64_t` in `Rig.h`, holding
   `4'294'967'295`, used at the ten sites and passed by the three producers,
   which already include `Rig.h` (`StreamingVoices.cpp:8`,
   `PackChunkLoader.cpp:8`). The producers already call `Record`, so no new
   dependency is added. Every edited line of this step swaps a literal for a
   name of the same value, so it changes no runtime behavior in any build.

## Critical files

- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.h`
- `Engine/Source/Agent/Commands/AudioStreamingHarnessRig.cpp`
- `Engine/Source/Agent/Commands/AgentCommandsAudioStreaming.cpp`
- `Engine/Source/File/PackChunks.h`, `Engine/Source/File/PackChunks.cpp`
- `Engine/Source/File/PackChunkLoader.cpp`; `PackChunkLoader.h` only if the
  teardown query lives on the loader
- `Engine/Source/Audio/StreamingVoices.cpp`
- `Engine/Source/File/AGENTS.md`, `Engine/Source/Agent/AGENTS.md` (only if the
  new query changes what they state)

## In scope

- Teardown:
  - The `WaitForLoadersIdle` call in `AudioStreamingHarnessRig::Shutdown`.
  - One query on `PackChunks` or `PackChunkLoader` that reports a finished
    eager task which never started the loaders.
- "No pool index" name:
  - One new constant in `Rig.h`.
  - The ten `4'294'967'295i64` sites and the three
    `std::numeric_limits<uint32_t>::max()` arguments to
    `AudioStreamingHarnessRig::Record` listed in `## Context`.

## Out of scope

- `PackChunkLoader::WaitForLoadersIdle` itself and its other caller
  (`Engine/Source/Graphics/Graphics.cpp:733`).
- `PackChunkLoader::Stop`, `~PackChunks`, and the eager task.
- Teardown that begins while the eager task is still running: it keeps
  today's behavior.
- The startup order in `Main.cpp`.
- `PackChunks::kiInvalidAudioReadEntry` and the
  `ChunkReadRequest::miEntryIndex` initializer:
  `Documents/Plans/Engine/AudioReadPathCleanup.md` owns them. This Plan edits
  no line that Plan names, and neither needs the other first.
- `FileManager.h`, and every line of `PackChunks.h` and `PackChunks.cpp` other
  than the teardown query.
- The `static_assert` lines in `Rig.cpp` that tie `AudioChunkReadState` to
  `AudioStreamingHarnessRigQueueState`.
- `AudioStreamingHarnessRigHoldOwner`, the `poolEntries` shape, and any other
  rig behavior or JSON field apart from the drain decision in `Shutdown`.
- The action dispatch in `CommandAudioStreamingHarnessRig`.

## Risk tier

Tier 3. Triggers:

- Threading: the teardown change decides whether main-thread teardown waits
  for the loader threads, and a wrong condition detaches the rig while a
  loader can still record into it.
- A change spanning independently owned subsystems: the constant is added to
  `Rig.h` (Agent) and the edit replaces arguments in File and Audio; each of
  the three directories has its own `AGENTS.md`.

Exposure: Debug client only. Every edited line of the "no pool index" step
sits under `BT_CLIENT` and `BT_DEBUG`. No determinism, wire, serialization, or
`.pack` exposure.

The BrokenEngineSandbox client Debug build compiles all thirteen "no pool
index" sites. The ModelViewer Debug build compiles the three `Rig.h` sites and
the two `PackChunkLoader.cpp` sites, the latter in discarded
`if constexpr (kbAudioStreamingHarnessRig)` branches that never run; it does
not compile `Rig.cpp`, `AgentCommandsAudioStreaming.cpp`, or
`StreamingVoices.cpp`.

## Acceptance criteria

1. A Debug client launched with `--agent-port` and a missing or truncated
   eager pack exits with a nonzero code instead of hanging. This is settled by
   a live `/agent-harness` launch; the supplied evidence records no setup that
   reproduced the hang, because it was proven by code reading only, so the
   executing session first reproduces it on the unfixed build.
2. A Debug agent launch with intact packs still drains the loaders in
   `Shutdown` and exits cleanly.
3. Code read: `Rig.h` declares one `constexpr int64_t` equal to
   `4'294'967'295`; each of the ten former literal sites names it, so no bare
   `4'294'967'295i64` remains in the three Agent files outside that
   declaration; the three producers pass it to `Record`; outside the teardown
   change in `## In scope`, the change edits no other line. Together these
   show the value at every site is unchanged.
4. `/compile` builds the BrokenEngineSandbox client in Debug and Release, and
   ModelViewer in Debug and Release.

## Coordination

- `Documents/Plans/Engine/ConsolidatedSmallDeduplications.md` (its item 1)
  rewrites the action chain of `CommandAudioStreamingHarnessRig` around the
  sentinel site at `AgentCommandsAudioStreaming.cpp:347`. Neither Plan needs
  the other first; whichever lands second applies its edit to that function as
  it finds it.
