# AreaLights - Quad-Based Area Lights

Owner-driven quad lights producing two GPU outputs per light: a lighting deposit quad drawn into the lighting layer of the flight plane nearest the quad centre (`NearestPlaneIndex`), projected onto that plane (blurred texture variant) and a world-space visible-light quad (unblurred).

## Unique Behaviors

- Externally-supplied geometry: owners `Sync` four visible corner positions plus a per-light intensity multiplier every frame — the geometry itself is parent-supplied, not just a position; type index selects baked appearance.
- Dual outputs share one frustum-cull decision (AABB over all 8 visible + lighting vertices). Deposits pack into one buffer and cursor per lighting layer (`FlightPlaneDepositKey`), independent of the single visible-light cursor, so deposit and visible buffers are not index-aligned. Only the refreshing layer publishes its deposit count, and the player-facing Lighting toggle may publish zero, while the visible-light draw retains the full rendered count.
- Orientation via corners: visible quad uses raw corners; deposit quad scales the corner offsets about the center so its horizontal center-to-corner-0 distance equals the type's lighting size, floored at `MinLightingDepositSize()`. The deposit therefore keeps the visible quad's shape and rotation without a rotation field, but its size comes only from the type, never from the owner's quad size. `ProjectToFlightPlane` then projects it onto its layer's flight plane.
- Minimum deposit size = 8 * max lighting-deposit texel size, preventing sub-texel flicker. `MinLightingDepositSize()` is authoritative: its texel basis is the un-bumped detail texture size, not the lighting-headroom-bumped one, because the headroom factor cancels in the shader's texel formula.
- Per-type render-time overrides: visible intensity, lighting size, and lighting intensity each read an optional `Wrapper*` on the type (live UI tuning) falling back to the baked value; the per-light multiplier scales both intensities but never the size.
