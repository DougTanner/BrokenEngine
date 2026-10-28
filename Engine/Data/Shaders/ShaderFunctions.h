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

vec2 WorldToVisibleArea(vec3 f3WorldPosition, vec4 f4VisibleArea)
{
	float fMultiplierX = 1.0f / (f4VisibleArea.z - f4VisibleArea.x);
	float fMultiplierY = 1.0f / (f4VisibleArea.y - f4VisibleArea.w);
	return vec2(fMultiplierX * (f3WorldPosition.x - f4VisibleArea.x), 1.0f - fMultiplierY * (f3WorldPosition.y - f4VisibleArea.w));
}

// Inverse of WorldToVisibleArea (incl. its Y-flip): a [0,1] texel UV back to world XY for the given area.
vec2 VisibleAreaToWorld(vec2 f2Uv, vec4 f4VisibleArea)
{
	return vec2(f4VisibleArea.x + f2Uv.x * (f4VisibleArea.z - f4VisibleArea.x),
	            f4VisibleArea.w + (1.0f - f2Uv.y) * (f4VisibleArea.y - f4VisibleArea.w));
}

vec4 ShadowStretchProjection(GlobalLayout globalLayout, vec3 f3WorldPosition, vec3 f3ObjectPosition)
{
	vec2 f2Lateral = f3WorldPosition.xy - f3ObjectPosition.xy;
	float fAwayExtent = max(dot(f2Lateral, globalLayout.f2ObjectShadowsAwayLight), 0.0f);
	vec3 f3ShadowPosition = f3WorldPosition;
	f3ShadowPosition.xy += vec2(0.0f, -globalLayout.fObjectShadowsSouthOffset)
	                    + globalLayout.f2ObjectShadowsAwayLight * globalLayout.fObjectShadowsExtension * fAwayExtent;
	vec2 f2VisibleAreaUV = WorldToVisibleArea(f3ShadowPosition, globalLayout.f4VisibleArea);
	return vec4(2.0f * f2VisibleAreaUV.x - 1.0f, 1.0f - 2.0f * f2VisibleAreaUV.y, 0.0f, 1.0f);
}

// The fragment-to-eye ray's XY at flight plane iPlane's height; a fragment above the plane reads at its own XY.
vec2 FlightPlanePosition(GlobalLayout globalLayout, MainLayout mainLayout, vec3 f3InPosition, int iPlane)
{
	vec3 f3ToEyeNormal = normalize(mainLayout.f4EyePosition.xyz - f3InPosition);
	const float fPlaneHeight = globalLayout.pfFlightPlaneHeights[iPlane];
	float fMult = (fPlaneHeight - f3InPosition.z) / f3ToEyeNormal.z;
	return (f3InPosition + max(fMult, 0.0f) * f3ToEyeNormal).xy;
}

vec2 BaseHeightPosition(GlobalLayout globalLayout, MainLayout mainLayout, vec3 f3InPosition)
{
	return FlightPlanePosition(globalLayout, mainLayout, f3InPosition, 0);
}

vec3 SampleNormal(GlobalLayout globalLayout, sampler2D normalSampler, vec2 f2Position, float fSize, float fSpeed, vec2 f2Offset, vec2 f2PositionGradX, vec2 f2PositionGradY)
{
	// BC5 normal map: only RG stored, standard convention (encoded=0.5 -> 0, encoded=1 -> +1).
	vec2 f2RG = textureGrad(normalSampler, f2Offset + fSize * f2Position + fSpeed * vec2(globalLayout.fElapsedTime, globalLayout.fElapsedTime), fSize * f2PositionGradX, fSize * f2PositionGradY).rg;
	vec2 f2XY = 2.0f * f2RG - 1.0f;
	float fZ = sqrt(clamp(1.0f - dot(f2XY, f2XY), 0.0f, 1.0f));
	return vec3(f2XY, fZ);
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

float Specular(vec3 f3ToEyeNormal, vec3 f3LightNormal, vec3 f3Normal, float fSpecularOne, float fSpecularOnePower, float fSpecularTwo, float fSpecularTwoPower, float fSpecularThree, float fSpecularThreePower)
{
	vec3 f3LightReflectionNormal = reflect(f3LightNormal, f3Normal);
	float fSpecularFactor = dot(f3ToEyeNormal, f3LightReflectionNormal);
	if (fSpecularFactor <= 0.0f)
		return 0.0f;

	float fLog2 = log2(fSpecularFactor);
	return fSpecularOne   * exp2(fSpecularOnePower   * fLog2) +
	       fSpecularTwo   * exp2(fSpecularTwoPower   * fLog2) +
	       fSpecularThree * exp2(fSpecularThreePower * fLog2);
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

// Vertical, sun-independent attenuation at the fragment XY: the product of every flight plane's factor, each in
// [0, 1], so overlapping planes never shade lighter than either alone and an empty plane multiplies by exactly 1.
float SmokeShadow(GlobalLayout globalLayout, vec3 f3InPosition, sampler2D pSmokeSamplers[kiMaxFlightPlanes], float fMulti)
{
	float fShadow = 1.0f;
	for (int iPlane = 0; iPlane < int(globalLayout.uiFlightPlaneCount); ++iPlane)
	{
		vec2 f2SmokeTexcoord = WorldToSmokeTexcoord(globalLayout.pf4SmokeAreas[iPlane], f3InPosition.xy);
		float fSmokeShadow = globalLayout.fSmokeMax * texture(pSmokeSamplers[iPlane], f2SmokeTexcoord).x;
		fSmokeShadow = clamp(pow(fSmokeShadow, globalLayout.fSmokePower), 0.0f, 1.0f);
		fShadow *= 1.0f - fMulti * fSmokeShadow;
	}
	return fShadow;
}

// SmokeShadow with each plane's factor faded by the fragment's height above that plane: full for planes above the
// fragment, zero once the fragment is 1 / fSmokeObjectHeightInverse above the plane.
float SmokeShadowFaded(GlobalLayout globalLayout, vec3 f3InPosition, sampler2D pSmokeSamplers[kiMaxFlightPlanes])
{
	float fShadow = 1.0f;
	for (int iPlane = 0; iPlane < int(globalLayout.uiFlightPlaneCount); ++iPlane)
	{
		vec2 f2SmokeTexcoord = WorldToSmokeTexcoord(globalLayout.pf4SmokeAreas[iPlane], f3InPosition.xy);
		float fHeightFraction = clamp((f3InPosition.z - globalLayout.pfFlightPlaneHeights[iPlane]) * globalLayout.fSmokeObjectHeightInverse, 0.0f, 1.0f);
		float fSmokeFade = 1.0f - fHeightFraction * fHeightFraction;
		float fSmokeShadow = globalLayout.fSmokeMax * texture(pSmokeSamplers[iPlane], f2SmokeTexcoord).x;
		fSmokeShadow = clamp(pow(fSmokeShadow, globalLayout.fSmokePower), 0.0f, 1.0f);
		fShadow *= 1.0f - fSmokeFade * fSmokeShadow;
	}
	return fShadow;
}

vec3 AddSmoke(GlobalLayout globalLayout, vec3 f3InColor, vec2 f2InPosition, sampler2D smokeSampler, int iPlane, float fInMax)
{
	vec2 f2SmokeTexcoord = WorldToSmokeTexcoord(globalLayout.pf4SmokeAreas[iPlane], f2InPosition);
	float fSmoke = globalLayout.fSmokeMax * texture(smokeSampler, f2SmokeTexcoord).x;
	fSmoke = clamp(pow(fSmoke, globalLayout.fSmokePower), 0.0f, 1.0f);
	float fDensity = globalLayout.fSmokeColorMin + globalLayout.fSmokeColorMultiplier * fSmoke;
	fSmoke *= fInMax;

	vec3 f3Final = globalLayout.f4SmokeBaseLighting.xyz;
	f3Final = min(vec3(1.0f, 1.0f, 1.0f), f3Final);
	return (1.0f - fSmoke) * f3InColor + fSmoke * f3Final * min(vec3(1.25f, 1.25f, 1.25f), vec3(fDensity, fDensity, fDensity));
}

vec3 BlendSmokePrecomputed(vec3 f3Color, float fSmokePow, GlobalLayout globalLayout)
{
	float fSmokeDensity = globalLayout.fSmokeColorMin + globalLayout.fSmokeColorMultiplier * fSmokePow;
	vec3 f3SmokeLighting = globalLayout.f4SmokeBaseLighting.xyz;
	f3SmokeLighting = min(vec3(1.0f), f3SmokeLighting);
	return (1.0f - fSmokePow) * f3Color + fSmokePow * f3SmokeLighting * min(vec3(1.25f), vec3(fSmokeDensity));
}

// Blend weight in [0, 1] for every wind-driven Low/High pair, from the wind velocity magnitude: the wind solver's own
// constants and the smoke curl strength share it so both agree on what counts as calm and strong wind.
float WindMagnitudeFactor(GlobalLayout globalLayout, float fMagnitude)
{
	return clamp((fMagnitude - globalLayout.fWindThresholdLow) / max(globalLayout.fWindThresholdHigh - globalLayout.fWindThresholdLow, 0.001f), 0.0f, 1.0f);
}

