<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:22.437Z","dependsOn":[]} -->
# Destroy lighting framebuffers before their attachment views

## Context

Final survivor `S009-C004` is a retained HIGH Vulkan lifetime finding. `RenderTargetTextures::DestroyLightingTextures` destroys all spread attachment textures/views before destroying each `mpSpreadVkFramebuffers[iPass]`, even though those views are framebuffer attachments (`Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:18-39`). The final locator verified that attachment views must remain valid until every referencing framebuffer is destroyed; device idle only handles in-flight work, not object dependency order.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-011.md` under `S009-C004 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-009.md:76` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:191`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to destroy each spread framebuffer before destroying the color/depth attachment Textures it references, then destroy the render pass in its existing position. Preserve the current synchronization/drain callers, per-pass ownership, and recreation of valid lighting resources.

## Critical files

- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:18-39,56-58` — lighting attachment/framebuffer teardown and recreation.
- `Engine/Source/Graphics/Objects/Texture.cpp:422-433` — attachment view/image destruction (read-only dependency evidence).
- `Engine/Source/Graphics/Managers/AGENTS.md` and `Engine/Source/Graphics/Objects/AGENTS.md` — Vulkan lifetime contract.

## In scope

- Destruction ordering between `mpSpreadVkFramebuffers`, spread attachment textures/views, and the lighting render pass.
- Existing synchronized lighting teardown/recreation and valid resource creation.

## Out of scope

- Texture destructor ownership, blur/reblur fencing, render-pass format/layout, descriptor updates, or shader behavior.
- GPU wait policy changes, deferred destruction architecture, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: Vulkan object dependency lifetime crosses manager-owned framebuffers, attachment views, and graphics recreation paths; resource ordering is a Tier-3 invariant.

Preserve these invariants:

- Every lighting attachment view remains alive until all referencing framebuffers are destroyed.
- Existing GPU-idle/fence synchronization still precedes destruction; no in-flight use is reintroduced.
- Valid spread textures, framebuffers, render pass, and recreation behavior remain unchanged.

## Acceptance criteria

- Object-lifetime validation over lighting teardown/recreate shows each framebuffer destroyed before every referenced attachment view/image.
- Normal lighting rendering and recreation preserve all six spread attachments and framebuffers with no validation/device error.
- Client Debug and Release builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/TextureFramebufferTeardownOrdering.md` owns the reusable `Texture::Destroy` path for render-target framebuffers. Keep this manager-owned spread ordering separate, and preserve the common Graphics synchronization before either destruction path.

## Notes

Origin: `S009-C004`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-011.md` (`S009-C004 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-009.md:76`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:191`. External claim `EXT-042` was VERIFIED. No source fix or build was performed during routing.
