<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:36:01.364Z","dependsOn":[]} -->
# Replay explosion particle texture registration after Graphics recreation

## Context

The frozen plan-trace survivor `CPT/shard-0018/005` is decision-complete and
pre-existing at `80896f33661aaab99cf180a96db54600099be652`. Each
`ExplosionType` carries a nonzero particle texture CRC
(`Engine/Source/Frame/Collections/Explosions/Explosions.h:100-103`), and the
explosion type registrations retain it while
`TypeRegistry::RegisterType` requests it only during startup
(`Engine/Source/Frame/Collections/Collection.h:152-160`). A full Graphics
teardown destroys TextureManager and resets ready lazy texture chunks
(`Engine/Source/Graphics/Graphics.cpp:437-458,751-782`;
`Engine/Source/File/PackChunks.cpp:851-909`), after which the replacement
TextureManager points deferred textures at white placeholders
(`Engine/Source/Graphics/Managers/TextureManager.cpp:92-97,165-213`).

`ExplosionsPostRender::Spawn` reaches `ParticleManager::Spawn` with the
particle CRC (`Engine/Source/Frame/Collections/Explosions/ExplosionsSpawn.cpp:188-236`).
`ParticleManager::GetOrAssignTextureIndex` only assigns a bindless descriptor
through `TextureDescriptors::CrcToIndex`
(`Engine/Source/Graphics/Managers/ParticleManager.cpp:25-55`); it does not
request the lazy chunk. The boot priority list does not include the authored
explosion particle CRC (`Engine/Source/Graphics/Managers/TextureManager.cpp:242-258`).

The controlling contracts are `Engine/Source/Frame/Collections/Explosions/AGENTS.md:1-6,15-17`,
`Engine/Source/Frame/Collections/AGENTS.md:17`, and
`Engine/Source/Graphics/Managers/TextureManager.AGENTS.md`: the visible child
effect must use its authored texture, and lazy textures enter adoption after
a request. The boundary is fresh-manager client resource recreation before
particle staging; PointLights/lighting-preblur recovery is outside this Plan.

Impact: after device/surface recovery, a valid explosion can stage particles
and assign a bindless slot while sampling the white placeholder indefinitely,
breaking the authored explosion child-effect contract.

## Design

Author's recommendation: replay every nonzero registered explosion
`particleCrc` into the fresh TextureManager through the existing texture-load
request path before the first `ParticleManager::Spawn` can publish a live
particle descriptor. Keep the immutable explosion type registry and bindless
indices unchanged, preserve unconditional random draws and the existing
client-only guard, and leave partial pipeline/swapchain recreation alone.

## Critical files

- `Engine/Source/Frame/Collections/Explosions/Explosions.h:90-103` and `ExplosionsSpawn.cpp:188-236` — particle CRC ownership and spawn consumer.
- `Engine/Source/Frame/Collections/Collection.h:152-160` — startup particle request.
- `Engine/Source/Graphics/Managers/ParticleManager.cpp:25-55` and `TextureDescriptors.cpp:693-713` — descriptor assignment without lazy-load admission.
- `Engine/Source/Graphics/Graphics.cpp:437-458,751-782` and `Engine/Source/File/PackChunks.cpp:851-909` — full recreation/reset.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:92-97,165-213,242-258` — fresh placeholders and priority requests.
- `Engine/Source/Frame/Collections/Explosions/AGENTS.md` and `Engine/Source/Graphics/Managers/TextureManager.AGENTS.md` — effect and lazy-resource contracts.

## In scope

- Replaying registered explosion particle texture load requests at the fresh TextureManager resource boundary.
- Ensuring the authored particle texture is admitted before particle staging after full Graphics/device recreation.
- Preserving random-stream lockstep, immutable type/descriptor identity, client-only state, and partial recreation behavior.

## Out of scope

- PointLights or AreaLights source/pre-blur registration, particle simulation/tuning, staging capacity, shader code, and descriptor-index redesign.
- Graphics teardown behavior, boot priority-list policy for unrelated textures, and new wire/save/replay/`.pack` formats.
- Suppressing random draws or changing the existing `kRecalculated` replay marker policy.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: the fix crosses explosion type
registration, Graphics/TextureManager recreation, lazy pack-texture adoption,
and bindless particle publication on the client.

Preserve these invariants:

- Every nonzero registered explosion `particleCrc` is requested again by a
  fresh manager before a particle row can sample it.
- Particle staging keeps its existing bindless index and authored texture;
  the white placeholder is not the post-recovery steady state.
- Random draws happen on both builds in the existing order, and particle
  visuals remain outside deterministic CRC, save, replay, and wire state.

## Acceptance criteria

- After full Graphics/device recreation, the next valid explosion particle
  spawn adopts the authored particle texture rather than the white placeholder
  and has no descriptor-generation error.
- All registered explosion particle CRCs are covered by one idempotent replay;
  normal startup and partial pipeline/swapchain recreation retain their current
  behavior.
- Client Debug and Release builds pass through `/compile`, and a focused
  recovery scenario observes authored particle sampling with unchanged random
  and shared-CRC state.

## Notes

The PointLights survivor is routed separately because its blurred lighting
deposit has a different resource consumer and acceptance boundary.
