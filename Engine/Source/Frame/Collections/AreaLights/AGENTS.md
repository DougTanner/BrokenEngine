# AreaLights - Quad-Based Area Lights

Owner-driven quad lights producing two GPU outputs per light: a lighting deposit
quad drawn into the lighting layer of the flight plane nearest the quad centre
(`NearestPlaneIndex`), projected onto that plane (blurred texture variant) and a
world-space visible-light quad (unblurred).

## Unique Behaviors

- Externally-supplied geometry: owners `Sync` four visible corner positions plus
  a per-light intensity multiplier every frame — the geometry itself is
  parent-supplied, not just a position; type index selects baked appearance.
- Independent culling and single deposit buffer: the visible-light quad tests
  an AABB over all 8 visible + lighting vertices against the live visible area,
  while deposits bypass that cull (see `AreaLightsRender.cpp` for off-area
  clipping) and pack into one deposit buffer, pipeline, and cursor (`kuiCrc`)
  separate from the visible-light cursor, so deposit and visible buffers are
  not index-aligned. Only lights matching an active `giLightingRefreshLayer`
  (`Render.h`) emit deposits; when no layer is active, deposit buffer resizing
  and emission are skipped and the deposit indirect count is zero, while
  visible-light draws keep their full rendered count.
- Orientation via corners: visible quad uses raw corners; deposit quad scales
  the corner offsets about the center so, for a nonzero horizontal visible
  center-to-corner-0 extent, the base scale makes that distance equal the type's
  lighting size in world-space units. A zero extent uses an unscaled base.
  Both branches then multiply the offsets by the square root of
  `gAreaLightAreaMultiplier`, increasing the source XY footprint area by that
  wrapper's factor and dividing deposit intensity by the same factor. The
  deposit keeps the visible quad's rotation without a rotation field.
  `gAreaLightForcedSquare` then stretches the shorter source XY side toward
  the longer side, symmetrically about the center, before culling and
  projection. This adjustment preserves each scaled corner's Z/W and leaves
  intensity unchanged. `ProjectToFlightPlane` projects each adjusted corner
  onto its layer's flight plane; differing corner heights can make the
  projected footprint nonsquare.
- Per-type render-time overrides: visible intensity, lighting size, and lighting
  intensity each read an optional `Wrapper*` on the type (live UI tuning)
  falling back to the baked value; the per-light multiplier scales both
  intensities but never the size.
