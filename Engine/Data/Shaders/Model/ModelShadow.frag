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
layout (location = 9) in flat int iInLayer;

// Output
layout (location = 0) out vec4 f4OutColor;

void main()
{
	// Coverage merged by MAX blending. R and G split ground coverage by ship height above the ground below it, between
	// the lowest and highest flight-plane heights, and blur with Sigma Low and High, so the ground blur varies smoothly
	// with height. B and A hold ships on flight planes 1 and 2 only, for shadows on ships and smoke below them; nothing
	// reads plane 0's.
	float fHeightFraction = clamp(fInHeightAboveBase * globalLayout.fObjectShadowsBlurHeightRangeInverse, 0.0f, 1.0f);
	float fUnderwaterFade = clamp(1.0f + f3InWorldPosition.z * globalLayout.fObjectShadowsUnderwaterFadeInverse, 0.0f, 1.0f);
	f4OutColor = fUnderwaterFade * vec4(1.0f - fHeightFraction, fHeightFraction, equal(ivec2(1, 2), ivec2(iInLayer)));
}
