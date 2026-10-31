# Smoke Shaders - Sparse Camera-Following Simulation

Smoke is a client-only 2D density field per flight plane; each plane's field follows the camera on its own held world rectangle and refreshes on its own frames. Objects deposit into the fields, two compute passes advect and diffuse each plane's field, and world shaders sample the planes, lowest first, through shared smoke helpers; Model (`../Model/AGENTS.md`) owns which planes a model samples.

## Frame and Remap Contract

Each refresh runs pass B, pass A, then deposit. Pass B reads the previous refresh's pass-A output in previous-area coordinates without remapping. Pass A performs the single previous-area-to-current-area remap. Fresh deposits then enter pass A's output so consumers see them in the same frame.

A frame between a plane's refreshes runs no spread for that plane, and every
reader samples its texture where it lies; deposits keep landing in it. The
refresh schedule and the area publication for those frames are owned by Render
(`../../../Source/Graphics/Render/AGENTS.md` `## Precision and History`). The
dilate radius stays 2 tiles at every step count, so smoke whose per-refresh
gather offset passes that radius-2 halo has no active output tile and is
deleted, and strong-wind plume fronts thin at higher step counts.

The asymmetric remap requires matching occupancy dilation: B uses direct tile lookup, while A remaps output-tile centers into the previous grid before lookup. Both variants union input activity with existing output occupancy so texels left behind by camera movement are revisited and cleared.

## Sparse Dispatch Invariants

- Occupancy is bit-packed by tile and compacted into an indirect active-tile list. Spread cost scales with occupied coverage.
- Outputs must converge to exact zero after storage-format quantization. Decay and threshold choices must not create positive steady states — values the simulation settles toward instead of reaching zero; occupancy is rebuilt from nonzero output, so residual values keep tiles active forever.
- The two spread passes are not interchangeable. Both call the shared
  `SmokeSpread` helper, with different noise scales, but only pass B adds the
  terms that kill off faint smoke: extra decay where terrain rises more than
  the plane's height above the lowest plane, a fade
  toward the edge of the simulation area, and the final step-down walk with
  threshold zeroing that satisfies the exact-zero rule above. Copying those
  terms into pass A for symmetry doubles the extinction rate and visibly
  shortens every plume; dropping them from pass B leaves every touched tile
  occupied forever and quietly turns the sparse dispatch back into a
  full-texture one.
- Enable, disable, and recreation use indirect fullscreen clears. Texture occupancy then drains through the following spread frame.
- Wind is sampled in its own area. The UV conversion deliberately negates rescaled X while inverted UV Y cancels the world-to-texture Y sign; changing one sign independently reverses advection.

Wind (`../Wind/AGENTS.md`) owns the velocity field and how it reads the smoke area arrays.
