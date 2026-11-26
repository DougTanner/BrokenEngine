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

layout (set = 1, binding = 1) uniform sampler2D redSampler;
layout (set = 1, binding = 2) uniform sampler2D greenSampler;
layout (set = 1, binding = 3) uniform sampler2D blueSampler;
layout (set = 1, binding = 4) uniform sampler2D elevationSampler;

// Input
layout (location = 0) in flat int iInInstanceIndex;
layout (location = 1) in vec2 f2InTexcoord;

// Output
layout (location = 0) out vec4 f4OutRed;
layout (location = 1) out vec4 f4OutGreen;
layout (location = 2) out vec4 f4OutBlue;
layout (location = 3) out vec4 f4OutRedSpread;
layout (location = 4) out vec4 f4OutGreenSpread;
layout (location = 5) out vec4 f4OutBlueSpread;

// Unthresholded Reinhard compression preserves hue in each EWNS component. Zero strength leaves RGB unchanged, and zero RGB stays zero.
void CompressHuePreserving(inout vec4 f4Red, inout vec4 f4Green, inout vec4 f4Blue, float fCompress)
{
	vec4 f4Avg = max((f4Red + f4Green + f4Blue) * (1.0f / 3.0f), vec4(0.0f));
	vec4 f4Ratio = 1.0f / (1.0f + f4Avg * fCompress);
	f4Red *= f4Ratio;
	f4Green *= f4Ratio;
	f4Blue *= f4Ratio;
}

void main()
{
	// Per-pass interpolation: t=0 at pass 0 (Start values), t=1 at last pass (End values)
	float fPassIndex = pushConstantsLayout.f4Pipeline.z;
	const uint32_t uiPassIndex = uint32_t(fPassIndex);
	float fT = fPassIndex / max(globalLayout.fSpreadPassCount - 1.0f, 1.0f);

	// Interpolate spread parameters between Start and End
	float fDirectionality = mix(globalLayout.fSpreadDirectionalityStart, globalLayout.fSpreadDirectionalityEnd, fT);
	float fSpreadDistance = mix(globalLayout.fSpreadDistanceStart, globalLayout.fSpreadDistanceEnd, fT);
	float fDecay = mix(globalLayout.fSpreadDecayStart, globalLayout.fSpreadDecayEnd, fT);
	float fAccumulationDecay = mix(globalLayout.fSpreadAccumulationDecayStart, globalLayout.fSpreadAccumulationDecayEnd, fT);
	float fDistanceFalloff = mix(globalLayout.fSpreadDistanceFalloffStart, globalLayout.fSpreadDistanceFalloffEnd, fT);
	float fOutputCompress = mix(globalLayout.fSpreadOutputCompressStart, globalLayout.fSpreadOutputCompressEnd, fT);

	// Height fade: attenuate spread above base height.
	vec2 f2WorldPos = VisibleAreaToWorld(f2InTexcoord, globalLayout.f4LightingArea);
	vec2 f2ElevTexcoord = WorldToVisibleArea(vec3(f2WorldPos, 0.0f), globalLayout.visibleAreaToUv);

	float fElevation = texture(elevationSampler, f2ElevTexcoord).x;
	float fHeightT = clamp((fElevation - globalLayout.fBaseHeight) * globalLayout.fSpreadHeightEndHeightInverse, 0.0f, 1.0f);
	float fHeightFade = pow(fHeightT, globalLayout.fSpreadHeightPower) * globalLayout.fSpreadHeightMultiplier;
	fSpreadDistance *= 1.0f - fHeightFade;
	fDecay *= 1.0f - fHeightFade;

	// World-to-texcoord conversion: texcoord 0-1 covers the lighting area. The mapping's y scale carries the Y flip, so
	// its negation is the positive reciprocal extent.
	float fAspectRatioX = globalLayout.lightingAreaToUv.f4ScaleOffset.x;
	float fAspectRatioY = -globalLayout.lightingAreaToUv.f4ScaleOffset.y;

	f4OutRed = vec4(0.0f);
	f4OutGreen = vec4(0.0f);
	f4OutBlue = vec4(0.0f);

	// The CPU precomputes per-pass loop counts and EWNS weight-sum reciprocals for ideal evenly spaced directions.
	const SpreadPassNormalizationLayout passNormalization = globalLayout.pSpreadPassNormalizations[uiPassIndex];
	const uint32_t uiRingCount = passNormalization.uiRingCount;
	const uint32_t uiDirectionCountBase = passNormalization.uiDirectionCountBase;

	for (uint32_t j = 0; j < uiRingCount; ++j)
	{
		float fRingFalloff = 1.0f - fDistanceFalloff * (float(j + 1) / float(uiRingCount));

		const uint32_t uiDirectionCount = uiDirectionCountBase + j;
		const float fDirectionStep = (2.0f * kfPi) / float(uiDirectionCount);

		// Reset each ring to the positive-X unit vector to bound drift from incremental rotation.
		vec2 f2Direction = vec2(1.0f, 0.0f);
		float fCosStep = cos(fDirectionStep);
		float fSinStep = sin(fDirectionStep);
		for (uint32_t i = 0; i < uiDirectionCount; ++i)
		{
			// Per-channel EWNS directional weights for this sampling direction
			// Direction is in texcoord space: +X = east (pixel right), +Y = south (pixel down)
			// EWNS channels: R=East(+X), G=West(-X), B=North(-Y pixel), A=South(+Y pixel)
			// Weight = alignment of the sampling direction with each channel's axis
			vec4 f4DirWeight = max(vec4(f2Direction.x, -f2Direction.x, -f2Direction.y, f2Direction.y), 0.0f);

			// Ring distance: total reach = fSpreadDistance, more rings = denser sampling (not wider spread)
			vec2 f2Coord = f2InTexcoord + vec2(fAspectRatioX, fAspectRatioY) * (fSpreadDistance * (float(j + 1) / float(uiRingCount)) * f2Direction);

			vec4 f4Red = texture(redSampler, f2Coord);
			vec4 f4Green = texture(greenSampler, f2Coord);
			vec4 f4Blue = texture(blueSampler, f2Coord);

			// Pass 0 compresses deposit samples before directional weighting.
			if (fPassIndex < 0.5f)
			{
				CompressHuePreserving(f4Red, f4Green, f4Blue, globalLayout.fLightingDepositCompress);
			}

			vec4 f4TapWeight = mix(vec4(1.0f), f4DirWeight, fDirectionality) * fRingFalloff;
			f4OutRed += f4Red * f4TapWeight;
			f4OutGreen += f4Green * f4TapWeight;
			f4OutBlue += f4Blue * f4TapWeight;

			// Advance to the next evenly-spaced direction by rotating f2Direction by fDirectionStep
			f2Direction = vec2(f2Direction.x * fCosStep - f2Direction.y * fSinStep,
			                   f2Direction.x * fSinStep + f2Direction.y * fCosStep);
		}
	}

	vec4 f4WeightSumInverse = passNormalization.f2WeightSumInverse.xxyy;
	f4OutRed *= f4WeightSumInverse;
	f4OutGreen *= f4WeightSumInverse;
	f4OutBlue *= f4WeightSumInverse;

	f4OutRed *= fDecay;
	f4OutGreen *= fDecay;
	f4OutBlue *= fDecay;

	// Per-pass output Reinhard compressor: hue-preserving per-EWNS. Identity when fOutputCompress == 0.
	CompressHuePreserving(f4OutRed, f4OutGreen, f4OutBlue, fOutputCompress);

	// Spread-only outputs: pre-accumulation snapshot, weighted by the per-pass curve, read by LightCombine
	float fCurveWeight = globalLayout.pfCombineCurvePoints[uiPassIndex];
	f4OutRedSpread = f4OutRed * fCurveWeight;
	f4OutGreenSpread = f4OutGreen * fCurveWeight;
	f4OutBlueSpread = f4OutBlue * fCurveWeight;

	// Accumulate: add source pixel value to carry forward through passes (skip first pass — deposit already sampled)
	if (fPassIndex > 0.0f)
	{
		f4OutRed += fAccumulationDecay * texture(redSampler, f2InTexcoord);
		f4OutGreen += fAccumulationDecay * texture(greenSampler, f2InTexcoord);
		f4OutBlue += fAccumulationDecay * texture(blueSampler, f2InTexcoord);
	}
}
