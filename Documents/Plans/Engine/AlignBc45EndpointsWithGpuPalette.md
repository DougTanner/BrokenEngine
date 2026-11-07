<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-07T15:16:13.700Z","dependsOn":[]} -->
# Align BC4/BC5 endpoints with the GPU's exact palette

## Context

A BC4/BC5 block decodes on the GPU with an exactly interpolated palette, but the
DataPacker's BC4/BC5 encoder may be choosing endpoints for a different palette.
`Texture::EncodeBlocks` calls `rgbcx::encode_bc4_hq` and `rgbcx::encode_bc5_hq`
(`DataPacker/Source/ExportJobs/Texture/Texture.cpp:404` and `:407`) with search
radius `kiBc45SearchRadius` and `rgbcx::BC4_USE_ALL_MODES`. rgbcx builds its
palette with truncating integer division (`bc4_block::get_block_values8` and
`get_block_values6`, `ThirdParty/bc7enc_rdo/rgbcx.h:391` to `:414`), and its
`encode_bc4_hq` scores trial endpoints through that palette
(`ThirdParty/bc7enc_rdo/rgbcx.cpp:2821`). `DecodeBcnToRgba8` (`Texture.cpp:665`)
decodes with the same truncating palette through `rgbcx::unpack_bc4` and
`unpack_bc5`.

The symptom shows in one image exported twice, once with green inverted. The
source was `Projects/BrokenEngineSandbox/Data/Models/MolniaFbx/textures/NormalDirectX.png`,
copied twice as uncommitted scratch files whose names carry the `[BC5]` tag
(`DataPacker/Source/ExportJobs/ExportTexture.cpp:61`), the second with green
replaced by 255 - green, and both exported through `ExportTexture`. Both BC5 mip
chains were decoded twice, once with the exact palette the Vulkan specification
defines for BC4/BC5 UNORM (what the GPU samples) and once with the rgbcx palette.
An unbiased encode and decode gives a per-mip mean of g_a + g_b - 255 of zero,
and a one-way bias b per texel gives 2b. Measured after texel rounding replaced
truncation in `EncodeBlocks`, the exact-palette means were:

| Mip | Mean of g_a + g_b - 255, exact palette |
|---|---|
| 0 | +0.107 |
| 1 | +0.156 |
| 2 | +0.198 |
| 3 | +0.263 |
| 4 | +0.350 |
| 5 | +0.395 |
| 6 | +0.335 |
| 7 | +0.191 |
| 8 | +0.092 |
| 9 | +0.277 |
| 10 | 0.000 |

With the rgbcx palette, every mip's mean was within -0.007 to +0.046. So the GPU samples BC4/BC5 data up to about +0.2 of a byte step per channel
higher than the encoder's own decode, which is close to unbiased. The offset
predates the rounding change: before it, the exact-palette mean minus the
rgbcx-palette mean was about the same (mip 1 +0.17, mip 2 +0.24, mip 3 +0.31,
mip 4 +0.36, mip 5 +0.36, mip 6 +0.31, mip 7 +0.19, mip 8 +0.09). It was seen
verifying that change and is outside its boundary.

The unconfirmed hypothesis is that rgbcx optimizes endpoints and selectors
against its truncating palette, so the exactly interpolated values the GPU
reads sit above the values the encoder targeted. The root cause is not
confirmed.

Render reach is client-only: BC4 holds island ambient occlusion and BC5 holds
island and scene normal-map X and Y.

## Design

First confirm the root cause with `/external-diagnose-bug`, starting from the
hypothesis above and the measurement in `## Context`. Then make the smallest fix
inside the BC4/BC5 encode path of `EncodeBlocks` and its encoder parameters. A
fix that needs a change elsewhere, including any edit under `ThirdParty/`,
which is not modified, is surfaced for re-planning instead of widening scope.

The diagnosis decides the fix, so no mechanism is chosen here. The author's
suggestion is to weigh, once the cause is confirmed, an encoder parameter
setting that removes the offset, against a local step in `Texture.cpp` that
re-scores the encoder's candidate endpoints and selectors with the exact
palette. The rationale is that both stay inside the region and leave rgbcx
untouched.

A fix changes BC4/BC5 bytes, so it bumps each version that owns a changed
output, as `DataPacker/Source/ExportJobs/AGENTS.md` `## Shared Pipeline`
requires:

- `ExportTexture::kiVersion` (`ExportTexture.h`), whose per-job chunk cache
  holds `[BC4]`/`[BC5]`-tagged encodes;
- the raw value of `ExportScene::kiVersion` (`ExportScene.h`), which
  re-exports every scene and regenerates its `.PreExport` marker;
- `ExportIsland::kiTextureVersion` (`ExportIsland.h`), which re-encodes all
  four island textures, AmbientOcclusion (BC4) and Normals (BC5) among them.

The cost is the same kind as the texel-rounding change. There are 158 tracked
BC4/BC5 intermediates: 140 under `Engine/Data/Islands` and 18 BC5 maps under
`Projects/BrokenEngineSandbox/Data/Models`. Rewriting them adds blobs of about
their size to Git history, and every checkout rebuilds its texture packs once.
The island bump is the expensive one. The author recommends it anyway, because
otherwise tracked island textures disagree with the exporter until each is next
edited; leaving islands on the old encode is the alternative to weigh at claim
time.

## Critical files

- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` — the BC4/BC5 branch of
  `EncodeBlocks`, `kiBc45SearchRadius`.
- `DataPacker/Source/ExportJobs/ExportTexture.h`, `ExportScene.h`,
  `ExportIsland.h` — the version constants.
- Every tracked `*.BC4_UNORM_BLOCK` and `*.BC5_UNORM_BLOCK` intermediate, plus
  the other textures and `.PreExport` markers the bumps regenerate.

## In scope

- The root-cause investigation of the exact-palette offset.
- The fix, bounded to the BC4/BC5 encode path in `Texture::EncodeBlocks` and its
  encoder parameters (`kiBc45SearchRadius` and the arguments of the
  `rgbcx::encode_bc4_hq` and `rgbcx::encode_bc5_hq` calls).
- The three version bumps and the tracked outputs they regenerate.

## Out of scope

No change under `ThirdParty/`, to the BC7 path, to the float-to-byte texel
conversion, to `MakeMipmaps`, to `DecodeBcnToRgba8` or its callers
(`ExportIsland.cpp:316`, `MigrateLegacyIntermediates.cpp:151`), to engine
shaders or sampling, or to the intermediate or chunk formats. No new tracked
fixture, harness command, or unit test.

## Acceptance criteria

- `/external-diagnose-bug` records the confirmed root cause before any fix.
- Repeating the `## Context` measurement on the same source, with uncommitted
  scratch copies that are deleted afterward, gives an exact-palette mean of
  g_a + g_b - 255 within 0.05 of zero on every mip. The author recommends that
  limit because it is the range the rgbcx-palette means already reach
  (-0.007 to +0.046), against the recorded +0.092 to +0.395 on mips 0 to 9.
- On the same chains, the exact-palette mean absolute error against the source
  mip does not increase on any mip; the supplied evidence records no baseline
  for it, so the session records the before and after values.
- `/compile` of DataPacker, ModelViewer and the BrokenEngineSandbox client and
  server succeeds with every tracked intermediate regenerated, and a Local
  generation build restores the sparse data trees it needs.
- A ModelViewer capture of `Models\MolniaGltf\scene.gltf` returns code `ok`, and
  an `/agent-harness` client and server launch reaches a running game session,
  with neither log holding `DEBUG_BREAK at` or a CRC mismatch.

Live setup: `/compile` `-Target ModelViewer -Configuration Debug`, then the
`/agent-harness` capture with that `-ModelPath`. The supplied evidence records no
ordered setup for the game session; establish and record it during execution.

## Notes

Future implementation is Tier 3: trigger is build/bootstrap coordination that can
block other sessions. Landing rewrites every tracked BC4/BC5 intermediate and
the scene and island outputs the bumps regenerate, so any session holding an
edit to one meets a binary conflict. `git sparse-checkout disable` is needed
before editing `Engine/Data/Islands`, and a Local generation build through
`/compile` restores it.

The change itself is one subsystem's tool behavior. Texel bytes change and formats
do not, so there is no serialization layout change. Texture payloads reach only
client GPU sampling and the client map thumbnail, outside PostRender state, so
determinism/CRC, replay, wire, threading and allocation are unaffected. The island
chunk fingerprint covers Color and AO, so island chunks re-pack too.

No prerequisite Plan or mandatory coordination constraint was identified. Any Plan
that bumps a scene version or edits a tracked texture first simply re-exports
once more.
