# Object Shadow Per-Pixel Stretch Blur

Revisit When: sunset ship-shadow appearance is next prioritized, or an object-shadow frame-time budget exists to measure the extra blur cost against.

## Context

At sunset the ship drop shadows stretch away from the light (`ShadowStretchProjection` in `Engine/Data/Shaders/ShaderFunctions.h`; the stretch amount comes from `PopulateShadowStretch` in `Engine/Source/Graphics/Render/GlobalUniforms.cpp`).
The wanted look: the sunset blur applies fully to the stretched side of each shadow and not at all to the unstretched side.
The Object Blur Radius Sunset slider maximum would also double, `kiMaxObjectShadowsBlurRadius` 128 to 256 (shared with the Noon slider); `GlobalLayout` stays under its 64 KiB cap checked by the `static_assert` in `Engine/Data/Shaders/ShaderLayoutsBase.h`.

The current pipeline cannot do this. All ships draw their shadows in one pass into one shared RGBA16F raster: R/G hold ground coverage split by height, B/A hold flight planes 1 and 2, so all four channels are used.
`ObjectShadowsBlurH`/`ObjectShadowsBlurV` then blur the whole texture with one radius and precomputed per-channel weights. The ship fragment shader cannot blur because it writes only its own pixel. Per-ship render targets were considered and rejected: one pass per ship plus a merge breaks the record-once command buffers, and each target would still need blurring.

A tried and rejected approach reused the R/G height split as noon and current kernels, with a per-vertex stretch side (1.0 on the half away from the light, 0.0 otherwise) and a variance-matched split.
Measured in the harness, most ships (lowest flight plane, height fraction 0) kept about 99% of their coverage in the noon-radius channel at sunset, so the Sunset slider had almost no effect: the whole-texture maximum changed by 0.017 between Sunset 8 and 64. High ships' away half spread into an invisible haze. Two kernels cannot cover low/high crossed with noon/sunset.

## Design

- The shadow-mode vertex shader outputs the per-vertex stretch side (1.0 or 0.0), interpolated by the rasterizer. The 0-to-1 ramp only spans triangles that cross the hull centre line on the mesh.
- The ship shadow pass writes that value to a second, small full-screen attachment (for example R8 with MAX blend) beside the existing raster, still in one pass for all ships.
- The horizontal blur becomes a scatter-as-gather blur: each source tap is weighted by a Gaussian using the source pixel's own radius (noon radius lerped toward the current radius by its stretch value), so the stretched edge softens outward. A small one-channel intermediate carries the coverage-weighted stretch value to the vertical pass, which does the same.
- The R/G height split stays, and the Sunset slider affects every ship.
- A separable blur with per-source sigma is an approximation, acceptable for soft shadows. The tap loop runs to the maximum radius, as today.

## Performance

The cost must be measured in large battles. One option is a per-rendered-frame time budget for the object-shadow passes, where more ships on screen means their shadows update less often, a refresh cadence like the terrain shadow update interval in `Engine/Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`.

## Out of scope

- Per-ship render targets or per-ship blur passes.
- Changing the stretch projection itself or how its amount is derived.

## Notes

- Client-only rendering; outside the CRC.
- The ship-on-ship B/A channels could use the same per-source radius.
