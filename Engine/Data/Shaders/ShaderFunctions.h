const vec3 kRec709 = vec3(0.2126f, 0.7152f, 0.0722f);

vec2 Rotate(vec2 f2, float f)
{
	float fSin = sin(f);
	float fCos = cos(f);
	mat2 matRotation = mat2(fCos, -fSin, fSin, fCos);
	return matRotation * f2;
}

vec4 Transform(vec4 f4Vec, vec4[4] f4x4Matrix)
{
	return vec4(dot(f4Vec, f4x4Matrix[0]), dot(f4Vec, f4x4Matrix[1]), dot(f4Vec, f4x4Matrix[2]), dot(f4Vec, f4x4Matrix[3]));
}

vec3 Transform(vec4 f4Vec, vec4[3] f3x4Matrix)
{
	return vec3(dot(f4Vec, f3x4Matrix[0]), dot(f4Vec, f3x4Matrix[1]), dot(f4Vec, f3x4Matrix[2]));
}

vec2 WorldToVisibleArea(vec3 f3WorldPosition, AreaToUvLayout areaToUv)
{
	return fma(f3WorldPosition.xy, areaToUv.f4ScaleOffset.xy, areaToUv.f4ScaleOffset.zw);
}

// Inverse of WorldToVisibleArea (incl. its Y-flip): a [0,1] texel UV back to world XY for the given area.
vec2 VisibleAreaToWorld(vec2 f2Uv, vec4 f4VisibleArea)
{
	return vec2(f4VisibleArea.x + f2Uv.x * (f4VisibleArea.z - f4VisibleArea.x),
	            f4VisibleArea.w + (1.0f - f2Uv.y) * (f4VisibleArea.y - f4VisibleArea.w));
}

vec4 ShadowStretchProjection(GlobalLayout globalLayout, vec3 f3WorldPosition, vec3 f3ObjectPosition, vec2 f2WorldXRange)
{
	vec3 f3ShadowPosition = f3WorldPosition;
	float fWidth = f2WorldXRange.y - f2WorldXRange.x;
	if (fWidth > 0.0f)
	{
		float fDirection = globalLayout.fObjectShadowsAwayLightX;
		float fNearX = fDirection > 0.0f ? f2WorldXRange.x : f2WorldXRange.y;
		float fRelativeX = f3WorldPosition.x - f3ObjectPosition.x;
		float fDistance = clamp(fDirection * (fRelativeX - fNearX) / fWidth, 0.0f, 1.0f);
		f3ShadowPosition.x += fDirection * globalLayout.fObjectShadowsExtension * fWidth * pow(fDistance, globalLayout.fObjectShadowsStretchPower);
	}
	vec2 f2ObjectShadowsUV = WorldToVisibleArea(f3ShadowPosition, globalLayout.objectShadowsAreaToUv);
	return vec4(2.0f * f2ObjectShadowsUV.x - 1.0f, 1.0f - 2.0f * f2ObjectShadowsUV.y, 0.0f, 1.0f);
}

// Unnormalized direction from f3Position toward the world camera along its eye line.
vec3 ToEyeLine(MainLayout mainLayout, vec3 f3Position)
{
	return mainLayout.f4ToEyeNormal.w > 0.5f ? mainLayout.f4ToEyeNormal.xyz : mainLayout.f4EyePosition.xyz - f3Position;
}

// The fragment-to-eye ray's XY at fBaseHeight; a fragment above the plane reads at its own XY.
vec2 BaseHeightPosition(GlobalLayout globalLayout, MainLayout mainLayout, vec3 f3InPosition)
{
	vec3 f3ToEyeNormal = normalize(ToEyeLine(mainLayout, f3InPosition));
	float fMult = (globalLayout.fBaseHeight - f3InPosition.z) / f3ToEyeNormal.z;
	return (f3InPosition + max(fMult, 0.0f) * f3ToEyeNormal).xy;
}

vec3 SampleNormal(GlobalLayout globalLayout, sampler2D normalSampler, vec2 f2Position, float fSize, float fSpeed, vec2 f2Offset, vec2 f2PositionGradX, vec2 f2PositionGradY)
{
	// BC5 normal map: only RG stored, standard convention (encoded=0.5 -> 0, encoded=1 -> +1).
	vec2 f2RG = textureGrad(normalSampler, f2Offset + fSize * f2Position + fSpeed * vec2(globalLayout.fElapsedTime, globalLayout.fElapsedTime), fSize * f2PositionGradX, fSize * f2PositionGradY).rg;
	vec2 f2XY = 2.0f * f2RG - 1.0f;
	float fZ = sqrt(clamp(1.0f - dot(f2XY, f2XY), 0.0f, 1.0f));
	return vec3(f2XY, fZ);
}

// BC5 normal map: only XY stored, reconstruct Z = sqrt(1 - X^2 - Y^2). Sign-inverted XY decode is intentional (matches per-island flip convention upstream).
vec3 DecodeWaterNormal(vec2 f2Encoded)
{
	vec2 f2XY = vec2(1.0f - 2.0f * f2Encoded.x, 1.0f - 2.0f * f2Encoded.y);
	return vec3(f2XY, sqrt(clamp(1.0f - dot(f2XY, f2XY), 0.0f, 1.0f)));
}

// fShadowSun gates the sun's directional contribution; fShadowMoon gates the moon's directional
// contribution. Callers compute fShadowMoon to bypass the terrain ray-march shadow (per the
// sun/moon split design) so moonlight is unaffected by landform self-shadowing while object/smoke
// shadows still apply to both lights. Ambient gating tracks the dominant light's shadow via a
// Rec.601 luma-weighted blend: at noon by sun-shadow, at midnight by moon-shadow (which excludes
// the terrain ray-march), with a smooth transition in twilight. Without this weighting, terrain
// in self-shadow visibly darkens during sunset/sunrise even though the moon is filling those
// regions.
vec3 SunLighting(vec3 f3MaterialColor, GlobalLayout globalLayout, vec4 f4Position, vec3 f3Normal, float fShadowSun, float fShadowMoon, float fAmbientOcclusion)
{
	// Terrain.frag is SunLighting's sole caller. GlobalUniforms.cpp folds per-target colors, Rec.601 magnitudes, and
	// ambient split on the CPU, leaving shadow terms per pixel. f3Normal must be unit; the caller supplies normalized
	// f3SunNormal, so no extra normalize is needed.
	vec3 f3Sun  = globalLayout.f4SunColorTerrain.xyz;
	vec3 f3Moon = globalLayout.f4MoonColorTerrain.xyz;
	float fNdotL = max(0.0f, dot(f3Normal, globalLayout.f4SunMoonNormal.xyz));
	vec3 f3SunLight  = fShadowSun  * fNdotL * f3Sun;
	vec3 f3MoonLight = fShadowMoon * fNdotL * f3Moon;
	float fAmbientShadow = (globalLayout.fSunMagnitudeTerrain * fShadowSun + globalLayout.fMoonMagnitudeTerrain * fShadowMoon) * globalLayout.fSunMoonMagnitudeSumInverseTerrain;
	return f3MaterialColor * fAmbientOcclusion * (max(f3SunLight, f3MoonLight) + globalLayout.f4AmbientUnshadowed.xyz + fAmbientShadow * globalLayout.f4AmbientShadowed.xyz);
}

float IntensityLighting(vec4 f4Lighting)
{
	return f4Lighting.x + f4Lighting.y + f4Lighting.z + f4Lighting.w;
}

// The transparent-black border sampler returns no lighting outside the footprint, so these are plain reads at mip 0
// (every lighting target has one mip).
void ReadLighting(inout vec4 pf4Lighting[3], sampler2D pLightingSamplers[3], vec2 f2Texcoord)
{
	pf4Lighting[0] = textureLod(pLightingSamplers[0], f2Texcoord, 0.0f);
	pf4Lighting[1] = textureLod(pLightingSamplers[1], f2Texcoord, 0.0f);
	pf4Lighting[2] = textureLod(pLightingSamplers[2], f2Texcoord, 0.0f);
}

vec3 ReadAmbientLighting(sampler2D ambientLightingSampler, vec2 f2Texcoord)
{
	return textureLod(ambientLightingSampler, f2Texcoord, 0.0f).xyz;
}

vec3 DirectionalLighting(vec4 pf4Lighting[3], vec3 f3Normal, float fIntensity, float fPower, float fPowerMode)
{
	// Guard exact (0,0,1) normals (flat-top geometry): normalize() of a zero-length xy NaNs. Zero weights = no
	// directional contribution, the intended degenerate behavior.
	float fLen = length(f3Normal.xy);
	vec2 f2Normal = fLen > kfEpsilon ? f3Normal.xy / fLen : vec2(0.0f);
	float fWeightE = max(0.0f, f2Normal.x);
	float fWeightW = max(0.0f, -f2Normal.x);
	float fWeightN = max(0.0f, f2Normal.y);
	float fWeightS = max(0.0f, -f2Normal.y);

	vec3 f3Result = vec3(pf4Lighting[0].x * fWeightE + pf4Lighting[0].y * fWeightW + pf4Lighting[0].z * fWeightN + pf4Lighting[0].w * fWeightS,
		                 pf4Lighting[1].x * fWeightE + pf4Lighting[1].y * fWeightW + pf4Lighting[1].z * fWeightN + pf4Lighting[1].w * fWeightS,
		                 pf4Lighting[2].x * fWeightE + pf4Lighting[2].y * fWeightW + pf4Lighting[2].z * fWeightN + pf4Lighting[2].w * fWeightS);

	// Skip the pow for whichever branch the mode discards at its extremes (fPowerMode is a uniform, so the branch is warp-coherent)
	bool bNeedLum = fPowerMode < 0.999f;
	bool bNeedAvg = fPowerMode > 0.001f;

	float fLuminance = dot(f3Result, kRec709);
	vec3 f3LumDir = f3Result / max(fLuminance, 0.001f);
	vec3 f3LumResult = bNeedLum ? (fIntensity * pow(fLuminance, fPower) * f3LumDir) : vec3(0.0f);

	float fAverage = (f3Result.x + f3Result.y + f3Result.z) / 3.0f;
	vec3 f3AvgDir = f3Result / max(fAverage, 0.001f);
	vec3 f3AvgResult = bNeedAvg ? (fIntensity * pow(fAverage, fPower) * f3AvgDir) : vec3(0.0f);

	return mix(f3LumResult, f3AvgResult, fPowerMode);
}

vec3 WaterLighting(vec4 pf4Lighting[3], vec3 f3Normal, float fSoften, float fOne, float fOnePower, float fTwo, float fTwoPower, float fThree, float fThreePower, float fPowerMode)
{
	// Zero-length xy normals use a zero direction to avoid undefined normalization; fSoften blends the resulting zero weights toward 0.25f.
	float fLen = length(f3Normal.xy);
	vec2 f2Normal = fLen > kfEpsilon ? f3Normal.xy / fLen : vec2(0.0f);
	float fWeightE = mix(max(0.0f, f2Normal.x), 0.25f, fSoften);
	float fWeightW = mix(max(0.0f, -f2Normal.x), 0.25f, fSoften);
	float fWeightN = mix(max(0.0f, f2Normal.y), 0.25f, fSoften);
	float fWeightS = mix(max(0.0f, -f2Normal.y), 0.25f, fSoften);

	vec3 f3Result = vec3(pf4Lighting[0].x * fWeightE + pf4Lighting[0].y * fWeightW + pf4Lighting[0].z * fWeightN + pf4Lighting[0].w * fWeightS,
		                 pf4Lighting[1].x * fWeightE + pf4Lighting[1].y * fWeightW + pf4Lighting[1].z * fWeightN + pf4Lighting[1].w * fWeightS,
		                 pf4Lighting[2].x * fWeightE + pf4Lighting[2].y * fWeightW + pf4Lighting[2].z * fWeightN + pf4Lighting[2].w * fWeightS);

	// Skip the three pow calls for whichever branch the mode discards at its extremes (fPowerMode is a uniform, so the branch is warp-coherent)
	bool bNeedLum = fPowerMode < 0.999f;
	bool bNeedAvg = fPowerMode > 0.001f;

	float fLuminance = dot(f3Result, kRec709);
	vec3 f3LumDir = f3Result / max(fLuminance, 0.001f);
	float fLumScalar = bNeedLum ? (fOne * pow(fLuminance, fOnePower) + fTwo * pow(fLuminance, fTwoPower) + fThree * pow(fLuminance, fThreePower)) : 0.0f;
	vec3 f3LumResult = fLumScalar * f3LumDir;

	float fAverage = (f3Result.x + f3Result.y + f3Result.z) / 3.0f;
	vec3 f3AvgDir = f3Result / max(fAverage, 0.001f);
	float fAvgScalar = bNeedAvg ? (fOne * pow(fAverage, fOnePower) + fTwo * pow(fAverage, fTwoPower) + fThree * pow(fAverage, fThreePower)) : 0.0f;
	vec3 f3AvgResult = fAvgScalar * f3AvgDir;

	return mix(f3LumResult, f3AvgResult, fPowerMode);
}

// Operates on a precomputed direction-averaged sum (e.g. an mAmbientCombineTexture sample).
vec3 AmbientLightingPrecomputed(vec3 f3Result, float fIntensity, float fPower, float fPowerMode)
{
	// Skip the pow for whichever branch the mode discards at its extremes (fPowerMode is a uniform, so the branch is warp-coherent)
	bool bNeedLum = fPowerMode < 0.999f;
	bool bNeedAvg = fPowerMode > 0.001f;

	float fLuminance = dot(f3Result, kRec709);
	vec3 f3LumDir = f3Result / max(fLuminance, 0.001f);
	vec3 f3LumResult = bNeedLum ? (fIntensity * pow(fLuminance, fPower) * f3LumDir) : vec3(0.0f);

	float fAverage = (f3Result.x + f3Result.y + f3Result.z) / 3.0f;
	vec3 f3AvgDir = f3Result / max(fAverage, 0.001f);
	vec3 f3AvgResult = bNeedAvg ? (fIntensity * pow(fAverage, fPower) * f3AvgDir) : vec3(0.0f);

	return mix(f3LumResult, f3AvgResult, fPowerMode);
}

vec3 AmbientLighting(vec4 pf4Lighting[3], float fIntensity, float fPower, float fPowerMode)
{
	vec3 f3Result = 0.25f * vec3(pf4Lighting[0].x + pf4Lighting[0].y + pf4Lighting[0].z + pf4Lighting[0].w,
		                         pf4Lighting[1].x + pf4Lighting[1].y + pf4Lighting[1].z + pf4Lighting[1].w,
		                         pf4Lighting[2].x + pf4Lighting[2].y + pf4Lighting[2].z + pf4Lighting[2].w);
	return AmbientLightingPrecomputed(f3Result, fIntensity, fPower, fPowerMode);
}

vec2 WorldToSmokeTexcoord(vec4 f4SmokeArea, vec2 f2Position)
{
	return vec2((f2Position.x - f4SmokeArea.x) / (f4SmokeArea.z - f4SmokeArea.x),
		        (f2Position.y - f4SmokeArea.y) / (f4SmokeArea.w - f4SmokeArea.y));
}

// The border-white sampler returns unshadowed outside the footprint, so this is a plain read.
float SampleTerrainShadow(sampler2D shadowTextureSampler, vec2 f2Uv)
{
	return textureLod(shadowTextureSampler, f2Uv, 0.0f).x;
}

// Model.frag uses the smoke amount to gate AddSmoke.
float ReadSmoke(GlobalLayout globalLayout, vec2 f2InPosition, sampler2D smokeSampler)
{
	vec2 f2SmokeTexcoord = WorldToSmokeTexcoord(globalLayout.f4SmokeArea, f2InPosition);
	float fSmoke = globalLayout.fSmokeMax * texture(smokeSampler, f2SmokeTexcoord).x;
	return clamp(pow(fSmoke, globalLayout.fSmokePower), 0.0f, 1.0f);
}

// Vertical, sun-independent attenuation at the fragment XY, scaled by fIntensity.
float SmokeShadow(GlobalLayout globalLayout, vec3 f3InPosition, sampler2D smokeSampler, float fIntensity)
{
	return 1.0f - fIntensity * ReadSmoke(globalLayout, f3InPosition.xy, smokeSampler);
}

float SmokeObjectHeightFade(GlobalLayout globalLayout, float fHeight)
{
	float fHeightFraction = clamp((fHeight - globalLayout.fBaseHeight) * globalLayout.fSmokeObjectHeightInverse, 0.0f, 1.0f);
	return 1.0f - pow(fHeightFraction, globalLayout.fSmokeObjectHeightPower);
}

// fObjectShadow darkens only the sun/moon and ambient base lighting; the EWNS light deposits stay unshadowed.
vec3 AddSmoke(GlobalLayout globalLayout, vec3 f3InColor, float fSmoke, float fInMax, vec4 pf4Lighting[3], float fObjectShadow)
{
	float fDensity = globalLayout.fSmokeColorMin + globalLayout.fSmokeColorMultiplier * fSmoke;
	fSmoke *= fInMax;

	float fRed = IntensityLighting(pf4Lighting[0]);
	float fGreen = IntensityLighting(pf4Lighting[1]);
	float fBlue = IntensityLighting(pf4Lighting[2]);
	// fSmokeLightingCombinedMultiplier (= gSmokeLightingMultiplier * fLightingTimeOfDayMultiplier) and
	// f4SmokeBaseLighting (max(sun,moon) sky term + ambient) are folded CPU-side (GlobalUniforms.cpp).
	vec3 f3Final = vec3(fRed, fGreen, fBlue) * globalLayout.fSmokeLightingCombinedMultiplier;
	f3Final += fObjectShadow * globalLayout.f4SmokeBaseLighting.xyz;

	f3Final = min(vec3(1.0f, 1.0f, 1.0f), f3Final);
	return (1.0f - fSmoke) * f3InColor + fSmoke * f3Final * min(vec3(1.25f, 1.25f, 1.25f), vec3(fDensity, fDensity, fDensity));
}

// Caller passes 4.0 * an mAmbientCombineTexture sample (recovers IntensityLighting magnitude — the texture stores 0.25*sum).
// fObjectShadow darkens only the base lighting, as in AddSmoke.
vec3 BlendSmokePrecomputed(vec3 f3Color, float fSmokePow, vec3 f3LightingSum, GlobalLayout globalLayout, float fObjectShadow)
{
	float fSmokeDensity = globalLayout.fSmokeColorMin + globalLayout.fSmokeColorMultiplier * fSmokePow;
	vec3 f3SmokeLighting = f3LightingSum * globalLayout.fSmokeLightingCombinedMultiplier;
	f3SmokeLighting += fObjectShadow * globalLayout.f4SmokeBaseLighting.xyz;
	f3SmokeLighting = min(vec3(1.0f), f3SmokeLighting);
	return (1.0f - fSmokePow) * f3Color + fSmokePow * f3SmokeLighting * min(vec3(1.25f), vec3(fSmokeDensity));
}

// Blend weight in [0, 1] for every wind-driven Low/High pair, from the wind velocity magnitude: the wind solver's own
// constants and the smoke curl strength share it so both agree on what counts as calm and strong wind.
float WindMagnitudeFactor(GlobalLayout globalLayout, float fMagnitude)
{
	return clamp((fMagnitude - globalLayout.fWindThresholdLow) / max(globalLayout.fWindThresholdHigh - globalLayout.fWindThresholdLow, 0.001f), 0.0f, 1.0f);
}

float LightingDepositEdgeFade(vec2 f2FragCoord, vec2 f2SizeInverse)
{
	vec2 f2Uv = f2FragCoord * f2SizeInverse;
	float fEdgeDist = min(min(f2Uv.x, 1.0f - f2Uv.x), min(f2Uv.y, 1.0f - f2Uv.y));
	return smoothstep(0.0f, 0.05f, fEdgeDist);
}

// Piecewise Uchimura tone curve (toe, linear, exponential shoulder). Segment constants S0/S1/CP are precomputed on the
// CPU; clamping the linear ceiling to S1 and anchoring the shoulder at S1 keeps the S0 join monotonic, and capping the
// shoulder exponent at 0 keeps inactive vector lanes finite.
vec4 Uchimura(GlobalLayout globalLayout, vec4 f4X)
{
	float fP = globalLayout.fCombineMaxBrightness;
	float fA = globalLayout.fCombineContrast;
	float fM = globalLayout.fCombineLinearStart;
	float fC = globalLayout.fCombineToe;
	float fB = globalLayout.fCombineBlackTightness;
	float fS0 = globalLayout.fCombineShoulderInputStart;
	float fS1 = globalLayout.fCombineShoulderOutputStart;
	float fCP = globalLayout.fCombineShoulderExponentCoefficient;

	vec4 f4Toe = fB + (fM - fB) * pow(clamp(f4X / fM, 0.0f, 1.0f), vec4(fC));
	vec4 f4Linear = min(fM + fA * (f4X - fM), vec4(fS1));
	vec4 f4Shoulder = min(vec4(fS1) + vec4(fP - fS1) * (vec4(1.0f) - exp(min(fCP * (f4X - fS0), 0.0f))), vec4(fP));

	vec4 f4Result = mix(f4Linear, f4Toe, lessThan(f4X, vec4(fM)));
	return mix(f4Shoulder, f4Result, lessThan(f4X, vec4(fS0)));
}
