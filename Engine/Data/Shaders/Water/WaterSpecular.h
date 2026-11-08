struct WaterSpecularResult
{
	vec3 f3SkyboxColor;
	vec3 f3SkyboxColorSun;
	float fReflectionTerrainMultiplier;
	float fReflection;
};

WaterSpecularResult ComposeWaterSpecular(WaterNormalSamplingResult normalResult, vec3 f3ToEyeNormal, vec3 f3InNormal, vec3 f3SunOrMoon, float fTerrainElevation, vec2 f2InitialPosition, vec2 f2InitialDx, vec2 f2InitialDy, vec3 f3NormalDx, vec3 f3NormalDy, PatchTriangle pWaterNormalPatchTriangles[3])
{
	WaterSpecularResult result = WaterSpecularResult(vec3(0.0f), vec3(0.0f), 0.0f, 0.0f);

	// Skybox. The Ryfjallet prefiltered cubemap bound here (kPrefilteredWaterCrc) is oriented to
	// match engine Z-up, so the reflection vector is sampled directly with no Y-up swizzle.
	float fSkyboxNormalBlendWave = mainLayout.fLightingWaterSkyboxNormalBlendWave;
	vec3 f3SkyboxWaveNormal = normalize((1.0f - fSkyboxNormalBlendWave) * normalResult.f3SampledNormal + fSkyboxNormalBlendWave * f3InNormal);
	result.f3SkyboxColor = textureLod(skyboxSampler, -reflect(f3ToEyeNormal, f3SkyboxWaveNormal), mainLayout.fLightingWaterSkyboxLevelOfDetail).xyz;
	result.f3SkyboxColorSun = result.f3SkyboxColor * f3SunOrMoon;

	result.fReflectionTerrainMultiplier = clamp(-fTerrainElevation * globalLayout.fWaterDepthReflectionFeatherInverse, 0.0f, 1.0f);
	// f4WaterBiasedSunNormal = normalize((0,0,fLightingWaterSkyboxSunBias) + f4SunMoonNormal), folded CPU-side.
	vec3 f3BiasedSunNormal = globalLayout.f4WaterBiasedSunNormal.xyz;

	float fBeachBlend = 1.0f - clamp(-fTerrainElevation * globalLayout.fWaterBreakEndDepthInverse, 0.0f, 1.0f);
	float fIntensityOne = globalLayout.fLightingWaterSkyboxOne * (1.0f - fBeachBlend * mainLayout.fLightingWaterSkyboxOneBeachReduction);
	float fPowerOne = mainLayout.fLightingWaterSkyboxOnePower;
	float fIntensityTwo = mainLayout.fLightingWaterSkyboxTwo * (1.0f - fBeachBlend * mainLayout.fLightingWaterSkyboxTwoBeachReduction);
	float fPowerTwo = mainLayout.fLightingWaterSkyboxTwoPower;
	float fIntensityThree = mainLayout.fLightingWaterSkyboxThree * (1.0f - fBeachBlend * mainLayout.fLightingWaterSkyboxThreeBeachReduction);
	float fPowerThree = mainLayout.fLightingWaterSkyboxThreePower;
	float fSpecularSum = 0.0f;
	float fSpecularOne = 0.0f;
	vec3 f3LightReflectionNormal = reflect(f3BiasedSunNormal, reflect(f3ToEyeNormal, f3SkyboxWaveNormal));
	float fSpecularFactor = dot(vec3(-1.0f, 1.0f, -1.0f) * f3ToEyeNormal, f3LightReflectionNormal);

	if (fSpecularFactor > 0.0f)
	{
		float fSpecularLog2 = log2(fSpecularFactor);
		fSpecularOne = exp2(fPowerOne * fSpecularLog2);
		fSpecularSum =
			fIntensityTwo   * exp2(fPowerTwo   * fSpecularLog2) +
			fIntensityThree * exp2(fPowerThree * fSpecularLog2);
	}

	int iSampleCount = mainLayout.iLightingWaterSkyboxOneSampleCount;
	if (iSampleCount > 1 && mainLayout.fLightingWaterSkyboxOneSampleBlend > 0.0f)
	{
		float fSampleSum = fSpecularOne;
		for (int i = 0; i < iSampleCount - 1; ++i)
		{
			vec2 f2SampleOffset = mainLayout.pf2LightingWaterSkyboxOneSampleOffsets[i];
			vec2 f2SamplePosition = f2InitialPosition + f2SampleOffset.x * f2InitialDx + f2SampleOffset.y * f2InitialDy;
			// Sample positions at most a pixel away reuse the fragment's patch triangles rather than rebuilding them.
			WaterNormalSamplingResult sampleNormalResult = SampleWaterNormals(f2SamplePosition, f2InitialDx, f2InitialDy, pWaterNormalPatchTriangles);
			vec3 f3SampleToEyeNormal = normalize(vec3(-f2SamplePosition, mainLayout.f4WaterEyePosition.z));
			f3SampleToEyeNormal = normalize(mix(f3SampleToEyeNormal, vec3(0.0f, 0.0f, 1.0f), globalLayout.fLightingWaterSkyboxNormalSoften));
			vec3 f3SampleGeometryNormal = normalize(f3InNormal + f2SampleOffset.x * f3NormalDx + f2SampleOffset.y * f3NormalDy);
			vec3 f3SampleSkyboxWaveNormal = normalize((1.0f - fSkyboxNormalBlendWave) * sampleNormalResult.f3SampledNormal + fSkyboxNormalBlendWave * f3SampleGeometryNormal);
			vec3 f3SampleLightReflectionNormal = reflect(f3BiasedSunNormal, reflect(f3SampleToEyeNormal, f3SampleSkyboxWaveNormal));
			float fSampleFactor = dot(vec3(-1.0f, 1.0f, -1.0f) * f3SampleToEyeNormal, f3SampleLightReflectionNormal);
			if (fSampleFactor > 0.0f)
			{
				fSampleSum += exp2(fPowerOne * log2(fSampleFactor));
			}
		}
		float fSampleMean = fSampleSum * mainLayout.fLightingWaterSkyboxOneSampleCountInverse;
		fSpecularOne = mix(fSpecularOne, fSampleMean, mainLayout.fLightingWaterSkyboxOneSampleBlend);
	}
	fSpecularSum += fIntensityOne * fSpecularOne;
	result.fReflection = mainLayout.fLightingWaterSkyboxIntensity * result.fReflectionTerrainMultiplier * fSpecularSum;
	return result;
}
