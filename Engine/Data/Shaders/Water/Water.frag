#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Lighting/LightingCompose.h"

// WATER_SPEC_AA_MODE selects compile-time filtering of sub-pixel high-power skybox lobes; re-run DataPacker after
// define edits, as for DT_LIGHTING_ONLY. 0: pointwise reference; 1: closed-form box integral of s^p over the pixel
// footprint; 2: Kaplanyan/Tokuyoshi NDF variance widening via Vlachos; 3: Toksvig variance from weighted normal-sum
// agreement; 4: 2x2 ALU supersampling at derivative-extrapolated normals. fWaterSpecularAntialiasingVariance controls modes 1-3
// and fWaterSpecularAntialiasingThreshold modes 2-3. Analytic filtering needs neither MSAA sample shading nor a prepass.
#define WATER_SPEC_AA_MODE 3

// Zoom/minification handoff for modes 2-3: adds DataPacker-baked per-mip Toksvig variance — the
// normal variance the BC5 mip chain averages away, invisible to the screen-space kernels above
// because DecodeNormal re-unitizes every fetch — into the lobe kernel via an analytic per-octave
// LOD (Bruneton-style geometry->BRDF transition). Fixes camera-zoom specular flicker.
// Runtime sliders: fWaterSpecularAntialiasingMipmapScale scales the term; fWaterNormalMipmapBias biases the analytic
// LOD in lockstep with the water-normal sampler's LOD bias.
#define WATER_SPEC_AA_MIP_HANDOFF 1
// Also hand off the variance the camera-height weight fade removes, referenced to the near-camera
// full-weight look — far water keeps its statistical roughness instead of flattening to gloss.
#define WATER_SPEC_AA_FADE_HANDOFF 0

// Uniforms
layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
	GlobalLayout globalLayout;
};

layout (set = 0, binding = kiGlobalBindingMainUniform) uniform mainUniform
{
	MainLayout mainLayout;
};

layout (set = 1, binding = 2) uniform sampler2D shadowTextureSampler;
layout (set = 1, binding = 3) uniform sampler2D objectShadowsTextureSampler;
layout (set = 1, binding = 4) uniform sampler2D elevationTextureSampler;
layout (set = 1, binding = 5) uniform samplerCube skyboxSampler;
layout (set = 1, binding = 6) uniform sampler2D noiseTextureSampler;
layout (set = 1, binding = 7) uniform sampler2D pWaterNormalSamplers[kiWaterNormalCount];
layout (set = 1, binding = 8) uniform sampler2D depthLutSampler;
layout (set = 1, binding = 9) uniform sampler2D pSmokeSamplers[kiMaxFlightPlanes];
layout (set = 1, binding = kiWaterBindingFoam) uniform sampler2D foamTextureSampler;
layout (set = 1, binding = kiWaterBindingFoamColor) uniform sampler2D pFoamColorTextureSamplers[kiWaterFoamTextureCount];
layout (set = 1, binding = kiWaterBindingFoamBreakup) uniform sampler2D pFoamBreakupTextureSamplers[kiWaterFoamBreakupTextureCount];

// Input
layout (location = 0) in vec2 f2InInitialPosition;
layout (location = 1) in vec3 f3InPosition;
layout (location = 2) in vec2 f2InVisibleAreaTexcoord;
layout (location = 3) in vec3 f3InNormal;

// Output
layout (location = 0) out vec4 f4OutColor;

#include "WaterNormalSampling.h"
#include "WaterSpecular.h"
#include "Water/WaterMaterial.h"

void main()
{
	vec2 f2VisibleAreaTexcoord = WorldToVisibleArea(f3InPosition, globalLayout.f4VisibleArea);

	float fTerrainElevation = texture(elevationTextureSampler, f2VisibleAreaTexcoord).x - globalLayout.fWaterHeight;
	if (fTerrainElevation > globalLayout.fWaterEarlyOut)
	{
		f4OutColor = vec4(0.0f, 0.0f, 0.0f, 0.0f);
		discard;
		return;
	}

	// Camera-relative to world-space reconstruction
	vec2 f2WaterOrigin = vec2(globalLayout.fWaterOriginX, globalLayout.fWaterOriginY);
	vec2 f2WorldInitialPosition = f2InInitialPosition + f2WaterOrigin;

	// Bias eye normal up a bit for skybox. Compute eye-to-water in camera-relative space rather than
	// reconstructing world position and subtracting f4WaterEyePosition: at large camera coords, the world
	// round-trip drops low bits from f2InInitialPosition (small + large then subtract large), and the
	// lost bits shift with camera motion — visible as a velocity-correlated specular darkening at
	// intermediate fLightingWaterSkyboxNormalSoften. Top-down camera: f4WaterEyePosition.xy == fWaterOrigin
	// by construction (CameraBase.cpp sets it directly above the look-at), so eye - waterWorld = -f2InInitial.
	vec3 f3ToEyeNormal = normalize(vec3(-f2InInitialPosition, mainLayout.f4WaterEyePosition.z));
	f3ToEyeNormal = normalize(mix(f3ToEyeNormal, mainLayout.f4ToEyeNormal.xyz, globalLayout.fLightingWaterSkyboxNormalSoften));

	WaterNormalSamplingResult normalResult = SampleWaterNormals();

	vec3 f3PreLightingColor;
	bool bFoamEnabled;
	vec3 f3FoamColor;
	float fFoamShade;
	WaterMaterial(fTerrainElevation, f2WaterOrigin, f2WorldInitialPosition, f3PreLightingColor, bFoamEnabled, f3FoamColor, fFoamShade);

	float fDirectionalLighting = max(1.0f - globalLayout.fWaterDirectional, dot(f3InNormal, globalLayout.f4SunMoonNormal.xyz));
	vec3 f3DirectionalLighting = f3PreLightingColor * fDirectionalLighting;
	vec3 f3LightingColor = mix(f3PreLightingColor, f3DirectionalLighting, 0.75f);
	// GlobalUniforms.cpp folds max-combined water sun/moon color plus /3 sun and ambient scalars from uniform inputs;
	// at noon f4MoonColor is near zero. Separate scalars keep fShadowAffectAmbient on ambient while f3SunOrMoon
	// modulates skybox color.
	vec3 f3SunOrMoon = globalLayout.f4WaterSunOrMoon.xyz;
	float fSunScalar     = globalLayout.fWaterSunScalar;
	float fAmbientScalar = globalLayout.fWaterAmbientScalar;

	WaterSpecularResult specularResult = ComposeWaterSpecular(normalResult, f3ToEyeNormal, f3InNormal, f3SunOrMoon, fTerrainElevation);
	vec3 f3SkyboxColor = specularResult.f3SkyboxColor;
	vec3 f3SkyboxColorSun = specularResult.f3SkyboxColorSun;
	float fReflectionTerrainMultiplier = specularResult.fReflectionTerrainMultiplier;
	float fReflection = specularResult.fReflection;

	// Factor the skybox combine so Height Darken can weight base color and skybox specular independently:
	// mix(A, B, t) + add*t*B = (1-t)*A + (1+add)*t*B → base = (1-fReflection)*f3LightingColor, specular = (1+fSkyboxAdd)*fReflection*f3SkyboxColorSun.
	float fSkyboxAdd = mainLayout.fLightingWaterSkyboxAdd;
	vec3 f3SkyboxSpecular;
	// fReflection is unbounded (~2.5 at defaults at highlight peaks); clamp only this attenuation use so
	// (1-fReflection) can't go negative and drive negative RGB into the F16 target (UNORM used to clamp this).
	if (bFoamEnabled)
	{
		vec3 f3FoamLightingColor = mix(f3FoamColor, f3FoamColor * fDirectionalLighting, 0.75f);
		f3SkyboxSpecular = (1.0f - fFoamShade) * (1.0f + fSkyboxAdd) * fReflection * f3SkyboxColorSun;
		f3LightingColor = mix((1.0f - min(fReflection, 1.0f)) * f3LightingColor, f3FoamLightingColor, fFoamShade);
		f3PreLightingColor = mix(f3PreLightingColor, f3FoamColor, fFoamShade);
	}
	else
	{
		f3SkyboxSpecular = (1.0f + fSkyboxAdd) * fReflection * f3SkyboxColorSun;
		f3LightingColor = (1.0f - min(fReflection, 1.0f)) * f3LightingColor;
	}

	// Wave trough darken: at z >= Top no darkening (multiplier 1.0); at z <= Bottom max darkening (multiplier 1.0 - Target).
	// Source/Lighting weights independently mix the per-path multiplier toward 1.0 so each contribution can opt in/out.
	// Lighting weights the skybox specular.
	float fHeightT = clamp((f3InPosition.z - mainLayout.fWaterHeightDarkenBottom) * mainLayout.fWaterHeightDarkenRangeInverse, 0.0f, 1.0f);
	float fHeightDarken = mix(1.0f - mainLayout.fWaterHeightDarkenTarget, 1.0f, fHeightT);
	float fHeightDarkenSource = mix(1.0f, fHeightDarken, mainLayout.fWaterHeightDarkenSource);
	float fHeightDarkenLighting = mix(1.0f, fHeightDarken, mainLayout.fWaterHeightDarkenLighting);
	vec3 f3BaseDarkened           = fHeightDarkenSource   * f3LightingColor;
	vec3 f3SkyboxSpecularDarkened = fHeightDarkenLighting * f3SkyboxSpecular;

	// Shadow with smoke at world position. Moon bypasses the terrain ray-march shadow only;
	// object shadows + smoke volumetric attenuation still apply to both lights. Use a scalar
	// luminance-weighted blend of the two shadow values rather than a per-channel split: the
	// f3SkyboxColor mix above breaks pure linearity in (Sun + Moon), so a per-channel divide
	// would zero entire channels when Sun.c + Moon.c happens to be ~0 (e.g. morning sun has B=0).
	float fShadowMoon = SmokeShadow(globalLayout, f3InPosition, pSmokeSamplers, mainLayout.fSmokeShadowIntensity) * texture(objectShadowsTextureSampler, f2InVisibleAreaTexcoord).x;
	float fShadowSun  = fShadowMoon * SampleTerrainShadow(shadowTextureSampler, WorldToVisibleArea(f3InPosition, globalLayout.f4ShadowArea));
	// Rec.601 sun/moon weights and their guarded reciprocal-sum are folded CPU-side (GlobalUniforms.cpp).
	float fEffectiveShadow = (fShadowSun * globalLayout.fWaterSunWeight + fShadowMoon * globalLayout.fWaterMoonWeight) * globalLayout.fWaterShadowWeightSumInverse;
	// fShadowAffectAmbient relaxes shadow on the sky-ambient half only; sun + skybox specular keep full shadow.
	// Mirrors SunLighting() at ShaderFunctions.h:76-80, reusing fEffectiveShadow as its fAmbientShadow (identical Rec.601-weighted formula).
	float fAmbientShadowApplied = mix(1.0f, fEffectiveShadow, globalLayout.fShadowAffectAmbient);
	vec3 f3SunContribution     = fEffectiveShadow      * (fSunScalar     * f3BaseDarkened + f3SkyboxSpecularDarkened);
	vec3 f3AmbientContribution = fAmbientShadowApplied *  fAmbientScalar * f3BaseDarkened;
	f4OutColor.xyz = f3SunContribution + f3AmbientContribution;
	// Cubemap-colored 0.5 ambient floor lifts only darker shadowed water; componentwise max preserves
	// brighter lighting/highlights, while fAmbientShadowApplied keeps Affect Ambient control.
	f4OutColor.xyz = max(f4OutColor.xyz, 0.5f * fAmbientShadowApplied * globalLayout.f4AmbientColor.xyz * f3SkyboxColor);

	// Terrain elevation (for water transparency)
	f4OutColor.w = clamp(-fTerrainElevation * globalLayout.fWaterTerrainFadeInverse, globalLayout.fWaterTerrainFadeClamp, 1.0f);

	vec3 f3LocalDiffuse = LightingReferenceDiffuse();
	f4OutColor.xyz += f3LocalDiffuse;

	// Additive smoke per flight plane, lowest first.
	for (int iPlane = 0; iPlane < int(globalLayout.uiFlightPlaneCount); ++iPlane)
	{
		vec2 f2SmokeTexcoord = WorldToSmokeTexcoord(globalLayout.pf4SmokeAreas[iPlane], FlightPlanePosition(globalLayout, mainLayout, vec3(f2WorldInitialPosition, 0.0f), iPlane));
		float fSmokeRaw = globalLayout.fSmokeMax * texture(pSmokeSamplers[iPlane], f2SmokeTexcoord).x;
		float fSmokePow = clamp(pow(fSmokeRaw, globalLayout.fSmokePower), 0.0f, 1.0f);
		f4OutColor.xyz = BlendSmokePrecomputed(f4OutColor.xyz, fSmokePow, globalLayout);
	}
}
