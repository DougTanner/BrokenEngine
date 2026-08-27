<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:49.813Z","dependsOn":[]} -->
# Fix: Replay AreaLights texture registration after full Graphics recreation

## Context

The accepted survivor `CAI/shard-0015/001` shows that a full client Graphics
recreation constructs a fresh `TextureManager` whose lighting-CRC set is empty.
`TypeRegistry::RegisterType` requests an AreaLights texture and records its
pre-blur CRC only during startup (`Engine/Source/Frame/Collections/Collection.h:124-160`),
while `GameBase::GameBase` registers the frame types only once
(`Engine/Source/GameBase.cpp:20-23`).  The full-surface path destroys the old
manager and resets ready lazy chunks to `kNotLoaded`
(`Engine/Source/Graphics/Graphics.cpp:765-778`; `Engine/Source/File/PackChunks.cpp:754-812`).
`AreaLightsInterpolate::GraphicsResources` currently creates buffers and
pipelines only (`Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp:13-19`),
so no request or pre-blur registration reaches the replacement manager.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0015.md:49`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:705`.
The report proved all four frozen target rows unchanged and found no separate
AreaLights lifecycle defect.  The finding predates this routing session: the
frozen audit baseline is `76d303f0eeeb86c1ed241edc81634e60070ba5a5`, and the
working tree contains only the six unrelated Plan Context edits.

Impact: after a supported device/surface-loss recovery, live AreaLights stay
on the white placeholder; if a later path loads the source, the missing
lighting-CRC registration also makes the deposit use the unblurred fallback.

## Design

Author's recommendation: make `AreaLightsInterpolate::GraphicsResources` replay
the already-registered AreaLights texture CRCs into the current
`TextureManager` by calling the existing `RequestTextureChunkLoad` and
`RegisterLightingTextureCrc` helpers for each nonzero entry in `sTypes` before
creating the AreaLights pipelines.  This replay is deliberately separate from
`RegisterType`: type indices and the immutable registry remain startup-only,
while the fresh manager receives the request and blur metadata it lost.  The
existing adoption path must then blur a ready source before
`CrcToBlurredIndex` is consumed.  Do not add a second AreaLights type registry
or change partial-recreation behavior.

## Critical files

- `Engine/Source/Frame/Collections/AreaLights/AreaLightsRender.cpp:13-19` —
  fresh-manager resource hook.
- `Engine/Source/Frame/Collections/AreaLights/AreaLights.h:13-32` — the
  registered CRC-bearing type and immutable registry.
- `Engine/Source/Frame/Collections/Collection.h:118-160` — existing request
  and pre-blur registration helpers (reference only unless the replay needs a
  narrowly shared helper).
- `Engine/Source/Graphics/Managers/TextureManager.cpp:577-635,814-860` —
  adoption and blurred-index behavior that the replay must feed.

## In scope

- Replaying every nonzero AreaLights type texture CRC when
  `AreaLightsInterpolate::GraphicsResources` runs for a fresh Graphics/
  TextureManager instance.
- Ensuring the replay occurs before the replacement manager can adopt the
  chunk and that visible AreaLights continue to use the source descriptor while
  deposits use the registered blurred descriptor.
- Keeping static type indices, client-only collection layout, partial
  recreation, and the existing request/adoption API unchanged.

## Out of scope

- PointLights or explosion-particle texture recovery, which are separate
  resource channels even though the audit recorded a probable shared family.
- Graphics destructor/device-loss teardown, TextureManager allocation policy,
  AreaLights collection lifecycle, or shader code.
- Any source fix to this session's six Context files, and backward-compatibility
  handling for an old texture registration format (there is no format change).

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the fix crosses the frame collection,
TextureManager, pack-backed lazy loading, bindless descriptors, and Graphics
device-recovery lifetimes; independently owned client subsystems and resource
publication are Tier-3 surfaces.

Preserve these invariants:

- Registered AreaLights type indices remain immutable and valid across Graphics
  recreation.
- Every registered AreaLights source CRC is requested again by a fresh manager
  and is added to the lighting pre-blur set before adoption.
- A ready source texture is never published to an AreaLights deposit as an
  unblurred texture merely because the manager was recreated.
- Client-only texture state stays outside server/PostRender CRCs; no wire,
  replay, or `.pack` bytes change.

## Acceptance criteria

- A client Graphics/device-loss or full-surface recreation followed by a
  successful replacement causes every registered AreaLights source to leave the
  placeholder and causes its deposit descriptor to resolve to the blurred
  texture.
- A pipeline/swapchain-only recreation keeps the existing AreaLights texture
  registration behavior and does not duplicate type indices or requests that
  alter visible output.
- Client Debug and Release builds pass `/compile`, and a harness recovery
  scenario records no texture-adoption or descriptor-generation error.
- A scoped search shows the replay is tied to the existing AreaLights registry;
  no new wire, `.pack`, save, replay, or CRC-surface field was introduced.

## Notes

The audit's cross-shard `DUP-005` hint also names explosion and PointLights
channels, but their affected resources and acceptance boundaries differ, so
they are not grouped into this Plan.
