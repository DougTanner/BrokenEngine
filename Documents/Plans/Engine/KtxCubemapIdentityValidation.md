<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:58:17.849Z","dependsOn":[]} -->
# Preserve cubemap identity for extension-owned KTX inputs

## Context

Final survivor `S002-C013` is a retained HIGH texture-routing finding. `Handles` claims every `.ktx`, while `kCubemap` is assigned only from the `[C]` filename tag. `ProcessKtxCubemap` accepts a cube payload but does not add the missing cubemap flag, so an untagged cube serializes six faces as an ordinary texture (`DataPacker/Source/ExportJobs/ExportTexture.cpp:8-17,92-110`). Runtime derives layer count and cube-compatible image creation from `kCubemap`, accepts the one-layer byte fit, and can ignore five faces.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` under `S002-C013 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:209` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:100`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

Keep `[C]` as the cubemap discriminator. After loading the KTX, reject a `gli::TARGET_CUBE` payload whose input filename lacks `[C]` through the existing export-job failure aggregate, before `AllocateHeaderAndData` can publish a chunk. Leave the current non-cube target rejection in place; tagged cube KTX retains its `kCubemap` flag, six-face payload, and runtime layer behavior. Do not derive the flag from the payload or change the filename convention.

## Critical files

- `DataPacker/Source/ExportJobs/ExportTexture.cpp:8-17,92-110` — filename handling and KTX route.
- `DataPacker/Source/ExportJobs/Texture/Texture.cpp` — KTX payload parsing and shared flag helpers.
- `Common/DataFile.h` — texture flag/chunk contract.
- `DataPacker/Source/ExportJobs/Texture/AGENTS.md` — cubemap routing contract.

## In scope

- `ProcessKtxCubemap` validation that rejects a `gli::TARGET_CUBE` payload without the `[C]` filename tag through the existing export-job failure aggregate before `AllocateHeaderAndData`.
- `[C]` as the cubemap discriminator, with existing tagged-cube flags, six-face payload ordering, and runtime layer behavior.
- The existing rejection path for non-cube KTX targets and unchanged valid-input compression.

## Out of scope

- KTX decoder changes, cubemap face-dimension checks, IBL pre-pass inputs, runtime texture-format redesign, or shader changes.
- Changing filename conventions for valid assets, chunk layout/version, or compatibility readers.
- New unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker texture-routing behavior). Trigger: opaque KTX shape crosses the existing serialized texture-flag/runtime image boundary, but the correction preserves the current format and valid output.

Preserve these invariants:

- Every published KTX chunk's cubemap flag, layer count, face payload, and runtime image shape agree.
- Unsupported or contradictory KTX shape fails before chunk publication.
- Valid 2D and cubemap inputs retain current bytes, ordering, and compression.

## Acceptance criteria

- An untagged KTX whose target is `gli::TARGET_CUBE` fails through the existing export-failure aggregate before `AllocateHeaderAndData`, and no chunk is published.
- A `[C]`-tagged cube retains its `kTexture|kCubemap` flags, six-face payload/order, and runtime six-layer behavior.
- A non-cube KTX continues through the existing rejection path.
- DataPacker `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `S002-C013`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-002.md` (`S002-C013 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-002.md:209`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:100`. No exact existing Plan was found. No source fix or build was performed during routing.
