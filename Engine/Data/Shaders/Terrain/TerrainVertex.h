#ifndef TERRAIN_VERTEX_H
#define TERRAIN_VERTEX_H

void TerrainVertexCoordinates(vec2 f2InPosition, vec4 f4VertexRect, float fRotation, vec4 f4VisibleArea,
	                          out vec2 f2World, out vec2 f2IslandTexcoord, out vec2 f2VisibleTexcoord, out vec2 f2RotationCosSin)
{
	// The rectangle starts at the top-left corner and stores a negative height.
	float fCenterX = f4VertexRect.x + 0.5f * f4VertexRect.z;
	float fCenterY = f4VertexRect.y + 0.5f * f4VertexRect.w;

	// Rotate island-local XY into world space; Z is rotation-invariant. Matches the per-instance
	// rotation that QuadsAxisAlignedVisibleArea.vert still applies for the elevation prepass.
	float fCos = cos(fRotation);
	float fSin = sin(fRotation);
	f2RotationCosSin = vec2(fCos, fSin);
	float fWorldX = fCenterX + f2InPosition.x * fCos - f2InPosition.y * fSin;
	float fWorldY = fCenterY + f2InPosition.x * fSin + f2InPosition.y * fCos;

	// Textures are baked in island-local axes, so rotation does not enter the UV.
	f2IslandTexcoord = vec2(0.5f + f2InPosition.x / f4VertexRect.z,
	                           0.5f + f2InPosition.y / f4VertexRect.w);

	// Visible-area UV used by Terrain.frag to sample composite G-buffer textures
	// (mTerrainElevationTexture etc., rendered earlier this frame by the per-island G-buffer prepass).
	// Convention matches the original Terrain.vert: u in [0,1] maps to [minX, maxX]; v in [0,1] maps
	// to [maxY, minY] (image-space Y down). f4VisibleArea = {minX, maxY, maxX, minY}.
	f2VisibleTexcoord = vec2(
		(fWorldX - f4VisibleArea.x) / (f4VisibleArea.z - f4VisibleArea.x),
		(fWorldY - f4VisibleArea.y) / (f4VisibleArea.w - f4VisibleArea.y)
	);

	f2World = vec2(fWorldX, fWorldY);
}

#endif // TERRAIN_VERTEX_H
