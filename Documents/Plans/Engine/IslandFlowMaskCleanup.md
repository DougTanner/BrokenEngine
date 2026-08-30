<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:56:00.201Z","dependsOn":[]} -->
# Remove unused island Flow mask input

## Context

The false required condition is that island mask export must require and
preserve Gaea's `Flow.png`. `EncodeMaterialMasks` currently loads Rock, Sand,
Snow, and Flow into an RGBA BC7 texture
(`DataPacker/Source/ExportJobs/ExportIsland.cpp:225-285`), and both payload and
texture fingerprints include Flow (`ExportIsland.cpp:522-569`). The Gaea route
also requires the file in its raw intermediate list
(`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:24-35`). The live terrain
shader samples only mask `.r`, `.g`, and `.b`
(`Engine/Data/Shaders/Terrain/Terrain.frag:78-84`); its alpha is explicitly
reserved and has no consumer.

The originating candidate is `CPS/shard-0002/002`. The user explicitly directs removal of the required,
read, and fingerprinted Flow input while retaining the existing BC7 RGB mask
texture. The concern is pre-existing at session baseline
`80896f33661aaab99cf180a96db54600099be652`; no active source implementation
is part of this Plan-writing session.

## Design

The author's recommendation is to keep `Masks.BC7_UNORM_BLOCK` and its RGB
meaning unchanged, load only Rock/Sand/Snow, and write constant zero alpha for
every packed texel. Zero matches the existing all-zero placeholder and the
underwater flat-mask convention; no shader reads the alpha. Remove `Flow.png`
from `EncodeMaterialMasks`'s input list, both `ExportIsland` texture-fingerprint
lists, and `BakeRoute`'s required raw-output list and diagnostics. Existing
route/archetype files may still emit an unlisted extra Flow output; the
DataPacker must no longer require, read, fingerprint, or publish it.

Advance `ExportIsland::kiTextureVersion` from `1` to `2` so cached masks are
re-encoded under the new input contract. Keep `ExportIsland::GetVersion`, the
island payload layout, and the shared `IslandHeader` unchanged. Update the
island, terrain-shader, world-lighting binding, and placeholder comments to
describe RGB masks plus constant unused alpha. Do not remove the bindless
mask texture, change the BC7 format, or introduce a future Flow consumer.

## Critical files

- `DataPacker/Source/ExportJobs/ExportIsland.cpp:225-285,460-480,522-569` — mask loading, constant alpha, and fingerprints.
- `DataPacker/Source/ExportJobs/ExportIsland.h:30-40` — texture-stage version.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:24-35,48-56,318-323` — Gaea raw-output contract and version.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md:17-23` — producer/consumer mask authority.
- `Engine/Data/Shaders/Terrain/Terrain.frag:38-41,78-84` and `Engine/Data/Shaders/Terrain/AGENTS.md:11-17` — mask binding and channel contract.
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:318-326` — mask descriptor binding comment.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:156-163` — placeholder channel comment.

## In scope

- Removing `Flow.png` from the Gaea required-output list, mask decode, and both
  texture-stage fingerprints.
- Packing Rock/Sand/Snow into RGB with constant zero alpha in the existing
  BC7 RGBA mask, including the underwater flat value.
- Bumping `kiTextureVersion` and regenerating the affected mask textures and
  packed outputs through the normal Local DataPacker route.
- Updating only comments/authority prose needed to state that alpha is
  constant and unused, while preserving the existing binding and RGB shader
  behavior.

## Out of scope

- Removing or redesigning the existing `Masks.BC7_UNORM_BLOCK` resource,
  bindless descriptor, RGB mask channels, terrain material blending, or
  underwater threshold.
- Rewriting `Engine/Data/Islands/Island.terrain`'s future/extra Flow export
  node; an unlisted Gaea output is not a DataPacker input after this change.
- Adding a Flow shader consumer, a replacement channel, compatibility for old
  texture fingerprints, a wire/save/replay format, or unrelated island fields.
- The separate Island peak-height metadata removal, except for reciprocal
  coordination in the shared ExportIsland translation unit.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: this changes a Gaea intermediate and
texture fingerprint contract, regenerates tracked BC7 assets, and must stay
coherent with a Vulkan shader binding and runtime placeholder.

Preserve these invariants:

- `Masks.BC7_UNORM_BLOCK` remains RGBA with Rock/Sand/Snow in R/G/B and a
  deterministic constant zero A; RGB output and format do not change semantically.
- The terrain shader continues to use only the three live RGB mask channels,
  with the same binding, sampler, mip, and underwater masking behavior.
- Dirty detection invalidates the old texture-stage contract through
  `kiTextureVersion`; no old fingerprint or dual-input compatibility path is
  retained.
- No simulation CRC, wire, save, replay, or island payload layout changes.

## Acceptance criteria

- Exact repository searches show no current `ExportIsland` read or fingerprint
  dependency on `Flow.png`; `BakeRoute` no longer rejects a bake solely for a
  missing Flow output, while Rock/Sand/Snow remain required.
- A Local DataPacker run with the new texture version produces a BC7 mask whose
  decoded alpha is constant zero and whose RGB channels match the prior
  Rock/Sand/Snow inputs, including underwater flat regions.
- Client terrain shader compilation and descriptor reflection continue to
  agree on the existing RGBA mask binding, and terrain material blending is
  unchanged because it still reads only `.r/.g/.b`.
- The placeholder and authority comments describe the constant unused alpha,
  and no new Flow consumer or capability is present.
- Client/server Debug builds and the DataPacker target pass `/compile`; no unit
  tests are added.

## Coordination

`Documents/Plans/Engine/IslandPeakHeightMetadataRemoval.md` also changes
`DataPacker/Source/ExportJobs/ExportIsland.cpp`. Neither Plan depends on the
other. Implement and review both ExportIsland regions as one coordinated pass
(or rebase before editing), preserving this Plan's texture-version/fingerprint
changes alongside the peak Plan's payload/header version changes.

## Notes

The ignored CPS triage record is session provenance only. Durable source,
shader, route, and authority citations above define the implementation after
Temp evidence is removed.
