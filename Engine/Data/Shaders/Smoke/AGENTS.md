# Smoke Shaders - Sparse Camera-Following Simulation

Smoke is a single client-only 2D density field at the base flight-plane height;
it follows the camera on a held world rectangle and refreshes on its own
cadence. Objects deposit into the field, two compute passes advect and diffuse
it, and world shaders sample it through shared smoke helpers, reusing the
caller's single lighting-layer sample; Model (`../Model/AGENTS.md`) owns how a
model fades smoke with height. Smoke has its sun/moon and ambient base lighting
multiplied by object-shadow R (`../Shadow/AGENTS.md` `## Overview`).
Point-light and explosion lighting on smoke stays unshadowed. Terrain and water
read that shadow at the base-height projection, and models at the fragment.

## Controls

Terrain and water smoke-shadow intensity (`gSmokeShadowIntensity`) and natural
noise-map wind quantity (`gSmokeWindNoiseQuantity`) each have a single control.
Intensity scales terrain and water smoke shadows; particles pass
`SmokeObjectHeightFade` instead. Quantity feeds both spread passes,
independently of the simulated wind field.

## Frame and Remap Contract

Each refresh runs pass B, pass A, then deposit. Pass B reads the previous
refresh's pass-A output in previous-area coordinates without remapping. Pass A
performs the single previous-area-to-current-area remap. Fresh deposits then
enter pass A's output so consumers see them in the same frame.

A frame between refreshes runs no spread, and every reader samples the texture
where it lies; deposits keep landing in it. The refresh schedule and the area
publication for those frames are owned by Render
(`../../../Source/Graphics/Render/AGENTS.md` `## Precision and History`). The
dilate radius stays 2 tiles at every step count, so smoke whose per-refresh
gather offset passes that radius-2 halo has no active output tile and is
deleted, and strong-wind plume fronts thin at higher step counts.

The asymmetric remap requires matching occupancy dilation: B uses direct tile
lookup, while A remaps output-tile centers into the previous grid before lookup.
Both variants union input activity with existing output occupancy so texels left
behind by camera movement are revisited and cleared.

## Sparse Dispatch Invariants

- Occupancy is bit-packed by tile and compacted into an indirect active-tile
  list. Spread cost scales with occupied coverage.
- Outputs must converge to exact zero after storage-format quantization. Decay
  and threshold choices must not create positive steady states — values the
  simulation settles toward instead of reaching zero; occupancy is rebuilt from
  nonzero output, so residual values keep tiles active forever.
- The two spread passes are not interchangeable. Both call the shared
  `SmokeSpread` helper, with different noise scales, but only pass B adds the
  terms that kill off faint smoke: extra decay over terrain (cutoff in
  `SmokeSpreadTwo.comp`), a fade toward the edge of the simulation area, and
  the final step-down walk with threshold zeroing that satisfies the exact-zero
  rule above. Copying those terms into pass A for symmetry doubles the
  extinction rate and visibly shortens every plume; dropping them from pass B
  leaves every touched tile occupied forever and quietly turns the sparse
  dispatch back into a full-texture one.
- Enable, disable, and recreation use indirect fullscreen clears. Texture
  occupancy then drains through the following spread frame.
- Wind is sampled in its own area. The UV conversion deliberately negates
  rescaled X while inverted UV Y cancels the world-to-texture Y sign; changing
  one sign independently reverses advection.

Wind (`../Wind/AGENTS.md`) owns the velocity field and how it reads the smoke
area.
