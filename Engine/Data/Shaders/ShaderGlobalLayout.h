#ifndef SHADER_GLOBAL_LAYOUT_H
#define SHADER_GLOBAL_LAYOUT_H
struct GlobalLayout
{
	uint32_t uiRandomSeed INIT;

	float fElapsedTime INIT;
	float fBaseHeight INIT;
	float fBaseHeightInv INIT; // 1 / max(fBaseHeight, 0.001) (Terrain.frag below-base height ratio)
	float fAspectRatioInv INIT; // 1 / aspect ratio (Billboards.vert screen-space width divide, folded CPU-side)

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
	float fSunMagTerrain INIT;        // Rec.601 luma of f4SunColorTerrain (SunLighting ambient-shadow blend)
	float fMoonMagTerrain INIT;       // Rec.601 luma of f4MoonColorTerrain (SunLighting ambient-shadow blend)
	float fSunMoonMagSumInvTerrain INIT; // 1 / max(fSunMagTerrain + fMoonMagTerrain, 0.001)
	float fWaterSunScalar INIT;       // (f4WaterSunOrMoon.x+y+z)/3 (Water.frag sun contribution)
	float fWaterAmbientScalar INIT;   // (f4AmbientColor.x+y+z)/3 (Water.frag ambient contribution)
	float fWaterSunWeight INIT;       // Rec.601 luma of fSunIntensityWater*f4SunColor (Water.frag effective-shadow blend)
	float fWaterMoonWeight INIT;      // Rec.601 luma of fMoonIntensityWater*f4MoonColor (Water.frag effective-shadow blend)
	float fWaterShadowWeightSumInv INIT; // 1 / max(fWaterSunWeight + fWaterMoonWeight, 0.001)
	float fSmokeLightingCombinedMultiplier INIT; // fSmokeLightingMultiplier * fLightingTimeOfDayMultiplier (AddSmoke/BlendSmokePrecomputed)

	// Objects (Model.frag) precomputed sun/moon products. Direct BRDF and IBL specular are kept as separate
	// fields so the IBL path stays linear in fPbrSun (its Rec.709 luminance uses the UNSCALED color; folding
	// fPbrSun into that luminance would compound to fPbrSun^3). w unused.
	vec4 f4PbrSunColorObjects INIT;     // fSunIntensityObjects * fPbrSun * f4SunColor (Model.frag direct BRDF sun term)
	vec4 f4PbrMoonColorObjects INIT;    // fMoonIntensityObjects * fPbrSun * f4MoonColor (Model.frag direct BRDF moon term)
	vec4 f4PbrSunColorObjectsIbl INIT;  // (dot(f4SunColor.rgb, kRec709) / fPbrDayBrightness) * f4PbrSunColorObjects (Model.frag IBL specular sun term)
	vec4 f4PbrMoonColorObjectsIbl INIT; // (dot(f4MoonColor.rgb, kRec709) / fPbrDayBrightness) * f4PbrMoonColorObjects (Model.frag IBL specular moon term)

	// Smoke
	vec4 f4SmokeArea INIT;
	vec4 f4PreviousSmokeArea INIT;
	vec2 f2PreviousSmokeAreaSizeInv INIT; // (1/(z-x), 1/(w-y)) of f4PreviousSmokeArea (Smoke/Wind OccupancyDilate previous-area remap)
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
	float fSmokeEdgeDecayDistanceInv INIT;
	uint32_t uiSmokeTilesX INIT;
	uint32_t uiSmokeTilesY INIT;
	float fSmokeDepositTileScale INIT;
	float fWindDisplacementNoiseScale INIT;
	float fWindSmokeAdvection INIT;
	// Wind
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
	float fWindTexelSize INIT;
	float fWindTime INIT;
	float fWindSmokeRetention INIT;
	float fWindToSmokePower INIT;
	float fWindDiffusionHigh INIT;
	float fWindDiffusionLow INIT;
	float fWindTextureIndex INIT; // Blend factor: 0.0 = TextureOne, 1.0 = TextureTwo (continuous for interpolation)
	uint32_t uiWindTilesX INIT;
	uint32_t uiWindTilesY INIT;
	vec2 f2WindTilesInv INIT; // (1/uiWindTilesX, 1/uiWindTilesY) (WindOccupancyDilate output-tile-center UV)

	// Lighting
	float fLightingObjectsAdd INIT;
	float fLightingDepositThreshold INIT;
	float fLightingDepositCompress INIT;
	// Uchimura tone curve inputs (LightCombine.comp / DebugTexture.frag). fCombineLinearLength folds CPU-side into
	// fCombineS0/fCombineS1; fCombineCP is the segment CP constant (epsilon-guarded on P - S1 in LightingUniforms.cpp).
	float fCombineMaxBrightness INIT;
	float fCombineContrast INIT;
	float fCombineLinearStart INIT;
	float fCombineToe INIT;
	float fCombineBlackTightness INIT;
	float fCombineS0 INIT;
	float fCombineS1 INIT;
	float fCombineCP INIT;
	float fCombineHuePreserve INIT;
	// Pass normalization/exposure scaling precomputed: fCombinePassNormScale = passNorm * passScale (DebugTexture.frag),
	// fCombinePassTotalScale = fCombinePassNormScale / passCount (LightCombine.comp averaging).
	float fCombinePassNormScale INIT;
	float fCombinePassTotalScale INIT;
	float pfCombineCurvePoints[kiMaxSpreadPasses] INIT;
	float fLightingTerrain INIT;
	float fLightingObjects INIT;

	float fLightingAddTerrain INIT;
	float fSpreadDirectionalityStart INIT;
	vec4 f4LightingArea INIT;
	vec4 f4LightingAreaPrevious INIT; // Previous-frame f4LightingArea, for LightingTemporal.comp reprojection
	vec2 f2LightingAreaExtentInv INIT; // 1 / lighting-area extent (LightingSpread.frag world->texcoord)
	float fLightingTemporalBlend INIT; // EMA weight toward current; 1.0 on the first frame so seeded history is never shown
	vec2 f2LightingDepositSizeInv INIT; // 1 / (lightTiles * kiComputeTileSize) (LightingDepositEdgeFade)

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

	// Spread Height Fade
	float fSpreadHeightMultiplier INIT;
	float fSpreadHeightEndHeightInv INIT; // 1 / max(spreadHeightEndHeight, 0.001) (LightingSpread.frag height fade)
	float fSpreadHeightPower INIT;

	// Shadow
	float fShadowWidthScale INIT;
	float fShadowDirectionMultiplier INIT;
	float fShadowMoonMultiplier INIT;
	float fShadowFeather INIT;
	float fShadowDistanceFalloff INIT;
	float fShadowDistanceFalloffInv INIT;
	float pfShadowBlurWeights[kiShadowBlurRadius + 1] INIT; // Symmetric Gaussian half-kernel: ShadowBlurH/V index by abs(offset), scale by fShadowBlurWeightSumInv.
	float fShadowBlurWeightSumInv INIT;
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

	// Terrain. Heightmap pixels carry absolute meters directly — fIslandHeight / fWaterDepth
	// scale factors retired with the meters-everywhere refactor.
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

	// Water
	int32_t iWaterLowCount INIT;
	int32_t iWaterMediumCount INIT;
	int32_t iWaterActiveQuadX INIT;
	int32_t iWaterActiveQuadY INIT;
	float fWaterOriginX INIT;
	float fWaterOriginY INIT;
	float fWaterHeight INIT;
	float fWaterTerrainHeight INIT;
	float fWaterTerrainFadeInv INIT; // 1 / gWaterTerrainFade (Water.frag alpha fade); unguarded (reproduces the shader's original divide)
	float fWaterTerrainFadeClamp INIT;
	float fWaterDepthLutFeather INIT;
	float fWaterDepthColorFeather INIT;
	float fWaterDepthColorFloor INIT;
	float fWaterUnderseaCompressionInv INIT; // 1 / gWaterUnderseaCompression = TerrainElevation.frag undersea depth-curve pow() exponent
	float fWaterDepthReflectionFeatherInv INIT; // 1 / (fDayPercent * gWaterDepthReflectionFeather); unguarded (night dayPercent=0 -> +inf absorbed by the shader clamp)
	float fWaterColorNoiseFrequency INIT;
	float fWaterDepthLutSunsetFade INIT;
	float fWaterFresnel INIT;
	float fWaterColorBottom INIT;
	float fWaterColorHeightInv INIT;
	float fWaterColorNoiseAmount INIT;
	float fWaterColorNoiseWeightOne INIT;
	float fWaterColorNoiseWeightTwo INIT;
	float fWaterColorNoiseMultiplierOne INIT;
	float fWaterColorNoiseMultiplierTwo INIT;
	float fWaterDirectional INIT;
	float fWaterBreakEndDepthInv INIT; // 1 / gWaterBreakEndDepth, CPU-folded for WaterDisplacement.comp and WaterSpecular.h
	float fWaterBreakBlendStart INIT; // min(gWaterBreakStartDepth, gWaterBreakEndDepth), CPU-folded for WaterDisplacement.comp
	float fWaterBreakBlendInvRange INIT; // 1 / (gWaterBreakEndDepth - fWaterBreakBlendStart) when positive, 0 for the hard step, CPU-folded for WaterDisplacement.comp
	float fWaterBreakBlendCurve INIT; // gWaterBreakBlendCurve exponent for pow(t, exponent), [0.125, 8]; 1 is linear, below 1 introduces low earlier, above 1 retains medium longer
	float fWaterMediumShoreFadeInvWidth INIT; // 1 / gWaterMediumShoreSoftness when positive (0 disables), CPU-folded for the WaterDisplacement.comp medium-only shore fade
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

	// Particles
	float fParticlesStretchVelocityStart INIT;
	float fParticlesStretchVelocityMultiplier INIT;
	float fParticlesStretchRangeInv INIT; // 1 / max(stretchVelocityEnd - stretchVelocityStart, kfEpsilon) (LongParticlesRender.vert length ramp)

	// Shadow
	vec2 f2ShadowTextureSizeInv INIT; // 1 / shadow texture extent (ShadowBlurH/V, ShadowTemporal).
	vec2 f2ShadowElevationTextureSizeInv INIT; // 1 / elevation texture extent (Shadow.comp).
	float fShadowHeightFadeBottom INIT;
	float fShadowHeightFadeRangeInv INIT; // 1 / (fade top - fade bottom); no guard, reproduces Shadow.comp's original divide.

	// Terrain
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

	// Time of day
	float fLightingTimeOfDayMultiplier INIT;
	float fLightingWaterSkyboxOne INIT;
	float fLightingWaterSkyboxNormalSoften INIT;

	// Debug
	float fDebugTextureIndex INIT;
	float fDebugTextureFormat INIT;
	float fDebugTextureLinearRange INIT;
	float fSeaFloorElevation INIT;
	float fSeaFloorElevationInv INIT; // 1 / fSeaFloorElevation (TerrainElevation.frag undersea depth normalization)
	float fUnderwaterMaskThreshold INIT;
	float fDebugTerrainElevationHigh INIT;
};

#endif // SHADER_GLOBAL_LAYOUT_H
