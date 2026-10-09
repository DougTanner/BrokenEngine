// Sample wind field from the current ping-pong buffer (wind is in current-area UV)
vec2 SmokeWindSample(GlobalLayout globalLayout, sampler2D windTextureSamplerOne, sampler2D windTextureSamplerTwo, vec2 f2WindTextureCoordinate)
{
	return globalLayout.fWindTextureIndex < 0.5f ? textureLod(windTextureSamplerOne, f2WindTextureCoordinate, 0.0f).rg : textureLod(windTextureSamplerTwo, f2WindTextureCoordinate, 0.0f).rg;
}

// Spread smoke using noise displacement and wind-driven advection.
// f2InputTextureCoordinate:  UV in the previous-refresh smoke texture's coord system (= previous-area UV).
// f2WorldPosition:  world position of this output texel (computed from the area the texel lies in + output UV).
// f2WindSample:  current-frame wind velocity at this output texel (SmokeWindSample).
// Decoupling the input UV, world position, and the wind sample's UV lets the smoke world-area shift AND scale between refreshes.
vec4 SmokeSpread(GlobalLayout globalLayout, sampler2D textureSampler, sampler2D noiseTextureSampler, vec2 f2InputTextureCoordinate, vec2 f2WorldPosition, vec2 f2WindSample, float fNoiseScale)
{
	vec2 f2WindWorldPosition = f2WorldPosition + vec2(sin(0.5f * globalLayout.fElapsedTime), cos(0.5f * globalLayout.fElapsedTime));
	float fWindNoiseSample = -1.0f + 2.0f * textureLod(noiseTextureSampler, globalLayout.fSmokeWindNoiseScale * fNoiseScale * f2WindWorldPosition, 0.0f).x;
	float fWindNoise = max(0.0f, globalLayout.fSmokeWindNoiseQuantity * fWindNoiseSample);

	vec2 f2TimeNoise = 2.0f * vec2(-1.0f + 2.0f * sin(0.01f * globalLayout.fElapsedTime), -1.0f + 2.0f * cos(0.01f * globalLayout.fElapsedTime));
	float fSwirlNoiseSampleX = -1.0f + 2.0f * textureLod(noiseTextureSampler, f2TimeNoise + fNoiseScale * f2WorldPosition, 0.0f).x;
	float fSwirlNoiseSampleY = -1.0f + 2.0f * textureLod(noiseTextureSampler, f2TimeNoise + fNoiseScale * f2WorldPosition.yx, 0.0f).x;
	float fNoiseX = globalLayout.fSmokeNoiseQuantity * fSwirlNoiseSampleX;
	float fNoiseY = globalLayout.fSmokeNoiseQuantity * fSwirlNoiseSampleY;

	vec2 f2Noise = (fWindNoise * vec2(0.75f, 1.0f) + vec2(fNoiseX, fNoiseY)) * 0.5f;

	float fWindMagnitude = length(f2WindSample);
	float fWindMagnitudeSafe = max(fWindMagnitude, 1.0e-3f);
	float fWindMagnitudeNew = globalLayout.fWindToSmokeStrength * pow(fWindMagnitude, globalLayout.fWindToSmokePower);
	vec2 f2WindRescaled = vec2(fWindMagnitudeNew) * (f2WindSample / vec2(fWindMagnitudeSafe));
	f2WindRescaled.x = -f2WindRescaled.x;  // Additive sampling reverses direction; Y cancels with inverted texcoord Y

	float fHasWind = step(1.0e-3f, fWindMagnitude);
	// Direct wind advection: shift base sampling in wind direction (works in uniform fields)
	vec2 f2WindAdvection = fHasWind * globalLayout.fWindSmokeAdvection * f2WindRescaled;
	// Noise-modulated displacement for visual variation (works in gradient fields)
	vec2 f2WindDisplacement = globalLayout.fSmokeStepCount * globalLayout.fWindDisplacementNoiseScale * abs(fWindNoiseSample) * f2WindRescaled;
	// One refresh advances by every frame since the previous refresh
	vec2 f2Base = f2InputTextureCoordinate + globalLayout.fSmokeStepCount * f2Noise + globalLayout.fSmokeStepCount * f2WindAdvection;
	float fSmokeStayed = textureLod(textureSampler, f2Base, 0.0f).x;
	float fSmokeMoved = textureLod(textureSampler, f2Base + f2WindDisplacement, 0.0f).x;
	return globalLayout.fSmokeDecay * vec4(mix(fSmokeStayed, mix(fSmokeMoved, fSmokeStayed, globalLayout.fWindSmokeRetention), fHasWind));
}

// Per-step backward-lookup UV offset from the 2D curl (dP/dy, -dP/dx) of the noise texture read as a world-anchored
// potential P. The normalized central difference is clamped to unit length, and the strength times
// f2SmokeCurlOffsetScale scales both components by the same world-meter length, so the field is divergence-free
// wherever the clamp is inactive and the strength is uniform.
// The strength blends Low to High by the local wind's WindMagnitudeFactor, so it lies between the two sliders and the
// offset is at most that strength in smoke texels on either axis at every resolution.
// f2SmokeCurlOffsetScale divides by the signed area extents (w - y < 0), which applies the world-Y-to-UV flip to both
// components together.
vec2 SmokeCurlOffset(GlobalLayout globalLayout, sampler2D noiseTextureSampler, vec2 f2WorldPosition, float fWindMagnitude)
{
	if (globalLayout.f2SmokeCurlOffsetScale.x == 0.0f)
		return vec2(0.0f);

	// Half-step of 4 noise texels: the 8-texel lag where the glass texture's mean |dP| is about 0.08, hence the x10 normalization.
	const float kfSmokeCurlHalfStep = 4.0f / 2048.0f;
	const float kfSmokeCurlGradientNormalize = 10.0f;

	vec2 f2NoiseCoordinate = globalLayout.fSmokeCurlScale * f2WorldPosition + globalLayout.f2SmokeCurlTimeOffset;
	float fPotentialEast = textureLod(noiseTextureSampler, f2NoiseCoordinate + vec2(kfSmokeCurlHalfStep, 0.0f), 0.0f).x;
	float fPotentialWest = textureLod(noiseTextureSampler, f2NoiseCoordinate - vec2(kfSmokeCurlHalfStep, 0.0f), 0.0f).x;
	float fPotentialNorth = textureLod(noiseTextureSampler, f2NoiseCoordinate + vec2(0.0f, kfSmokeCurlHalfStep), 0.0f).x;
	float fPotentialSouth = textureLod(noiseTextureSampler, f2NoiseCoordinate - vec2(0.0f, kfSmokeCurlHalfStep), 0.0f).x;
	vec2 f2Curl = kfSmokeCurlGradientNormalize * vec2(fPotentialNorth - fPotentialSouth, fPotentialWest - fPotentialEast);
	f2Curl /= max(1.0f, length(f2Curl));
	float fStrength = mix(globalLayout.fSmokeCurlStrengthLow, globalLayout.fSmokeCurlStrengthHigh, WindMagnitudeFactor(globalLayout, fWindMagnitude));
	return fStrength * globalLayout.f2SmokeCurlOffsetScale * f2Curl;
}
