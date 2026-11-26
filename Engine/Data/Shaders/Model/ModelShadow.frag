#version 460

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"

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

// Input
layout (location = 0) in vec3 f3InWorldPosition;
layout (location = 1) in vec3 f3InNormal;
layout (location = 2) in vec2 f2InUV;
layout (location = 3) in vec2 f2InUV1;
layout (location = 4) in vec2 f2InUV2;
layout (location = 5) in vec2 f2InUV3;
layout (location = 6) in vec2 f2InUV4;
layout (location = 7) in vec4 f4InColorAdd;
layout (location = 8) in float fInHeightAboveBase;
layout (location = 10) in vec4 f4InTangent;

// Output
layout (location = 0) out float fOutColor;

void main()
{
	// Single-channel object-shadow coverage merged by MAX blending, faded linearly below sea level.
	float fUnderwaterFade = clamp(1.0f + f3InWorldPosition.z * globalLayout.fObjectShadowsUnderwaterFadeInverse, 0.0f, 1.0f);
	fOutColor = fUnderwaterFade;
}
