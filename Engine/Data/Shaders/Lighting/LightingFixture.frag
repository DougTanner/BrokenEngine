#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Lighting/LightingCompose.h"

layout(set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform { GlobalLayout globalLayout; };
layout(set = 0, binding = kiGlobalBindingMainUniform) uniform mainUniform { MainLayout mainLayout; };
layout(set = 1, binding = 2) uniform sampler2D shadowTextureSampler;
layout(set = 1, binding = 3) uniform sampler2D objectShadowsTextureSampler;
layout(set = 1, binding = 4) uniform sampler2D pSmokeSamplers[kiMaxFlightPlanes];

layout(location = 0) in vec3 f3InPosition;
layout(location = 1) in vec3 f3InNormal;
layout(location = 2) in vec3 f3InAlbedo;
layout(location = 3) in flat uint uiInInstance;

layout(location = 0) out vec4 f4OutColor;

void main()
{
	vec2 visibleAreaTexcoord = WorldToVisibleArea(f3InPosition, globalLayout.f4VisibleArea);
	float shadowMoon = SmokeShadow(globalLayout, f3InPosition, pSmokeSamplers, mainLayout.fSmokeShadowIntensity) * texture(objectShadowsTextureSampler, visibleAreaTexcoord).x;
	float shadowSun = shadowMoon * SampleTerrainShadow(shadowTextureSampler, WorldToVisibleArea(f3InPosition, globalLayout.f4ShadowArea));
	vec3 color = SunLighting(f3InAlbedo, globalLayout, vec4(f3InPosition, 1.0f), normalize(f3InNormal), shadowSun, shadowMoon, 1.0f);
	vec3 localDiffuse = LightingReferenceDiffuse();
	color += localDiffuse;
	for (int plane = 0; plane < int(globalLayout.uiFlightPlaneCount); ++plane)
	{
		vec2 smokeTexcoord = WorldToSmokeTexcoord(globalLayout.pf4SmokeAreas[plane], FlightPlanePosition(globalLayout, mainLayout, f3InPosition, plane));
		float smokeRaw = globalLayout.fSmokeMax * texture(pSmokeSamplers[plane], smokeTexcoord).x;
		float smokePow = clamp(pow(smokeRaw, globalLayout.fSmokePower), 0.0f, 1.0f);
		color = BlendSmokePrecomputed(color, smokePow, globalLayout);
	}
	f4OutColor = vec4(color, 1.0f);
}
