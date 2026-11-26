# Model Shaders - Physically Based Rendering

Static and skinned model paths feed a Cook-Torrance material shader with
split-sum image-based lighting. DataPacker generates irradiance and
prefiltered-radiance cubemaps; the renderer generates and caches the BRDF lookup
texture with this shader family.

## Architecture Notes

- The material push-constant index also selects the mesh-data slot relative to
  the instance base. CPU draw generation must keep one material/primitive draw
  aligned with that mesh slot.
- The generated BRDF lookup stores inverted roughness on its V axis. Generator
  and runtime lookup must remain paired.
- `GetNormal` builds tangent space from the vertex tangent, authored or
  generated at export
  ([Scene and Model](../../../../DataPacker/Source/ExportJobs/AGENTS.md#scene-and-model)):
  T is that tangent orthogonalized against the normal, and the bitangent is the
  sign of its `w` times cross(N, T), so each normal map's green axis follows the
  bitangent its vertices carry. A tangent whose orthogonalized length collapses,
  such as zero `xyz` (`ModelVertex::f4Tangent` owns the encoding) or one almost
  parallel to the normal, falls back to the geometric normal to avoid NaNs, as a
  normal texture scale of 0 does.
- Authored occlusion strength blends the linear red sample from neutral 1 and
  attenuates only the diffuse/specular image-based lighting sum. Direct
  sun/moon, engine directional lighting, and emissive remain independent of
  ambient occlusion.
- Skinning uses compact three-row joint matrices in a buffer separate from mesh
  data. That split is a layout choice — each mesh indexes the shared buffer
  through an offset, so no per-mesh joint cap is baked into the mesh-data struct
  — and not a workaround for the shader-compiler hang the hub records; dynamic
  indexing of that buffer is exactly the case that ruled large arrays out as a
  cause. The skinned normal path combines the mesh normal matrix with
  `mat3(skinMatrix)` and therefore assumes near-rigid joint transforms rather
  than arbitrary non-uniform joint scaling.
- Object shadow footprints use model world XY, independent of camera position.
  Horizon stretch changes world X only: it fixes the sun-facing endpoint of the
  transformed rest bounding box and extends the opposite one, shaped by Stretch
  Power. Every model-layout producer supplies world-X minimum and maximum
  relative to the model centre, independent of light;
  [Model Alignment](../../../../DataPacker/Source/ExportJobs/AGENTS.md#model-alignment)
  owns alignment.
- Object shadow coverage uses each surface point's original interpolated world
  Z: full at and above sea level, fading linearly to zero at the configured
  depth below Z=0, written in `kVkFormatObjectShadows` before MAX blending;
  model-centre height and displaced water waves do not determine this fade.
- A model's height above base height fades the terrain shadow it receives and
  moves its single lighting-layer sample along the eye line by the model
  centre's height above base height (shared smoke reuse:
  `../Smoke/AGENTS.md`).
- A model samples the single base-height smoke field faded by the fragment's
  height above base height (`SmokeObjectHeightFade`) so it pokes through;
  `../Smoke/AGENTS.md` owns how object shadow affects that smoke's lighting.
- Material output remains linear HDR for the fullscreen resolve. Engine cubemaps
  use Y-up sampling, so model shaders perform the Z-up/Y-up conversion at the
  sampling boundary.
