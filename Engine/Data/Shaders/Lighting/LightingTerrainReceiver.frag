#version 460

#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"
#include "Terrain/TerrainMaterial.h"

layout(set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform { GlobalLayout globalLayout; };
layout(set = 1, binding = 4) uniform sampler2D elevationTextureSampler;
layout(set = 1, binding = 5) uniform sampler2D colorTextureSamplers[kiMaxIslands];
layout(set = 1, binding = 6) uniform sampler2D normalTextureSamplers[kiMaxIslands];
layout(set = 1, binding = 9) uniform sampler2D rockSampler;
layout(set = 1, binding = 10) uniform sampler2D sandNormalsSampler0;
layout(set = 1, binding = 11) uniform sampler2D sandNormalsSampler1;
layout(set = 1, binding = 12) uniform sampler2D sandNormalsSampler2;
layout(set = 1, binding = 13) uniform sampler2D sandSampler;
layout(set = 1, binding = 14) uniform sampler2D rockNormalsSampler0;
layout(set = 1, binding = 15) uniform sampler2D rockNormalsSampler1;
layout(set = 1, binding = 16) uniform sampler2D rockNormalsSampler2;
layout(set = 1, binding = 18) uniform sampler2D masksTextureSamplers[kiMaxIslands];

layout(location = 0) in vec2 f2InVisibleAreaTexcoord;
layout(location = 1) in vec2 f2InIslandTexcoord;
layout(location = 2) in flat uint uiInTextureSlot;
layout(location = 3) in flat vec2 f2InRotationCosSin;
layout(location = 4) in vec3 f3InRasterPosition;

#include "Lighting/LightingReceiver.h"

void main()
{
	vec3 materialPosition = vec3(VisibleAreaToWorld(f2InVisibleAreaTexcoord, globalLayout.f4VisibleArea),
	                             texture(elevationTextureSampler, f2InVisibleAreaTexcoord).x);
	vec3 color;
	vec3 normal;
	float snowPercent;
	TerrainMaterial(globalLayout, materialPosition, f2InIslandTexcoord, f2InRotationCosSin,
	                colorTextureSamplers[nonuniformEXT(uiInTextureSlot)], normalTextureSamplers[nonuniformEXT(uiInTextureSlot)], masksTextureSamplers[nonuniformEXT(uiInTextureSlot)],
	                rockSampler, sandSampler, rockNormalsSampler0, rockNormalsSampler1, rockNormalsSampler2,
	                sandNormalsSampler0, sandNormalsSampler1, sandNormalsSampler2, color, normal, snowPercent);
	LightingReceiver(f3InRasterPosition, normal, vec3(0.0f, 0.0f, 1.0f), color);
}
