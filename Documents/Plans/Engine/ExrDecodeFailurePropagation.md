<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:23.907Z","dependsOn":[]} -->
# Propagate EXR scanline decode failures

## Context

The frozen audit retained `CAI/shard-0008/002`. `Texture::LoadExr` checks only
`exr_start_read`; it ignores data-window, chunk-info, decoder setup/run, and
destroy results at `DataPacker/Source/ExportJobs/Texture/Texture.cpp:170-243`.
The caller accepts an existing `Normals.exr` and publishes the resulting BC5
texture. The source tree is unchanged from baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the manager accepted the fresh
review's external OpenEXR claim.

## Design

Check every OpenEXR result needed to establish the data window and decode each
scanline chunk, including cleanup failure where it affects validity. On any
non-success, fail `LoadExr` through the existing export aggregate before
copying destination vectors into `mData`; retain valid channel ordering and
normal-map encoding.

## Critical files

- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` — EXR read/decode loop.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp` — caller/cache boundary.
- `ThirdParty/openexr/src/lib/OpenEXRCore/openexr_decode.h` — API contract (read-only).

## In scope

- Result checks for EXR setup, per-chunk read/decode, and validity propagation.
- Preventing zero/partial normal texture publication after a decode failure.

## Out of scope

- OpenEXR library changes, normal-map channel math, cache existence policy, or unrelated image loaders.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: opaque EXR cache input crosses a
threaded/serialized island texture publication boundary. Only complete
successful decodes are adopted; valid EXRs retain current output.

Tier rationale: the fix is fully specified and confined to one offline
DataPacker function — check the OpenEXR results already being discarded and
fail through the existing export aggregate. It adds only rejection of corrupt
input; no serialized layout, decode math, or output for valid EXRs changes.

## Acceptance criteria

- A header-valid but truncated/corrupt scanline causes a structured export failure and no BC5 normal output replacement.
- A complete EXR still produces identical channel data and texture encoding.
- No ignored `exr_result_t` can lead to successful publication.

## Notes

Origin: `CAI/shard-0008/002`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0008.md:56`.
No source fix, build, or EXR run was performed during routing.
