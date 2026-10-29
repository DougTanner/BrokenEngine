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
- Terrain production computes each drawn placement's vertex XYZ with the
  terrain vertex expressions, sampling the visible composite elevation. Vertices
  outside the visible elevation texture take the sea-floor elevation, so terrain
  beyond the drawn area is not traced. Traced terrain agrees with drawn terrain
  only within the shared ray offset `kfLightingRayOffset`, which both the
  reference and candidates use; no exact raster equivalence is claimed.

CPU publication and resource ownership belong to
[Graphics](../../../Source/Graphics/AGENTS.md#lighting-publication).
