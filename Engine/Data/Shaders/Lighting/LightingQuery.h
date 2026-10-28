#ifndef LIGHTING_QUERY_H
#define LIGHTING_QUERY_H

#extension GL_EXT_ray_query : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "Lighting/LightingModelHit.h"
#include "Lighting/LightingTerrainCommon.h"
#define LIGHTING_TERRAIN_RAY_MATERIAL
#include "Terrain/TerrainMaterial.h"
#undef LIGHTING_TERRAIN_RAY_MATERIAL

layout(set = 0, binding = kiGlobalBindingGlobalUniform) uniform lightingGlobalUniform { GlobalLayout globalLayout; };
layout(set = 0, binding = kiGlobalBindingSamplerRepeatModelData) uniform sampler lightingModelSampler;
layout(set = 0, binding = kiGlobalBindingSamplerRepeat) uniform sampler lightingColorSampler;
layout(set = 0, binding = kiGlobalBindingBindlessTextures) uniform texture2D lightingTextures[];
layout(set = 1, binding = 5) uniform sampler2D lightingTerrainColors[kiMaxIslands];
layout(set = 1, binding = 6) uniform sampler2D lightingTerrainNormals[kiMaxIslands];
layout(set = 1, binding = 7) uniform sampler2D lightingTerrainMasks[kiMaxIslands];
layout(set = 1, binding = 8) uniform sampler2D lightingRock;
layout(set = 1, binding = 9) uniform sampler2D lightingSand;
layout(set = 1, binding = 10) uniform sampler2D lightingRockNormal0;
layout(set = 1, binding = 11) uniform sampler2D lightingRockNormal1;
layout(set = 1, binding = 14) uniform sampler2D lightingRockNormal2;
layout(set = 1, binding = 15) uniform sampler2D lightingSandNormal0;
layout(set = 1, binding = 16) uniform sampler2D lightingSandNormal1;
layout(set = 1, binding = 17) uniform sampler2D lightingSandNormal2;
layout(scalar, set = 1, binding = kiLightingBindingSources) readonly buffer lightingSourcesBuffer { LightingSource lightingSources[]; };
layout(scalar, set = 1, binding = kiLightingBindingPublication) readonly buffer lightingPublicationBuffer { LightingPublication lightingPublication; };
layout(set = 1, binding = kiLightingBindingScene) uniform accelerationStructureEXT lightingScene;
layout(scalar, set = 1, binding = kiLightingBindingInstances) readonly buffer lightingInstancesBuffer { LightingInstance lightingInstances[]; };
layout(scalar, set = 1, binding = kiLightingBindingGeometry) readonly buffer lightingGeometryBuffer { LightingGeometry lightingGeometries[]; };
layout(scalar, set = 1, binding = kiLightingBindingMaterials) readonly buffer lightingFixtureMaterialsBuffer { vec4 lightingFixtureAlbedos[]; };
layout(scalar, set = 1, binding = kiLightingBindingSamples) readonly buffer lightingSamplesBuffer { vec2 lightingSamples[]; };
layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer LightingTerrainMaterials { LightingTerrainMaterial pMaterials[]; };

bool LightingOccluded(vec3 origin, vec3 direction, float tMin, float tMax)
{
	if (lightingPublication.uiInstanceCount == 0u || tMax <= tMin) return false;
	rayQueryEXT query;
	rayQueryInitializeEXT(query, lightingScene, gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT, 0xffu, origin, tMin, direction, tMax);
	while (rayQueryProceedEXT(query)) {}
	return rayQueryGetIntersectionTypeEXT(query, true) == gl_RayQueryCommittedIntersectionTriangleEXT;
}

bool LightingClosestHit(vec3 origin, vec3 direction, float tMin, float tMax, out LightingHit hit)
{
	if (lightingPublication.uiInstanceCount == 0u || tMax <= tMin) return false;
	rayQueryEXT query;
	rayQueryInitializeEXT(query, lightingScene, gl_RayFlagsOpaqueEXT, 0xffu, origin, tMin, direction, tMax);
	while (rayQueryProceedEXT(query)) {}
	if (rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionTriangleEXT) return false;
	hit.uiInstance = rayQueryGetIntersectionInstanceCustomIndexEXT(query, true);
	hit.uiPrimitive = rayQueryGetIntersectionPrimitiveIndexEXT(query, true);
	hit.f2Barycentrics = rayQueryGetIntersectionBarycentricsEXT(query, true);
	hit.fDistance = rayQueryGetIntersectionTEXT(query, true);
	LightingInstance instance = lightingInstances[hit.uiInstance];
	LightingGeometry geometry = lightingGeometries[instance.uiFirstGeometry + rayQueryGetIntersectionGeometryIndexEXT(query, true)];
	hit.uiMaterial = geometry.uiMaterial;
	if (geometry.uiKind == kiLightingGeometryModel)
	{
		vec2 colorUv, physicalUv;
		LightingModelHit(geometry, instance, hit.uiPrimitive, hit.f2Barycentrics, hit.f3Position, hit.f3GeometricNormal, hit.f3ShadingNormal, colorUv, physicalUv);
		if (instance.uiPadding == 1u)
		{
			hit.f3Albedo = vec3(0.5f);
		}
		else
		{
			LightingModelMaterial material = LightingModelMaterials(geometry.uiMaterialAddress).pMaterials[geometry.uiMaterial];
			vec3 color = material.iColorTextureSet < 0 ? vec3(1.0f) : pow(textureLod(sampler2D(lightingTextures[nonuniformEXT(material.uiColorTexture)], lightingColorSampler), colorUv, 0.0f).rgb, vec3(2.2f));
			float physical = material.iPhysicalTextureSet < 0 ? 1.0f : textureLod(sampler2D(lightingTextures[nonuniformEXT(material.uiPhysicalTexture)], lightingModelSampler), physicalUv, 0.0f).b;
			hit.f3Albedo = LightingModelDiffuse(material, color, physical);
			if (material.iNormalTextureSet >= 0)
			{
				uint first = geometry.uiFirstIndex + 3u * hit.uiPrimitive;
				vec3 weights = vec3(1.0f - hit.f2Barycentrics.x - hit.f2Barycentrics.y, hit.f2Barycentrics);
				vec2 normalUv = vec2(0.0f);
				for (uint i = 0u; i < 3u; ++i) normalUv += weights[i] * LightingModelUv(geometry, LightingModelIndex(geometry, first + i), material.iNormalTextureSet);
				vec2 normalXY = 2.0f * textureLod(sampler2D(lightingTextures[nonuniformEXT(material.uiNormalTexture)], lightingModelSampler), normalUv, 0.0f).rg - 1.0f;
				hit.f3ShadingNormal = LightingModelMappedNormal(geometry, instance, hit.uiPrimitive, hit.f3ShadingNormal, normalXY);
			}
		}
	}
	else
	{
		uint first = geometry.uiFirstIndex + 3u * hit.uiPrimitive;
		uvec3 indices = uvec3(LightingModelIndex(geometry, first), LightingModelIndex(geometry, first + 1u), LightingModelIndex(geometry, first + 2u));
		vec3 weights = vec3(1.0f - hit.f2Barycentrics.x - hit.f2Barycentrics.y, hit.f2Barycentrics);
		vec3 a = LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.x) * geometry.uiPositionStride), 1.0f), instance.pf4ObjectToWorld);
		vec3 b = LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.y) * geometry.uiPositionStride), 1.0f), instance.pf4ObjectToWorld);
		vec3 c = LightingModelTransform(vec4(LightingModelFloat3(geometry.uiPositionAddress + uint64_t(indices.z) * geometry.uiPositionStride), 1.0f), instance.pf4ObjectToWorld);
		hit.f3Position = weights.x * a + weights.y * b + weights.z * c;
		hit.f3GeometricNormal = normalize(cross(b - a, c - a));
		hit.f3ShadingNormal = hit.f3GeometricNormal;
		if (geometry.uiKind == kiLightingGeometryFixture)
		{
			hit.f3Albedo = lightingFixtureAlbedos[geometry.uiMaterial].rgb;
		}
		else
		{
			LightingTerrainMaterial material = LightingTerrainMaterials(geometry.uiMaterialAddress).pMaterials[geometry.uiMaterial];
			vec2 local = vec2(0.0f);
			for (uint i = 0u; i < 3u; ++i)
			{
				LightingModelFloats attributes = LightingModelFloats(geometry.uiAttributeAddress + uint64_t(indices[i]) * geometry.uiAttributeStride);
				local += weights[i] * vec2(attributes.pFloats[0], attributes.pFloats[1]);
			}
			vec2 uv = vec2(0.5f) + local / material.f2Footprint;
			float snow;
			TerrainMaterial(globalLayout, hit.f3Position, uv, vec2(cos(material.fRotation), sin(material.fRotation)),
			                lightingTerrainColors[nonuniformEXT(material.uiTextureSlot)], lightingTerrainNormals[nonuniformEXT(material.uiTextureSlot)], lightingTerrainMasks[nonuniformEXT(material.uiTextureSlot)],
			                lightingRock, lightingSand, lightingRockNormal0, lightingRockNormal1, lightingRockNormal2,
			                lightingSandNormal0, lightingSandNormal1, lightingSandNormal2, hit.f3Albedo, hit.f3ShadingNormal, snow);
		}
	}
	if (dot(hit.f3GeometricNormal, direction) > 0.0f) hit.f3GeometricNormal = -hit.f3GeometricNormal;
	if (dot(hit.f3ShadingNormal, hit.f3GeometricNormal) < 0.0f) hit.f3ShadingNormal = -hit.f3ShadingNormal;
	return true;
}

vec3 LightingRayOrigin(vec3 position, vec3 geometricNormal, vec3 outgoing)
{
	return position + (dot(geometricNormal, outgoing) < 0.0f ? -kfLightingRayOffset : kfLightingRayOffset) * geometricNormal;
}

#endif // LIGHTING_QUERY_H
