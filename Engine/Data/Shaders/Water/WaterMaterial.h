#ifndef WATER_MATERIAL_H
#define WATER_MATERIAL_H

void WaterMaterial(float fTerrainElevation, vec2 f2WaterOrigin, vec2 f2WorldInitialPosition,
	               out vec3 f3PreLightingColor, out bool bFoamEnabled, out vec3 f3FoamColor, out float fFoamShade)
{
	// Color (noise with precision-safe UV — same contract as SAMPLE_NORMAL_PRECISE above).
	// CPU stores fmod(freq*camera, 10.0) so mult * 10 must be integer for fract() to absorb the wrap.
	// gWaterColorNoiseMultiplierOne/Two sliders (WaterWrappersBase.cpp) snap to a 0.1 grid so the
	// product stays integer for any tuning (0.0, 0.1, 0.2, ...). Derivatives taken from the un-scaled
	// local position and scaled the same way as the UV keep mip selection stable across the wrap
	// (plain texture() pops at the seam).
	vec2 f2LocalDisplacedPos = f3InPosition.xy - f2WaterOrigin;
	vec2 f2ReducedNoiseOrigin = vec2(globalLayout.fWaterReducedNoiseOriginX, globalLayout.fWaterReducedNoiseOriginY);
	float fMultOne = globalLayout.fWaterColorNoiseMultiplierOne;
	float fMultTwo = globalLayout.fWaterColorNoiseMultiplierTwo;
	vec2 f2NoiseLocalDx = dFdx(f2LocalDisplacedPos);
	vec2 f2NoiseLocalDy = dFdy(f2LocalDisplacedPos);
	float fScaleOne = fMultOne * globalLayout.fWaterColorNoiseFrequency;
	vec2 f2NoiseUvOne = fScaleOne * f2LocalDisplacedPos + fMultOne * f2ReducedNoiseOrigin;
	float fNoiseColorOne = clamp(globalLayout.fWaterColorNoiseWeightOne * globalLayout.fWaterColorNoiseAmount * textureGrad(noiseTextureSampler, fract(f2NoiseUvOne), fScaleOne * f2NoiseLocalDx, fScaleOne * f2NoiseLocalDy).x, -1.0f, 1.0f);
	float fScaleTwo = fMultTwo * globalLayout.fWaterColorNoiseFrequency;
	vec2 f2NoiseUvTwo = fScaleTwo * f2LocalDisplacedPos + fMultTwo * f2ReducedNoiseOrigin;
	float fNoiseColorTwo = clamp(globalLayout.fWaterColorNoiseWeightTwo * globalLayout.fWaterColorNoiseAmount * textureGrad(noiseTextureSampler, fract(f2NoiseUvTwo), fScaleTwo * f2NoiseLocalDx, fScaleTwo * f2NoiseLocalDy).x, -1.0f, 1.0f);
	vec3 f3WaterColor = mix(1.0f * vec3(0.0f, 15.0f / 100.0f, 25.0f / 100.0f), 1.5f * vec3(15.0f / 100.0f, 30.0f / 100.0f, 50.0f / 100.0f), clamp(fNoiseColorOne + fNoiseColorTwo + (f3InPosition.z * globalLayout.fWaterColorHeightInverse + globalLayout.fWaterColorBottom), 0.0f, 1.0f));

	vec3 f3DepthColor = texture(depthLutSampler, vec2(globalLayout.fWaterDepthLookupTableFeather * -fTerrainElevation, 0.0f)).xyz;

	float fHeight = f3InPosition.z - fTerrainElevation;
	f3PreLightingColor = mix(f3DepthColor, f3WaterColor, clamp(fHeight * globalLayout.fWaterDepthColorFeather + globalLayout.fWaterDepthLookupTableSunsetFade, globalLayout.fWaterDepthColorFloor, 1.0f));

	bFoamEnabled = globalLayout.iWaterFoamEnabled != 0;
	if (bFoamEnabled)
	{
		vec2 f2FoamUv = vec2(f2WorldInitialPosition.x - globalLayout.f4WaterFoamArea.x, globalLayout.f4WaterFoamArea.y - f2WorldInitialPosition.y) * globalLayout.f2WaterFoamAreaInverseSize;
		float fCoverage = texture(foamTextureSampler, f2FoamUv).x;
		vec2 f2FoamTextureUv = globalLayout.fWaterFoamTextureInverseTileSize * f2InInitialPosition + globalLayout.f2WaterFoamReducedTextureOrigin;
		f3FoamColor = textureGrad(pFoamColorTextureSamplers[globalLayout.uiWaterFoamTextureIndex], f2FoamTextureUv, globalLayout.fWaterFoamTextureInverseTileSize * dFdx(f2InInitialPosition), globalLayout.fWaterFoamTextureInverseTileSize * dFdy(f2InInitialPosition)).rgb;
		vec2 f2FoamBreakupUv = globalLayout.fWaterFoamBreakupInverseTileSize * f2InInitialPosition + globalLayout.f2WaterFoamBreakupReducedTextureOrigin;
		float fFoamBreakupOpacity = textureGrad(pFoamBreakupTextureSamplers[globalLayout.uiWaterFoamBreakupTextureIndex], f2FoamBreakupUv, globalLayout.fWaterFoamBreakupInverseTileSize * dFdx(f2InInitialPosition), globalLayout.fWaterFoamBreakupInverseTileSize * dFdy(f2InInitialPosition)).x;
		float fFoamMask = mix(1.0f, fFoamBreakupOpacity, globalLayout.fWaterFoamBreakupAmount);
		fFoamShade = clamp(fCoverage * fFoamMask * globalLayout.fWaterFoamTextureStrength, 0.0f, 1.0f);
		f3FoamColor = pow(f3FoamColor, vec3(2.2));
	}

}

#endif
