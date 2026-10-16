<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T20:46:47.205Z","dependsOn":[]} -->
# Describe temporal history as the last refresh, not the previous frame

## Context

The terrain-shadow and lighting temporal chains each refresh on their own update cadence (`Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`: a refresh advances the current and previous area pair and publishes full-texture work; a skipped frame holds both areas and dispatches nothing). `ShadowTemporal.comp` and `LightingTemporal.comp` therefore run only on refresh frames, and their history texture and previous-area uniform hold the state of the last refresh, not of the previous render frame. At cadence 1 the two coincide; at cadence 2-4 they do not.

Comments at these sites still say "previous frame" (the shadow wording became false with the terrain shadow update cadence; the lighting wording has been false since the lighting update cadence landed):

- `Engine/Data/Shaders/ShaderGlobalLayout.h:19` (`f4ShadowAreaPrevious`) and `:140` (`f4LightingAreaPrevious`).
- `Engine/Data/Shaders/Shadow/ShadowTemporal.comp:17` and `:32`.
- `Engine/Data/Shaders/Lighting/LightingTemporal.comp:14`, `:24`, and `:42`.
- `Engine/Source/Graphics/Managers/RenderTargetTextures.h:54` (`mpLightingHistoryTextures`) and `:71` (`mShadowHistoryTexture`).
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp:317` and `:354`.
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp:214` (the `kPipelineShadowTemporal` comment).

No consumer reads a wrong value: the only reader of each previous-area uniform and history texture is its temporal pass, which runs only on refresh frames. The defect is the false claim in the comments (`Documents/C++StyleGuide.txt` rule 64). The shadow sites were left unfixed by the terrain shadow update cadence change because `ShaderGlobalLayout.h` is a dual-language GLSL header, so editing it triggers Local data generation.

## Design

The author recommends rewording each listed comment so that it names the last refresh instead of the previous frame, and changing nothing else on those lines:

- `ShaderGlobalLayout.h:19`: "f4ShadowArea as it stood before the latest refresh, for ShadowTemporal.comp reprojection" (wording proposed by `/comment-review`).
- `ShaderGlobalLayout.h:140`: the same wording with `f4LightingArea` and `LightingTemporal.comp`.
- Every other site: replace "previous-frame" or "previous frame's" with the last-refresh equivalent ("the last refresh's result", "last-refresh UV", "last-refresh combine outputs"). Keep each comment's other content unchanged.

The shadow and lighting sites stay one change because they make the same false claim about mirrored chains and are verified the same way.

Risk tier: Tier 1 (`.agents/references/risk-tiers.md`) — trigger: comment-only documentation, behavior-preserving, with no public signature or invariant exposure. Build exposure: the `Engine/Data/**` edits are a Local data-mode path trigger (`.agents/skills/compile/references/runtime-data-mode.md` `## Mode selection`), so the client build needs Local generation; acceptance criterion 2 is that authorization once the Plan is approved. No determinism/CRC, serialization, `.pack` format version, wire, threading, or affinity exposure: no layout member, binding, or shader statement changes.

## Critical files

- `Engine/Data/Shaders/ShaderGlobalLayout.h`
- `Engine/Data/Shaders/Shadow/ShadowTemporal.comp`
- `Engine/Data/Shaders/Lighting/LightingTemporal.comp`
- `Engine/Source/Graphics/Managers/RenderTargetTextures.h`
- `Engine/Source/Graphics/Managers/RenderTargetTexturesLighting.cpp`
- `Engine/Source/Graphics/Managers/WorldLightingShadowPipelines.cpp`

## In scope

- The trailing comments on `f4ShadowAreaPrevious` (`ShaderGlobalLayout.h:19`) and `f4LightingAreaPrevious` (`:140`).
- The block comments at `ShadowTemporal.comp:17` and `:32`, and at `LightingTemporal.comp:14`, `:24`, and `:42`.
- The comments on `mpLightingHistoryTextures` (`RenderTargetTextures.h:54`) and `mShadowHistoryTexture` (`:71`).
- The combine-texture and history-texture comments in `RenderTargetTexturesLighting.cpp` (`:317`, `:354`).
- The `kPipelineShadowTemporal` comment in `WorldLightingShadowPipelines.cpp` `CreatePipelineShadows` (`:214`).

## Out of scope

- Any code, layout member, binding, or shader statement.
- Comments where "previous frame" refers to the frame-in-flight or ping-pong order and is correct: `RenderTargetTexturesLighting.cpp:119` and `:251`, `Engine/Source/Graphics/Managers/AGENTS.md`, `SwapchainManager.cpp`, and the Wind shaders.
- Smoke, wind, and object-shadow comments.
- `AGENTS.md` files: the Render and Shadow contracts already describe the refresh semantics.

## Acceptance criteria

1. No listed comment says "previous frame" or "previous-frame"; each names the last refresh.
2. The client builds with Local data generation (`/compile`, `-DataBuildMode Local -RunDataPacker`); the server builds.

## Notes

- Review route: `/comment-review` over the changed C++ and GLSL comments. The change is comment-only, so `/glsl-review` has no layout or binding change to review.
- `Documents/Plans/Engine/SmokePerFlightPlane.md` and `Documents/Plans/Engine/FlightPlanePresentation.md` also edit `ShaderGlobalLayout.h` and `RenderTargetTextures.h`, at other members; the overlap is textual only, so whichever Plan lands later rebases.
