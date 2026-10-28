#ifndef LIGHTING_MODEL_HIT_H
#define LIGHTING_MODEL_HIT_H

#extension GL_EXT_buffer_reference : require

#include "LightingModelsCommon.h"
#include "Model/ModelDiffuse.h"

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer LightingModelWords { uint pWords[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer LightingModelFloats { float pFloats[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer LightingModelMaterials { LightingModelMaterial pMaterials[]; };

uint LightingModelIndex(LightingGeometry geometry, uint index)
{
	LightingModelWords words = LightingModelWords(geometry.uiIndexAddress);
	if (geometry.uiIndexBytes == 4u) return words.pWords[index];
	return (words.pWords[index / 2u] >> ((index & 1u) * 16u)) & 0xffffu;
}

vec3 LightingModelFloat3(uint64_t address)
{
	LightingModelFloats values = LightingModelFloats(address);
	return vec3(values.pFloats[0], values.pFloats[1], values.pFloats[2]);
}

vec2 LightingModelUv(LightingGeometry geometry, uint vertex, int setIndex)
{
	// Raster's selector maps every set above three to UV4.
	uint set = uint(clamp(setIndex, 0, 4));
	LightingModelFloats values = LightingModelFloats(geometry.uiAttributeAddress + uint64_t(vertex) * geometry.uiAttributeStride + 24ul + uint64_t(set) * 8ul);
	return vec2(values.pFloats[0], values.pFloats[1]);
}

vec3 LightingModelTransform(vec4 value, vec4 rows[3])
{
	return vec3(dot(value, rows[0]), dot(value, rows[1]), dot(value, rows[2]));
}

void LightingModelHit(LightingGeometry geometry, LightingInstance instance, uint primitive, vec2 barycentrics, out vec3 position, out vec3 geometricNormal, out vec3 shadingNormal, out vec2 colorUv, out vec2 physicalUv)
{
	uint first = geometry.uiFirstIndex + 3u * primitive;
	uvec3 indices = uvec3(LightingModelIndex(geometry, first), LightingModelIndex(geometry, first + 1u), LightingModelIndex(geometry, first + 2u));
	vec3 weights = vec3(1.0 - barycentrics.x - barycentrics.y, barycentrics);
	vec3 a = LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.x) * geometry.uiPositionStride);
	vec3 b = LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.y) * geometry.uiPositionStride);
	vec3 c = LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.z) * geometry.uiPositionStride);
	a = LightingModelTransform(vec4(a, 1.0), instance.pf4ObjectToWorld);
	b = LightingModelTransform(vec4(b, 1.0), instance.pf4ObjectToWorld);
	c = LightingModelTransform(vec4(c, 1.0), instance.pf4ObjectToWorld);
	position = weights.x * a + weights.y * b + weights.z * c;
	geometricNormal = normalize(cross(b - a, c - a));
	vec3 na = normalize(LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.x) * geometry.uiPositionStride + 12ul), 0.0), instance.pf4NormalToWorld));
	vec3 nb = normalize(LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.y) * geometry.uiPositionStride + 12ul), 0.0), instance.pf4NormalToWorld));
	vec3 nc = normalize(LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.z) * geometry.uiPositionStride + 12ul), 0.0), instance.pf4NormalToWorld));
	shadingNormal = normalize(weights.x * na + weights.y * nb + weights.z * nc);
	LightingModelMaterial material = LightingModelMaterials(geometry.uiMaterialAddress).pMaterials[geometry.uiMaterial];
	colorUv = weights.x * LightingModelUv(geometry, indices.x, material.iColorTextureSet) + weights.y * LightingModelUv(geometry, indices.y, material.iColorTextureSet) + weights.z * LightingModelUv(geometry, indices.z, material.iColorTextureSet);
	physicalUv = weights.x * LightingModelUv(geometry, indices.x, material.iPhysicalTextureSet) + weights.y * LightingModelUv(geometry, indices.y, material.iPhysicalTextureSet) + weights.z * LightingModelUv(geometry, indices.z, material.iPhysicalTextureSet);
}

// Callers use explicit LOD zero, repeat addressing, linearized color and linear physical samples; absent textures supply one.
vec3 LightingModelDiffuse(LightingModelMaterial material, vec3 linearColorSample, float physicalBlueSample)
{
	float metallic = clamp(material.fMetallicFactor * physicalBlueSample, 0.0, 1.0);
	return ModelDiffuse(material.f4BaseColorFactor.rgb * linearColorSample, metallic);
}

vec3 LightingModelMappedNormal(LightingGeometry geometry, LightingInstance instance, uint primitive, vec3 normal, vec2 normalXY)
{
	uint first = geometry.uiFirstIndex + 3u * primitive;
	uvec3 indices = uvec3(LightingModelIndex(geometry, first), LightingModelIndex(geometry, first + 1u), LightingModelIndex(geometry, first + 2u));
	vec3 a = LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.x) * geometry.uiPositionStride), 1.0f), instance.pf4ObjectToWorld);
	vec3 b = LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.y) * geometry.uiPositionStride), 1.0f), instance.pf4ObjectToWorld);
	vec3 c = LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.z) * geometry.uiPositionStride), 1.0f), instance.pf4ObjectToWorld);
	vec2 uvA = LightingModelUv(geometry, indices.x, 0);
	vec2 uvB = LightingModelUv(geometry, indices.y, 0);
	vec2 uvC = LightingModelUv(geometry, indices.z, 0);
	vec2 edgeB = uvB - uvA;
	vec2 edgeC = uvC - uvA;
	float determinant = edgeB.x * edgeC.y - edgeC.x * edgeB.y;
	if (abs(determinant) < 1e-8f) return normal;
	vec3 tangent = (edgeC.y * (b - a) - edgeB.y * (c - a)) / determinant;
	tangent -= normal * dot(normal, tangent);
	if (dot(tangent, tangent) < kfEpsilon * kfEpsilon) return normal;
	tangent = normalize(tangent);
	vec3 bitangent = cross(normal, tangent);
	return normalize(mat3(tangent, bitangent, normal) * vec3(normalXY, sqrt(clamp(1.0f - dot(normalXY, normalXY), 0.0f, 1.0f))));
}

#endif // LIGHTING_MODEL_HIT_H
