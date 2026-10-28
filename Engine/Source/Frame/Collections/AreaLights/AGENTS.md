# AreaLights - Quad-Based Area Lights

Owner-driven quad lights render visible sprites using the original texture. Their appearance scalars remain collection state; they do not publish physical diffuse sources.

## Unique Behaviors

- Externally-supplied geometry: owners `Sync` four visible corner positions plus a per-light intensity multiplier every frame — the geometry itself is parent-supplied, not just a position; type index selects baked appearance.
- Culling uses the visible corners' AABB, rebased into the camera cell; only visible quads advance the packed render cursor.
- Visible intensity uses its optional type wrapper or baked value, multiplied by the per-light intensity. Physical emission requires a separately authored source; do not derive power from sprite intensity.
