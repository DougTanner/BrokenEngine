#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Lighting/LightingCompose.h"

// Debug toggles for lighting contributions
#define ENABLE_BRDF 1
#define ENABLE_IBL 1
#define ENABLE_EMISSIVE 1

#define ENABLE_SMOKE 1

// Push constants
layout(push_constant) uniform pushConstants
{
	PushConstantsLayout pushConstantsLayout;
};

// Uniforms
layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
	GlobalLayout globalLayout;
};

layout (set = 0, binding = kiGlobalBindingMainUniform) uniform mainUniform
{
	MainLayout mainLayout;
};

layout (scalar, set = 1, binding = 2) buffer readonly modelsUniform
{
	ModelLayout pModels[];
};

// Bindless texture array
layout (set = 0, binding = kiGlobalBindingSamplerRepeatModelData) uniform sampler samplerRepeatModelData;
layout (set = 0, binding = kiGlobalBindingSamplerRepeat) uniform sampler samplerRepeat;
layout (set = 0, binding = kiGlobalBindingBindlessTextures) uniform texture2D pTextures[];

// IBL textures
layout (set = 1, binding = 5) uniform samplerCube samplerIrradiance;
layout (set = 1, binding = 6) uniform samplerCube prefilteredMap;
layout (set = 1, binding = 7) uniform sampler2D samplerBRDFLUT;

// Material buffer (per-material, Set 2)
layout (scalar, set = 2, binding = 8) buffer readonly pbrMaterialsUniform
{
	PbrMaterialLayout pMaterials[];
};

// Engine-specific textures
layout (set = 1, binding = 9) uniform sampler2D shadowTextureSampler;
layout (set = 1, binding = 10) uniform sampler2D pSmokeSamplers[kiMaxFlightPlanes];

// Vertex inputs
layout (location = 0) in vec3 f3InWorldPosition;
layout (location = 1) in vec3 f3InNormal;
layout (location = 2) in vec2 f2InUV;
layout (location = 3) in vec2 f2InUV1;
layout (location = 4) in vec2 f2InUV2;
layout (location = 5) in vec2 f2InUV3;
layout (location = 6) in vec2 f2InUV4;
layout (location = 7) in vec4 f4InColorAdd;
layout (location = 8) in float fInHeightAboveBase;

// Fragment output
layout (location = 0) out vec4 f4OutColor;

// Constants
const float kfIblSpecularMax = 16.0; // Firefly clamp: prefiltered sun-disc texels reach F16 max at sharp mips

#include "Model/ModelMaterial.h"

// GGX/Trowbridge-Reitz normal distribution function
float D_GGX(float NdotH, float alphaRoughness)
{
	float a2 = alphaRoughness * alphaRoughness;
	float f = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
	return a2 / (kfPi * f * f);
}

// Schlick Fresnel approximation
vec3 F_Schlick(float VdotH, vec3 F0, vec3 F90)
{
	float x = clamp(1.0 - VdotH, 0.0, 1.0);
	float x2 = x * x;
	return F0 + (F90 - F0) * (x2 * x2 * x);
}

// Smith-GGX geometry visibility function (separable form)
float V_SmithGGXCorrelated(float NdotL, float NdotV, float alphaRoughness)
{
	float a2 = alphaRoughness * alphaRoughness;
	float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
	float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
	float GGX = GGXV + GGXL;
	return GGX > 0.0 ? 0.5 / GGX : 0.0;
}

// Lambertian diffuse BRDF
vec3 DiffuseLambert(vec3 diffuseColor)
{
	return diffuseColor / kfPi;
}

// Convert cubemap coordinates from Z-up engine space to Y-up cubemap space
vec3 ToCubemapCoord(vec3 worldNormal)
{
	return vec3(worldNormal.x, worldNormal.z, worldNormal.y);
}

// IBL contribution using split-sum approximation
void GetIBLContribution(float NdotV, float perceptualRoughness, vec3 diffuseColor, vec3 specularColor,
	                    vec3 n, vec3 reflection, out vec3 f3Diffuse, out vec3 f3Specular)
{
	float lod = pow(perceptualRoughness, mainLayout.fPhysicallyBasedRenderingCubemapLevelOfDetailPower) * mainLayout.fPhysicallyBasedRenderingMipmapCount
	            + perceptualRoughness * mainLayout.fPhysicallyBasedRenderingCubemapLevelOfDetailOffset;
	vec3 brdf = texture(samplerBRDFLUT, vec2(NdotV, 1.0 - perceptualRoughness)).rgb;
	vec3 diffuseLight = texture(samplerIrradiance, ToCubemapCoord(n)).rgb;
	vec3 specularLight = min(textureLod(prefilteredMap, ToCubemapCoord(reflection), lod).rgb, vec3(kfIblSpecularMax));
	f3Diffuse = diffuseLight * diffuseColor;
	f3Specular = specularLight * (specularColor * brdf.x + brdf.y);
}

void main()
{
	PbrMaterialLayout material = pMaterials[int32_t(pushConstantsLayout.f4Pipeline.w)];
#ifdef LIGHTING_NEUTRAL_MODEL
	material.f4BaseColorFactor = vec4(vec3(0.5f) / (vec3(1.0f) - kModelDielectricReflectance), 1.0f);
	material.fMetallicFactor = 0.0f;
	material.iColorTextureSet = -1;
	material.iNormalTextureSet = -1;
#endif

	vec4 baseColor;
	float metallic;
	float perceptualRoughness;
	float alphaRoughness;
	vec3 f0;
	vec3 diffuseColor;
	ModelMaterial(material, baseColor, metallic, perceptualRoughness, alphaRoughness, f0, diffuseColor);

	// Specular color: F0 for dielectrics, baseColor for metals
	vec3 specularColor = mix(f0, baseColor.rgb, metallic);

	// Compute vectors
	vec3 n = GetNormal(material);
	vec3 v = normalize(mainLayout.f4ModelEyePosition.xyz - f3InWorldPosition);
	// GlobalUniforms.cpp normalizes f4ModelSunMoonNormal before this shader uses it, so l is unit.
	vec3 l = globalLayout.f4ModelSunMoonNormal.xyz;
	vec3 h = normalize(l + v);
	vec3 reflection = -reflect(v, n);

	// Compute dot products
	float NdotL = clamp(dot(n, l), 0.001, 1.0);
	float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
	float NdotH = clamp(dot(n, h), 0.0, 1.0);
	float LdotH = clamp(dot(l, h), 0.0, 1.0);
	float VdotH = clamp(dot(v, h), 0.0, 1.0);

	// Reflectance at grazing angle
	vec3 reflectance90 = vec3(clamp(max(max(specularColor.r, specularColor.g), specularColor.b) * 25.0, 0.0, 1.0));

	// Sample ambient occlusion
	float fEffectiveAo = 1.0;
	if (material.iOcclusionTextureSet > -1)
	{
		float fAo = texture(sampler2D(pTextures[nonuniformEXT(int(material.fOcclusionTextureIndex))], samplerRepeat), getUV(material.iOcclusionTextureSet)).r;
		fEffectiveAo = 1.0 + material.fOcclusionStrength * (fAo - 1.0);
	}

	// Engine-specific lighting. Sun and moon split: moon bypasses shadow attenuation (Model.frag samples only
	// the terrain shadow texture), sun still receives it. The sun/moon color terms are precomputed CPU-side
	// (GlobalUniforms.cpp PopulateDayCycleColors): the per-target Objects intensity and fPhysicallyBasedRenderingSun fold into
	// f4PbrSun/MoonColorObjects, and the IBL specular path multiplies in the Rec.709 luminance / fPhysicallyBasedRenderingDayBrightness
	// via the separate f4PbrSun/MoonColorObjectsIbl. Kept as two fields so the IBL path stays linear in fPhysicallyBasedRenderingSun
	// (its luminance comes from the UNSCALED color; folding fPhysicallyBasedRenderingSun into the luminance would compound to fPhysicallyBasedRenderingSun^3).
	vec3 f3AmbientColor = globalLayout.f4AmbientColor.rgb;

	vec2 f2VisibleAreaPosition = WorldToVisibleArea(f3InWorldPosition, globalLayout.f4VisibleArea);
	vec2 f2ShadowPosition = WorldToVisibleArea(f3InWorldPosition, globalLayout.f4ShadowArea);
	float fShadow = max(mainLayout.fPhysicallyBasedRenderingShadowFloor, SampleTerrainShadow(shadowTextureSampler, f2ShadowPosition));
	// The terrain shadow lies on the ground, so a ship climbing above the lowest flight plane fades out of it.
	fShadow = mix(fShadow, 1.0f, clamp(fInHeightAboveBase * globalLayout.fObjectShadowsTerrainShadowFadeInverse, 0.0f, 1.0f));
	const float fShadowMoon = 1.0; // moon bypasses terrain shadow; Model.frag has no other shadow inputs

	// Accumulate lighting
	vec3 color = vec3(0.0);

	// Cook-Torrance microfacet BRDF for direct sun lighting
	// Combines three terms: F (Fresnel), D (Distribution), V (Visibility)
	// - F: Surface reflectivity increases at grazing angles (Schlick approximation)
	// - D: Microfacet normal distribution controlling highlight shape (GGX/Trowbridge-Reitz)
	// - V: Self-shadowing between microfacets based on roughness (Smith-GGX)
#if ENABLE_BRDF
	vec3 F = F_Schlick(VdotH, specularColor, reflectance90);
	float D = D_GGX(NdotH, alphaRoughness);
	float V = V_SmithGGXCorrelated(NdotL, NdotV, alphaRoughness);
	vec3 diffuseContrib = (1.0 - F) * DiffuseLambert(diffuseColor);
	vec3 specularContrib = F * D * V;
	vec3 diffuseResult = pow(mainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffuse * diffuseContrib, vec3(mainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionDiffusePower));
	vec3 specularResult = pow(mainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecular * specularContrib, vec3(mainLayout.fPhysicallyBasedRenderingBidirectionalReflectanceDistributionFunctionSpecularPower));
	vec3 brdf = NdotL * (diffuseResult + specularResult);
	color += max(globalLayout.f4PhysicallyBasedRenderingSunColorObjects.rgb  * fShadow * fShadow,
	             globalLayout.f4PhysicallyBasedRenderingMoonColorObjects.rgb * fShadowMoon * fShadowMoon) * brdf;
#endif

	// Image based lighting
#if ENABLE_IBL
	vec3 f3IblDiffuse;
	vec3 f3IblSpecular;
	GetIBLContribution(NdotV, perceptualRoughness, diffuseColor, specularColor, n, reflection, f3IblDiffuse, f3IblSpecular);
	f3IblDiffuse *= mainLayout.fPhysicallyBasedRenderingAmbient * f3AmbientColor * mix(1.0, fShadow, mainLayout.fPhysicallyBasedRenderingImageBasedLightingShadowBlend);
	f3IblSpecular *= max(globalLayout.f4PhysicallyBasedRenderingSunColorObjectsImageBasedLighting.rgb  * fShadow,
	                     globalLayout.f4PhysicallyBasedRenderingMoonColorObjectsImageBasedLighting.rgb * fShadowMoon);
	vec3 f3IblDiffuseResult = pow(mainLayout.fPhysicallyBasedRenderingImageBasedLightingDiffuse * f3IblDiffuse, vec3(mainLayout.fPhysicallyBasedRenderingImageBasedLightingDiffusePower));
	vec3 f3IblSpecularResult = pow(mainLayout.fPhysicallyBasedRenderingImageBasedLightingSpecular * f3IblSpecular, vec3(mainLayout.fPhysicallyBasedRenderingImageBasedLightingSpecularPower));
	color += fEffectiveAo * (f3IblDiffuseResult + f3IblSpecularResult);
#endif

	vec3 f3LocalDiffuse = material.fAlphaMask >= 2.0 ? vec3(0.0) : LightingReferenceDiffuse();
	color += f3LocalDiffuse;

	// Add emissive
#if ENABLE_EMISSIVE
	vec3 emissive = material.f3EmissiveFactor;
	if (material.iEmissiveTextureSet > -1)
	{
		emissive *= SRGBtoLinear(texture(sampler2D(pTextures[nonuniformEXT(int(material.fEmissiveTextureIndex))], samplerRepeat), getUV(material.iEmissiveTextureSet)).rgb);
	}
	color += mainLayout.fPhysicallyBasedRenderingEmissive * emissive;
#endif

	// Apply smoke/fog per flight plane, lowest first (project position to the plane, fade with height above it; planes
	// above the fragment apply in full)
#if ENABLE_SMOKE
	for (int iPlane = 0; iPlane < int(globalLayout.uiFlightPlaneCount); ++iPlane)
	{
		vec2 f2PositionAtPlane = FlightPlanePosition(globalLayout, mainLayout, f3InWorldPosition, iPlane);
		float fHeightFraction = clamp((f3InWorldPosition.z - globalLayout.pfFlightPlaneHeights[iPlane]) * globalLayout.fSmokeObjectHeightInverse, 0.0f, 1.0f);
		float fSmokeFade = 1.0f - fHeightFraction * fHeightFraction;
		color = AddSmoke(globalLayout, color, f2PositionAtPlane, pSmokeSamplers[iPlane], iPlane, fSmokeFade * mainLayout.fPhysicallyBasedRenderingSmoke);
	}
#endif

	// Output color
	f4OutColor = vec4(color, baseColor.a);

	// Per-instance color add (for effects like damage flash)
	f4OutColor += f4InColorAdd;
}
