# Model Shaders - Physically Based Rendering

Static and skinned model paths feed a Cook-Torrance material shader with split-sum image-based lighting. DataPacker generates irradiance and prefiltered-radiance cubemaps; the renderer generates and caches the BRDF lookup texture with this shader family.

## Architecture Notes

- The material push-constant index also selects the mesh-data slot relative to the instance base. CPU draw generation must keep one material/primitive draw aligned with that mesh slot.
- The generated BRDF lookup stores inverted roughness on its V axis. Generator and runtime lookup must remain paired.
- Tangent space is derived from screen-space position and UV derivatives. Degenerate UV gradients, a collapsed orthogonalized tangent, or a normal texture scale of 0 fall back to the geometric normal to avoid NaNs.
- Authored occlusion strength blends the linear red sample from neutral 1 and attenuates only the diffuse/specular image-based lighting sum. Direct sun/moon, engine directional lighting, and emissive remain independent of ambient occlusion.
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
  The shadow-mode vertex
  samples the terrain elevation render target at the model centre for the
  blur height (`../Shadow/AGENTS.md` `## Overview`); the vertex shader is
  shared, so every model pipeline binds that texture.
- Object shadow coverage uses each surface point's original interpolated world Z:
  full at and above sea level, fading linearly to zero at the configured depth
  below Z=0. Scale all RGBA coverage channels before MAX blending; model-centre
  height and displaced water waves do not determine this fade.
- A model's height above the lowest flight plane fades the terrain shadow it
  receives and moves its lighting sample: each lighting layer is sampled along
  the eye line by the model centre's height above that layer's plane. The
  fragment blends the low and high layers by its own height between the two
  planes, reading only the layers its weight needs, and plane k's smoke is lit
  by layer k, a skipped layer being read only where that plane's smoke shows.
- A model takes no smoke from planes below its own flight plane, smoke faded by
  the fragment's height above its own plane so it pokes through, and full smoke
  from every plane above. Its own plane is the one nearest its centre height
  (`FlightPlanes::NearestPlaneIndex`), stored per instance by every CPU writer
  of the model layout, because instances in one draw can sit on different
  planes.
- A model receives object shadow only from planes above its own (channel
  layout: `../Shadow/AGENTS.md` `## Overview`): it takes the darker of the B
  and A planes above `iInLayer`, or 1 when no plane lies above. That value
  multiplies into `fShadow`, reaching sun direct, IBL diffuse, and IBL
  specular, and is `fShadowMoon` for the moon terms.
- Material output remains linear HDR for the fullscreen resolve. Engine cubemaps use Y-up sampling, so model shaders perform the Z-up/Y-up conversion at the sampling boundary.
