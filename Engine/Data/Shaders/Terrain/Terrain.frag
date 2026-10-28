#version 460

#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Lighting/LightingCompose.h"
#include "Terrain/TerrainMaterial.h"

// Uniform buffers
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
// Bindless per-island arrays; each is indexed
// per-fragment by the flat-interpolated `uiInTextureSlot` forwarded from Terrain.vert.
layout (set = 1, binding = 5) uniform sampler2D colorTextureSamplers[kiMaxIslands];
layout (set = 1, binding = 6) uniform sampler2D normalTextureSamplers[kiMaxIslands];
layout (set = 1, binding = 7) uniform sampler2D ambientOcclusionTextureSamplers[kiMaxIslands];
layout (set = 1, binding = 8) uniform sampler2D pSmokeSamplers[kiMaxFlightPlanes];
layout (set = 1, binding = 9) uniform sampler2D rockSampler;
layout (set = 1, binding = 10) uniform sampler2D sandNormalsSampler0;
layout (set = 1, binding = 11) uniform sampler2D sandNormalsSampler1;
layout (set = 1, binding = 12) uniform sampler2D sandNormalsSampler2;
layout (set = 1, binding = 13) uniform sampler2D sandSampler;
layout (set = 1, binding = 14) uniform sampler2D rockNormalsSampler0;
layout (set = 1, binding = 15) uniform sampler2D rockNormalsSampler1;
layout (set = 1, binding = 16) uniform sampler2D rockNormalsSampler2;
// Per-island material masks (BC7 RGBA). R=Rock, G=Sand, B=Snow, A=Flow (reserved) — drives material
// detail blending. Bound at binding 18 — the SSBO at
// binding 17 is owned by Terrain.vert (AxisAlignedQuadLayout instance buffer).
layout (set = 1, binding = 18) uniform sampler2D masksTextureSamplers[kiMaxIslands];

// Input
layout (location = 0) in vec2 f2InVisibleAreaTexcoord;
layout (location = 1) in vec2 f2InIslandTexcoord;
layout (location = 2) in flat uint uiInTextureSlot;
layout (location = 3) in flat vec2 f2InRotationCosSin;

// Output
layout (location = 0) out vec4 f4OutColor;

void main()
{
	vec3 f3InPosition = vec3
	(
		VisibleAreaToWorld(f2InVisibleAreaTexcoord, globalLayout.f4VisibleArea),
		texture(elevationTextureSampler, f2InVisibleAreaTexcoord).x
	);

	// No terrain early-out discard here: the per-island Gaea Mesher mesh only draws where geometry
	// exists (including the underwater skirt), so a discard would black-out valid underwater content.
	// The mesh is cropped to the heightmap bbox at bake time (ProcessBakedRegion.cpp), so no fringe
	// overshoot survives to sample neighbouring composite content.

	vec3 f3Color;
	vec3 f3SunNormal;
	float fSnowPercent;
	TerrainMaterial(globalLayout, f3InPosition, f2InIslandTexcoord, f2InRotationCosSin,
	                colorTextureSamplers[nonuniformEXT(uiInTextureSlot)], normalTextureSamplers[nonuniformEXT(uiInTextureSlot)], masksTextureSamplers[nonuniformEXT(uiInTextureSlot)],
	                rockSampler, sandSampler, rockNormalsSampler0, rockNormalsSampler1, rockNormalsSampler2,
	                sandNormalsSampler0, sandNormalsSampler1, sandNormalsSampler2, f3Color, f3SunNormal, fSnowPercent);
	vec3 f3LocalDiffuse = LightingReferenceDiffuse();

	// Shadow with smoke at world position. Moon bypasses the terrain ray-march shadow only;
	// object shadows and smoke volumetric attenuation still apply to both lights.
	float fShadowMoon = SmokeShadow(globalLayout, f3InPosition, pSmokeSamplers, mainLayout.fSmokeShadowIntensity) * texture(objectShadowsTextureSampler, f2InVisibleAreaTexcoord).x;
	float fShadowSun  = fShadowMoon * SampleTerrainShadow(shadowTextureSampler, WorldToVisibleArea(f3InPosition, globalLayout.f4ShadowArea));
	// AO: sample the per-island occlusion and fold `fIslandAmbientOcclusion` into the SunLighting
	// occlusion factor below.
	float fAmbientOcclusionRaw = texture(ambientOcclusionTextureSamplers[nonuniformEXT(uiInTextureSlot)], f2InIslandTexcoord).x;
	// Snow pixels skip AO darkening so accumulated snow looks fresh / bright rather than crevice-shaded.
	float fAmbientOcclusionFactor = 1.0f - (1.0f - fSnowPercent * globalLayout.fTerrainSnowAmbientOcclusionExclusion) * globalLayout.fIslandAmbientOcclusion * (1.0f - fAmbientOcclusionRaw);
	f4OutColor = vec4(SunLighting(f3Color, globalLayout, vec4(f3InPosition, 1.0f), f3SunNormal, fShadowSun, fShadowMoon, fAmbientOcclusionFactor), 1.0f);
	f4OutColor.xyz += f3LocalDiffuse;

	// Sample each flight plane's smoke at that plane's eye-ray projected position, lowest first so the plane nearest the
	// eye blends last.
	for (int iPlane = 0; iPlane < int(globalLayout.uiFlightPlaneCount); ++iPlane)
	{
		vec2 f2SmokeTexcoord = WorldToSmokeTexcoord(globalLayout.pf4SmokeAreas[iPlane], FlightPlanePosition(globalLayout, mainLayout, f3InPosition, iPlane));
		float fSmokeRaw = globalLayout.fSmokeMax * texture(pSmokeSamplers[iPlane], f2SmokeTexcoord).x;
		float fSmokePow = clamp(pow(fSmokeRaw, globalLayout.fSmokePower), 0.0f, 1.0f);
		f4OutColor.xyz = BlendSmokePrecomputed(f4OutColor.xyz, fSmokePow, globalLayout);
	}
}
