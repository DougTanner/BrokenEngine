<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T23:08:57.419Z","dependsOn":[]} -->
# Split client island residency out of IslandTerrain into IslandTerrainResidency

## Context

`engine::IslandTerrain` (`Engine/Source/Frame/IslandTerrain.h`) is implemented across two `.cpp` files with different build affinity:

- `Engine/Source/Frame/IslandTerrain.cpp` — shared client+server (both vcxprojs), ~8,268 bt-token-v1: constructor/destructor, `WaitForElevationMaps`, `FrameNormal`/`MakeFrameElevationSampler`/`BuildElevationGrid`, `GlobalElevation`/`GlobalNormal`.
- `Engine/Source/Frame/IslandTerrainResidency.cpp` — client only (whole-file `#if defined(BT_CLIENT)`, client vcxproj only), ~4,839 bt-token-v1: `AcquireTextureSlot`, `FirstMintTextureSlot`, `AnyEvictionPending`, `AnyRestorationPending`, `IsEvictionPending`, `IsRestorationPending`, `HasArenaEvictionCandidate`, `EvictTemplate`, `EvictionSweep`, `RestorationSweep`, `ReleaseGpuResources`, `ResetTextureSlots`.

Merged, the class's member definitions total ~13,107 bt-token-v1, over the `/reduce-file` `.cpp` threshold of 10,000 (`.agents/skills/reduce-file/references/worker.md`), and one class is split between a client-only file and a shared file. The user decided that such a class is split into multiple classes, each keeping all its member definitions in one `.cpp` under the threshold, with no behavior change.

Code evidence for the boundary: every member defined in `IslandTerrainResidency.cpp` is declared under `#if defined(BT_CLIENT)` in `IslandTerrain.h` (public block after `GlobalNormal`, private block at the end of the class), and the private state they use — `miNextTextureSlot`, `mFreeTextureSlots`, `MeshEvictionReason` — is used by no shared member. The only shared state they touch is `mIslands` (iterated or looked up) plus the per-template client-only fields of the `IslandTemplate` struct. The shared members never call a residency member. The boundary is therefore already clean; only the class identity is shared.

## Design

The recommended design moves the client residency members into a new client-only manager class `engine::IslandTerrainResidency`, named after its existing implementation file and the "terrain residency" term `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation` already uses. Recommended over embedding it as a `BT_CLIENT` member object of `IslandTerrain` because `.agents/references/cpp-conventions.md` makes managers singletons reached through a `gp*` global whose constructor asserts the global is null and assigns `this`; that also lets residency code reach templates the way it already reaches every other manager, without storing a back-reference.

1. Create `Engine/Source/Frame/IslandTerrainResidency.h`: `#pragma once`, then a whole-file `#if defined(BT_CLIENT)` wrap (per `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/AGENTS.md`), including `Frame/IslandTerrain.h`. Declare `class IslandTerrainResidency` with:
   - public: constructor, destructor, and the seven public client members moved verbatim with their comments from `IslandTerrain` — `AcquireTextureSlot`, `EvictionSweep`, `RestorationSweep`, `AnyEvictionPending`, `AnyRestorationPending`, `ReleaseGpuResources`, `ResetTextureSlots`;
   - private: `FirstMintTextureSlot`, `MeshEvictionReason`, `EvictTemplate`, `IsEvictionPending`, `IsRestorationPending`, `HasArenaEvictionCandidate`, `miNextTextureSlot = 1`, `mFreeTextureSlots`, moved verbatim with their comments.
   - `inline IslandTerrainResidency* gpIslandTerrainResidency = nullptr;` after the class.
   - Move `kuiGraceRenderFrames` here from `IslandTerrain.h`; its only code use is `IsEvictionPending`. `IslandMeshResidency` stays in `IslandTerrain.h` because the shared header's `IslandTemplate` struct uses it.
2. `IslandTerrain.h`: delete the two `BT_CLIENT` blocks inside `class IslandTerrain` (the public residency methods and the trailing `private:` block). `IslandTemplate` (a struct, outside this restructure) and the `Buffer.h`/`Texture.h` client includes it needs stay unchanged.
3. `IslandTerrainResidency.cpp`: put `#include "IslandTerrainResidency.h"` first, alone in its group (style guide rule 47 a)), replacing the `IslandTerrain.h` include; keep the `Graphics/Managers/TextureManager.h` and `Graphics/Islands.h` includes in the next group. Re-qualify every member definition `IslandTerrain::` → `IslandTerrainResidency::`. Where a member body reads `mIslands`, read `gpIslandTerrain->mIslands` instead (a local `rIslands` reference at the top of a function is the recommended form where the map is used more than once). Add the constructor (`ASSERT(gpIslandTerrainResidency == nullptr); gpIslandTerrainResidency = this;`) and a destructor that clears the global when it equals `this`, mirroring `IslandTerrain::~IslandTerrain`. The file-local `static` helpers (`GetMeshRange`, `ReleaseMeshCpuRange`, `IsTextureRestorationPending`, `CreateElevationTextureFromHeightmap`) and the `MeshRange` struct stay as they are. No body logic changes.
4. `Engine/Source/Engine.h`: immediately after `#include "Frame/IslandTerrain.h"`, add a `#if defined(BT_CLIENT)` span including `Frame/IslandTerrainResidency.h`, so client callers that reach `gpIslandTerrain` through the PCH reach `gpIslandTerrainResidency` the same way.
5. `Engine/Source/Main.cpp` client path: immediately after `auto pIslandTerrain = std::make_unique<IslandTerrain>();`, add `auto pIslandTerrainResidency = std::make_unique<IslandTerrainResidency>();`. This keeps today's lifetime exactly: it exists before the `Graphics` constructor (whose `TextureManager` constructor calls `ResetTextureSlots`) and, being declared before `pGraphics`, is destroyed after `Graphics` teardown calls `ReleaseGpuResources`. The server path is unchanged.
6. Re-route the client call sites from `gpIslandTerrain->` to `gpIslandTerrainResidency->`:
   - `Engine/Source/Graphics/Graphics.cpp` `RenderGlobal`: `AnyEvictionPending`, `AnyRestorationPending`, `EvictionSweep`, `RestorationSweep`; and the teardown block that calls `ReleaseGpuResources` — change its `gpIslandTerrain != nullptr` guard to `gpIslandTerrainResidency != nullptr` and its "IslandTerrain is game-frame-owned (outlives Graphics)" comment to name `IslandTerrainResidency`.
   - `Engine/Source/Graphics/Islands.cpp` `UpdateActiveIslands`: `AcquireTextureSlot`.
   - `Engine/Source/Graphics/Managers/TextureManager.cpp` constructor: `ResetTextureSlots`.
   - `Projects/BrokenEngineSandbox/Source/Game.cpp` (menu island cycling, `BT_CLIENT`): `engine::gpIslandTerrainResidency->AcquireTextureSlot`.
   - `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp` (`ApplyReceivedStaticData`): same.
7. Comments that qualify a moved member as `IslandTerrain::` become `IslandTerrainResidency::` so they stay true: `Engine/Source/Graphics/Managers/TextureDescriptors.cpp` (three comments naming `IslandTerrain::AcquireTextureSlot` / `IslandTerrain::EvictionSweep`), `Engine/Source/Graphics/Objects/Pipeline.h`, `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`. Unqualified mentions (`AcquireTextureSlot`, `RestorationSweep`) need no change.
8. Project membership: add `Engine/Source/Frame/IslandTerrainResidency.h` as a client-only `ClInclude` in `BrokenEngineSandbox.vcxproj` and its `.filters` (same filter as `IslandTerrain.h`), routed through `/update-vcxproj`. `IslandTerrainResidency.cpp` membership (client only) is unchanged; the server project gains nothing.

Expected sizes: `IslandTerrain.cpp` ~8,268 bt-token-v1 (unchanged); `IslandTerrainResidency.cpp` ~4,950 (from ~4,839; constructor, destructor, global accesses); `IslandTerrain.h` ~2,500 (from ~3,437); `IslandTerrainResidency.h` ~1,100. Each class's member definitions then live in exactly one `.cpp`, each under 10,000, and each class is wholly shared or wholly client-only.

Change Workflow tier: Tier 2 — highest trigger: public interface moved across independently owned subsystems (Frame, Graphics, `Main.cpp` boot ordering, game client callers) with a new manager global whose construction and destruction order must match today's. Not Tier 3: determinism/CRC is untouched (residency is client-only visual state outside every CRC per `Engine/Source/Frame/AGENTS.md` `## Terrain and Navigation`; no shared or server code path changes); no serialized, wire, `.pack`, or collection layout changes (`IslandTemplate` and `IslandTerrain`'s shared members are unchanged; `IslandTerrain` is not a Collection); threading is unchanged (all residency calls stay on the main/render thread at their current call points).

## Critical files

- `Engine/Source/Frame/IslandTerrain.h`
- `Engine/Source/Frame/IslandTerrainResidency.h` (new)
- `Engine/Source/Frame/IslandTerrainResidency.cpp`
- `Engine/Source/Engine.h`
- `Engine/Source/Main.cpp`
- `Engine/Source/Graphics/Graphics.cpp`
- `Engine/Source/Graphics/Islands.cpp`
- `Engine/Source/Graphics/Managers/TextureManager.cpp`
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp`
- `Engine/Source/Graphics/Objects/Pipeline.h`
- `Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp`
- `Projects/BrokenEngineSandbox/Source/Game.cpp`
- `Projects/BrokenEngineSandbox/Source/Network/Client/ClientSessionReceive.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj` and `.vcxproj.filters`

## In scope

- `class IslandTerrain` declaration: removal of its two `BT_CLIENT` member blocks; `kuiGraceRenderFrames` moved out of `IslandTerrain.h`.
- New `class IslandTerrainResidency`, its constructor/destructor, and `gpIslandTerrainResidency` in the new header.
- `IslandTerrainResidency.cpp`: include block, member qualifiers, `mIslands` → `gpIslandTerrain->mIslands` accesses, new constructor/destructor.
- `Engine.h`: the one new `BT_CLIENT` include span.
- `Main.cpp` client path: the one `make_unique<IslandTerrainResidency>()` line.
- The call sites and the `IslandTerrain::`-qualified comments listed in Design steps 6 and 7.
- Client project/filter `ClInclude` entry for the new header.

## Out of scope

- Any logic change inside the moved member bodies or the file-local helpers.
- `struct IslandTemplate` (including its client-only fields and `IslandMeshResidency`), `FrameElevationSampler`, and every shared `IslandTerrain` member and data member, including the `BT_CLIENT` channel-CRC uniqueness ASSERT in the `IslandTerrain` constructor and the `BT_CLIENT` block in `WaitForElevationMaps`.
- Moving residency into `Graphics/` or into `Islands`; renaming `IslandTerrain`.
- Server code, `Main.cpp` server path, server project membership.
- `AGENTS.md` changes: no AGENTS.md names the moved members or the class; `/update-claude-docs` still runs as usual.

## Acceptance criteria

- Client and server builds succeed through `/compile`.
- Every `IslandTerrain::` member definition is in `IslandTerrain.cpp`; every `IslandTerrainResidency::` member definition is in `IslandTerrainResidency.cpp`; no `IslandTerrain::` definition remains in `IslandTerrainResidency.cpp`.
- `IslandTerrain.h`'s `class IslandTerrain` contains no `BT_CLIENT` block; `IslandTerrainResidency.h` is whole-file `BT_CLIENT`-wrapped and in the client project only.
- `IslandTerrain.cpp` and `IslandTerrainResidency.cpp` each measure at most 10,000 bt-token-v1.
- `/update-vcxproj` validation passes.
- Live (`/agent-harness`): launch client and server, reach a cell with islands, and confirm islands render with their real textures and mesh (screenshot) with no ASSERT and `Island resident` / `Restored island mesh` log lines present.

## Notes

- Supersedes the `Engine/Source/Frame/IslandTerrainResidency.cpp` row of `Documents/Plans/Game/SplitCppCorrespondingHeaderOrder.md`: Design step 3 makes this file's corresponding header `IslandTerrainResidency.h` and puts it first. No `dependsOn` edge either way — that Plan's acceptance is its `-All` rerun, so whichever lands first, the other still passes: if this lands first its row is already satisfied; if that lands first this Plan rewrites the include block anyway.
- Run `.agents/scripts/Test-IncludeOrder.ps1` over the changed `.cpp` files as `/code-style-review` documents it.
