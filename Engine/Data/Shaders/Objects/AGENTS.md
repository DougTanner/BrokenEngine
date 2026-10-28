# Object Shaders - Hex Shields

Hex shields render visible color with directional damage visualization.

## Invariants

- Geometry and shading use separate per-direction intensities. Vertex intensities drive displacement waves; fragment intensities drive visible alpha. Keep the two lifetimes independent.
- Visible shading derives the center-normal by blending the geodesic center direction with the mesh normal. This shared normal keeps facet smoothing and reflection consistent.
