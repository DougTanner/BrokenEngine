#pragma once

#if defined(BT_CLIENT)

#include "CurveData.h"
#include "HeightLerpWrapperQuartet.h"
#include "WrapperBase.h"

namespace engine
{

// Write - Pre-Blur
inline Wrapper gLightingBlurSigma(2.0f, 0.01f, 4.0f);
inline Wrapper gLightingBlurSampleCount(400.0f, 16.0f, 1024.0f, 1.0f);
inline Wrapper gLightingBlurEdgeFalloff(0.6f, 0.01f, 1.0f);

// Write - Deposit
inline Wrapper gLightingDepositTextureMultiplier(0.2f, 0.1f, 1.0f);
inline Wrapper gLightingDepositCompress(0.0f, 0.0f, 128.0f);
inline Wrapper gAreaLightAreaMultiplier(3.0f, 1.0f, 16.0f);
inline Wrapper gAreaLightForcedSquare(0.2f, 0.0f, 1.0f);

// Write - Spread
inline Wrapper gSpreadPassCount(64.0f, 1.0f, static_cast<float>(shaders::kiMaxSpreadPasses), 1.0f); // min >= 1.0f load-bearing: keeps std::pow(fPassCount, -fCombineExposurePassScale) base positive and the 1/fPassCount pass scaling finite (LightingUniforms.cpp). Step 1.0f load-bearing: RenderTargetTexturesLighting.cpp / CommandBufferRecordMain.cpp and uint() in LightCombine.comp truncate this value while LightingUniforms.cpp normalization and LightingSpread.frag interpolation use it raw, so only an integral value makes every phase describe the same pass count.
inline Wrapper gSpreadDecay(1.0f, 0.0f, 1.0f);
inline Wrapper gSpreadDecayEnd(1.0f, 0.0f, 1.0f);
inline Wrapper gSpreadAccumulationDecay(1.0f, 0.0f, 1.0f);
inline Wrapper gSpreadAccumulationDecayEnd(1.0f, 0.0f, 1.0f);
inline Wrapper gSpreadTextureMultiplierStart(0.05f, 0.01f, 0.1f);
inline Wrapper gSpreadTextureMultiplierEnd(0.01f, 0.005f, 0.02f);

// Write - Temporal (mirror of the shadow texel-contraction + temporal-blend tunables)
inline Wrapper gLightingTexelRampMetersPerSecond(150.0f, 10.0f, 2'000.0f);
inline Wrapper gLightingTemporalBlend(0.5f, 0.05f, 1.0f);
inline Wrapper gLightingUpdateCadence(2.0f, 1.0f, 4.0f, 1.0f);

// Write - Spread Sampling
inline Wrapper gSpreadDirectionality(0.9f, 0.0f, 1.0f);
inline Wrapper gSpreadDirectionalityEnd(1.0f, 0.0f, 1.0f);
inline Wrapper gSpreadDirectionCount(8.0f, 4.0f, 32.0f, 1.0f); // min >= 4.0f load-bearing: guarantees every EWNS lobe receives a positive directional tap weight at full directionality so LightingUniforms.cpp's EW/NS weight-sum reciprocals stay finite.
inline Wrapper gSpreadDirectionCountEnd(4.0f, 4.0f, 16.0f, 1.0f); // min >= 4.0f load-bearing: see gSpreadDirectionCount — interpolated direction count must also stay >= 4.
inline Wrapper gSpreadDistance(0.0f, 0.0f, 10.0f);
inline HeightLerpWrapperQuartet gSpreadDistanceEnd  // camera-height-lerped fSpreadDistanceEnd
{
	.startHeight = Wrapper(150.0f, 0.0f, 1'000.0f),
	.endHeight = Wrapper(600.0f, 0.0f, 1'000.0f),
	.low = Wrapper(20.0f, 1.0f, 40.0f),
	.high = Wrapper(20.0f, 1.0f, 40.0f),
};
inline Wrapper gSpreadRingCount(8.0f, 2.0f, 24.0f, 1.0f); // min >= 2.0f load-bearing: keeps inner-ring falloff positive at fDistanceFalloff=1.0 so LightingUniforms.cpp's EW/NS weight-sum reciprocals stay finite.
inline Wrapper gSpreadRingCountEnd(2.0f, 2.0f, 16.0f, 1.0f); // min >= 2.0f load-bearing: see gSpreadRingCount — interpolated ring count must also stay >= 2.
inline Wrapper gSpreadDistanceFalloff(0.0f, 0.0f, 1.0f);
inline Wrapper gSpreadDistanceFalloffEnd(0.0f, 0.0f, 1.0f);

// Write - Spread Height
inline Wrapper gSpreadHeightMultiplier(0.2f, 0.0f, 0.2f);
inline Wrapper gSpreadHeightEndHeight(10.0f, 0.1f, 100.0f);
inline Wrapper gSpreadHeightPower(3.0f, 0.1f, 8.0f);

// Write - Spread Output
inline Wrapper gSpreadOutputCompress(0.0f, 0.0f, 2.0f);
inline Wrapper gSpreadOutputCompressEnd(2.5f, 0.0f, 4.0f);

// Combine (Uchimura tone curve)
inline Wrapper gCombineMaxBrightness(1.3f, 0.1f, 4.0f);
inline Wrapper gCombineContrast(1.3f, 0.1f, 2.0f); // Contrast must stay positive: LightingUniforms.cpp divides by fCombineContrast when computing the tone-curve segment length.
inline Wrapper gCombineLinearStart(0.3f / 1.3f, 0.01f, 0.8f); // Fraction of gCombineMaxBrightness; paired with gCombineMaxBrightness >= 0.1f and gCombineLinearLength <= 0.8f, bounds keep 0 < m < S1 < P, m >= 0.001f, and P - S1 >= 0.004f.
inline Wrapper gCombineLinearLength(0.5f, 0.01f, 0.8f);
inline Wrapper gCombineToe(3.5f, 1.3f, 6.0f);
inline Wrapper gCombineBlackTightness(0.0f, 0.0f, 1.0f); // Fraction of resolved linear start m; bounds keep 0 <= b <= m across the entire slider domain.
inline Wrapper gCombinePassNormalize(0.7f, 0.0f, 1.0f);
inline Wrapper gCombineExposurePassScale(0.0f, 0.0f, 1.0f); // Paired with gSpreadPassCount min=1.0f: pow(fPassCount, -scale) stays finite (base>=1, exponent in [-1,0]).
inline Wrapper gCombineHuePreserve(0.0f, 0.0f, 1.0f);
inline CurveData gCombineCurveOld({ImVec2(0.0000f, 32.0000f), ImVec2(0.3880f, 25.9852f), ImVec2(0.5766f, 0.9778f), ImVec2(1.0000f, 8.4444f)}, 0.0000f, 32.0000f);
inline CurveData gCombineCurveNew({ImVec2(0.f, 21.132076f), ImVec2(0.049473684f, 2.9886792f), ImVec2(0.14263158f, 1.9018868f), ImVec2(0.21947369f, 0.f), ImVec2(0.3131579f, 1.6905661f), ImVec2(0.4231579f, 0.96603775f), ImVec2(0.5105263f, 0.9056604f), ImVec2(1.f, 1.0867925f)}, 0.0000f, 32.0000f);
inline bool gbUseCombineCurveNew = true;

// Read - Terrain Lighting
inline Wrapper gLightingDirectionalIntensity(0.7f, 0.0f, 2.0f);
inline Wrapper gLightingDirectionalPower(0.85f, 0.5f, 2.0f);
inline Wrapper gLightingDirectionalPowerMode(0.0f, 0.0f, 1.0f);
inline Wrapper gLightingAmbientIntensity(11.0f, 0.0f, 20.0f);
inline Wrapper gLightingAmbientPower(4.0f, 0.1f, 5.0f);
inline Wrapper gLightingAmbientPowerMode(1.0f, 0.0f, 1.0f);
inline Wrapper gLightingTerrain(1.5f, 0.0f, 3.0f);
inline Wrapper gLightingAddTerrain(0.25f, 0.0f, 0.5f);
inline Wrapper gLightingTerrainBelowBaseMultiplier(0.7f, 0.0f, 1.0f);
inline Wrapper gLightingTerrainBelowBasePower(0.3f, 0.1f, 1.0f);
inline Wrapper gLightingObjects(4.0f, 0.0f, 8.0f);
inline Wrapper gLightingObjectsAdd(0.2f, 0.0f, 1.0f);
inline Wrapper gLightingDayFinalMultiplier(1.0f, 0.0f, 1.0f);
inline Wrapper gLightingNightFinalMultiplier(0.4f, 0.0f, 1.0f);

// Read - Water Lighting
inline Wrapper gLightingWaterEastWestNorthSouthPower(1.0f, 0.1f, 6.0f);
inline Wrapper gLightingWaterEastWestNorthSouthPowerMode(0.6f, 0.0f, 1.0f);
inline Wrapper gLightingWaterAmbientIntensity(1.5f, 0.0f, 10.0f);
inline Wrapper gLightingWaterAmbientPower(2.5f, 0.1f, 5.0f);
inline Wrapper gLightingWaterAmbientPowerMode(1.0f, 0.0f, 1.0f);
inline Wrapper gLightingWaterNormalSoften(0.81f, 0.5f, 1.0f);
inline Wrapper gLightingWaterNormalBlendWave(0.15f, 0.0f, 0.4f);
inline Wrapper gLightingWaterIntensity(0.04f, 0.0f, 0.1f);
inline Wrapper gLightingWaterAdd(0.9f, 0.0f, 1.0f);
inline Wrapper gLightingWaterOne(30.0f, 0.0f, 40.0f);
inline Wrapper gLightingWaterOnePower(2.5f, 1.0f, 5.0f);
inline Wrapper gLightingWaterTwo(4.0f, 0.0f, 10.0f);
inline Wrapper gLightingWaterTwoPower(1.0f, 0.75f, 2.0f);
inline Wrapper gLightingWaterThree(4.0f, 0.0f, 15.0f);
inline Wrapper gLightingWaterThreePower(0.5f, 0.25f, 1.0f);
inline Wrapper gLightingWaterPowerMode(0.8f, 0.0f, 1.0f);

// Read - Water Reflected
inline Wrapper gLightingWaterReflectedAmount(0.1f, 0.0f, 1.0f);
inline Wrapper gLightingWaterReflectedNormalBlendWave(0.2f, 0.0f, 0.4f);
inline Wrapper gLightingWaterReflectedDistortion(23.0f, 0.0f, 40.0f);
inline Wrapper gLightingWaterReflectedFalloffStart(0.0f, 0.0f, 100.0f);
inline Wrapper gLightingWaterReflectedFalloffPower(0.6f, 0.1f, 1.0f);
inline Wrapper gLightingWaterReflectedFresnel(0.9f, 0.0f, 1.0f);
inline Wrapper gLightingWaterReflectedIntensity(0.4f, 0.0f, 5.0f);

} // namespace engine

#endif // BT_CLIENT
