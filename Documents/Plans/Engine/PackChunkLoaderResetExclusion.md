<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-05T18:38:12.215Z","dependsOn":[]} -->
# Exclude PackChunks loaders from lazy-pool reset mutation

## Context

The codebase-comment sweep exposed a pre-existing race outside its approved comment-only boundary. `PackChunks` creates persistent loading workers during `LoadPackFiles` and destroys them only with `PackChunks` (`Engine/Source/File/PackChunks.cpp:478-482`, `PackChunks::LoadingThread`, and `PackChunks::~PackChunks`). A worker removes a request from `mRequestQueue` under `mQueueMutex`, releases that mutex, and then runs either `LoadChunk` or `RecommitAndReloadChunkRange`. Those paths read `LazyChunk::pData` and `iDataSize` while performing whole-chunk and range loads (`PackChunks.cpp:689-729,805-815,1032-1061`).

`PackChunks::ResetTextureChunkStates` concurrently rewrites those plain fields for every lazy chunk before it filters for textures or the caller's target CRCs (`PackChunks.cpp:857-888`). Active-island processing can enqueue whole texture loads and island range reloads before the same render cycle reaches the targeted eviction reset (`Engine/Source/Graphics/Islands.cpp:241-255`, `Engine/Source/Frame/IslandTerrainResidency.cpp:108-134,303-317`, and `Engine/Source/Graphics/Graphics.cpp:198-215`). The descriptor-fence drain does not drain PackChunks workers. Full device-loss teardown joins the texture-upload thread before resetting chunk states, but the PackChunks loaders remain live (`Graphics.cpp:655-677,758-764`). Identical pointer and size values do not synchronize the concurrent reads and writes.

The comment sweep preserves truthful synchronization text but cannot change runtime behavior. The race is present at session baseline `e420380d71e95ddd89fb7b1590e9c2813186d0ae` in the source paths and methods cited above.

Current streamed music does not add another reachable reset reader. The menu and game playlists contain music CRCs and pass them only to `PlayMusic` (`Projects/BrokenEngineSandbox/Source/Game.h:157-190`, `Game.cpp:66-72,527-550,683-696`). `StreamingVoices::Play` and `CreateStream` create the stream without requesting whole-chunk residency, and `StreamingVoice::FillSlot` calls `ReadChunkData` (`Engine/Source/Audio/StreamingVoices.cpp:26-79,197-206`, `StreamingVoice.cpp:45-99`). The current whole-audio request path belongs to static voices (`Engine/Source/Audio/StaticVoice.cpp:12-30`), so music remains `kNotLoaded` and follows `PackChunks::ReadChunkData`'s direct disk fallback.

## Design

The author's recommendation is to remove the routine reset/load overlap instead of putting disk I/O behind the render thread. Keep each lazy chunk's pool address and allocation extent stable after `LoadPackFiles`: texture adoption may decommit the resident pages, but it should retain `pData` and `iDataSize`; a later whole load already recommits the range before writing it. Then make the targeted `ResetTextureChunkStates` path touch only the selected texture state and GPU handles. This removes the complete-map pointer/size rewrite and lets unrelated whole and range loaders continue during per-island eviction without a new render-thread wait.

For full Graphics/device recovery, add the smallest PackChunks-owned drain needed to establish a clear disk-I/O boundary. Close admission, let every accepted whole and range job finish its PackChunks work, and only then tear down the texture-upload worker and run the all-texture reset. A completed range job publishes its range result before leaving the drain. A completed whole-texture job has finished its recommit, read, and decompression and no longer accesses the lazy chunk through a PackChunks loader; its GPU upload or adoption may still be pending and is re-established by normal recovery after reset. Reopen ordinary request admission only after the reset interval. Preserve accepted jobs and their existing I/O publication order without treating legitimate post-recovery GPU state publication as a duplicate disk load. Preparation should prove the complete request-producer set and choose the minimal expression of this existing lifecycle boundary. Do not introduce a general task system, per-chunk lock framework, generation protocol, or steady-state render synchronization.

Update the packed-asset instructions and the comments around texture adoption, both reset callers, resident reads/statistics, and the RenderGlobal drained descriptor window so they state the resulting ownership accurately. The descriptor drain protects Vulkan descriptor/image use; it is not a PackChunks loader drain. Keep streaming music documented as disk-only under current producers.

## Critical files

- `Engine/Source/File/PackChunks.cpp` and `PackChunks.h` — loading-worker lifetime, whole/range access, reset behavior, and full-recovery drain boundary.
- `Engine/Source/File/FileManager.cpp`, `FileManager.h`, and `AGENTS.md` — reset forwarding plus packed-asset and lazy-pool ownership contracts.
- `Engine/Source/Graphics/Managers/TextureManager.cpp` — adoption-time decommit and lazy-chunk metadata lifetime.
- `Engine/Source/Frame/IslandTerrainResidency.cpp` and `Engine/Source/Graphics/Islands.cpp` — range/whole request producers and targeted eviction reset.
- `Engine/Source/Graphics/Graphics.cpp` — RenderGlobal descriptor drain and full device-recovery ordering.

## In scope

- Stable lifetime for `LazyChunk::pData` and `iDataSize` after lazy-pool construction, including texture adoption, decommit, reload, and both reset variants.
- Removing the complete-map metadata rewrite from targeted texture reset so unrelated whole and range loaders can continue without a render-thread disk-load wait.
- A PackChunks-owned full-recovery boundary that drains accepted whole and range work and excludes new loading work while the all-texture reset runs.
- State/publication ordering needed to preserve accepted requests across that boundary and resume loading after Graphics recreation.
- Corrections to the packed-asset instructions and the affected synchronization/drained-window comments named in `## Design`.

## Out of scope

- Pack or manifest formats, `kiVersion`, save/replay/wire data, deterministic frame CRC, or backward compatibility.
- Loader count, priority, queue ordering, sub-read sizing, eviction policy, texture-upload architecture, or a general-purpose synchronization abstraction.
- Audio residency changes: current music CRC producers never request whole-chunk residency and `ReadChunkData` uses the disk fallback.
- New Agent Harness commands, unit tests, unrelated PackChunks validation/failure Plans, or Graphics recovery features.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the fix changes threading exclusion and recovery ordering across PackChunks workers, frame-driven island residency, and Graphics device teardown.

- No reset writes a field while a whole or range loader can read or write that field.
- Targeted island eviction does not wait for unrelated disk work; render-frame smoothness retains the existing conditional descriptor-fence cost only.
- Full recovery admits no new PackChunks work inside its drain/reset interval. Every accepted whole or range job finishes PackChunks disk I/O and leaves the loader before texture-upload teardown and reset; later GPU upload/adoption publication remains part of normal recovery rather than a second disk-I/O completion.
- `pData` continues to identify the same reserved pool range after decommit; `iDataSize` continues to describe that allocation, while `eState` remains the authority for whether resident bytes may be read.
- Whole texture reload recommits before writing, island range reload addresses the same stable pool allocation, and successful texture adoption/recovery preserves existing release/acquire publication.
- Client-only resource state stays outside deterministic CRC, save, replay, wire, and `.pack` layout.

## Acceptance criteria

- A changed-source audit proves `LazyChunk::pData` and `iDataSize` are assigned during lazy-pool construction and are not cleared or rebuilt by texture adoption or reset; every resident consumer still gates access through the existing chunk-state publication.
- With unrelated whole texture loads and island range reloads active, targeted island eviction resets only its selected texture chunks, the loaders complete, and the render path contains no PackChunks queue/worker drain or wait for their disk I/O.
- Verify the full-recovery boundary with the existing executable and a Visual Studio debugger, without adding an Agent command. Use `/agent-harness` to launch the Debug client, establish an island scene that queues whole texture and island range work, and collect state/log evidence. Attach Visual Studio and stop one loading thread in `PackChunks::LoadChunk` after it has accepted a whole request, freeze that thread, continue until the other loading thread enters `PackChunks::RecommitAndReloadChunkRange` for an accepted range request, and freeze it too. At a safe main-thread frame boundary, set the public `engine::gpGraphics->meDestroyType` to `engine::DestroyType::kSurface` (`Engine/Source/Graphics/Graphics.h:27-35,90-120`) and resume all unfrozen threads while holding only the two PackChunks loaders until the main thread waits in the new PackChunks drain. Prove `ResetTextureChunkStates` has not been entered while either loader is held. Thaw both loaders, explicitly keep the texture-upload worker and other required workers runnable, let their disk I/O and range-result publication finish, and stop at the all-texture reset (`Graphics.cpp:758-764`); prove admission is closed and no whole or range loader remains active. Continue through normal Graphics recreation, then use `/agent-harness` queries and logs to prove the texture and island mesh recover and no accepted disk job was lost or repeated. This is a future debugger-assisted verification procedure, not an existing harness command or a result observed while authoring this Plan.
- Ordinary startup loading, same-queue texture fallback, texture-upload adoption, repeated island eviction/reacquisition, and device recovery retain valid state transitions with no validation error, device-loss recursion, or CRC mismatch.
- The packed-asset instructions and affected comments distinguish the descriptor-fence drain, targeted reset behavior, and full PackChunks drain, and make no audio-worker exclusion claim while music remains disk-only.
- Client `Debug|x64`, Client `Release|x64`, and Server `Debug|x64` pass through `/compile`; targeted eviction/reacquisition passes through `/agent-harness`, and full recovery passes through the debugger-assisted `/agent-harness` procedure above.

## Coordination

No directional dependency is required. Existing PackChunks validation/failure Plans own separate malformed-data and allocation/error-state boundaries; this Plan owns only loader/reset exclusion and the lazy-pool metadata lifetime.

Coordinate with `Documents/Plans/Engine/AudioStreamingBackgroundRangeReads.md`, which will add audio jobs to the same PackChunks loaders and gives claimed audio work cancellation/acknowledgement rules. Either Plan may land first. Whichever lands second must reconcile the actual request producers, admission closure, full-recovery drain, cancellation acknowledgement, and File/Audio synchronization comments. Current disk-only music is evidence about the starting producer set, not an invariant after the audio Plan lands.

## Notes

Origin: the codebase-comment sweep's accepted pre-existing runtime residual, confirmed at the baseline and source sites in `## Context`. No separate audio-race Plan is warranted under the current music and request producers cited there. Plan authoring performed no runtime fix, build, debugger session, or harness run.
