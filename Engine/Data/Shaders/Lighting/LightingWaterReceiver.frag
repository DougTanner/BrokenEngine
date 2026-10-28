#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"

layout(set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform { GlobalLayout globalLayout; };
layout(set = 1, binding = 4) uniform sampler2D elevationTextureSampler;
layout(set = 1, binding = 6) uniform sampler2D noiseTextureSampler;
layout(set = 1, binding = 8) uniform sampler2D depthLutSampler;
layout(set = 1, binding = kiWaterBindingFoam) uniform sampler2D foamTextureSampler;
layout(set = 1, binding = kiWaterBindingFoamColor) uniform sampler2D pFoamColorTextureSamplers[kiWaterFoamTextureCount];
layout(set = 1, binding = kiWaterBindingFoamBreakup) uniform sampler2D pFoamBreakupTextureSamplers[kiWaterFoamBreakupTextureCount];

layout(location = 0) in vec2 f2InInitialPosition;
layout(location = 1) in vec3 f3InPosition;
layout(location = 2) in vec2 f2InVisibleAreaTexcoord;
layout(location = 3) in vec3 f3InNormal;

#include "Water/WaterMaterial.h"
#include "Lighting/LightingReceiver.h"

void main()
{
	vec2 visibleAreaTexcoord = WorldToVisibleArea(f3InPosition, globalLayout.f4VisibleArea);
	float terrainElevation = texture(elevationTextureSampler, visibleAreaTexcoord).x - globalLayout.fWaterHeight;
	if (terrainElevation > globalLayout.fWaterEarlyOut)
	{
		discard;
		return;
	}
	vec2 waterOrigin = vec2(globalLayout.fWaterOriginX, globalLayout.fWaterOriginY);
	vec2 worldInitialPosition = f2InInitialPosition + waterOrigin;
	vec3 color;
	bool foamEnabled;
	vec3 foamColor;
	float foamShade;
	WaterMaterial(terrainElevation, waterOrigin, worldInitialPosition, color, foamEnabled, foamColor, foamShade);
	if (foamEnabled)
	{
		color = mix(color, foamColor, foamShade);
	}
	LightingReceiver(f3InPosition, f3InNormal, f3InNormal, color);
}
