<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-08T23:36:39.349Z","dependsOn":[]} -->
# Handle the singleton teardown guard family in one change

## Context

A repository-wide over-engineering (YAGNI) sweep found that the singleton teardown guard `if (gpX == this) { gpX = nullptr; }` is always true at most of its sites. Four sweep Plans had each proposed converting a subset of these sites, while `Documents/Plans/Game/RemoveSandboxOverEngineering.md` refused its two sites to keep the mirrored pattern parallel. A coherence review flagged the split. The resolution was to remove the guard candidates from the Graphics, Common, Agent, and EngineRuntime sweep Plans and handle the whole family once, here, so the root `AGENTS.md` "mirrored patterns stay parallel" directive holds.

Why the guard is expected to be always true: `.agents/references/cpp-conventions.md` (Managers bullet) requires every `gp*` singleton constructor to `ASSERT` its global is null and then assign `this`. When the destructor is the only other writer, the global equals `this` whenever the destructor runs. A constructor that throws never runs its own destructor, so a failed construction does not reach the guard either.

**Every site is unverified input, not an approved change.** The sweep validated only some of these sites, and this Plan's author found one site (`Graphics.cpp:118`, below) whose guard can be false. When the sweep Plans were recorded, the user asked that the executing agent not trust these recommendations blindly. The executor re-verifies each site against current code, and line numbers have drifted, so locate each by symbol.

## Design

### Per-site verification

For each site:

1. Locate the destructor (or handler) and the constructor by symbol and confirm both still exist in the quoted shape.
2. Prove `gpX == this` holds at the guard: the constructor `ASSERT`s the global is null before assigning `this`, and a repository-wide search (`git grep` over `Engine/`, `Common/`, `DataPacker/`, `Projects/`, and `Tools/`) finds no other writer of the global. Also check every owner: no path constructs a second instance while one is alive, and no path destroys the object after something else cleared the global. Do not reuse this Plan's evidence without re-reading the code.
3. When step 2 finds a path on which the global can differ from `this` at the guard, the guard is a real check, not a member of this family: leave that site unchanged and report it with the `path:line` that makes it false.

### One form for the whole family

The author recommends the form `ASSERT(gpX == this);` followed by `gpX = nullptr;` (for `DiagnosticLog`, the `gpDiagnosticLogs[miIndex]` slot in place of `gpX`). It mirrors the constructor's null `ASSERT`. The `cpp-conventions.md` "No useless ASSERTs" rule does not argue for deletion here, because nothing crashes one line later if the invariant is broken: a plain `gpX = nullptr;` would silently clear another live instance's global. `ASSERT` (`Common/ErrorUtils.h:20`) is active in every build configuration; inside a destructor it ends the process, which is the intended hard stop.

Apply one form to every site that step 2 proves. If the executor concludes that a different form (plain removal of the guard) is better, it applies that form to every proven site instead, never a mix of forms. If the proof fails for so many sites that the remainder would no longer be a meaningful family, apply none and report every site.

### Sites (as of `main` `9d643e02`)

Constructor lines are the null `ASSERT` and the `this` assignment.

Common:

1. `Common/Threading/Multithreading.cpp:29` `~Multithreading` — `gpMultithreading` (constructor `:8-10`).
2. `Common/Log/DiagnosticLog.cpp:17` `~DiagnosticLog` — the `gpDiagnosticLogs[miIndex]` array slot rather than a single `gpX` global, otherwise the same guard (constructor `:10-12`; the class has deleted copy operations in `DiagnosticLog.h`, and `FILE_LOG_INIT` is its only construction site). This was candidate 1 of `Documents/Plans/Engine/RemoveCommonOverEngineering.md`.

Engine:

3. `Engine/Source/Agent/AgentInput.cpp:36` `~AgentInput` — `gpAgentInput` (constructor `:30-31`).
4. `Engine/Source/Agent/AgentUiRegistry.cpp:67` `~AgentUiRegistry` — `gpAgentUiRegistry` (constructor `:61-62`).
5. `Engine/Source/Audio/AudioManager.cpp:251` `~AudioManager` — `gpAudioManager` (constructor `:215-217`).
6. `Engine/Source/File/FileManager.cpp:201` `~FileManager` — `gpFileManager` (constructor `:131` null `ASSERT`, `:192` assignment after `mpPackChunks` is built). Keep the `mpPackChunks.reset()` before the clear and its comment.
7. `Engine/Source/File/Replay.cpp:265` `~Replay` — `gpReplay` (constructor `:256-258`).
8. `Engine/Source/Frame/IslandTerrain.cpp:124` `~IslandTerrain` — `gpIslandTerrain` (constructor `:24-26`).
9. `Engine/Source/Frame/IslandTerrainResidency.cpp:95` `~IslandTerrainResidency` — `gpIslandTerrainResidency` (constructor `:88-90`).
10. `Engine/Source/Graphics/CameraBase.cpp:72` `~CameraBase` — `gpCamera` (constructor `:63-65`).
11. `Engine/Source/Graphics/Graphics.cpp:118` `~Graphics` — `gpGraphics` (constructor `:87-89`). **Expected to fail step 2; see `### Graphics`.**
12. `Engine/Source/Graphics/Graphics.cpp:434` — the `gpGraphics` clear in the `catch (...)` around `mpTextureManager->InitializeBootTextures()` in `Graphics::Create`, before the rethrow.
13. `Engine/Source/Graphics/Islands.cpp:104` `~Islands` — `gpIslands` (constructor `:13-15`).
14. `Engine/Source/Graphics/Managers/BufferManager.cpp:259` `~BufferManager` — `gpBufferManager` (constructor `:14-16`).
15. `Engine/Source/Graphics/Managers/CommandBufferManager.cpp:41` `~CommandBufferManager` — `gpCommandBufferManager` (constructor `:15-17`).
16. `Engine/Source/Graphics/Managers/DeviceManager.cpp:359` `~DeviceManager` — `gpDeviceManager` (constructor `:10-12`).
17. `Engine/Source/Graphics/Managers/ImGuiManager.cpp:297` `~ImGuiManager` — `gpImGuiManager` (constructor `:84-86`).
18. `Engine/Source/Graphics/Managers/InstanceManager.cpp:726` `~InstanceManager` — `gpInstanceManager` (constructor `:248-250`).
19. `Engine/Source/Graphics/Managers/ParticleManager.cpp:19` `~ParticleManager` — `gpParticleManager` (constructor `:10-12`).
20. `Engine/Source/Graphics/Managers/PipelineManager.cpp:170` `~PipelineManager` — `gpPipelineManager` (constructor `:14-16`).
21. `Engine/Source/Graphics/Managers/SwapchainManager.cpp:528` `~SwapchainManager` — `gpSwapchainManager` (constructor `:14-16`).
22. `Engine/Source/Graphics/Managers/TextureManager.cpp:268` `~TextureManager` — `gpTextureManager` (constructor `:95-97`).
23. `Engine/Source/Graphics/Managers/TextureUploadManager.cpp:19` `~TextureUploadManager` — `gpTextureUploadManager` (constructor `:12-14`).
24. `Engine/Source/Input/RawInputManager.cpp:32` `~RawInputManager` — `gpRawInputManager` (constructor `:12-14`).
25. `Engine/Source/Network/Client/Client.cpp:83` `~Client` — `gpClient` (constructor `:19-21`).
26. `Engine/Source/Network/Server/Server.cpp:61` `~Server` — `gpServer` (constructor `:30-32`).

Projects:

27. `Projects/BrokenEngineSandbox/Source/Game.cpp:374` `~Game` — `gpGame` (constructor `:34-36`).
28. `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSession.cpp:53` `~ClientSession` — `gpClientSession` (constructor `:39-41`).
29. `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp:44` `~ProfileManager` — `gpProfileManager` (constructor `:11-13`).

DataPacker:

30. `DataPacker/Source/FileManager.cpp:588` `~FileManager` — the DataPacker's own `gpFileManager`, distinct from site 6 (constructor `:346` null `ASSERT`, `:349` assignment).

Sites 1 and 3-30 are the 29 `if (gp... == this)` matches of the repository-wide `git grep -nE "if \(gp[A-Za-z]+ == this\)" main -- . ':!ThirdParty'`; site 2 is the same guard on an array slot.

### Graphics

The author's reading of current code finds a reachable path on which `~Graphics` runs with `gpGraphics == nullptr`:

1. `VK_ERROR_SURFACE_LOST_KHR` sets `meDestroyType = DestroyType::kSurface` (`Engine/Source/Graphics/GraphicsUtils.cpp:51`).
2. The next `Graphics::Create` (called from the render path, `Graphics.cpp:251`) runs `Destroy`, whose full-destroy branch resets `mpTextureManager` (`:726`), so `Create` builds a new `TextureManager` and sets `bInitializeBootTextures` (`:381-384`).
3. A device-lost `CHECK_VK` inside `InitializeBootTextures` throws (`GraphicsUtils.cpp:55-58`). The `catch (...)` at `:430-439` clears `gpGraphics` and rethrows.
4. `Engine/Source/Main.cpp:441-449` catches the device-lost `std::system_error` and runs `pGraphics.reset()`, so `~Graphics` runs with `gpGraphics` already null.

On that path, site 11's guard is false, and `ASSERT(gpGraphics == this)` would end the process during device-loss recovery. If the executor confirms the path, site 11 keeps its `if` unchanged and is reported, and site 12 (always true: the constructor set the global and only this handler clears it before the object is destroyed) takes the family form like every other proven site. The catch-path clear itself stays: it is what keeps the global from dangling when `Create` throws from inside the constructor, where `~Graphics` never runs.

## Critical files

- `Common/Threading/Multithreading.cpp`, `Common/Log/DiagnosticLog.cpp`
- `Engine/Source/Agent/AgentInput.cpp`, `AgentUiRegistry.cpp`
- `Engine/Source/Audio/AudioManager.cpp`
- `Engine/Source/File/FileManager.cpp`, `Replay.cpp`
- `Engine/Source/Frame/IslandTerrain.cpp`, `IslandTerrainResidency.cpp`
- `Engine/Source/Graphics/CameraBase.cpp`, `Graphics.cpp`, `Islands.cpp`
- `Engine/Source/Graphics/Managers/BufferManager.cpp`, `CommandBufferManager.cpp`, `DeviceManager.cpp`, `ImGuiManager.cpp`, `InstanceManager.cpp`, `ParticleManager.cpp`, `PipelineManager.cpp`, `SwapchainManager.cpp`, `TextureManager.cpp`, `TextureUploadManager.cpp`
- `Engine/Source/Input/RawInputManager.cpp`
- `Engine/Source/Network/Client/Client.cpp`, `Engine/Source/Network/Server/Server.cpp`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`, `Source/Network/Client/ClientSession.cpp`, `Source/Profile/ProfileManager.cpp`
- `DataPacker/Source/FileManager.cpp`

## In scope

- Only the guard statement at each of sites 1-30 — the `if (gpX == this)` test and its braces around `gpX = nullptr;` — replaced by the one family form, and only at the sites the executor proves.
- A comment adjacent to a changed guard that the change makes false.

## Out of scope

- Every other statement in these destructors and in `Graphics::Create`, including the `Graphics.cpp:434` catch-path clear itself, the Engine `FileManager`'s `mpPackChunks.reset()` ordering, and every constructor.
- Destructors that already clear their global unconditionally, with no guard: `~ThreadLocal` (`Common/Threading/ThreadLocal.cpp:31`) and `~ServerSession` (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerSession.cpp:40`).
- The `gpAgentCommandServer` scope-guard clear in `Engine/Source/Main.cpp:157`, which is not a `this` comparison.
- Any site the executor cannot prove. Report it; do not substitute a different change for it.
- The other over-engineering sweep candidates, which their own Plans own: `Documents/Plans/Engine/GraphicsOverEngineeringCleanup.md`, `RemoveCommonOverEngineering.md`, `RemoveAgentOverEngineering.md`, `RemoveEngineRuntimeOverEngineering.md`, `FileOverEngineeringCleanup.md`, `NetworkServerOverEngineeringCleanup.md`, `RemoveDataPackerOverEngineering.md`, `Documents/Plans/Game/RemoveSandboxOverEngineering.md`, and `Documents/Plans/Tools/RemoveToolsOverEngineering.md`.

## Acceptance criteria

- The completion summary lists every site 1-30 as converted, with the executor's own proof (the constructor `ASSERT` and the absence of any other writer, cited against current code), or left unchanged, with the `path:line` that makes its guard false. Site 11 is either left unchanged with the confirmed device-loss path cited, or converted with a cited disproof of that path.
- Every converted site uses the same form.
- Client and server build clean in Debug and Release through `/compile`, and DataPacker builds clean in Release (the only configuration agents build it in, per `/compile` `## Inputs`).
- A DataPacker run exits `0` with no `ASSERT`; at exit it destroys both its `Multithreading` (site 1, constructed at `DataPacker/Source/Main.cpp:713`) and its `FileManager` (site 30). Changing `DataPacker/Source/FileManager.cpp` makes Local data mode mandatory for game builds (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`), so the executor requests Local generation authorization at plan approval; that generation build's DataPacker run is this check.
- `/agent-harness`: a client and a server launch, connect, and shut down cleanly with no `ASSERT` (every destructor in the family runs at shutdown), and a client return to the main menu and back into a game (recreating `Game`-owned and session objects) raises no `ASSERT`.

## Notes

- Change Workflow tier: **Tier 3** (`.agents/references/risk-tiers.md`). Trigger: the change spans independently owned subsystems (Common, Engine Agent, Audio, File, Frame, Graphics, Input, and Network, the DataPacker, and the game under `Projects/`). Each edit is local and touches no determinism/CRC, wire, serialization, or trust-boundary surface; the only behavior change is a hard stop where the old guard silently skipped the clear.
- No `AGENTS.md` or documentation names the guard form this Plan changes.
- The sweep Plans listed under `## Out of scope` edit other regions of many of these files; `NetworkServerOverEngineeringCleanup.md` candidate 4 edits another statement inside `~Client`, and `RemoveDataPackerOverEngineering.md` candidates D44-D46 edit other functions in `DataPacker/Source/FileManager.cpp`. Every such region is disjoint from these guards, so there is no required ordering; re-locating each site by symbol covers whichever lands first.
