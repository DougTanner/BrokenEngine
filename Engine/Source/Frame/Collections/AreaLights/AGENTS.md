# AreaLights - Quad-Based Area Lights

Owner-driven quad lights producing two GPU outputs per light: a lighting deposit
quad projected through `ProjectToBaseHeight`
(`../../../Graphics/GraphicsUtils.h`, blurred texture variant) and a world-space
visible-light quad (unblurred).

## Unique Behaviors

- Externally-supplied geometry: owners `Sync` four visible corner positions plus
  a per-light intensity multiplier every frame — the geometry itself is
  parent-supplied, not just a position; type index selects baked appearance.
- Independent culling and single deposit buffer: the visible-light quad tests
  an AABB over all 8 visible + lighting vertices against the live visible area,
  while deposits bypass that cull (see `AreaLightsRender.cpp` for off-area
  clipping) and pack into one deposit buffer, pipeline, and cursor (`kuiCrc`)
  separate from the visible-light cursor, so deposit and visible buffers are
  not index-aligned. Deposits are emitted only when `gbLightingRefreshFrame`
  (`Render.h`) is true; when it is false, deposit buffer resizing and emission
  are skipped and the deposit indirect count is zero, while visible-light draws
  keep their full rendered count.
- Orientation and per-side expansion: the visible quad uses raw corners, and
  `AreaLightsType::expandSides` (`AreaLightExpandSides`, `AreaLights.h` owns
  its default) controls which horizontal edges the lighting deposit may expand
  under `fLightingSize`, `gAreaLightAreaMultiplier`, and
  `gAreaLightForcedSquare` while keeping disabled sides locked and preserving
  corner Z/W before `ProjectToBaseHeight`. `AreaLightsInterpolate::Render`
  (`AreaLightsRender.cpp`) owns the expansion and squaring math. Deposit
  intensity divides by `gAreaLightAreaMultiplier` only when an expansion side
  is enabled, and stays unchanged under `gAreaLightForcedSquare`. Deposit
  corners project individually through `ProjectToBaseHeight`, so differing
  corner heights or terrain elevations can make the projected footprint
  nonsquare.
- Per-type render-time overrides: visible intensity, lighting size, and lighting
  intensity each read an optional `Wrapper*` on the type (live UI tuning)
  falling back to the baked value; the per-light multiplier scales both
  intensities but never the size.
