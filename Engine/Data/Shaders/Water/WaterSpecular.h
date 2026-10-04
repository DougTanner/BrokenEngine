#if WATER_SPEC_AA_MODE == 1
// Box-filtered power lobe: mean of x^p over the [fLow, fHigh] slice of the pixel footprint.
// Bounds arrive pre-clamped to [0, 1] as log2 values; the sub-zero footprint contributes 0,
// so the divisor stays the full width.
float BoxFilteredLobe(float fIntensity, float fPower, float fLogLow, float fLogHigh, float fFootprintInverse)
{
	return fIntensity * (exp2((fPower + 1.0f) * fLogHigh) - exp2((fPower + 1.0f) * fLogLow)) * fFootprintInverse / (fPower + 1.0f);
}
#elif WATER_SPEC_AA_MODE == 2 || WATER_SPEC_AA_MODE == 3
// Widen a power lobe by slope-space variance (Phong <-> Beckmann equivalence: alpha^2 ~= 2/(p+2)) and
// conserve integrated lobe energy: p' = 2/(2/(p+2) + kernel) - 2 clamped at 0 (a negative power would
// spike as s -> 0), amplitude x (1+p')/(1+p) — the highlight broadens and dims instead of just dimming
// (Toksvig/Hill energy form).
// fAlphaSquared = 2/(power+2) and fOnePlusPowerInverse = 1/(1+power) are the lobe's uniform-only constants, folded
// CPU-side (LightingUniforms.cpp) and passed per lobe. The varying (1+p') numerator amplitude stays here.
float FilteredPowerLobe(float fIntensity, float fAlphaSquared, float fOnePlusPowerInverse, float fSpecularLog2, float fKernel)
{
	float fFilteredPower = max(2.0f / (fAlphaSquared + fKernel) - 2.0f, 0.0f);
	return fIntensity * ((1.0f + fFilteredPower) * fOnePlusPowerInverse) * exp2(fFilteredPower * fSpecularLog2);
}

#if WATER_SPEC_AA_MIP_HANDOFF
// Lerped lookup into one group's slice of the baked per-mip Toksvig variance table. Entries past the
// real mip chain are pre-padded with the last value at pack time, so clamping to the table bounds is
// sufficient — no per-texture mip count needed.
float MipVarianceLookup(int iTableBase, float fLevelOfDetail)
{
	float fClamped = clamp(fLevelOfDetail, 0.0f, float(kiWaterSpecularAntialiasingMipTableSize - 1));
	int iLow = int(fClamped);
	int iHigh = min(iLow + 1, kiWaterSpecularAntialiasingMipTableSize - 1);
	return mix(mainLayout.pfWaterSpecularAntialiasingMipmapVariance[iTableBase + iLow], mainLayout.pfWaterSpecularAntialiasingMipmapVariance[iTableBase + iHigh], fClamped - float(iLow));
}

// Mean unresolved variance across one sample group's three octaves; the size multipliers are the
// group's compile-time constants from the SAMPLE_NORMAL_PRECISE call sites.
float GroupMipVariance(int iTableBase, float fLevelOfDetailBase, float fSizeMultiplierA, float fSizeMultiplierB, float fSizeMultiplierC)
{
	return (MipVarianceLookup(iTableBase, fLevelOfDetailBase + log2(fSizeMultiplierA))
		+ MipVarianceLookup(iTableBase, fLevelOfDetailBase + log2(fSizeMultiplierB))
		+ MipVarianceLookup(iTableBase, fLevelOfDetailBase + log2(fSizeMultiplierC))) / 3.0f;
}
#endif
#endif

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

	// The high-power One lobe (power ~200) is sub-pixel-narrow and flickers when evaluated pointwise.
	// WATER_SPEC_AA_MODE, defined in Water.frag, selects the lobe filter.
	float fBeachBlend = 1.0f - clamp(-fTerrainElevation * globalLayout.fWaterBreakEndDepthInverse, 0.0f, 1.0f);
	float fIntensityOne = globalLayout.fLightingWaterSkyboxOne * (1.0f - fBeachBlend * mainLayout.fLightingWaterSkyboxOneBeachReduction);
	float fPowerOne = mainLayout.fLightingWaterSkyboxOnePower;
	float fIntensityTwo = mainLayout.fLightingWaterSkyboxTwo * (1.0f - fBeachBlend * mainLayout.fLightingWaterSkyboxTwoBeachReduction);
	float fPowerTwo = mainLayout.fLightingWaterSkyboxTwoPower;
	float fIntensityThree = mainLayout.fLightingWaterSkyboxThree * (1.0f - fBeachBlend * mainLayout.fLightingWaterSkyboxThreeBeachReduction);
	float fPowerThree = mainLayout.fLightingWaterSkyboxThreePower;
	float fSpecularSum = 0.0f;
#if WATER_SPEC_AA_MODE != 4
	vec3 f3LightReflectionNormal = reflect(f3BiasedSunNormal, reflect(f3ToEyeNormal, f3SkyboxWaveNormal));
	float fSpecularFactor = dot(vec3(-1.0f, 1.0f, -1.0f) * f3ToEyeNormal, f3LightReflectionNormal);
#endif

#if WATER_SPEC_AA_MODE == 0
	if (fSpecularFactor > 0.0f)
	{
		float fSpecularLog2 = log2(fSpecularFactor);
		fSpecularSum =
			fIntensityOne   * exp2(fPowerOne   * fSpecularLog2) +
			fIntensityTwo   * exp2(fPowerTwo   * fSpecularLog2) +
			fIntensityThree * exp2(fPowerThree * fSpecularLog2);
	}
#elif WATER_SPEC_AA_MODE == 1
	// Derivatives taken before any branch (helper-invocation-safe). The variance slider scales the filter
	// width: 0.25 (default) maps to the exact pixel footprint.
	float fFootprint = 4.0f * mainLayout.fWaterSpecularAntialiasingVariance * length(vec2(dFdx(fSpecularFactor), dFdy(fSpecularFactor)));
	// min() guards the few-ULP case where the unit-vector dot exceeds 1.0 and both clamped bounds would
	// collapse to 1.0 — a zero-width integral (dark pixel) at the exact highlight peak.
	float fClampedFactor = min(fSpecularFactor, 1.0f);
	float fHigh = clamp(fClampedFactor + 0.5f * fFootprint, 0.0f, 1.0f);
	if (fFootprint > kfEpsilon && fHigh > 0.0f)
	{
		float fLow = clamp(fClampedFactor - 0.5f * fFootprint, 0.0f, 1.0f);
		float fLogHigh = log2(fHigh);
		float fLogLow = fLow > 0.0f ? log2(fLow) : -128.0f; // exp2((p+1) * -128) flushes to +0
		float fFootprintInverse = 1.0f / fFootprint;
		fSpecularSum =
			BoxFilteredLobe(fIntensityOne,   fPowerOne,   fLogLow, fLogHigh, fFootprintInverse) +
			BoxFilteredLobe(fIntensityTwo,   fPowerTwo,   fLogLow, fLogHigh, fFootprintInverse) +
			BoxFilteredLobe(fIntensityThree, fPowerThree, fLogLow, fLogHigh, fFootprintInverse);
	}
	else if (fSpecularFactor > 0.0f)
	{
		// Degenerate footprint — fall back to the pointwise lobes
		float fSpecularLog2 = log2(fSpecularFactor);
		fSpecularSum =
			fIntensityOne   * exp2(fPowerOne   * fSpecularLog2) +
			fIntensityTwo   * exp2(fPowerTwo   * fSpecularLog2) +
			fIntensityThree * exp2(fPowerThree * fSpecularLog2);
	}
#elif WATER_SPEC_AA_MODE == 2 || WATER_SPEC_AA_MODE == 3
	#if WATER_SPEC_AA_MIP_HANDOFF
	// Minification handoff: the octave fetches' mips have already averaged away sub-texel normal
	// variance (BC5 + DecodeNormal re-unitize every sample), so the screen-space kernels below can't
	// see it — the source of camera-zoom flicker. Recompute each octave's fetch LOD analytically from
	// the same gradients SAMPLE_NORMAL_PRECISE passed to textureGrad, look up the baked per-mip
	// Toksvig variance, and add the unresolved slope variance to the lobe kernel. Weight shares are
	// squared because weighted-sum-then-normalize scales each octave's slope contribution linearly.
	// ALU only — no extra fetches.
	float fMipKernel = 0.0f;
	float fWeightTotal = normalResult.fWeightOne + normalResult.fWeightTwo + normalResult.fWeightThree;
	if (fWeightTotal > 0.0f)
	{
		float fDerivativeLog2 = log2(max(max(length(normalResult.f2LocalDx), length(normalResult.f2LocalDy)), 1.0e-12f)) + mainLayout.fWaterNormalMipmapBias;
		float fLevelOfDetailBaseOne = fDerivativeLog2 + log2(normalResult.fSizeOne * float(textureSize(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexOne], 0).x));
		float fLevelOfDetailBaseTwo = fDerivativeLog2 + log2(normalResult.fSizeTwo * float(textureSize(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexTwo], 0).x));
		float fLevelOfDetailBaseThree = fDerivativeLog2 + log2(normalResult.fSizeThree * float(textureSize(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexThree], 0).x));
		// Relative-weight squares are uniform (weights are CPU-resolved by camera height), folded into
		// fWaterNormalWRelSq* (LightingUniforms.cpp). Octave size multipliers must match the SAMPLE_NORMAL_PRECISE calls in WaterNormalSampling.h.
		float fMipVariance =
			mainLayout.fWaterNormalRelativeWeightSquaredOne   * GroupMipVariance(0 * kiWaterSpecularAntialiasingMipTableSize, fLevelOfDetailBaseOne, 0.2f, 1.1f, 2.5f) +
			mainLayout.fWaterNormalRelativeWeightSquaredTwo   * GroupMipVariance(1 * kiWaterSpecularAntialiasingMipTableSize, fLevelOfDetailBaseTwo, 0.3f, 1.2f, 3.0f) +
			mainLayout.fWaterNormalRelativeWeightSquaredThree * GroupMipVariance(2 * kiWaterSpecularAntialiasingMipTableSize, fLevelOfDetailBaseThree, 0.4f, 1.3f, 3.5f);
	#if WATER_SPEC_AA_FADE_HANDOFF
		// Fade handoff: reference the near-camera full-weight appearance — each group also adds its
		// TOTAL variance (last table entry, everything averaged away) times the weight share the
		// height fade removed, so far water keeps its statistical roughness through the fade band.
		float fWeightTotalFull = mainLayout.fWaterNormalWeightFullOne + mainLayout.fWaterNormalWeightFullTwo + mainLayout.fWaterNormalWeightFullThree;
		if (fWeightTotalFull > 0.0f)
		{
			float fWeightRelativeFullOne = mainLayout.fWaterNormalWeightFullOne / fWeightTotalFull;
			float fWeightRelativeFullTwo = mainLayout.fWaterNormalWeightFullTwo / fWeightTotalFull;
			float fWeightRelativeFullThree = mainLayout.fWaterNormalWeightFullThree / fWeightTotalFull;
			fMipVariance +=
				max(fWeightRelativeFullOne * fWeightRelativeFullOne - mainLayout.fWaterNormalRelativeWeightSquaredOne, 0.0f) * mainLayout.pfWaterSpecularAntialiasingMipmapVariance[1 * kiWaterSpecularAntialiasingMipTableSize - 1] +
				max(fWeightRelativeFullTwo * fWeightRelativeFullTwo - mainLayout.fWaterNormalRelativeWeightSquaredTwo, 0.0f) * mainLayout.pfWaterSpecularAntialiasingMipmapVariance[2 * kiWaterSpecularAntialiasingMipTableSize - 1] +
				max(fWeightRelativeFullThree * fWeightRelativeFullThree - mainLayout.fWaterNormalRelativeWeightSquaredThree, 0.0f) * mainLayout.pfWaterSpecularAntialiasingMipmapVariance[3 * kiWaterSpecularAntialiasingMipTableSize - 1];
		}
	#endif
		// The factor 2 maps Toksvig inverse-power variance into the kernel's alpha^2 ~= 2/(p+2) domain
		// (Toksvig: 1/p' = 1/p + variance, so the FilteredPowerLobe kernel contribution is 2*variance).
		fMipKernel = mainLayout.fWaterSpecularAntialiasingMipmapScale * 2.0f * fMipVariance;
	}
	#else
	float fMipKernel = 0.0f;
	#endif
	#if WATER_SPEC_AA_MODE == 2
	// Slope-space variance from the screen-space change of the reflection normal (Vlachos GDC15 / Filament form)
	vec3 f3NormalDerivativeX = dFdx(f3SkyboxWaveNormal);
	vec3 f3NormalDerivativeY = dFdy(f3SkyboxWaveNormal);
	float fKernel = min(2.0f * mainLayout.fWaterSpecularAntialiasingVariance * (dot(f3NormalDerivativeX, f3NormalDerivativeX) + dot(f3NormalDerivativeY, f3NormalDerivativeY)) + fMipKernel, mainLayout.fWaterSpecularAntialiasingThreshold);
	#else
	// Toksvig-style variance from the agreement of the nine summed octave normals: length(f3WeightedSum)
	// shrinks as the octaves disagree (each DecodeNormal result is ~unit). Note: BC5 + DecodeNormal
	// re-unitizes each sample, so this measures inter-wave disagreement, not true footprint mip variance
	// (that part is restored by the WATER_SPEC_AA_MIP_HANDOFF term).
	float fAgreement = length(normalResult.f3WeightedSum) * mainLayout.fWaterNormalWeightSumInverse;
	float fKernel = min(mainLayout.fWaterSpecularAntialiasingVariance * (1.0f - fAgreement) / max(fAgreement, 0.001f) + fMipKernel, mainLayout.fWaterSpecularAntialiasingThreshold);
	#endif
	if (fSpecularFactor > 0.0f)
	{
		float fSpecularLog2 = log2(fSpecularFactor);
		fSpecularSum =
			FilteredPowerLobe(fIntensityOne,   mainLayout.f4WaterSkyboxLobeAlphaSquared.x, mainLayout.f4WaterSkyboxLobeOnePlusPowerInverse.x, fSpecularLog2, fKernel) +
			FilteredPowerLobe(fIntensityTwo,   mainLayout.f4WaterSkyboxLobeAlphaSquared.y, mainLayout.f4WaterSkyboxLobeOnePlusPowerInverse.y, fSpecularLog2, fKernel) +
			FilteredPowerLobe(fIntensityThree, mainLayout.f4WaterSkyboxLobeAlphaSquared.z, mainLayout.f4WaterSkyboxLobeOnePlusPowerInverse.z, fSpecularLog2, fKernel);
	}
#elif WATER_SPEC_AA_MODE == 4
	// Genuine 4x supersample of the only aliasing term: re-evaluate the reflect->dot->lobe chain (pure ALU,
	// fetches unchanged) at four derivative-extrapolated normals and average.
	vec3 f3NormalDerivativeX = dFdx(f3SkyboxWaveNormal);
	vec3 f3NormalDerivativeY = dFdy(f3SkyboxWaveNormal);
	for (int i = 0; i < 4; i++)
	{
		vec2 f2Offset = vec2((i & 1) != 0 ? 0.5f : -0.5f, (i & 2) != 0 ? 0.5f : -0.5f);
		vec3 f3SubsampleNormal = normalize(f3SkyboxWaveNormal + f2Offset.x * f3NormalDerivativeX + f2Offset.y * f3NormalDerivativeY);
		float fSubsampleFactor = dot(vec3(-1.0f, 1.0f, -1.0f) * f3ToEyeNormal, reflect(f3BiasedSunNormal, reflect(f3ToEyeNormal, f3SubsampleNormal)));
		if (fSubsampleFactor > 0.0f)
		{
			float fSubsampleLog2 = log2(fSubsampleFactor);
			fSpecularSum +=
				fIntensityOne   * exp2(fPowerOne   * fSubsampleLog2) +
				fIntensityTwo   * exp2(fPowerTwo   * fSubsampleLog2) +
				fIntensityThree * exp2(fPowerThree * fSubsampleLog2);
		}
	}
	fSpecularSum *= 0.25f;
#endif
	result.fReflection = mainLayout.fLightingWaterSkyboxIntensity * result.fReflectionTerrainMultiplier * fSpecularSum;
	return result;
}
