# Lighting Shaders - Dynamic Light Deposit and Spread

Dynamic lights follow `deposit -> spread -> combine -> temporal`. Deposit
shaders write three EWNS targets (EWNS is defined in the shader hub);
visible-light billboards instead render directly into the main framebuffer. Hex
shields provide an additional surface-normal depositor from Objects
(`../Objects/AGENTS.md`).

## Pipeline Contracts

- Readers sample the single lighting layer through `lightingAreaToUv`;
  `f4LightingArea` holds its world area. The spread's terrain height fade
  applies unconditionally at `fBaseHeight`. Render
  (`../../../Source/Graphics/Render/AGENTS.md` `## Ordering and Publication`)
  owns the refresh schedule, targets, and per-frame attachment load clears.
- Area and point deposits fade against the padded lighting-texture boundary, not
  the visible-area edge. Rotation can move point-light texture coordinates
  outside `[0,1]`, so that path uses clamped sampling.
- Each spread pass carries an accumulation chain for the next pass and emits a
  separate pre-accumulation snapshot weighted by the combine curve. Combine sums
  the snapshots; substituting accumulated outputs changes the intended
  multi-pass weighting.
- Combine normalizes the snapshots and tone maps them into the directional
  combine targets (`RenderTargetTextures::mpCombineTextures`).
  `LightingTemporal.comp` owns the directional temporal blend, rejecting history
  whose reprojected coordinate leaves `[0, 1]`; reset uses current data only.
  History copy then reads the resolved directional outputs to publish history
  and derive the precomputed ambient target, so ambient matches those clamped
  outputs with no separate ambient history. That ambient target holds the
  average of the stored EWNS values (formula at `combineImageAmbient` in
  `LightingHistoryCopy.comp`), so a consumer reads ambient in one texture sample
  instead of sampling the directional targets. Consumers that need the
  un-averaged sum multiply by 4 — `../Water/Water.frag` and
  `../Terrain/Terrain.frag`, both feeding the smoke blend. Storing a differently
  scaled value here without changing those two puts smoke on water and terrain
  off by 4x, with nothing to catch it but the artwork looking wrong. Combine,
  temporal, and history copy dispatch over the whole combine texture.
- Light-type textures are pre-blurred for deposits. Visible sprites continue
  sampling the original textures.

Lighting coverage past the view follows `engine::gExtraArea`
(`../../../Source/Graphics/AGENTS.md` `## World and Camera Contracts`).
Consumers sample the lighting textures through the border sampler, whose
outside-the-texture color is transparent black, so a sample past the texture
edge reads as no light. Switching that sampler to clamp-to-edge — the more
common default — would instead smear the outermost lit texels across the whole
off-texture margin.

The renderer's Graphics documentation (`../../../Source/Graphics/AGENTS.md`)
owns the framing texel references, world-sized texels, zoom rescaling, and
recreation behavior.
