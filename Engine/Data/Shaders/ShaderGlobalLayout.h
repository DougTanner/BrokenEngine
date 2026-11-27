#if defined(BT_ENGINE)
#pragma once
#endif
#if !defined(SHADER_GLOBAL_LAYOUT_H)
#define SHADER_GLOBAL_LAYOUT_H
// One texture read's stationary patch lattice (ShaderPatchBlending.h MakePatchTriangle), from engine::PopulatePatchLattice;
// the lattice has 2^iExponent vertices per axial cell step.
struct PatchLatticeLayout
{
	int32_t iExponent INIT;
	uint32_t uiSeed INIT; // The read's fixed seed mixed with iExponent CPU-side, so the shader hashes only the vertex identity
	uint32_t puiAnchor[2] INIT; // Camera cell q, r bits; for iExponent < 0 their floor quotients by 2^-iExponent
	vec2 f2Remainder INIT; // For iExponent < 0, the camera cell's q, r remainders times 2^iExponent; 0 otherwise
};

// A world rectangle's world-to-UV mapping (WorldToVisibleArea), from engine::PopulateAreaToUv. xy is the scale and
// zw the offset, with the area origin and the Y flip folded in on the CPU, so the shader applies them to the raw
// world position with no subtraction.
struct AreaToUvLayout
{
	vec4 f4ScaleOffset INIT; // (1 / width, -1 / height, -left / width, top / height)
};

// LightingUniforms.cpp precomputes per-pass loop counts and ideal EWNS tap-weight sum reciprocals for LightingSpread.frag.
struct SpreadPassNormalizationLayout
{
	vec2 f2WeightSumInverse INIT; // (1 / eastWestWeightSum, 1 / northSouthWeightSum)
	uint32_t uiRingCount INIT;
	uint32_t uiDirectionCountBase INIT;
};

struct GlobalLayout
{
	float fElapsedTime INIT;
	float fBaseHeight INIT;
	float fBaseHeightInverse INIT; // 1 / max(fBaseHeight, 0.001) (Terrain.frag below-base height ratio)
	float fAspectRatioInverse INIT; // 1 / aspect ratio (Billboards.vert screen-space width divide, folded CPU-side)

	vec2 f2CameraPosition INIT;
	vec4 f4VisibleArea INIT;
	AreaToUvLayout visibleAreaToUv INIT; // f4VisibleArea's world-to-UV mapping
	AreaToUvLayout objectShadowsAreaToUv INIT; // World-to-UV mapping of the area the object-shadow textures were last rasterized for, the visible area scaled by the extra area; held between refreshes
	vec4 f4PatchInverseBasis INIT; // Column-major mat2 inverse of the axial cell basis: camera-cell position to cell steps (MakePatchTriangle)
	float fPatchWeightThreshold INIT; // MakePatchTriangle's weight threshold, its subtracted term capped just below the largest normalized cube
	vec4 f4ShadowArea INIT;
	AreaToUvLayout shadowAreaToUv INIT; // f4ShadowArea's world-to-UV mapping
	vec4 f4ShadowAreaExtra INIT;
	AreaToUvLayout shadowAreaPreviousToUv INIT; // World-to-UV mapping of f4ShadowArea as it stood before the latest refresh, for ShadowTemporal.comp reprojection

	vec4 f4SunMoonNormal INIT;
	vec4 f4ModelSunMoonNormal INIT; // Normalized model sun rotation applied after the world sun/moon tilt; w unused
	vec4 f4AmbientColor INIT;

	// Precomputed day-cycle products (CPU-folded uniform-only terms; GlobalUniforms.cpp
	// PopulateDayCycleColors / PopulateSunMoonDirection / PopulateShadowParameters). Every operand is
	// invocation-invariant, so folding them removes the equivalent per-pixel multiplies/dots/normalizes. vecSunMoon and
	// vecMoon are PopulateDayCycleColors's CPU sun and moon colors.
	vec4 f4SunColorTerrain INIT;      // fSunIntensityTerrain * vecSunMoon (SunLighting)
	vec4 f4MoonColorTerrain INIT;     // fMoonIntensityTerrain * vecMoon (SunLighting)
	vec4 f4TerrainSnowSunNormal INIT; // fTerrainSnowBlend * f4SunMoonNormal (Terrain.frag snow sun-normal tilt); w unused
	vec4 f4WaterBiasedSunNormal INIT; // normalize((0,0,fLightingWaterSkyboxSunBias) + f4SunMoonNormal) (Water.frag skybox specular); w unused
	vec4 f4SmokeBaseLighting INIT;    // max(fSunIntensitySmoke*vecSunMoon, fMoonIntensitySmoke*vecMoon) + f4AmbientColor (AddSmoke/BlendSmokePrecomputed); w unused
	vec4 f4WaterSunOrMoon INIT;       // max(fSunIntensityWater*vecSunMoon, fMoonIntensityWater*vecMoon) (Water.frag skybox sun tint); w unused
	vec4 f4AmbientUnshadowed INIT;    // (1 - fShadowAffectAmbient) * f4AmbientColor (SunLighting ambient, unshadowed half); w unused
	vec4 f4AmbientShadowed INIT;      // fShadowAffectAmbient * f4AmbientColor (SunLighting ambient, shadowed half); w unused
	float fSunMagnitudeTerrain INIT;        // Rec.601 luma of f4SunColorTerrain (SunLighting ambient-shadow blend)
	float fMoonMagnitudeTerrain INIT;       // Rec.601 luma of f4MoonColorTerrain (SunLighting ambient-shadow blend)
	float fSunMoonMagnitudeSumInverseTerrain INIT; // 1 / max(fSunMagnitudeTerrain + fMoonMagnitudeTerrain, 0.001)
	float fWaterSunScalar INIT;       // (f4WaterSunOrMoon.x+y+z)/3 (Water.frag sun contribution)
	float fWaterAmbientScalar INIT;   // (f4AmbientColor.x+y+z)/3 (Water.frag ambient contribution)
	float fWaterSunWeight INIT;       // Rec.601 luma of fSunIntensityWater*vecSunMoon (Water.frag effective-shadow blend)
	float fWaterMoonWeight INIT;      // Rec.601 luma of fMoonIntensityWater*vecMoon (Water.frag effective-shadow blend)
	float fWaterShadowWeightSumInverse INIT; // 1 / max(fWaterSunWeight + fWaterMoonWeight, 0.001)
	float fSmokeLightingCombinedMultiplier INIT; // gSmokeLightingMultiplier * fLightingTimeOfDayMultiplier (AddSmoke/BlendSmokePrecomputed)

	// Model.frag uses separate direct BRDF and IBL specular products. IBL stays linear in gPbrSun
	// because its Rec.709 luminance uses the unscaled sun/moon color. w unused.
	vec4 f4PhysicallyBasedRenderingSunColorObjects INIT;     // fSunIntensityObjects * gPbrSun * vecSunMoon (Model.frag direct BRDF sun term)
	vec4 f4PhysicallyBasedRenderingMoonColorObjects INIT;    // fMoonIntensityObjects * gPbrSun * vecMoon (Model.frag direct BRDF moon term)
	vec4 f4PhysicallyBasedRenderingSunColorObjectsImageBasedLighting INIT;  // (dot(vecSunMoon.rgb, kRec709) / fPbrDayBrightness) * f4PhysicallyBasedRenderingSunColorObjects (Model.frag IBL specular sun term)
	vec4 f4PhysicallyBasedRenderingMoonColorObjectsImageBasedLighting INIT; // (dot(vecMoon.rgb, kRec709) / fPbrDayBrightness) * f4PhysicallyBasedRenderingMoonColorObjects (Model.frag IBL specular moon term)

	vec4 f4SmokeArea INIT; // The world area the smoke texture lies in this frame; wind shares it
	vec4 f4PreviousSmokeArea INIT; // The area held before this frame's refresh, equal to f4SmokeArea on a skip; wind shares it
	vec2 f2PreviousSmokeAreaSizeInverse INIT; // (1/(z-x), 1/(w-y)) of f4PreviousSmokeArea (Smoke/Wind OccupancyDilate previous-area remap)
	float fSmokeMax INIT;
	float fSmokePower INIT;
	float fSmokeDecay INIT; // gSmokeDecay^fSmokeStepCount: one refresh decays for every frame since the previous refresh
	float fSmokeStepCount INIT; // Frames since the previous smoke refresh; scales spread offsets and edge decay
	float fSmokeColorMin INIT;
	float fSmokeColorMultiplier INIT;
	float fSmokeIntensityFalloff INIT;
	float fSmokeWindNoiseScale INIT;
	float fSmokeWindNoiseQuantity INIT;
	float fSmokeNoiseQuantity INIT;
	float fSmokeNoiseScaleOne INIT;
	float fSmokeNoiseScaleTwo INIT;
	float fSmokeCurlScale INIT; // Noise UV per world meter (SmokeCurlOffset); left raw because it multiplies the per-invocation world position
	vec2 f2SmokeSwirlTimeOffset INIT; // Circular time offset of the swirl noise in noise UV, from fElapsedTime only (SmokeSpread)
	vec2 f2SmokeWindTimeOffset INIT; // (sin, cos) of 0.5 * fElapsedTime: the wind-noise world-position offset in meters, from fElapsedTime only (SmokeSpread)
	vec2 f2SmokeCurlTimeOffset INIT; // gSmokeCurlSpeed-scaled circular time offset in noise UV, from fElapsedTime only (SmokeCurlOffset)
	vec2 f2SmokeCurlOffsetScale INIT; // Finer smoke texel in meters / signed extents of f4SmokeArea: smoke UV per unit curl per unit strength per step; 0 (both curl strengths 0) disables (SmokeCurlOffset)
	float fSmokeCurlStrengthLow INIT; // Calm-wind curl strength; left raw because SmokeCurlOffset blends it toward High by the per-invocation wind factor
	float fSmokeCurlStrengthHigh INIT; // Strong-wind curl strength; left raw for the same per-invocation blend
	float fSmokeObjectHeightInverse INIT;
	float fSmokeObjectHeightPower INIT;
	float fSmokeEdgeDecayDistanceInverse INIT;
	uint32_t uiSmokeTilesX INIT;
	uint32_t uiSmokeTilesY INIT;
	float fWindDisplacementNoiseScale INIT;
	float fWindSmokeAdvection INIT;
	float fWindAdvectionScaleHigh INIT;
	float fWindAdvectionScaleLow INIT;
	float fWindSwirlScaleHigh INIT;
	float fWindSwirlScaleLow INIT;
	float fWindSwirlAmountHigh INIT;
	float fWindSwirlAmountLow INIT;
	float fWindSwirlSpeedHigh INIT;
	float fWindSwirlSpeedLow INIT;
	float fWindVorticityConfinementHigh INIT;
	float fWindVorticityConfinementLow INIT;
	float fWindDecayHigh INIT;
	float fWindDecayLow INIT;
	float fWindMomentumHigh INIT;
	float fWindMomentumLow INIT;
	float fWindThresholdLow INIT;
	float fWindThresholdHigh INIT;
	float fWindToSmokeStrength INIT;
	float fWindTimeScale INIT;
	float fWindTime INIT;
	float fWindSmokeRetention INIT;
	float fWindToSmokePower INIT;
	float fWindDiffusionHigh INIT;
	float fWindDiffusionLow INIT;
	float fWindTextureIndex INIT; // Blend factor: 0.0 = TextureOne, 1.0 = TextureTwo (continuous for interpolation)
	uint32_t uiWindTilesX INIT;
	uint32_t uiWindTilesY INIT;
	vec2 f2WindTilesInverse INIT; // (1/uiWindTilesX, 1/uiWindTilesY) (WindOccupancyDilate output-tile-center UV)

	float fLightingObjectsAdd INIT;
	float fLightingDepositCompress INIT;
	// Uchimura tone curve inputs (LightCombine.comp / DebugTexture.frag). fCombineLinearStart and fCombineBlackTightness
	// resolve CPU-side from normalized wrapper fractions, fCombineLinearLength folds into fCombineShoulderInputStart and
	// fCombineShoulderOutputStart, and fCombineShoulderExponentCoefficient is -a / (P - S1) (the gCombineLinearStart wrapper bounds in LightingWrappersBase.h own P - S1's minimum).
	float fCombineMaxBrightness INIT;
	float fCombineContrast INIT;
	float fCombineLinearStart INIT;
	float fCombineToe INIT;
	float fCombineBlackTightness INIT;
	float fCombineShoulderInputStart INIT;
	float fCombineShoulderOutputStart INIT;
	float fCombineShoulderExponentCoefficient INIT;
	float fCombineHuePreserve INIT;
	// Pass normalization/exposure scaling precomputed: fCombinePassNormalizationScale = passNorm * passScale (DebugTexture.frag),
	// fCombinePassTotalScale = fCombinePassNormalizationScale / passCount (LightCombine.comp averaging).
	float fCombinePassNormalizationScale INIT;
	float fCombinePassTotalScale INIT;
	float pfCombineCurvePoints[kiMaxSpreadPasses] INIT; // Per-pass combine curve weights: LightingSpread.frag indexes by pass index; entries at or past fSpreadPassCount are unused
	SpreadPassNormalizationLayout pSpreadPassNormalizations[kiMaxSpreadPasses] INIT; // Per-pass spread loop counts and EW/NS weight-sum reciprocals: LightingSpread.frag indexes by pass index; entries at or past fSpreadPassCount are unused
	float fLightingTerrain INIT;
	float fLightingObjects INIT;

	float fLightingAddTerrain INIT;
	float fSpreadDirectionalityStart INIT;
	vec4 f4LightingArea INIT; // Current lighting area (deposit, spread, combine, temporal, consumer reads)
	AreaToUvLayout lightingAreaToUv INIT; // f4LightingArea's world-to-UV mapping
	AreaToUvLayout lightingAreaPreviousToUv INIT; // World-to-UV mapping of the lighting area before this refresh, for LightingTemporal.comp reprojection
	float fLightingTemporalBlend INIT; // EMA weight toward current; 1.0 on the first refresh so seeded history is never shown
	vec2 f2LightingDepositSizeInverse INIT; // 1 / (lightTiles * kiComputeTileSize) (LightingDepositEdgeFade)

	// Spread Start (radial directional spread)
	float fSpreadDistanceStart INIT;
	float fSpreadDecayStart INIT;
	float fSpreadAccumulationDecayStart INIT;
	float fSpreadDistanceFalloffStart INIT;
	float fSpreadOutputCompressStart INIT;
	float fSpreadPassCount INIT;

	// Spread End (interpolation targets for last spread pass)
	float fSpreadDirectionalityEnd INIT;
	float fSpreadDistanceEnd INIT;
	float fSpreadDecayEnd INIT;
	float fSpreadAccumulationDecayEnd INIT;
	float fSpreadDistanceFalloffEnd INIT;
	float fSpreadOutputCompressEnd INIT;

	float fSpreadHeightMultiplier INIT;
	float fSpreadHeightEndHeightInverse INIT; // 1 / max(spreadHeightEndHeight, 0.001) (LightingSpread.frag height fade)
	float fSpreadHeightPower INIT;

	float fShadowWidthScale INIT;
	float fShadowDirectionMultiplier INIT;
	float fShadowMoonMultiplier INIT;
	float fShadowFeather INIT;
	float fShadowDistanceFalloff INIT;
	float fShadowDistanceFalloffInverse INIT;
	float pfShadowBlurWeights[kiShadowBlurRadius + 1] INIT; // Symmetric Gaussian half-kernel: ShadowBlurH/V index by abs(offset), scale by fShadowBlurWeightSumInverse.
	float fShadowBlurWeightSumInverse INIT;
	float fShadowAngleOffsetSum INIT; // Shadow sun angle + noon + sunset feather offsets, summed CPU-side (Shadow.comp feather term).
	float pfObjectShadowsBlurWeights[kiMaxObjectShadowsBlurRadius + 1] INIT; // Pre-normalized Gaussian half-kernel: ObjectShadowsBlurH/V index by abs(offset); entries past iObjectShadowsBlurRadius are unused
	float fObjectShadowsIntensity INIT;
	float fObjectShadowsExtension INIT; // max(sunrise stretch cube, sunset stretch cube); the vertex supplies the world-X box width and power profile.
	float fObjectShadowsStretchPower INIT; // Positive exponent of normalized world-X distance from the sun-facing box edge.
	int32_t iObjectShadowsBlurRadius INIT;
	float fObjectShadowsAwayLightX INIT; // Opposite active sun/moon X sign; zero leaves world X unchanged.
	float fObjectShadowsTerrainShadowFadeInverse INIT; // 1 / gObjectShadowsTerrainShadowFade (Model.frag terrain-shadow fade by height above fBaseHeight)
	float fObjectShadowsUnderwaterFadeInverse INIT; // 1 / gObjectShadowsUnderwaterFade (1/meters); surface height remains per-fragment.
	float fShadowAffectAmbient INIT;
	float fWaterReducedNoiseOriginX INIT;
	int32_t iShadowElevationSize INIT;
	int32_t iShadowIncrement INIT;
	int32_t iShadowStartOffset INIT;
	float fWaterReducedNoiseOriginY INIT;
	float fShadowTemporalBlend INIT; // ShadowTemporal.comp: weight of the current frame (1.0 = no history)

	// Heightmap pixels carry heights in meters directly.
	float fIslandAmbientOcclusion INIT;
	vec2 f2WaterFoamReducedTextureOrigin INIT;
	float fWaterFoamTextureInverseTileSize INIT; // 1 / tile size; material position and derivatives remain per-fragment
	float fWaterFoamTextureStrength INIT;
	float fWaterFoamEdgeSoftness INIT;
	float fWaterFoamErosionCurve INIT;
	float fWaterFoamDistanceSoften INIT;
	vec2 f2WaterFoamBreakupReducedTextureOrigin INIT;
	float fWaterFoamBreakupInverseTileSize INIT; // 1 / breakup tile size; material position and derivatives remain per-fragment
	float fWaterFoamBreakupAmount INIT;
	uint32_t uiWaterFoamTextureIndex INIT;
	uint32_t uiWaterFoamBreakupTextureIndex INIT;
	vec4 f4WaterFoamArea INIT;
	vec4 f4WaterFoamPreviousArea INIT;
	vec2 f2WaterFoamAreaSize INIT; // (right-left, top-bottom), CPU-folded texel-center mapping
	vec2 f2WaterFoamAreaInverseSize INIT; // 1 / current area size, CPU-folded fragment mapping
	vec2 f2WaterFoamPreviousAreaInverseSize INIT; // 1 / previous area size, CPU-folded history reprojection
	vec2 f2WaterFoamSpawnNoiseReducedOrigin INIT; // absolute camera / spawn tile size modulo 2, CPU-folded mirrored phase
	float fWaterFoamSpawnNoiseInverseTileSize INIT; // 1 / spawn tile size; native local position remains per-sample
	float fWaterFoamSpawnNoiseAmount INIT;
	float fWaterFoamEmissionRate INIT;
	float fWaterFoamJStart INIT;
	float fWaterFoamJInverseRange INIT; // 1 / (Jstart - min(Jfull, Jstart-0.001)), CPU-folded source threshold
	float fWaterFoamDecayRate INIT; // ln(2) / half-life, CPU-folded exact integration
	float fWaterFoamDeltaTime INIT;
	int iWaterFoamReset INIT;
	int iWaterFoamEnabled INIT;
	float fWaterReducedNormalOriginX INIT;
	float fWaterReducedNormalOriginY INIT;
	float fWaterReducedNormalOriginTwoX INIT;
	float fWaterReducedNormalOriginTwoY INIT;
	float fWaterReducedNormalOriginThreeX INIT;
	float fWaterReducedNormalOriginThreeY INIT;
	vec4 f4WaterNormalRotationOne INIT;
	vec4 f4WaterNormalRotationTwo INIT;
	vec4 f4WaterNormalRotationThree INIT;

	float fWaterOriginX INIT;
	float fWaterOriginY INIT;
	float fWaterTerrainHeight INIT;
	float fWaterTerrainFadeInverse INIT; // 1 / gWaterTerrainFade (Water.frag alpha fade); unguarded.
	float fWaterDepthLookupTableFeather INIT;
	float fWaterDepthColorFeather INIT;
	float fWaterDepthColorFloor INIT;
	float fWaterUnderseaCompressionInverse INIT; // 1 / gWaterUnderseaCompression = TerrainElevation.frag undersea depth-curve pow() exponent
	float fWaterDepthReflectionFeatherInverse INIT; // 1 / (fDayPercent * gWaterDepthReflectionFeather); unguarded (night dayPercent=0 -> +inf absorbed by the shader clamp)
	float fWaterColorNoiseFrequency INIT;
	float fWaterDepthLookupTableSunsetFade INIT;
	float fWaterFresnel INIT;
	float fWaterColorBottom INIT;
	float fWaterColorHeightInverse INIT;
	float fWaterColorNoiseAmount INIT;
	float fWaterColorNoiseWeightOne INIT;
	float fWaterColorNoiseWeightTwo INIT;
	float fWaterColorNoiseMultiplierOne INIT;
	float fWaterColorNoiseMultiplierTwo INIT;
	float fWaterDirectional INIT;
	float fWaterBreakEndDepthInverse INIT; // 1 / gWaterBreakEndDepth, CPU-folded for WaterDisplacement.comp and WaterSpecular.h
	float fWaterBreakBlendStart INIT; // min(gWaterBreakStartDepth, gWaterBreakEndDepth), CPU-folded for WaterDisplacement.comp
	float fWaterBreakBlendInverseRange INIT; // 1 / (gWaterBreakEndDepth - fWaterBreakBlendStart) when positive, 0 for the hard step, CPU-folded for WaterDisplacement.comp
	float fWaterBreakBlendCurve INIT; // gWaterBreakBlendCurve exponent for pow(t, exponent); 1 is linear, below 1 introduces low earlier
	float fWaterMediumShoreFadeInverseWidth INIT; // 1 / gWaterMediumShoreSoftness when positive (0 disables), CPU-folded for the WaterDisplacement.comp medium-only shore fade
	float fWaterLowSteepness INIT;
	float fWaterMediumSteepness INIT;
	float fWaterWaveNormalBlend INIT;
	float fWaterLowAmplitude INIT;
	float fWaterMediumAmplitude INIT;
	float fWaterReducedNormalTimeX INIT;
	float fWaterReducedNormalTimeY INIT;
	float fWaterReducedNormalTimeTwoX INIT;
	float fWaterReducedNormalTimeTwoY INIT;
	float fWaterReducedNormalTimeThreeX INIT;
	float fWaterReducedNormalTimeThreeY INIT;
	// Randomized patch blend strengths in [0, 1]; 0 keeps the plain read. One lattice per blended texture read, at that
	// read's own frequency.
	float fWaterNormalPatchBlendOne INIT; // Band One's octave One read; octaves Two and Three stay plain
	float fWaterNormalPatchBlendTwo INIT;
	float fWaterNormalPatchBlendThree INIT;
	float fWaterColorNoisePatchBlend INIT; // Covers both color-noise octaves
	float fWaterFoamTexturePatchBlend INIT;
	float fWaterFoamBreakupPatchBlend INIT;
	float fWaterFoamSpawnNoisePatchBlend INIT;
	int32_t iWaterNormalExtraSamplePatchBlend INIT; // Nonzero: Skybox One's extra normal samples blend band octave One reads too
	PatchLatticeLayout pWaterNormalPatchLattices[3] INIT; // Octave One of bands One, Two, Three
	PatchLatticeLayout pWaterColorNoisePatchLattices[2] INIT; // Octaves One, Two
	PatchLatticeLayout waterFoamTexturePatchLattice INIT;
	PatchLatticeLayout waterFoamBreakupPatchLattice INIT;
	PatchLatticeLayout waterFoamSpawnNoisePatchLattice INIT;

	float fParticlesStretchVelocityStart INIT;
	float fParticlesStretchVelocityMultiplier INIT;
	float fParticlesStretchRangeInverse INIT; // 1 / max(stretchVelocityEnd - stretchVelocityStart, kfEpsilon) (LongParticlesRender.vert length ramp)
	vec2 f2ParticlesBasisShift INIT; // Whole-cell offset of a camera cell change, else 0 (ParticlesUpdate.comp moves live particles into the new cell's frame).

	vec2 f2ShadowTextureSizeInverse INIT; // 1 / shadow texture extent (ShadowBlurH/V, ShadowTemporal).
	vec2 f2ShadowElevationTextureSizeInverse INIT; // 1 / elevation texture extent (Shadow.comp).
	float fShadowHeightFadeBottom INIT;
	float fShadowHeightFadeRangeInverse INIT; // 1 / (fade top - fade bottom) (Shadow.comp height fade); unguarded.

	float fTerrainSnowAmbientOcclusionExclusion INIT;

	float fTerrainRockSize INIT;
	float fTerrainRockBlend INIT;
	float fTerrainRockNormalsSizeOne INIT;
	float fTerrainRockNormalsSizeTwo INIT;
	float fTerrainRockNormalsSizeThree INIT;
	float fTerrainRockNormalsBlend INIT;

	float fTerrainBeachSandSize INIT;
	float fTerrainBeachSandBlend INIT;
	float fTerrainBeachNormalsSizeOne INIT;
	float fTerrainBeachNormalsSizeTwo INIT;
	float fTerrainBeachNormalsSizeThree INIT;
	float fTerrainBeachNormalsBlend INIT;

	float fLightingTimeOfDayMultiplier INIT;
	float fLightingWaterSkyboxOne INIT;
	float fLightingWaterSkyboxNormalSoften INIT;

	float fDebugTextureIndex INIT;
	float fDebugTextureFormat INIT;
	float fDebugTextureLinearRange INIT;
	float fSeaFloorElevation INIT;
	float fSeaFloorElevationInverse INIT; // 1 / fSeaFloorElevation (TerrainElevation.frag undersea depth normalization)
	float fUnderwaterMaskThreshold INIT;
	float fDebugTerrainElevationHigh INIT;
};

#endif // SHADER_GLOBAL_LAYOUT_H
