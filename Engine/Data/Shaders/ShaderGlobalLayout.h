#if defined(BT_ENGINE)
#pragma once
#endif
#if !defined(SHADER_GLOBAL_LAYOUT_H)
#define SHADER_GLOBAL_LAYOUT_H
struct GlobalLayout
{
	uint32_t uiRandomSeed INIT;

	float fElapsedTime INIT;
	float fBaseHeight INIT;
	float fBaseHeightInverse INIT; // 1 / max(fBaseHeight, 0.001) (Terrain.frag below-base height ratio)
	float fAspectRatioInverse INIT; // 1 / aspect ratio (Billboards.vert screen-space width divide, folded CPU-side)

	vec2 f2CameraPosition INIT;
	vec4 f4VisibleArea INIT;
	vec4 f4ShadowArea INIT;
	vec4 f4ShadowAreaExtra INIT;
	vec4 f4ShadowAreaPrevious INIT; // Previous-frame f4ShadowArea, for ShadowTemporal.comp reprojection

	vec4 f4SunMoonNormal INIT;
	vec4 f4SunColor INIT;
	vec4 f4MoonColor INIT;
	vec4 f4AmbientColor INIT;

	// Per-target sun/moon intensity multipliers (applied at shader read sites). Terrain/Water/Objects are
	// pre-folded CPU-side into the precomputed products below, so only Smoke remains as a raw multiplier.
	float fSunIntensitySmoke INIT;
	float fMoonIntensitySmoke INIT;

	// Precomputed day-cycle products (CPU-folded uniform-only terms; GlobalUniforms.cpp
	// PopulateDayCycleColors / PopulateSunMoonDirection / PopulateShadowParameters). Every operand is
	// invocation-invariant, so folding them removes the equivalent per-pixel multiplies/dots/normalizes.
	vec4 f4SunColorTerrain INIT;      // fSunIntensityTerrain * f4SunColor (SunLighting)
	vec4 f4MoonColorTerrain INIT;     // fMoonIntensityTerrain * f4MoonColor (SunLighting)
	vec4 f4TerrainSnowSunNormal INIT; // fTerrainSnowBlend * f4SunMoonNormal (Terrain.frag snow sun-normal tilt); w unused
	vec4 f4WaterBiasedSunNormal INIT; // normalize((0,0,fLightingWaterSkyboxSunBias) + f4SunMoonNormal) (Water.frag skybox specular); w unused
	vec4 f4SmokeBaseLighting INIT;    // max(fSunIntensitySmoke*f4SunColor, fMoonIntensitySmoke*f4MoonColor) + f4AmbientColor (AddSmoke/BlendSmokePrecomputed); w unused
	vec4 f4WaterSunOrMoon INIT;       // max(fSunIntensityWater*f4SunColor, fMoonIntensityWater*f4MoonColor) (Water.frag skybox sun tint); w unused
	vec4 f4AmbientUnshadowed INIT;    // (1 - fShadowAffectAmbient) * f4AmbientColor (SunLighting ambient, unshadowed half); w unused
	vec4 f4AmbientShadowed INIT;      // fShadowAffectAmbient * f4AmbientColor (SunLighting ambient, shadowed half); w unused
	float fSunMagnitudeTerrain INIT;        // Rec.601 luma of f4SunColorTerrain (SunLighting ambient-shadow blend)
	float fMoonMagnitudeTerrain INIT;       // Rec.601 luma of f4MoonColorTerrain (SunLighting ambient-shadow blend)
	float fSunMoonMagnitudeSumInverseTerrain INIT; // 1 / max(fSunMagnitudeTerrain + fMoonMagnitudeTerrain, 0.001)
	float fWaterSunScalar INIT;       // (f4WaterSunOrMoon.x+y+z)/3 (Water.frag sun contribution)
	float fWaterAmbientScalar INIT;   // (f4AmbientColor.x+y+z)/3 (Water.frag ambient contribution)
	float fWaterSunWeight INIT;       // Rec.601 luma of fSunIntensityWater*f4SunColor (Water.frag effective-shadow blend)
	float fWaterMoonWeight INIT;      // Rec.601 luma of fMoonIntensityWater*f4MoonColor (Water.frag effective-shadow blend)
	float fWaterShadowWeightSumInverse INIT; // 1 / max(fWaterSunWeight + fWaterMoonWeight, 0.001)
	float fSmokeLightingCombinedMultiplier INIT; // fSmokeLightingMultiplier * fLightingTimeOfDayMultiplier (AddSmoke/BlendSmokePrecomputed)

	// Model.frag uses separate direct BRDF and IBL specular products. IBL stays linear in fPhysicallyBasedRenderingSun
	// because its Rec.709 luminance uses the unscaled sun/moon color. w unused.
	vec4 f4PhysicallyBasedRenderingSunColorObjects INIT;     // fSunIntensityObjects * fPhysicallyBasedRenderingSun * f4SunColor (Model.frag direct BRDF sun term)
	vec4 f4PhysicallyBasedRenderingMoonColorObjects INIT;    // fMoonIntensityObjects * fPhysicallyBasedRenderingSun * f4MoonColor (Model.frag direct BRDF moon term)
	vec4 f4PhysicallyBasedRenderingSunColorObjectsImageBasedLighting INIT;  // (dot(f4SunColor.rgb, kRec709) / fPhysicallyBasedRenderingDayBrightness) * f4PhysicallyBasedRenderingSunColorObjects (Model.frag IBL specular sun term)
	vec4 f4PhysicallyBasedRenderingMoonColorObjectsImageBasedLighting INIT; // (dot(f4MoonColor.rgb, kRec709) / fPhysicallyBasedRenderingDayBrightness) * f4PhysicallyBasedRenderingMoonColorObjects (Model.frag IBL specular moon term)

	vec4 f4SmokeArea INIT;
	vec4 f4PreviousSmokeArea INIT;
	vec2 f2PreviousSmokeAreaSizeInverse INIT; // (1/(z-x), 1/(w-y)) of f4PreviousSmokeArea (Smoke/Wind OccupancyDilate previous-area remap)
	float fSmokeMax INIT;
	float fSmokePower INIT;
	float fSmokeDecay INIT;
	float fSmokeColorMin INIT;
	float fSmokeColorMultiplier INIT;
	float fSmokeLightingMultiplier INIT;
	float fSmokeIntensityFalloff INIT;
	float fSmokeWindNoiseScale INIT;
	float fSmokeWindNoiseQuantity INIT;
	float fSmokeNoiseQuantity INIT;
	float fSmokeNoiseScaleOne INIT;
	float fSmokeNoiseScaleTwo INIT;
	float fSmokeObjectHeightInv INIT;
	float fSmokeEdgeDecayDistanceInverse INIT;
	uint32_t uiSmokeTilesX INIT;
	uint32_t uiSmokeTilesY INIT;
	float fSmokeDepositTileScale INIT;
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
	float fLightingDepositThreshold INIT;
	float fLightingDepositCompress INIT;
	// Uchimura tone curve inputs (LightCombine.comp / DebugTexture.frag). fCombineLinearLength folds CPU-side into
	// fCombineShoulderInputStart/fCombineShoulderOutputStart; fCombineShoulderExponentCoefficient is the segment CP constant (epsilon-guarded on P - S1 in LightingUniforms.cpp).
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
	float pfCombineCurvePoints[kiMaxSpreadPasses] INIT;
	float fLightingTerrain INIT;
	float fLightingObjects INIT;

	float fLightingAddTerrain INIT;
	float fSpreadDirectionalityStart INIT;
	vec4 f4LightingArea INIT;
	vec4 f4LightingAreaPrevious INIT; // Previous-frame f4LightingArea, for LightingTemporal.comp reprojection
	vec2 f2LightingAreaExtentInverse INIT; // 1 / lighting-area extent (LightingSpread.frag world->texcoord)
	float fLightingTemporalBlend INIT; // EMA weight toward current; 1.0 on the first frame so seeded history is never shown
	vec2 f2LightingDepositSizeInverse INIT; // 1 / (lightTiles * kiComputeTileSize) (LightingDepositEdgeFade)

	// Spread Start (radial directional spread)
	float fSpreadDirectionCountStart INIT;
	float fSpreadDistanceStart INIT;
	float fSpreadRingCountStart INIT;
	float fSpreadJitterStart INIT;
	float fSpreadSampleJitterRangeStart INIT;
	float fSpreadSampleJitterClusteringStart INIT;
	float fSpreadDecayStart INIT;
	float fSpreadAccumulationDecayStart INIT;
	float fSpreadDistanceFalloffStart INIT;
	float fSpreadOutputThresholdStart INIT;
	float fSpreadOutputCompressStart INIT;
	float fSpreadPassCount INIT;
	float pfSpreadRingRotations[kiMaxSpreadPasses] INIT;

	// Spread End (interpolation targets for last spread pass)
	float fSpreadDirectionalityEnd INIT;
	float fSpreadDirectionCountEnd INIT;
	float fSpreadDistanceEnd INIT;
	float fSpreadRingCountEnd INIT;
	float fSpreadJitterEnd INIT;
	float fSpreadSampleJitterRangeEnd INIT;
	float fSpreadSampleJitterClusteringEnd INIT;
	float fSpreadDecayEnd INIT;
	float fSpreadAccumulationDecayEnd INIT;
	float fSpreadDistanceFalloffEnd INIT;
	float fSpreadOutputThresholdEnd INIT;
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
	float fObjectShadowsBlurSigma INIT;
	float fObjectShadowsIntensity INIT;
	float fObjectShadowsGrow INIT;
	int32_t iObjectShadowsBlurRadius INIT;
	float fShadowSunriseStretchCubed INIT; // (fSunriseStretch * gObjectShadowsSunsetStretch)^3 (ShadowStretchProjection)
	float fShadowSunsetStretchCubed INIT;  // (fSunsetStretch  * gObjectShadowsSunsetStretch)^3 (ShadowStretchProjection)
	vec2 f2ShadowStretchTranslation INIT;  // (sunrise+sunset cubes) * -f4SunMoonNormal.xy (ShadowStretchProjection base translation)
	float fShadowAffectAmbient INIT;
	float fWaterReducedNoiseOriginX INIT;
	int32_t iShadowElevationSize INIT;
	int32_t iShadowIncrement INIT;
	int32_t iShadowStartOffset INIT;
	float fWaterReducedNoiseOriginY INIT;
	float fShadowTemporalBlend INIT; // ShadowTemporal.comp: weight of the current frame (1.0 = no history)

	// Heightmap pixels carry heights in meters directly.
	float fIslandAmbientOcclusion INIT;
	float fWaterEarlyOut INIT;
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
	float fWaterHeight INIT;
	float fWaterTerrainHeight INIT;
	float fWaterTerrainFadeInverse INIT; // 1 / gWaterTerrainFade (Water.frag alpha fade); unguarded.
	float fWaterTerrainFadeClamp INIT;
	float fWaterDepthLookupTableFeather INIT;
	float fWaterDepthColorFeather INIT;
	float fWaterDepthColorFloor INIT;
	float fWaterUnderseaCompressionInverse INIT; // 1 / gWaterUnderseaCompression = TerrainElevation.frag undersea depth-curve pow() exponent
	float fWaterDepthReflectionFeatherInverse INIT; // 1 / (fDayPercent * gWaterDepthReflectionFeather); unguarded (night dayPercent=0 -> +inf absorbed by the shader clamp)
	float fWaterColorNoiseFrequency INIT;
	float fWaterDepthLookupTableSunsetFade INIT;
	float fWaterFresnel INIT;
	float fWaterColorBottom INIT;
	float fWaterColorHeightInv INIT;
	float fWaterColorNoiseAmount INIT;
	float fWaterColorNoiseWeightOne INIT;
	float fWaterColorNoiseWeightTwo INIT;
	float fWaterColorNoiseMultiplierOne INIT;
	float fWaterColorNoiseMultiplierTwo INIT;
	float fWaterDirectional INIT;
	float fWaterBreakEndDepthInverse INIT; // 1 / gWaterBreakEndDepth, CPU-folded for WaterDisplacement.comp and WaterSpecular.h
	float fWaterBreakBlendStart INIT; // min(gWaterBreakStartDepth, gWaterBreakEndDepth), CPU-folded for WaterDisplacement.comp
	float fWaterBreakBlendInverseRange INIT; // 1 / (gWaterBreakEndDepth - fWaterBreakBlendStart) when positive, 0 for the hard step, CPU-folded for WaterDisplacement.comp
	float fWaterBreakBlendCurve INIT; // gWaterBreakBlendCurve exponent for pow(t, exponent), [0.125, 8]; 1 is linear, below 1 introduces low earlier, above 1 retains medium longer
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

	float fParticlesStretchVelocityStart INIT;
	float fParticlesStretchVelocityMultiplier INIT;
	float fParticlesStretchRangeInverse INIT; // 1 / max(stretchVelocityEnd - stretchVelocityStart, kfEpsilon) (LongParticlesRender.vert length ramp)

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
