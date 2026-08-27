<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:39:50.875Z","dependsOn":[]} -->
# Fix: Defer lighting reblur until the GPU is idle for mutation

## Context

The accepted survivor `CAI/shard-0030/001` shows that
`Graphics::Refresh` calls `ReblurAllLightingTextures` immediately when a blur
wrapper changes (`Engine/Source/Graphics/Graphics.cpp:562-568`).  The call runs
before `Graphics::Destroy` drains workers and the device
(`Graphics.cpp:662-690`), while `BlurLightingTexture` destroys old images and
rewrites global bindless descriptors (`Engine/Source/Graphics/Managers/TextureManager.cpp:763-860`).
An in-flight frame can therefore sample a destroyed image or race the
descriptor replacement.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0030.md:58`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:842`.
All ten frozen target rows matched baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; no source was changed in this
routing session.

Impact: an ordinary blur-slider change can cause a GPU image/view use-after-free,
descriptor race, GPU hang, or device loss.

## Design

Author's recommendation: replace the immediate reblur call with a pending
reblur flag and escalate the current graphics recreation to the existing
pipeline tier so the change is serviced.  In `Graphics::Destroy`, after all
present/submit/upload workers and the device have drained, finish any
`RecreateResources()` work first and then run `ReblurAllLightingTextures`
inside the existing bindless write-safety window for partial tiers only.  This
ordering also handles a simultaneous lighting-texture recreation.  Clear the
flag after that pass; for full surface/device teardown, clear it without
reblurring and let the replacement Graphics resource path handle its own
textures.  Keep the current blur-wrapper change detection and image
replacement implementation.

## Critical files

- `Engine/Source/Graphics/Graphics.cpp:537-568,662-711` — wrapper polling,
  pending flag, drain, and safe reblur point.
- `Engine/Source/Graphics/Graphics.h` — pending reblur state.
- `Engine/Source/Graphics/Managers/TextureManager.cpp:763-860` — image
  replacement and descriptor publication.
- `Engine/Source/Graphics/Managers/TextureDescriptors.cpp:133-152` — existing
  bindless write epoch/descriptor update seam.

## In scope

- Latching blur-setting changes, ensuring they trigger the existing graphics
  recreation tail, and moving the reblur operation after the existing all-worker
  and device drain.
- Wrapping replacement descriptor publication in the current safe mutation
  window and clearing the pending state on both successful partial handling and
  skipped full teardown.
- Preserving blur parameters, lighting CRC membership, texture dimensions, and
  current pipeline recreation behavior.

## Out of scope

- Texture-registration replay after device loss, wind/occupancy reset, or any
  other Graphics destructor fix.
- Replacing the bindless descriptor architecture, adding per-frame deferred
  destruction, or changing the lighting blur algorithm.
- Server/simulation state, wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: runtime settings mutate GPU images
and global descriptors across Graphics, TextureManager, worker drains, and
in-flight frame fences; resource lifetime and threading are Tier-3 surfaces.

Preserve these invariants:

- No image or image view referenced by an in-flight frame is destroyed before
  its framebuffer fence completes.
- Bindless descriptor writes and lighting-image replacement occur only after the
  all-framebuffer drain and inside the existing mutation window.
- A blur change is eventually applied once on a healthy partial recreation and
  is not applied against a lost device during full teardown.
- No deterministic simulation, wire, save, replay, or `.pack` data changes.

## Acceptance criteria

- A live blur-setting change causes a post-drain reblur; source inspection or
  validation confirms no `BlurLightingTexture` call occurs before the worker/
  device waits.
- While a prior frame is in flight, changing sigma/sample count/falloff does
  not destroy its sampled images or produce descriptor-generation/validation
  errors.
- Client Debug and Release builds pass `/compile`; an `/agent-harness` blur
  slider scenario completes and renders with the new blur settings.
- Full device-loss teardown clears the pending request without issuing a reblur
  against the old device.

## Notes

This Plan is distinct from the full-teardown and texture-registration recovery
Plans: it owns the live setting-change call order in TextureManager/Graphics.
