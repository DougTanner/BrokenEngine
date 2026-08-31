<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:25.762Z","dependsOn":[]} -->
# Destroy a Texture framebuffer before its attachment views

## Context

Final survivor `S010-C004` is a retained HIGH Vulkan lifetime finding. `Texture::CreateRenderTarget` builds `mVkFramebuffer` from the color and optional depth image views, but `Texture::Destroy` destroys the color view/image and resets depth before destroying its framebuffer and render pass (`Engine/Source/Graphics/Objects/Texture.cpp:403-444`). The final locator verified that attachment views must remain valid through destruction of every referencing framebuffer.

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-012.md` under `S010-C004 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-010.md:117` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:200`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to destroy the owned framebuffer and render pass before resetting the depth Texture or destroying the color image view/image, while retaining the borrowed-placeholder early return for deferred textures. Preserve synchronized caller teardown and valid render-target recreation.

## Critical files

- `Engine/Source/Graphics/Objects/Texture.cpp:403-444` — render-target creation and `Texture::Destroy` ordering.
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:18-39` — analogous manager-owned path (read-only coordination evidence).
- `Engine/Source/Graphics/Objects/AGENTS.md` and `Engine/Source/Graphics/Managers/AGENTS.md` — Vulkan lifetime/synchronization contracts.

## In scope

- `Texture::Destroy` dependency order for owned framebuffer, render pass, depth Texture, color view, and color image.
- Existing synchronized render-target recreation and borrowed-placeholder handling.

## Out of scope

- Manager-owned lighting framebuffer ordering, GPU wait policy, descriptor registration, render-pass formats, or texture creation semantics.
- Deferred-destruction architecture, handle ownership redesign, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: a reusable Vulkan RAII object owns a framebuffer and its attachment views across many graphics managers; object dependency lifetime is an invariant surface.

Preserve these invariants:

- A framebuffer is destroyed before any color/depth view it references.
- Existing manager synchronization still precedes `Texture::Destroy`; borrowed placeholder views are never freed.
- Valid render-target creation, recreation, render-pass compatibility, and descriptor behavior remain unchanged.

## Acceptance criteria

- Object-lifetime validation over one render-target recreate shows framebuffer/render-pass destruction before all attachment view/image destruction.
- Smoke, wind, terrain, shadow, lighting, object-shadow, and cache render targets recreate successfully with no Vulkan validation error.
- Client Debug and Release builds pass through `/compile`.

## Coordination

`Documents/Plans/Engine/LightingFramebufferTeardownOrdering.md` owns the separate `RenderTargetTextures` lighting loop. Keep the reusable Texture destructor and manager-owned loop independently ordered, while preserving the common synchronized teardown boundary.

## Notes

Origin: `S010-C004`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-012.md` (`S010-C004 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-010.md:117`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:200`. External claim `EXT-050` was VERIFIED. No source fix or build was performed during routing.
