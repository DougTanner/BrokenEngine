<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:25.838Z","dependsOn":[]} -->
# Generate complete mip tails for one-dimensional textures

## Context

The frozen audit retained `CAI/shard-0008/003`. `Texture::MakeMipmaps` loops
only while both dimensions exceed one (`DataPacker/Source/ExportJobs/Texture/Texture.cpp:267-299`).
Regular uncompressed images use this helper from `ExportTexture.cpp:263-286`,
so a valid `1x8` image publishes one mip instead of the complete `1x8,1x4,1x2,1x1`
chain. Source bytes match baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

For uncompressed regular textures, continue while either source dimension is
greater than one and halve each dimension independently with a floor of one.
Retain the existing BC block-alignment early return and max-level bound; publish
the generated level count with its complete payload.

## Critical files

- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` — mip loop.
- `DataPacker/Source/ExportJobs/ExportTexture.cpp` — regular texture publication.
- `Common/TextureFormat.cpp` — complete-chain sizing contract.

## In scope

- `Texture::MakeMipmaps` termination condition for uncompressed regular images.
- Mip count/payload pairing in the existing regular export path.

## Out of scope

- BC compressed sub-4x4 policy, cubemaps, filter quality, or runtime mip sampling.
- New texture formats or asset dimensions policy.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: serialized texture mip payload/count
crosses DataPacker/runtime and GPU image creation. Every regular image has the
complete independent-axis chain; existing ordinary images and BC behavior
remain valid.

Tier rationale: the fix is a single, fully specified loop-termination
correction in `Texture::MakeMipmaps` inside one producer subsystem. The texture
format and mip-count contract are unchanged, the runtime already consumes
complete chains, and every image whose dimensions both exceed one keeps
byte-identical output.

## Acceptance criteria

- A valid uncompressed `1x8` image publishes four mips and the matching payload size.
- `8x1` behaves symmetrically; `1x1` remains one level.
- BC textures retain their current block floor and valid texture output.

## Notes

Origin: `CAI/shard-0008/003`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0008.md:74`.
No source fix, build, or runtime scenario was performed while routing.
