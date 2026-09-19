<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-18T22:41:17.287Z","dependsOn":[]} -->
# Fix: Recover every registered lighting texture after full Graphics recreation

## Context

`TypeRegistry::RegisterType` is the only caller of the client lazy-texture
request helper, and it always pairs that request with the lighting pre-blur
registration for the same CRC
(`Engine/Source/Frame/Collections/Collection.h:127-155`; `RequestTextureChunkLoad`
declared at `Collection.h:111`, defined at `Engine/Source/File/PackChunks.cpp:1288`,
called only at `Collection.h:139` and `Collection.h:150`). Registration runs once,
from `GameBase::GameBase` (`Engine/Source/GameBase.cpp:23`) during `game::Game`
construction (`Engine/Source/Main.cpp:315`) — after `Graphics` construction
(`Main.cpp:311`) — and never again.

A full client Graphics recreation (destroy tier `kSurface`) destroys the texture
manager (`Engine/Source/Graphics/Graphics.cpp:745`, inside `Graphics::Destroy`)
and then resets every texture chunk's state
(`Graphics.cpp:775`, `PackChunks::ResetTextureChunkStates` at
`Engine/Source/File/PackChunks.cpp:616-663`: `kReady` demotes to `kNotLoaded`,
`kUploading`/`kGpuUploadComplete` demote to `kDiskLoaded`). The replacement
`Graphics::Create` builds a fresh `TextureManager` (`Graphics.cpp:418-435`) whose
lighting-CRC set `mLightingTextureCrcs`
(`Engine/Source/Graphics/Managers/TextureManager.h:185`) is empty and whose lazy
textures sit on the white placeholder. Nothing re-requests those chunks and
nothing re-registers them for pre-blur, so after recovery every registered
lighting texture stays on the placeholder and, if some other path later loads the
source, its lighting deposit keeps resolving to the unblurred fallback instead of
the `kBlurSalt`-salted blurred descriptor.

Six type registries are affected, because each carries a CRC that
`RegisterType` feeds to both helpers: `AreaLights`, `Billboards`, `PointLights`,
`Puffs`, `SmokeTrails` (a `crc` field each) and `Explosions` (a `particleCrc`).
`HexShields`, `WindRadials`, and the game's `Blasters` registries carry no CRC
and are unaffected.

The other lazy-load request sources already replay themselves after
recreation, which is why only the registry side effect is lost: pipelines
re-request in `Pipeline::Create` (`Engine/Source/Graphics/Objects/Pipeline.cpp:132`)
and on their first positive instance write (`Pipeline.cpp:331`) because
`PipelineManager` is rebuilt; boot, priority and IBL textures re-request in
`TextureManager::InitializeBootTextures`
(`Engine/Source/Graphics/Managers/TextureManager.cpp:247-254`); island channels
re-request through `ResetTextureSlots` and
`Engine/Source/Frame/IslandTerrainResidency.cpp`.

Impact: after a supported device- or surface-loss recovery, all lighting-textured
client visuals — area lights, billboards, point lights, puffs, smoke trails, and
explosion particles — render white and light the world with an unblurred deposit
for the rest of the process.

## Design

The user directed the central, texture-layer-owned fix over a per-collection
replay, and authorized it to exceed the scope of the per-collection Plans it
replaces. Three regions change, no collection leaf is touched:

1. The lighting-CRC set becomes process-lifetime storage, so it survives a
   texture-manager teardown and remains the authoritative list of CRCs whose
   requests were lost. It takes the same shape as the collection type registry's
   own process-lifetime vector `TypeRegistry::sTypes`
   (`Engine/Source/Frame/Collections/Collection.h:125`), with the Hungarian
   static prefix replacing the member prefix at its existing references. Its
   `kiLightingBlurSlots` capacity assertion
   (`TextureManager.cpp:739`, constant at `TextureManager.cpp:18`) is unaffected:
   re-registration re-inserts the same CRCs into a set.

2. `TextureManager::BlurLightingTexture` gains an early return when
   `gpPipelineManager` is null. This is mandatory, not defensive: with a
   surviving set, `InitializeBootTextures`' `WaitForTextures` spin
   (`TextureManager.cpp:247-248`, `TextureManager.cpp:656-679`) drives
   `ProcessPendingTextures`, which can adopt a lighting chunk the `kSurface`
   reset left at `kDiskLoaded` — and that runs before `Graphics::Create`
   constructs `PipelineManager` (`Graphics.cpp:418-435` versus
   `Graphics.cpp:440-443`), while the blur reaches its two blur pipelines through
   `gpPipelineManager` (`TextureManager.cpp:791-792`). Today the empty set makes
   that a silent skip; with a surviving set it would be a null dereference. One
   early return covers both adoption call sites
   (`TextureManager.cpp:566-569` in `ProcessPendingTextures` and
   `TextureManager.cpp:621-624` in `AdoptUploadedChunk`). Region 3's reblur then
   repairs any chunk this skip left unblurred.

3. `Graphics::Create` records whether it just constructed the texture manager, in
   a local flag set inside its `mpTextureManager == nullptr` block
   (`Graphics.cpp:418-435`). Only when that flag is set does it, immediately
   after the pipeline-manager construction block (`Graphics.cpp:440-443`),
   re-request every CRC in the surviving set at realtime priority and then call
   `TextureManager::ReblurAllLightingTextures` (`TextureManager.cpp:839-849`), so
   any chunk adopted during the boot-texture wait gets its blurred copy, while
   pipeline-only and swapchain-only recreation, which keep their texture manager,
   run neither the replay nor the reblur.

Author's recommendation for the one implementation choice this leaves: the
load-request entry point `FileManager::RequestChunkLoad`
(`Engine/Source/File/FileManager.h:229`) takes a contiguous span, which an
unordered set cannot supply, so copy the set into a workbuffer arena and issue
one batched request — the precedent already in
`TextureManager::WaitForTextures(std::span<Texture* const>)`
(`TextureManager.cpp:716-725`). Prefer it over a per-CRC loop through the
single-CRC helper `RequestTextureChunkLoad`, because that helper requests at the
default `LoadPriority::kNormal` while recovery wants the realtime priority
`InitializeBootTextures` uses for priority textures (`TextureManager.cpp:254`),
and because one batched call takes the request queue's lock and publishes its
loader wake once (`PackChunkLoader::RequestChunkLoad` at
`Engine/Source/File/PackChunkLoader.cpp:57-104`).

Why this is complete and safe, verified against the current tree:

- The reset leaves no lighting chunk ready, so the only way one becomes ready
  before region 3 is adoption during the boot-texture wait; region 2 leaves that
  one unblurred and region 3 blurs it. `ReblurAllLightingTextures` skips a CRC
  whose chunk is not yet ready, and those blur normally at adoption once the
  pipeline manager exists.
- The flag gate confines the replay to the path that rebuilt the texture manager,
  the only path whose chunk-state reset lost the requests; and even there the
  replay is idempotent, because `PackChunkLoader::RequestChunkLoad` skips any
  chunk at or past `kDiskLoaded` (`PackChunkLoader.cpp:73-77`).
- The replay runs in `Graphics::Create`, after `Graphics::Destroy` drained the
  pack loaders (`Graphics.cpp:680`), waited the upload thread
  (`Graphics.cpp:688`), waited the device (`Graphics.cpp:694`) and performed the
  reset (`Graphics.cpp:775`), and before command buffers are re-recorded — so
  there is no teardown race and no frame in flight when the reblur replaces
  images and rewrites bindless descriptors.
- Only island channels are ever evicted
  (`Engine/Source/Frame/IslandTerrainResidency.cpp:310`), and no lighting CRC is
  an island channel, so a process-lifetime lighting set cannot resurrect an
  evicted texture and island residency behavior is unchanged.

Documentation: extend the `### TextureManager` section of
`Engine/Source/Graphics/Managers/AGENTS.md` under `## Manager Contracts` — the
lazy-texture load-request enumeration (`AGENTS.md:62`, today naming
non-indirect pipelines at creation, indirect pipelines at their first positive
instance write, and priority textures at boot) gains the recreation replay,
without adding or restating a count, and the lighting-set bullet records the
set's process lifetime. No collection-leaf `AGENTS.md` changes, because
registration itself stays startup-only and once-per-type.

## Critical files

- `Engine/Source/Graphics/Managers/TextureManager.h:185` — `mLightingTextureCrcs`,
  the set whose lifetime changes.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:566-569,621-624,732-740,742-797,839-849` —
  the two adoption checks, `RegisterLightingTextureCrc` and its capacity
  assertion, `BlurLightingTexture`, and `ReblurAllLightingTextures`.
- `Engine/Source/Graphics/Graphics.cpp:418-443,674-694,745,775` — manager
  construction order in `Create`, and the drains, texture-manager reset, and
  chunk-state reset in `Destroy`.
- `Engine/Source/Frame/Collections/Collection.h:111-155` — the registration site
  that pairs the load request with the pre-blur registration, and `sTypes` as the
  process-lifetime precedent.
- `Engine/Source/File/PackChunks.cpp:616-663` and
  `Engine/Source/File/PackChunkLoader.cpp:57-104` — the full-recovery chunk-state
  reset and the idempotent request path.
- `Engine/Source/Graphics/Managers/AGENTS.md:62` (`### TextureManager`) — the
  lazy-texture load-request enumeration and the lighting-set contract.

## In scope

- `Engine/Source/Graphics/Managers/TextureManager.h`: changing the lighting-CRC
  set member `mLightingTextureCrcs` to process-lifetime storage, with the
  Hungarian static rename.
- `Engine/Source/Graphics/Managers/TextureManager.cpp`: the renamed references in
  `RegisterLightingTextureCrc`, `ProcessPendingTextures`, `AdoptUploadedChunk`
  and `ReblurAllLightingTextures`; and one early return in
  `TextureManager::BlurLightingTexture` when `gpPipelineManager` is null.
- `Engine/Source/Graphics/Graphics.cpp`: in `Graphics::Create`, a local flag set
  inside the `mpTextureManager == nullptr` block recording that this call
  constructed the texture manager, and, only when that flag is set, immediately
  after the pipeline-manager construction block, re-requesting every CRC in the
  surviving lighting set at realtime priority and then calling
  `ReblurAllLightingTextures`.
- `Engine/Source/Graphics/Managers/AGENTS.md`, `### TextureManager` under
  `## Manager Contracts`: the recreation-replay lazy-texture load-request source
  and the lighting set's process lifetime.

## Out of scope

- Re-requesting demoted CRCs inside `PackChunks::ResetTextureChunkStates`. That
  reset is the last step of a full `Graphics::Destroy`, which has already drained
  the loader threads precisely so none can publish `kUploading` after it, waited
  the upload thread, and destroyed the transfer resources; issuing requests there
  would re-arm work against a device being torn down.
- Excluded: a wide "remember every requested CRC" set at the File layer would
  intercept a path audio also uses and resurrect evicted island textures, and a
  per-chunk "was requested" bit has the same eviction problem and would not
  restore the pre-blur set.
- Any per-collection replay: no file under `Engine/Source/Frame/Collections/` or
  `Projects/**/Source/Frame/Collections/` changes, including their `AGENTS.md`
  files, and the startup registry contract and `TypeRegistry::RegisterType` are
  unchanged.
- Shaders, texture formats, the lighting blur algorithm, and the bindless
  descriptor architecture.
- `Graphics::Destroy` ordering, destroy-tier policy, device-loss teardown, and
  adding a debug device-loss or `kSurface` trigger.
- Wire, save, replay, `.pack`, `kiVersion`, deterministic CRC, and
  backward-compatibility changes.
- Island texture residency and eviction policy.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change alters a manager member's
lifetime and an adoption-path condition, and its correctness spans three
independently owned subsystems' invariants — Frame collection type registration,
File lazy-chunk state, and TextureManager bindless pre-blur publication — while
depending on Graphics device-recovery construction ordering.

Preserve these invariants:

- Registered type indices remain immutable and valid across Graphics recreation;
  the type registries are not read, written, or re-registered by this change.
- A lighting deposit samples the `kBlurSalt`-salted blurred texture while the
  visible sprite samples the unblurred source, and deposit/sprite cursors stay
  aligned.
- All touched state is client-only and stays outside the simulation/PostRender
  CRC, save, replay, wire, and `.pack` data.
- Pipeline-only and swapchain-only recreation keep their current behavior: the
  surviving texture manager keeps its ready textures, and with the flag unset
  neither the replay request nor the reblur runs.
- No lighting blur runs before `gpPipelineManager` exists, and the reblur runs
  only where no frame is in flight.
- The registered lighting-texture count still fits the `kiLightingBlurSlots`
  bindless reservation.

## Acceptance criteria

- Decisive evidence for the full-recreation leg is code inspection, not a live
  run: destroy tier `kSurface` is unreachable from settings or agent commands
  (no `Graphics::Refresh` poll escalates above `kSwapchain`, the agent `resize`
  and `fullscreen` commands drive only the swapchain tier, and no debug
  device-loss injection exists), so this Plan does not require a live
  full-recreation reproduction. The inspection must show that on the `kSurface`
  path every CRC in the surviving set is re-requested after the pipeline manager
  exists, that a chunk adopted during the boot-texture wait is blurred by the
  closing `ReblurAllLightingTextures`, and that no blur call can precede the blur
  pipelines — so no registered lighting source stays on the white placeholder and
  no lighting deposit keeps the unblurred fallback.
- The surviving set is shown to be the complete list of lost requests: a
  repository search confirms `TypeRegistry::RegisterType` is still the only
  caller of `RequestTextureChunkLoad` and still pairs it with
  `RegisterLightingTextureCrc` for the same CRC, covering all six CRC-bearing
  registries including the `Explosions` particle CRC.
- Partial-recreation regression check through `/agent-harness`, explicitly a
  regression guard and not evidence the defect is fixed: launch the client, make
  lighting textures live, drive `resize` and `fullscreen`; those tiers keep the
  texture manager, so the flag stays unset, neither the replay request nor the
  reblur pass runs, rendering is unchanged, and the log shows no
  texture-adoption, descriptor-generation, or validation error and no assertion.
- Client `Debug|x64` and `Release|x64` build clean through `/compile`, with no
  new warnings.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### TextureManager` names the
  recreation replay in its lazy-texture load-request enumeration alongside the
  sources it already lists, and records the lighting set's process lifetime; no
  collection `AGENTS.md` changed.

## Notes

This Plan replaces the former per-collection recovery Plans
`Documents/Plans/Engine/AreaLightsGraphicsRecovery.md` and
`Documents/Plans/Engine/ExplosionParticleGraphicsRecovery.md`, which were removed
when it was created: it achieves the outcome both asked for, at the texture layer
instead of in each collection, and covers `Billboards`, `Puffs` and `SmokeTrails`
as well, so no per-collection follow-up remains.

`Documents/Plans/Engine/LightingReblurFenceOrdering.md` is independent and is not
a prerequisite. It owns the live blur-setting change path — `Graphics::Refresh`
and the post-drain window in `Graphics::Destroy` — and explicitly excludes
texture-registration replay after device loss from its scope, expecting the
replacement resource path to handle its own textures, which is exactly what this
Plan adds. The new reblur call site is in `Graphics::Create`, already after that
Plan's drains and before command buffers are re-recorded, so it needs no state or
flag that Plan introduces; either Plan can land first without changing the other.
