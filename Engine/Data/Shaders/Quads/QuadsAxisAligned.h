#ifndef QUADS_AXIS_ALIGNED_H
#define QUADS_AXIS_ALIGNED_H

vec2 AxisAlignedQuadTexcoord(AxisAlignedQuadLayout quad, vec2 f2InQuadVertex)
{
	return vec2((1.0f - f2InQuadVertex.x) * quad.f4TextureRect.x + f2InQuadVertex.x * quad.f4TextureRect.z,
	                     (1.0f - f2InQuadVertex.y) * quad.f4TextureRect.y + f2InQuadVertex.y * quad.f4TextureRect.w);
}

vec4 AxisAlignedQuadWorld(AxisAlignedQuadLayout quad, vec2 f2InQuadVertex)
{
	float fCos = cos(quad.fRotation);
	float fSin = sin(quad.fRotation);
	float fLocalX = (f2InQuadVertex.x - 0.5f) * quad.f4VertexRect.z;
	float fLocalY = (f2InQuadVertex.y - 0.5f) * quad.f4VertexRect.w;
	float fRotX = fLocalX * fCos - fLocalY * fSin;
	float fRotY = fLocalX * fSin + fLocalY * fCos;
	float fCenterX = quad.f4VertexRect.x + quad.f4VertexRect.z * 0.5f;
	float fCenterY = quad.f4VertexRect.y + quad.f4VertexRect.w * 0.5f;
	float fWorldX = fCenterX + fRotX;
	float fWorldY = fCenterY + fRotY;
	return vec4(fWorldX, fWorldY, fCenterX, fCenterY);
}

#endif
