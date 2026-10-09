#version 460

#extension GL_EXT_nonuniform_qualifier : require

#include "ShaderLayouts.h"
#include "ShaderFunctions.h"

// Uniforms
layout (set = 0, binding = kiGlobalBindingGlobalUniform) uniform globalUniform
{
	GlobalLayout globalLayout;
};

layout (scalar, set = 1, binding = 2) buffer readonly renderUniform
{
	ParticlesLayout render;
};

layout (set = 1, binding = 3) uniform sampler2D smokeSampler;

layout (set = 0, binding = kiGlobalBindingSamplerClamp) uniform sampler particleSampler;
layout (set = 0, binding = kiGlobalBindingBindlessTextures) uniform texture2D pTextures[];

// Input
layout (location = 0) in flat int iInInstanceIndex;
layout (location = 1) in vec3 f3InWorldPosition;
layout (location = 2) in vec2 f2InTexcoord;

// Output
layout (location = 0) out vec4 f4OutColor;

void main()
{
	int i = iInInstanceIndex;

	// Hoist SSBO reads (one access per field)
	float fVisibleIntensity = render.pParticles[i].fVisibleIntensity;
	float fSpawnIntensity = render.pParticles[i].fSpawnIntensity;
	float fIntensityPower = render.pParticles[i].fIntensityPower;
	uint uiColor = render.pParticles[i].iColor;
	uint uiColorEnd = render.pParticles[i].iColorEnd;
	int iCookie = render.pParticles[i].iCookie;

	// Blend weight is the fraction of log-intensity decay from spawn to the update free threshold, which this value must
	// match (ParticlesUpdate.comp). Spawn guarantees fSpawnIntensity > 0 and update frees slots below the threshold, so
	// both log2 arguments and the intensity pow base are positive.
	const float fIntensityEpsilon = 0.5f;
	float fFade = clamp(log2(fSpawnIntensity / fVisibleIntensity) / max(log2(fSpawnIntensity / fIntensityEpsilon), kfEpsilon), 0.0f, 1.0f);
	float fIntensity = pow(fVisibleIntensity, fIntensityPower);
	vec4 f4Color = mix(unpackUnorm4x8(uiColor).abgr, unpackUnorm4x8(uiColorEnd).abgr, fFade);
	float fCookie = texture(sampler2D(pTextures[nonuniformEXT(iCookie)], particleSampler), f2InTexcoord).x;
	// Output alpha = 0 is correct under kAdd blend (VK_BLEND_FACTOR_ONE/ONE — alpha contribution discarded by additive sum)
	f4OutColor = vec4(fCookie * fIntensity * f4Color.w * f4Color.xyz, 0.0f);

	float fHeightFraction = clamp((f3InWorldPosition.z - globalLayout.fBaseHeight) * globalLayout.fSmokeObjectHeightInverse, 0.0f, 1.0f);
	float fSmokeFade = 1.0f - fHeightFraction * fHeightFraction;
	f4OutColor.xyz *= SmokeShadow(globalLayout, f3InWorldPosition, smokeSampler, fSmokeFade);
}
