struct WaterSpecularResult
{
	vec3 f3SkyboxColor;
	vec3 f3SkyboxColorSun;
	float fReflectionTerrainMultiplier;
	float fReflection;
};

WaterSpecularResult ComposeWaterSpecular(WaterNormalSamplingResult normalResult, vec3 f3ToEyeNormal, vec3 f3InNormal, vec3 f3SunOrMoon, float fTerrainElevation)
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
	vec3 f3LightReflectionNormal = reflect(f3BiasedSunNormal, reflect(f3ToEyeNormal, f3SkyboxWaveNormal));
	float fSpecularFactor = dot(vec3(-1.0f, 1.0f, -1.0f) * f3ToEyeNormal, f3LightReflectionNormal);

	if (fSpecularFactor > 0.0f)
	{
		float fSpecularLog2 = log2(fSpecularFactor);
		fSpecularSum =
			fIntensityOne   * exp2(fPowerOne   * fSpecularLog2) +
			fIntensityTwo   * exp2(fPowerTwo   * fSpecularLog2) +
			fIntensityThree * exp2(fPowerThree * fSpecularLog2);
	}
	result.fReflection = mainLayout.fLightingWaterSkyboxIntensity * result.fReflectionTerrainMultiplier * fSpecularSum;
	return result;
}
