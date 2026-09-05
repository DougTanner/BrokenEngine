// BC5 normal map: only XY stored, reconstruct Z = sqrt(1 - X^2 - Y^2). Sign-inverted XY decode is intentional (matches per-island flip convention upstream).
vec3 DecodeNormal(vec2 f2Encoded)
{
	vec2 f2XY = vec2(1.0f - 2.0f * f2Encoded.x, 1.0f - 2.0f * f2Encoded.y);
	return vec3(f2XY, sqrt(clamp(1.0f - dot(f2XY, f2XY), 0.0f, 1.0f)));
}

struct WaterNormalSamplingResult
{
	vec2 f2LocalDx;
	vec2 f2LocalDy;
	float fSizeOne;
	float fSizeTwo;
	float fSizeThree;
	float fWeightOne;
	float fWeightTwo;
	float fWeightThree;
	vec3 f3WeightedSum;
	vec3 f3SampledNormal;
};

WaterNormalSamplingResult SampleWaterNormals()
{
	WaterNormalSamplingResult result;

	// Normal map sampling with precision-safe UV computation
	result.fSizeOne = mainLayout.fLightingSampledNormalsOneSize;
	result.fSizeTwo = mainLayout.fLightingSampledNormalsTwoSize;
	result.fSizeThree = mainLayout.fLightingSampledNormalsThreeSize;
	vec2 f2ReducedOrigin = vec2(globalLayout.fWaterReducedNormalOriginX, globalLayout.fWaterReducedNormalOriginY);
	vec2 f2ReducedOriginTwo = vec2(globalLayout.fWaterReducedNormalOriginTwoX, globalLayout.fWaterReducedNormalOriginTwoY);
	vec2 f2ReducedOriginThree = vec2(globalLayout.fWaterReducedNormalOriginThreeX, globalLayout.fWaterReducedNormalOriginThreeY);
	vec2 f2ReducedTime = vec2(globalLayout.fWaterReducedNormalTimeX, globalLayout.fWaterReducedNormalTimeY);
	vec2 f2ReducedTimeTwo = vec2(globalLayout.fWaterReducedNormalTimeTwoX, globalLayout.fWaterReducedNormalTimeTwoY);
	vec2 f2ReducedTimeThree = vec2(globalLayout.fWaterReducedNormalTimeThreeX, globalLayout.fWaterReducedNormalTimeThreeY);
	result.f2LocalDx = dFdx(f2InInitialPosition);
	result.f2LocalDy = dFdy(f2InInitialPosition);

	// Per-sample weights resolved CPU-side by camera eye height (LightingUniforms.cpp), then weighted-sum-then-normalize.
	// normalize() is scale-invariant so absolute weight magnitudes don't matter; only ratios do. Weight 0 disables a
	// sample group entirely — the weight is a uniform (warp-coherent branch), so a faded-out band skips its 3 fetches.
	result.fWeightOne = mainLayout.fWaterNormalWeightOne;
	result.fWeightTwo = mainLayout.fWaterNormalWeightTwo;
	result.fWeightThree = mainLayout.fWaterNormalWeightThree;

	// m2UvRot=R(-theta) maps world UV to texture UV, making the pattern appear rotated CCW by theta; inverse
	// m2NormalRot=R(+theta) returns sampled tangent normal XY to world space. WaterUniforms.cpp rotates reducedOrigin
	// before fmod and reducedTime before accumulation/fmod. Applying rotation again breaks orientation and the integral
	// fract-wrap shift for arbitrary angles; apply m2UvRot only to camera-relative positions and derivatives, leaving
	// both reduced values unchanged.
	#define SAMPLE_NORMAL_PRECISE(sampler, size, reducedOrigin, reducedTime, m2UvRot, sizeMult, speedMult, offset) \
	{ \
		float fCallSize = sizeMult * size; \
		vec2 f2UV = offset \
			+ fCallSize * (m2UvRot * f2InInitialPosition) \
			+ sizeMult * reducedOrigin \
			+ speedMult * reducedTime; \
		vec2 f2Dx = fCallSize * (m2UvRot * result.f2LocalDx); \
		vec2 f2Dy = fCallSize * (m2UvRot * result.f2LocalDy); \
		f3Accum += DecodeNormal(textureGrad(sampler, fract(f2UV), f2Dx, f2Dy).rg); \
	}

	// Sample One (3 octaves) — atlas index selected at runtime via uiWaterNormalIndexOne.
	vec3 f3SampledNormalOne = vec3(0.0f);
	if (result.fWeightOne > 0.0f)
	{
		mat2 m2UvRotOne = mat2(globalLayout.f4WaterNormalRotationOne);
		mat2 m2NormalRotOne = mat2(globalLayout.f4WaterNormalRotationOne.xzyw);
		vec3 f3Accum = vec3(0.0f);
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexOne], result.fSizeOne, f2ReducedOrigin, f2ReducedTime, m2UvRotOne, 0.2f, 1.1f, vec2(0.1f, 0.2f))
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexOne], result.fSizeOne, f2ReducedOrigin, f2ReducedTime, m2UvRotOne, 1.1f, 1.2f, vec2(0.2f, 0.3f))
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexOne], result.fSizeOne, f2ReducedOrigin, f2ReducedTime, m2UvRotOne, 2.5f, 1.3f, vec2(0.3f, 0.4f))
		f3SampledNormalOne = f3Accum;
		f3SampledNormalOne.xy = m2NormalRotOne * f3SampledNormalOne.xy;
	}

	// Sample Two (3 octaves)
	vec3 f3SampledNormalTwo = vec3(0.0f);
	if (result.fWeightTwo > 0.0f)
	{
		mat2 m2UvRotTwo = mat2(globalLayout.f4WaterNormalRotationTwo);
		mat2 m2NormalRotTwo = mat2(globalLayout.f4WaterNormalRotationTwo.xzyw);
		vec3 f3Accum = vec3(0.0f);
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexTwo], result.fSizeTwo, f2ReducedOriginTwo, f2ReducedTimeTwo, m2UvRotTwo, 0.3f, 1.4f, vec2(0.4f, 0.5f))
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexTwo], result.fSizeTwo, f2ReducedOriginTwo, f2ReducedTimeTwo, m2UvRotTwo, 1.2f, 1.5f, vec2(0.6f, 0.7f))
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexTwo], result.fSizeTwo, f2ReducedOriginTwo, f2ReducedTimeTwo, m2UvRotTwo, 3.0f, 1.6f, vec2(0.8f, 0.9f))
		f3SampledNormalTwo = f3Accum;
		f3SampledNormalTwo.xy = m2NormalRotTwo * f3SampledNormalTwo.xy;
	}

	// Sample Three (3 octaves) — extends the One/Two octave pattern linearly.
	vec3 f3SampledNormalThree = vec3(0.0f);
	if (result.fWeightThree > 0.0f)
	{
		mat2 m2UvRotThree = mat2(globalLayout.f4WaterNormalRotationThree);
		mat2 m2NormalRotThree = mat2(globalLayout.f4WaterNormalRotationThree.xzyw);
		vec3 f3Accum = vec3(0.0f);
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexThree], result.fSizeThree, f2ReducedOriginThree, f2ReducedTimeThree, m2UvRotThree, 0.4f, 1.7f, vec2(1.0f, 1.1f))
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexThree], result.fSizeThree, f2ReducedOriginThree, f2ReducedTimeThree, m2UvRotThree, 1.3f, 1.8f, vec2(1.2f, 1.3f))
		SAMPLE_NORMAL_PRECISE(pWaterNormalSamplers[mainLayout.uiWaterNormalIndexThree], result.fSizeThree, f2ReducedOriginThree, f2ReducedTimeThree, m2UvRotThree, 3.5f, 1.9f, vec2(1.4f, 1.5f))
		f3SampledNormalThree = f3Accum;
		f3SampledNormalThree.xy = m2NormalRotThree * f3SampledNormalThree.xy;
	}

	#undef SAMPLE_NORMAL_PRECISE

	// Guard against NaN: if all three weight sliders resolve to 0 the sum is the zero vector and normalize() returns NaN.
	result.f3WeightedSum = result.fWeightOne * f3SampledNormalOne + result.fWeightTwo * f3SampledNormalTwo + result.fWeightThree * f3SampledNormalThree;
	result.f3SampledNormal = result.f3WeightedSum / max(length(result.f3WeightedSum), kfEpsilon);
	return result;
}
