<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:15.627Z","dependsOn":[]} -->
# Validate packed texture dimensions before same-queue image creation

## Context

Final survivor `S009-C001` is a retained HIGH Graphics trust-boundary finding. `TextureManager` narrows on-disk width, height, and mip values into `TextureInfo` without semantic validation. On the supported same-graphics/transfer-queue path, `HandleUploadEarlyOut` publishes `kDiskLoaded` before `ValidateTextureDimensions`; the main-thread fallback then computes the byte size and calls `Texture::Create` after only a resident-extent assertion (`Engine/Source/Graphics/Managers/TextureManager.cpp:165-193,548-560`; `TextureUploadManager.cpp:390-431`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-011.md` under `S009-C001 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-009.md:34` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:188`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to apply the existing texture dimension/mip/domain limits before any same-queue early-out, byte-size arithmetic, or image creation. Reject malformed signed/narrowed dimensions through the existing corrupt packed-asset path; keep dedicated-transfer validation, valid texture upload, placeholder, and resident-range behavior unchanged.

## Critical files

- `Engine/Source/Graphics/Managers/TextureManager.cpp:165-193,548-560` — header adoption and fallback creation.
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp:390-431` — early-out and dedicated-transfer validation.
- `Engine/Source/Graphics/Objects/Texture.cpp:177-213` — Vulkan image creation consumer.
- `Engine/Source/Graphics/Managers/AGENTS.md` and `Engine/Source/File/AGENTS.md` — pack-backed texture validation contracts.

## In scope

- Positive/representable/device-supported texture width, height, mip, and derived-size validation before both upload branches.
- Existing corrupt-pack failure propagation before `Texture::Create` or ready publication.
- Valid same-queue and dedicated-transfer uploads, placeholders, and descriptors.

## Out of scope

- Texture format redesign, pack layout/version, queue-family selection, image allocation strategy, shader changes, or placeholder policy.
- Runtime dimension repair, new compatibility readers, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped Graphics texture-admission behavior). Trigger: opaque pack texture metadata reaches image allocation through two manager branches, but the correction reuses one existing validation predicate without changing serialized data, queues, or threading structure.

Preserve these invariants:

- Every accepted texture has finite positive dimensions, supported mip count, and representable derived size before allocation.
- Same-queue early-out cannot publish malformed dimensions or bypass validation.
- Valid transfer/same-queue uploads and resident byte checks retain current behavior.

## Acceptance criteria

- A malformed same-queue texture with an over-limit, negative/wrapped, or otherwise invalid dimension is rejected before `ComputeImageByteSize`/`Texture::Create`.
- Valid textures continue through both queue configurations and render with unchanged descriptors/bytes.
- Client Debug and Release builds pass through `/compile`; a focused malformed-pack load observes controlled rejection.

## Notes

Origin: `S009-C001`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-011.md` (`S009-C001 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-009.md:34`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:188`. No exact existing Plan was found. No source fix or build was performed during routing.
