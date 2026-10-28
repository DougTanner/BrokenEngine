# Lighting Shaders - Resident Scene Queries

Client-only lighting queries use resident terrain and opaque model triangles,
with point and one-sided rectangle sources in the camera cell's frame.

## Query Contracts

- Closest-hit and visibility queries share resident geometry and opacity policy.
  Preserve original index widths and material partitions, including empty
  ranges. MASK follows the opaque raster policy; BLEND, water, smoke, sprites,
  and shields do not occlude queries.
- Model queries reuse the final rigid/skinned geometry and shared diffuse
  expression. Terrain queries use the shared terrain material expression at
  explicit LOD 0. Neutral fixture animation uses diffuse albedo 0.5 and the
  unperturbed normal without authored textures.
- Point and rectangle evaluation returns additive linear direct diffuse with
  albedo/pi applied once. Rectangle power is distributed over its area and
  sample count once; preserve emitter sidedness, full XYZ distance, and both
  shading and geometric normals.
- `LightingCompose.h` reads completed linear direct diffuse with `texelFetch`
  at the final fragment pixel only while the acquired framebuffer's Set 0
  control enables it. Opaque models, terrain, water and fixture draws add that
  result at their local diffuse seam before smoke; BLEND receives zero. Do not
  apply albedo/pi again. CPU validity and fallback lifetime belong to Graphics.
- Terrain production rasterizes the original elevation expression into reusable
  R16 scratch with sea-floor clear and MAX blending. Keep implicit sampling;
  compute reproduction or trace-domain border clamp is not equivalent. Tile
  coverage follows retained topology bounds, including vertices beyond the
  authored footprint. Original-output, raw-tap, four-shift, and coverage
  diagnostics must establish the required 0.001 m XYZ agreement on the device.
- Original-output terrain diagnostics use point-list draws over every retained
  vertex so incomplete triangle tails still publish evidence. Preserve that
  coverage independently of terrain raster topology.

CPU publication and resource ownership belong to
[Graphics](../../../Source/Graphics/AGENTS.md#lighting-publication).
