struct WaterNormalSamplingResult
{
	vec3 f3SampledNormal;
};

// Each normal band's octave One patch triangle at f2InitialPosition, built only for a drawn band with a positive patch
// blend strength; SampleWaterNormals reads the same three at every sample position of one fragment.
void MakeWaterNormalPatchTriangles(vec2 f2InitialPosition, out PatchTriangle pPatchTriangles[3])
{
	// Undisplaced, unrotated camera-cell position, so each lattice stays still under band rotation and scroll.
	vec2 f2LatticePosition = f2InitialPosition + vec2(globalLayout.fWaterOriginX, globalLayout.fWaterOriginY);
	if (mainLayout.fWaterNormalWeightOne > 0.0f && globalLayout.fWaterNormalPatchBlendOne > 0.0f)
	{
		pPatchTriangles[0] = MakePatchTriangle(globalLayout, globalLayout.pWaterNormalPatchLattices[0], f2LatticePosition);
	}
	if (mainLayout.fWaterNormalWeightTwo > 0.0f && globalLayout.fWaterNormalPatchBlendTwo > 0.0f)
	{
		pPatchTriangles[1] = MakePatchTriangle(globalLayout, globalLayout.pWaterNormalPatchLattices[1], f2LatticePosition);
	}
	if (mainLayout.fWaterNormalWeightThree > 0.0f && globalLayout.fWaterNormalPatchBlendThree > 0.0f)
	{
		pPatchTriangles[2] = MakePatchTriangle(globalLayout, globalLayout.pWaterNormalPatchLattices[2], f2LatticePosition);
	}
}

WaterNormalSamplingResult SampleWaterNormals(vec2 f2InitialPosition, vec2 f2LocalDx, vec2 f2LocalDy, PatchTriangle pPatchTriangles[3])
{
	WaterNormalSamplingResult result;

	float fSizeOne = mainLayout.fLightingSampledNormalsOneSize;
	float fSizeTwo = mainLayout.fLightingSampledNormalsTwoSize;
	float fSizeThree = mainLayout.fLightingSampledNormalsThreeSize;
	vec2 f2ReducedOrigin = vec2(globalLayout.fWaterReducedNormalOriginX, globalLayout.fWaterReducedNormalOriginY);
	vec2 f2ReducedOriginTwo = vec2(globalLayout.fWaterReducedNormalOriginTwoX, globalLayout.fWaterReducedNormalOriginTwoY);
	vec2 f2ReducedOriginThree = vec2(globalLayout.fWaterReducedNormalOriginThreeX, globalLayout.fWaterReducedNormalOriginThreeY);
	vec2 f2ReducedTime = vec2(globalLayout.fWaterReducedNormalTimeX, globalLayout.fWaterReducedNormalTimeY);
	vec2 f2ReducedTimeTwo = vec2(globalLayout.fWaterReducedNormalTimeTwoX, globalLayout.fWaterReducedNormalTimeTwoY);
	vec2 f2ReducedTimeThree = vec2(globalLayout.fWaterReducedNormalTimeThreeX, globalLayout.fWaterReducedNormalTimeThreeY);

	// Per-sample weights resolved CPU-side by camera framing (LightingUniforms.cpp), then weighted-sum-then-normalize.
	// normalize() is scale-invariant so absolute weight magnitudes don't matter; only ratios do. Weight 0 disables a
	// sample group entirely — the weight is a uniform (warp-coherent branch), so a faded-out band skips its 3 fetches.
	float fWeightOne = mainLayout.fWaterNormalWeightOne;
	float fWeightTwo = mainLayout.fWaterNormalWeightTwo;
	float fWeightThree = mainLayout.fWaterNormalWeightThree;

	// m2UvRot=R(-theta) maps world UV to texture UV, making the pattern appear rotated CCW by theta; inverse
	// m2NormalRot=R(+theta) returns sampled tangent normal XY to world space. WaterUniforms.cpp rotates reducedOrigin
	// before fmod and reducedTime before accumulation/fmod. Applying rotation again breaks orientation and the integral
	// fract-wrap shift for arbitrary angles; apply m2UvRot only to camera-relative positions and derivatives, leaving
	// both reduced values unchanged. Only each band's octave One, its largest tile, is patch blended: octaves Two and
	// Three pass strength 0 and keep their plain read.
	#define SAMPLE_NORMAL_PRECISE(sampler, size, reducedOrigin, reducedTime, m2UvRot, sizeMult, speedMult, offset, patchTriangle, patchBlend) \
	{ \
		float fCallSize = sizeMult * size; \
		vec2 f2UV = offset \
			+ fCallSize * (m2UvRot * f2InitialPosition) \
			+ sizeMult * reducedOrigin \
			+ speedMult * reducedTime; \
		vec2 f2Dx = fCallSize * (m2UvRot * f2LocalDx); \
		vec2 f2Dy = fCallSize * (m2UvRot * f2LocalDy); \
		f3Accum += WaterNormalGradPatched(sampler, f2UV, f2Dx, f2Dy, patchTriangle, patchBlend); \
	}

	vec3 f3SampledNormalOne = vec3(0.0f);
	if (fWeightOne > 0.0f)
	{
		mat2 m2UvRotOne = mat2(globalLayout.f4WaterNormalRotationOne);
		mat2 m2NormalRotOne = mat2(globalLayout.f4WaterNormalRotationOne.xzyw);
		vec3 f3Accum = vec3(0.0f);
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexOne], fSizeOne, f2ReducedOrigin, f2ReducedTime, m2UvRotOne, kfWaterNormalOneOctaveOneSizeMultiplier, 1.1f, vec2(0.1f, 0.2f), pPatchTriangles[0], globalLayout.fWaterNormalPatchBlendOne)
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexOne], fSizeOne, f2ReducedOrigin, f2ReducedTime, m2UvRotOne, kfWaterNormalOneOctaveTwoSizeMultiplier, 1.2f, vec2(0.2f, 0.3f), pPatchTriangles[0], 0.0f)
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexOne], fSizeOne, f2ReducedOrigin, f2ReducedTime, m2UvRotOne, kfWaterNormalOneOctaveThreeSizeMultiplier, 1.3f, vec2(0.3f, 0.4f), pPatchTriangles[0], 0.0f)
		f3SampledNormalOne = f3Accum;
		f3SampledNormalOne.xy = m2NormalRotOne * f3SampledNormalOne.xy;
	}

	vec3 f3SampledNormalTwo = vec3(0.0f);
	if (fWeightTwo > 0.0f)
	{
		mat2 m2UvRotTwo = mat2(globalLayout.f4WaterNormalRotationTwo);
		mat2 m2NormalRotTwo = mat2(globalLayout.f4WaterNormalRotationTwo.xzyw);
		vec3 f3Accum = vec3(0.0f);
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexTwo], fSizeTwo, f2ReducedOriginTwo, f2ReducedTimeTwo, m2UvRotTwo, kfWaterNormalTwoOctaveOneSizeMultiplier, 1.4f, vec2(0.4f, 0.5f), pPatchTriangles[1], globalLayout.fWaterNormalPatchBlendTwo)
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexTwo], fSizeTwo, f2ReducedOriginTwo, f2ReducedTimeTwo, m2UvRotTwo, kfWaterNormalTwoOctaveTwoSizeMultiplier, 1.5f, vec2(0.6f, 0.7f), pPatchTriangles[1], 0.0f)
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexTwo], fSizeTwo, f2ReducedOriginTwo, f2ReducedTimeTwo, m2UvRotTwo, kfWaterNormalTwoOctaveThreeSizeMultiplier, 1.6f, vec2(0.8f, 0.9f), pPatchTriangles[1], 0.0f)
		f3SampledNormalTwo = f3Accum;
		f3SampledNormalTwo.xy = m2NormalRotTwo * f3SampledNormalTwo.xy;
	}

	vec3 f3SampledNormalThree = vec3(0.0f);
	if (fWeightThree > 0.0f)
	{
		mat2 m2UvRotThree = mat2(globalLayout.f4WaterNormalRotationThree);
		mat2 m2NormalRotThree = mat2(globalLayout.f4WaterNormalRotationThree.xzyw);
		vec3 f3Accum = vec3(0.0f);
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexThree], fSizeThree, f2ReducedOriginThree, f2ReducedTimeThree, m2UvRotThree, kfWaterNormalThreeOctaveOneSizeMultiplier, 1.7f, vec2(1.0f, 1.1f), pPatchTriangles[2], globalLayout.fWaterNormalPatchBlendThree)
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexThree], fSizeThree, f2ReducedOriginThree, f2ReducedTimeThree, m2UvRotThree, kfWaterNormalThreeOctaveTwoSizeMultiplier, 1.8f, vec2(1.2f, 1.3f), pPatchTriangles[2], 0.0f)
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexThree], fSizeThree, f2ReducedOriginThree, f2ReducedTimeThree, m2UvRotThree, kfWaterNormalThreeOctaveThreeSizeMultiplier, 1.9f, vec2(1.4f, 1.5f), pPatchTriangles[2], 0.0f)
		f3SampledNormalThree = f3Accum;
		f3SampledNormalThree.xy = m2NormalRotThree * f3SampledNormalThree.xy;
	}

	#undef SAMPLE_NORMAL_PRECISE

	// Guard against NaN: if all three weight sliders resolve to 0 the sum is the zero vector and normalize() returns NaN.
	vec3 f3WeightedSum = fWeightOne * f3SampledNormalOne + fWeightTwo * f3SampledNormalTwo + fWeightThree * f3SampledNormalThree;
	result.f3SampledNormal = f3WeightedSum / max(length(f3WeightedSum), kfEpsilon);
	return result;
}
