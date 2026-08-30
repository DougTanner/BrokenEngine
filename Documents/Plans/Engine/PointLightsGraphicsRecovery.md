<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:35:52.259Z","dependsOn":[]} -->
# Replay PointLights texture registration after full Graphics recreation

## Context

The frozen plan-trace survivors `CPT/shard-0018/001` and
`CPT/shard-0020/001` share one root cause and one recovery boundary at frozen
SHA `80896f33661aaab99cf180a96db54600099be652`. Startup registrations call
`TypeRegistry::RegisterType`, whose client path requests each nonzero type CRC
and records it for lighting pre-blur (`Engine/Source/Frame/Collections/Collection.h:124-160`).
The explosion registration is one concrete PointLights producer at
`Engine/Source/Frame/Collections/Explosions/Explosions.cpp:93-102`; the other
PointLights types use the same registry contract. `GameBase::GameBase` invokes
registration once at `Engine/Source/GameBase.cpp:20-23`.

`PointLightsInterpolate::GraphicsResources` only recreates buffers and
pipelines (`Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp:12-18`),
while PointLights rendering consumes both the source and blurred descriptors
at `PointLightsRender.cpp:66-95`. Full Graphics recreation constructs a fresh
TextureManager and PipelineManager and resets ready lazy chunks
(`Engine/Source/Graphics/Graphics.cpp:437-462,751-782`;
`Engine/Source/File/PackChunks.cpp:851-909`;
`Engine/Source/Graphics/Managers/PipelineManager.cpp:168-171`). The fresh
manager starts its lazy textures on white placeholders
(`Engine/Source/Graphics/Managers/TextureManager.cpp:92-97,165-213`), and
`TextureDescriptors::CrcToBlurredIndex` falls back to the unblurred source
when no salted registration exists (`Engine/Source/Graphics/Managers/TextureDescriptors.cpp:716-724`).

The controlling contracts are `Engine/Source/Frame/Collections/AGENTS.md:17`,
`Engine/Source/Frame/Collections/PointLights/AGENTS.md`,
`Engine/Source/Graphics/Managers/TextureManager.AGENTS.md`, and
`Engine/Source/Graphics/Managers/PipelineManager.AGENTS.md`: registered
client type side effects must feed lazy loading and blurred deposits, and
dynamic collection resources must repopulate after recreation. The boundary
is the fresh-manager PointLights graphics-resource hook, including every
registered PointLights CRC; AreaLights and explosion-particle channels remain
separate resource owners.

Impact: after supported device/surface recovery, live PointLights can remain
white and their lighting deposits can use the unblurred fallback, violating
the source/sprite and blurred-deposit contract while the static type registry
and live rows still exist.

## Design

Author's recommendation: when `PointLightsInterpolate::GraphicsResources`
runs for a fresh Graphics/TextureManager instance, replay every nonzero CRC in
the existing immutable PointLights type registry through the existing
`RequestTextureChunkLoad` and `RegisterLightingTextureCrc` helpers before the
manager can adopt and render those textures. The replay must cover the
explosion-registered PointLights type and all other registered PointLights
types, remain idempotent for the pipeline hook, and leave type indices and
partial pipeline/swapchain recreation unchanged. Do not create a second
registry or merge the distinct AreaLights or explosion-particle resource
contracts into this one.

## Critical files

- `Engine/Source/Frame/Collections/PointLights/PointLightsRender.cpp:12-18,66-95` — resource hook and descriptor consumers.
- `Engine/Source/Frame/Collections/Explosions/Explosions.cpp:93-102` — explosion PointLights registration.
- `Engine/Source/Frame/Collections/Collection.h:124-160` — startup request/pre-blur helpers and immutable registry.
- `Engine/Source/GameBase.cpp:20-23` — one-time registration owner.
- `Engine/Source/Graphics/Graphics.cpp:437-462,751-782` and `Engine/Source/Graphics/Managers/PipelineManager.cpp:168-171` — fresh-manager lifecycle.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:92-97,165-213`, `TextureDescriptors.cpp:716-724`, and `Engine/Source/File/PackChunks.cpp:851-909` — placeholder, pre-blur, and lazy-state behavior.
- `Engine/Source/Frame/Collections/PointLights/AGENTS.md` and `Engine/Source/Graphics/Managers/TextureManager.AGENTS.md` — resource invariants.

## In scope

- Replaying all registered PointLights source CRC load requests and lighting-preblur registrations at fresh-manager resource creation.
- Ensuring source textures are adopted before use and deposits resolve to the salted blurred descriptor after full Graphics/device recreation.
- Preserving immutable type indices, visible/deposit cursor alignment, client-only status, and partial recreation behavior.

## Out of scope

- AreaLights recovery, explosion particle texture recovery, PointLights simulation/collection layout, shader changes, or new texture formats.
- Graphics destructor/device-loss teardown, unrelated dynamic pipelines, and changing the startup registry contract.
- Wire, save, replay, `.pack`, deterministic CRC, or backward-compatibility changes.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the fix crosses frame collection
registration, TextureManager lazy pack resources, bindless descriptor/pre-blur
publication, and Graphics device-recovery lifetimes.

Preserve these invariants:

- Every immutable PointLights type index remains stable and its nonzero source
  CRC is requested in a fresh manager before adoption/render.
- A registered PointLights deposit uses its salted blurred texture while the
  visible sprite uses the unblurred source texture.
- Full Graphics recreation recovers all PointLights texture side effects;
  pipeline-only and swapchain-only paths do not duplicate indices or alter
  existing behavior.
- Resource state remains client-only and outside simulation CRCs, save,
  replay, wire, and `.pack` layouts.

## Acceptance criteria

- After full Graphics/device recreation with live PointLights, every registered
  source leaves the placeholder, a salted blurred descriptor is created, and
  the next render consumes source and blurred descriptors as required.
- The explosion-registered PointLights type and every other registered type
  are covered by one scoped registry replay; no type index is duplicated.
- Pipeline-only and swapchain-only recreation preserve current resource
  behavior and do not issue an unintended second registration sequence.
- Client Debug and Release builds pass through `/compile`, and a focused
  recovery scenario reports no texture-adoption or descriptor-generation
  error.

## Notes

This Plan deliberately groups the two frozen survivors because their root
cause, fresh-manager boundary, invariant, and blurred-descriptor verification
signal are identical. The separate `AreaLightsGraphicsRecovery.md` Plan
excludes PointLights, and explosion-particle recovery remains a separate
resource channel.
