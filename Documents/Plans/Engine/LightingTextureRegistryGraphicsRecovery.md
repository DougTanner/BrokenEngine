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
textures sit on the white placeholder (`TextureManager.cpp:211`). Nothing
re-requests those chunks and nothing re-registers them for pre-blur, so after
recovery every registered lighting texture stays on the placeholder and, if some
other path later loads the source, its lighting deposit keeps resolving to the
unblurred fallback (`TextureDescriptors::CrcToBlurredIndex` at
`Engine/Source/Graphics/Managers/TextureDescriptors.cpp:712-721`) instead of the
`kBlurSalt`-salted blurred descriptor.

Both recovery legs lose the registration side effect. In place, a lost surface
escalates the destroy tier (`Engine/Source/Graphics/GraphicsUtils.cpp:52`) and the
next `Graphics::Create` runs `Destroy()` itself (`Graphics.cpp:356`), which nulls
the texture manager. On `VK_ERROR_DEVICE_LOST` the whole object is replaced: the
render try/catch discards the `Graphics` unique pointer and constructs a new one
(`Engine/Source/Main.cpp:462-470`), so nothing owned by a `Graphics` can carry the
list across. Neither leg is reachable from a setting or an agent command — no
`Graphics::Refresh` poll escalates above `kSwapchain`, and the remaining
`kSurface` uses are shutdown (`Graphics.cpp:156`) and the boot-texture failure
path that rethrows instead of recovering (`Graphics.cpp:427`).

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
re-request through `ResetTextureSlots` (`TextureManager.cpp:227`) and
`Engine/Source/Frame/IslandTerrainResidency.cpp`.

Impact: after a supported device- or surface-loss recovery, all lighting-textured
client visuals — area lights, billboards, point lights, puffs, smoke trails, and
explosion particles — render white and light the world with an unblurred deposit
for the rest of the process.

## Design

The user chose the upload-manager mechanism: the registry lives on the one client
manager that already outlives Graphics recreation, and the replay rides the boot
path that already runs exactly once per texture-manager construction. Four
regions change; no collection leaf and no File-layer source changes.

1. Ownership moves from `TextureManager` to `TextureUploadManager`. That manager
   is constructed in `wWinMain` (`Engine/Source/Main.cpp:904`) before
   `FileManager` (`Main.cpp:912`) and before `MainThread` runs at all
   (`Main.cpp:929`), it publishes its global in its own constructor
   (`Engine/Source/Graphics/Managers/TextureUploadManager.cpp:8-13`), and
   `Graphics::Destroy` never resets it — full teardown only calls
   `DestroyTransferResources` on it (`Graphics.cpp:773`). So it survives the
   in-place `kSurface` recreate and the whole-object replacement at
   `Main.cpp:462-470` alike. This is the established home for exactly this kind
   of state: the pending-adoption counter already lives there for the same
   reason (`Engine/Source/Graphics/Managers/TextureUploadManager.h:30-38`,
   documented at `Engine/Source/Graphics/Managers/AGENTS.md:80`). Add the
   registered-CRC set as a public member beside that counter, with a comment
   giving the same outlives-recreation rationale. No static or process-lifetime
   storage is introduced anywhere.

   The free `RegisterLightingTextureCrc` (`TextureManager.cpp:727-730`), which is
   what `Collection.h:114` declares and `Collection.h:140,151` call, becomes the
   registration itself: it inserts into the upload manager's set under the
   existing allocation-tracking suppression and keeps the
   `kiLightingBlurSlots` capacity assertion (constant at `TextureManager.cpp:18`,
   assertion today at `TextureManager.cpp:739`). It stays defined in
   `TextureManager.cpp`, which is where that file-local constant and the matching
   descriptor reservation (`TextureManager.cpp:211`) already live, so the constant
   needs no new visibility. `TextureManager::mLightingTextureCrcs`
   (`TextureManager.h:185`) and `TextureManager::RegisterLightingTextureCrc`
   (`TextureManager.h:188`, `TextureManager.cpp:732-740`) are deleted. The three
   read sites query the upload manager's set instead:
   `ProcessPendingTextures` (`TextureManager.cpp:566`), `AdoptUploadedChunk`
   (`TextureManager.cpp:601`), and `ReblurAllLightingTextures`
   (`TextureManager.cpp:841`). Registration remains startup-only and once per
   type, and a live upload manager always exists at every registration call.

2. `TextureManager::InitializeBootTextures` gains one batched realtime request of
   every registered CRC, beside its existing priority-texture request
   (`TextureManager.cpp:254`): copy the set into a workbuffer arena and issue one
   `gpFileManager->RequestChunkLoad(span, LoadPriority::kRealtime)`
   (`Engine/Source/File/FileManager.h:229`), the same set-to-span pattern as
   `TextureManager::WaitForTextures(std::span<Texture* const>)`
   (`TextureManager.cpp:716-724`). One batched call is preferred over a per-CRC
   loop through `RequestTextureChunkLoad` because that helper requests at the
   default `LoadPriority::kNormal` (`FileManager.h:229`) and because the batched
   call takes the request-queue lock and publishes the loader wake once
   (`Engine/Source/File/PackChunkLoader.cpp:57-103`).

   The replay needs no flag of its own: it lives inside `InitializeBootTextures`,
   which runs exactly once per texture-manager construction. The one "constructed
   this call" latch item 3 introduces is what gates that boot call itself, and
   texture-manager construction is precisely the condition the replay wants. On
   first boot the set is still empty, because
   registration follows `Graphics` construction (`Main.cpp:311` then `:315`), so
   the request is a no-op. The replay is idempotent because
   `PackChunkLoader::RequestChunkLoad` skips any chunk at or past `kDiskLoaded`
   (`PackChunkLoader.cpp:76-79`), which is also why a chunk the reset left at
   `kDiskLoaded` is simply re-adopted rather than re-read from disk.

3. `Graphics::Create` splits its existing `mpTextureManager == nullptr` block
   (today `Graphics.cpp:418-435`) instead of moving whole blocks, keeping every
   manager's own construction guard, and latches the boot call the way
   `bRecordCommandBuffers` already latches command-buffer recording
   (`Graphics.cpp:400-404`):

   a. `if (mpTextureManager == nullptr)` constructs `TextureManager` only (today
      `Graphics.cpp:420`) and sets a local `bool` recording that this call
      constructed it. Its `else if (bSwapchainRecreated)` branch keeps
      `CreateScreenDependentResources` exactly as it is
      (`Graphics.cpp:436-439`).
   b. The pipeline-manager block keeps its own `mpPipelineManager == nullptr`
      guard at its current position (`Graphics.cpp:440-443`). `TextureManager`
      construction therefore still precedes `PipelineManager` construction, which
      that constructor requires because it dereferences `gpTextureManager`
      throughout
      (`Engine/Source/Graphics/Managers/PipelineManager.cpp:84,87,102-103,128,211-212,233-234,294-319,365-366,418,433`).
   c. Immediately after that block, the latch gates the `InitializeBootTextures`
      call, which carries today's try/catch with it (today
      `Graphics.cpp:421-434`).

   Keeping the two guards separate is what preserves the partial tiers:
   `Graphics::Destroy` resets `mpPipelineManager` from the `kPipelines` tier up
   (`Graphics.cpp:723-725`) but resets `mpTextureManager` only at `kSurface` and
   above (`Graphics.cpp:745`), so a pipeline- or swapchain-tier recreate still
   rebuilds the pipeline manager through its own guard while the surviving texture
   manager takes the `CreateScreenDependentResources` branch, the latch stays
   unset, and neither the boot wait nor the replay runs. Nesting the pipeline
   manager inside the texture-manager condition would leave those tiers with no
   pipeline manager at all.

   With the pipeline manager constructed before the boot call, every adoption that
   can reach
   `TextureManager::BlurLightingTexture` — including those the `WaitForTextures`
   spin drives through `ProcessPendingTextures` (`TextureManager.cpp:248`, spin
   body `TextureManager.cpp:656-713`) — already has the two blur pipelines the blur
   reaches through `gpPipelineManager` (`TextureManager.cpp:791-792`), created in
   the pipeline-manager constructor (`PipelineManager.cpp:90`).

   That order is safe, verified against the tree. The pipeline-manager
   constructor needs the eager shader chunks (`PipelineManager.cpp:25-31`), which
   `FileManager` owns; the BRDF LUT, which it generates itself
   (`PipelineManager.cpp:84`); cleared texture bindings
   (`PipelineManager.cpp:87`); and the quad vertex buffer and render targets from
   the buffer, texture and swapchain managers (`PipelineManager.cpp:101-103,157-159`)
   — all of which are already constructed where the pipeline-manager block sits:
   the command-buffer block (`Graphics.cpp:400-405`), the buffer-manager block
   (`Graphics.cpp:406-413`), the islands block (`Graphics.cpp:414-417`), the
   swapchain block above them, and the texture manager step (a) constructs
   (`Graphics.cpp:420`). None of them is produced by
   `InitializeBootTextures`. Nothing in that constructor reads the IBL
   mip count: `miPbrCubeMipCount` is written in `InitializeBootTextures`
   (`TextureManager.cpp:249`) and read only in the per-frame lighting uniform
   write (`Engine/Source/Graphics/Render/LightingUniforms.cpp:375`), which no
   frame reaches before `Create` returns. Pipelines register their texture
   bindings at creation (`Engine/Source/Graphics/Objects/PipelineDescriptorWriter.cpp:78,299,304,328,333`),
   start on the white placeholder the descriptor array is pre-filled with
   (`TextureManager.cpp:211`), and ordinary adoption rewrites those bindings
   (`TextureDescriptors::UpdateDescriptorsForTexture` at
   `TextureDescriptors.cpp:541-568`), so creating pipelines before any texture is
   ready is the normal lazy path, not a new state. `Pipeline::Create`'s own
   non-indirect texture requests (`Pipeline.cpp:132`) then land before the boot
   wait rather than after it, and the boot wait services them; the priority
   textures requested at `TextureManager.cpp:254` are still awaited after `Create`
   returns (`Main.cpp:331`), unchanged.

   Because of that order, no null-`gpPipelineManager` guard is added to
   `BlurLightingTexture`, and `Graphics::Create` makes no
   `ReblurAllLightingTextures` call: every lighting chunk that becomes ready is
   blurred by its own adoption, and `ReblurAllLightingTextures`
   (`TextureManager.cpp:839-849`) keeps exactly its current single caller in
   `Graphics::Refresh` (`Graphics.cpp:560`).

4. Documentation. In `Engine/Source/Graphics/Managers/AGENTS.md` under
   `## Manager Contracts`: the `### TextureManager` lazy-texture load-request
   enumeration (`AGENTS.md:62`, today naming non-indirect pipelines at creation,
   indirect pipelines at their first positive instance write, and priority
   textures at boot) gains the recreation replay issued from
   `InitializeBootTextures`, without adding or restating a count; and
   `### TextureUploadManager` gains one new bullet beside the pending-adoption
   bullet (`AGENTS.md:73-80`) recording that the registered lighting-CRC set
   belongs to this manager because it outlives every Graphics recreation. No
   `### TextureManager` bullet documents that set's ownership today, so none moves:
   the bindless slot-reservation bullet (`AGENTS.md:66`) and the `kBlurSalt` bullet
   (`AGENTS.md:67`) both state behavior that stays with `TextureManager` — the
   `kiLightingBlurSlots` reservation this change keeps (`TextureManager.cpp:18,211`)
   and the salted lookup — and are unchanged. The `Graphics::Create` construction-order
   dependency belongs to `Engine/Source/Graphics/AGENTS.md`
   `## Frame and Resource Lifecycle`, which owns renderer-wide recreation
   ordering by the routing line at
   `Engine/Source/Graphics/Managers/AGENTS.md:19` and today documents only the
   destroy side (`Engine/Source/Graphics/AGENTS.md:12`): add the rule that the
   pipeline manager is constructed before boot textures, so no adoption can run
   without the blur pipelines. No collection-leaf `AGENTS.md` changes, because
   registration itself stays startup-only and once-per-type.

## Critical files

- `Engine/Source/Graphics/Managers/TextureUploadManager.h:30-38` — the
  pending-adoption counter whose ownership rationale the new set shares, and
  where that set is added.
- `Engine/Source/Graphics/Managers/TextureManager.h:185,188` — the set member and
  the member registration function to delete.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:18,211,239-255,566,601,716-724,727-740,839-849` —
  the blur-slot constant and its descriptor reservation, `InitializeBootTextures`
  and the new batched replay, the two adoption read sites, the set-to-span
  precedent, the registration functions, and `ReblurAllLightingTextures`.
- `Engine/Source/Graphics/Graphics.cpp:400-404,418-443,560,674-694,723-725,745,773,775` —
  the `bRecordCommandBuffers` latch precedent, the `mpTextureManager == nullptr`
  block the split reduces and the pipeline-manager block the boot call moves after,
  the sole reblur caller, and the drains, per-tier pipeline-manager and
  texture-manager resets, transfer-resource teardown, and chunk-state reset in
  `Destroy`.
- `Engine/Source/Graphics/Managers/PipelineManager.cpp:14-31,84-90,101-103,128,157-159,211-212,233-234,294-319,365-366,418,433` —
  the pipeline-manager constructor's prerequisites, its `gpTextureManager`
  dereferences, and its blur-pipeline creation.
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp:541-568,712-721` — the
  adoption-time binding rewrite and the blurred-index lookup with its unblurred
  fallback.
- `Engine/Source/Frame/Collections/Collection.h:111-155` — the registration site
  that pairs the load request with the pre-blur registration.
- `Engine/Source/File/PackChunks.cpp:616-663` and
  `Engine/Source/File/PackChunkLoader.cpp:57-103` — the full-recovery chunk-state
  reset and the idempotent request path.
- `Engine/Source/Graphics/Managers/AGENTS.md:62,66-67,73-80` and
  `Engine/Source/Graphics/AGENTS.md:5-14` — the lazy-texture request enumeration,
  the bindless-reservation and `kBlurSalt` contracts that stay with
  `TextureManager`, the `### TextureUploadManager` subsection the new
  ownership bullet joins, and the recreation-ordering rules.

## In scope

- `Engine/Source/Graphics/Managers/TextureUploadManager.h`: adding the registered
  lighting-CRC set as a public member beside the pending-adoption counter, with
  its outlives-recreation comment.
- `Engine/Source/Graphics/Managers/TextureManager.h`: deleting the
  `mLightingTextureCrcs` member and the `RegisterLightingTextureCrc` member
  declaration.
- `Engine/Source/Graphics/Managers/TextureManager.cpp`: pointing the free
  `RegisterLightingTextureCrc` at the upload manager's set and keeping the
  `kiLightingBlurSlots` capacity assertion with it; deleting
  `TextureManager::RegisterLightingTextureCrc`; repointing the set reads in
  `ProcessPendingTextures`, `AdoptUploadedChunk` and
  `ReblurAllLightingTextures`; and adding to `InitializeBootTextures` one
  workbuffer-backed batched realtime `FileManager::RequestChunkLoad` of every
  registered CRC beside the existing priority-texture request.
- `Engine/Source/Graphics/Graphics.cpp`: in `Graphics::Create`, reducing the
  `mpTextureManager == nullptr` block to the `TextureManager` construction plus a
  local `bool` latch recording that this call constructed it, keeping its
  `else if (bSwapchainRecreated)` `CreateScreenDependentResources` branch and the
  `mpPipelineManager == nullptr` block unchanged in place, and calling
  `InitializeBootTextures` with its existing try/catch immediately after that
  pipeline-manager block when the latch is set.
- `Engine/Source/Graphics/Managers/AGENTS.md`, under `## Manager Contracts`: the
  recreation-replay lazy-texture load-request source in the `### TextureManager`
  enumeration, and one new `### TextureUploadManager` bullet recording the
  registered lighting-CRC set's ownership and its outlives-recreation rationale.
  The existing `### TextureManager` bindless-reservation and `kBlurSalt` bullets
  are unchanged.
- `Engine/Source/Graphics/AGENTS.md`, `## Frame and Resource Lifecycle`: the rule
  that `Graphics::Create` constructs the pipeline manager before boot textures so
  no texture adoption can run without the blur pipelines.

## Out of scope

- Static or process-lifetime storage in `TextureManager` for the lighting-CRC
  set, and any null-`gpPipelineManager` guard in
  `TextureManager::BlurLightingTexture`: the construction order removes the need
  for both.
- Any new `ReblurAllLightingTextures` call site; that function keeps its single
  `Graphics::Refresh` caller.
- Re-requesting demoted CRCs inside `PackChunks::ResetTextureChunkStates`. That
  reset is the last step of a full `Graphics::Destroy`, which has already drained
  the loader threads through `WaitForLoadersIdle` precisely so none can publish
  `kUploading` after it, then waited the upload thread and the device
  (`Graphics.cpp:674-694`), and destroyed the transfer resources. Issuing requests
  there would re-arm exactly the loaders that drain closed, against a device being
  torn down; replaying after the reset, from the fresh texture manager, is what
  keeps the drain meaningful.
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
- The upload thread, the staging budget, the upload queue, and the
  pending-adoption counter.
- Wire, save, replay, `.pack`, `kiVersion`, deterministic CRC, and
  backward-compatibility changes.
- Island texture residency and eviction policy.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the change moves a registry's ownership
from one manager to another, reorders manager construction inside
`Graphics::Create`, and adds a load replay on the device-recovery resource path —
a resource-lifetime and construction-ordering surface whose correctness spans
three independently owned subsystems' invariants: Frame collection type
registration, File lazy-chunk state, and TextureManager bindless pre-blur
publication.

Preserve these invariants:

- Registered type indices remain immutable and valid across Graphics recreation;
  the type registries are not read, written, or re-registered by this change.
- A lighting deposit samples the `kBlurSalt`-salted blurred texture while the
  visible sprite samples the unblurred source, and deposit/sprite cursors stay
  aligned.
- All touched state is client-only and stays outside the simulation/PostRender
  CRC, save, replay, wire, and `.pack` data.
- Pipeline-only and swapchain-only recreation keep their current behavior: the
  pipeline manager is still rebuilt through its own `mpPipelineManager == nullptr`
  guard, the surviving texture manager keeps its ready textures and takes the
  `CreateScreenDependentResources` branch, the construction latch stays unset, and
  so `InitializeBootTextures`, the replay request, and any reblur all do not run.
- No lighting blur runs before the blur pipelines exist, and no lighting-CRC
  state is owned by an object a full `Graphics::Destroy` frees.
- The registered lighting-texture count still fits the `kiLightingBlurSlots`
  bindless reservation, and the capacity assertion still fires at registration.
- `TextureManager` construction stays ahead of `PipelineManager` construction,
  because the pipeline-manager constructor dereferences `gpTextureManager`; each
  manager keeps its own construction guard, so neither tier's recreate can skip
  building one of them.
- `InitializeBootTextures` still runs exactly once per texture-manager
  construction, gated by the latch rather than by the pipeline-manager guard.
- Every pipeline-manager constructor prerequisite is still satisfied where that
  block sits, and no pipeline reads a texture or a derived value that only
  `InitializeBootTextures` produces.

## Acceptance criteria

- Decisive evidence for the full-recreation legs is code inspection, not a live
  run: neither leg is reachable from settings or agent commands (no
  `Graphics::Refresh` poll escalates above `kSwapchain`, the agent `resize` and
  `fullscreen` commands drive only the swapchain tier, and no debug device-loss
  injection exists), so this Plan does not require a live full-recreation
  reproduction. The inspection must cover both the in-place `kSurface` leg
  (`GraphicsUtils.cpp:52`) and the whole-object device-loss leg
  (`Main.cpp:462-470`), and must show that every registered CRC is re-requested
  from `InitializeBootTextures` after the `Graphics::Destroy` drains
  (`Graphics.cpp:674-694`) on the in-place leg and after fresh device creation on
  the replacement leg; that the pipeline manager exists before any adoption can
  reach the blur, so no blur call can precede the blur pipelines; that after
  recovery each registered CRC resolves through
  `TextureDescriptors::CrcToBlurredIndex` to a blurred index distinct from its
  unblurred one rather than to the unblurred fallback; and that on first boot the
  set is empty so the replay is a no-op.
- The inspection also shows the reordering is safe: `Graphics::Create` reads as the
  `mpTextureManager == nullptr` block constructing only `TextureManager` and setting
  the latch, its `else if (bSwapchainRecreated)`
  `CreateScreenDependentResources` branch, then the `mpPipelineManager == nullptr`
  block under its own guard, then the latched `InitializeBootTextures` call with its
  try/catch — so a pipeline- or swapchain-tier recreate rebuilds the pipeline
  manager with the latch unset and runs no boot wait and no replay; the
  IBL boot wait now runs after pipeline creation, those pipelines start on the
  white placeholder
  descriptors registered at their creation, and ordinary adoption rewrites those
  bindings, with no pipeline-constructor prerequisite produced by
  `InitializeBootTextures`.
- The surviving set is shown to be the complete list of lost requests: a
  repository search confirms `TypeRegistry::RegisterType` is still the only
  caller of `RequestTextureChunkLoad` and still pairs it with
  `RegisterLightingTextureCrc` for the same CRC, covering all six CRC-bearing
  registries including the `Explosions` particle CRC.
- Partial-recreation regression check through `/agent-harness`, explicitly a
  regression guard and not evidence the defect is fixed: launch the client, make
  lighting textures live, drive `resize` and `fullscreen`; those tiers keep the
  texture manager and leave the latch unset, so they rebuild the pipeline manager
  while `InitializeBootTextures` and the replay do not run,
  rendering is unchanged, and the log shows no texture-adoption,
  descriptor-generation, or validation error and no assertion.
- Client `Debug|x64` and `Release|x64` build clean through `/compile`, with no
  new warnings.
- The changed-path diff contains exactly the `## In scope` paths — the two
  manager headers and implementation files it names, `Graphics.cpp`, and the two
  `AGENTS.md` files — and no path under `Engine/Source/Frame/Collections/` or
  `Projects/**/Source/Frame/Collections/`.
- `Engine/Source/Graphics/Managers/AGENTS.md` `### TextureManager` names the
  recreation replay in its lazy-texture load-request enumeration alongside the
  sources it already lists and keeps its bindless-reservation and `kBlurSalt`
  bullets unchanged, `### TextureUploadManager` carries a new bullet recording the
  registered lighting-CRC set's ownership and why it lives there, and
  `Engine/Source/Graphics/AGENTS.md`
  `## Frame and Resource Lifecycle` records the pipeline-manager-before-boot-
  textures construction rule; no collection `AGENTS.md` changed.

## Notes

This Plan replaces the two former per-collection recovery Plans of this area,
`AreaLightsGraphicsRecovery` and `ExplosionParticleGraphicsRecovery`, which were
removed from the tree when it was created: it achieves the outcome both asked for, at the texture layer
instead of in each collection, and covers `Billboards`, `Puffs` and `SmokeTrails`
as well, so no per-collection follow-up remains.

`Documents/Plans/Engine/LightingReblurFenceOrdering.md` is independent and is not
a prerequisite. It owns the live blur-setting change path — `Graphics::Refresh`
and the post-drain window in `Graphics::Destroy` — and explicitly excludes
texture-registration replay after device loss from its scope, expecting the
replacement resource path to handle its own textures, which is exactly what this
Plan adds. This Plan adds no reblur call site and leaves that function's single
`Graphics::Refresh` caller (`Graphics.cpp:560`) exactly where that Plan expects to
find it, so either Plan can land first without changing the other.
