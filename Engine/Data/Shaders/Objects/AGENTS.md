# Object Shaders - Hex Shields

Hex shields share one vertex stage between the visible-color and
lighting-deposit passes.

## Invariants

- Geometry and shading use separate per-direction intensities. Vertex
  intensities drive displacement waves; fragment intensities drive visible alpha
  and emitted lighting. Keep the two lifetimes independent.
- Both passes derive the center-normal by blending the geodesic center direction
  with the mesh normal. This shared normal keeps facet smoothing, reflection,
  and lighting direction consistent.
- The lighting pass belongs to the surface-normal EWNS family: it projects the
  center-normal XY rather than a source-to-fragment offset. A nearly up-facing
  normal produces zero directional deposit instead of an omnidirectional
  fallback.
- The lighting pass places each vertex where its eye line meets the refreshing
  lighting layer's flight plane (`uiLightingRefreshLayer`), the plane that
  layer's readers sample, at any camera tilt.

Shared shader layout and EWNS conventions live in the shader hub.
