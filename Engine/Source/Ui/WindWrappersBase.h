#pragma once

#include "WrapperBase.h"

namespace engine
{

inline Wrapper gWindTimeScale(0.35f, 0.005f, 0.5f);
inline Wrapper gWindThresholdLow(0.02f, 0.0f, 0.05f);
inline Wrapper gWindThresholdHigh(0.05f, 0.05f, 0.1f);

// Propagation (Low/High side-by-side in UI)
inline Wrapper gWindAdvectionScaleLow(1.0f, 0.0f, 1.0f);
inline Wrapper gWindAdvectionScaleHigh(5.0f, 0.5f, 5.0f);
inline Wrapper gWindSwirlScaleLow(1.0f, 0.5f, 3.0f);
inline Wrapper gWindSwirlScaleHigh(0.5f, 0.5f, 4.0f);
inline Wrapper gWindSwirlAmountLow(90.0f, 0.0f, 100.0f);
inline Wrapper gWindSwirlAmountHigh(110.0f, 0.0f, 500.0f);
inline Wrapper gWindSwirlSpeedLow(5.0f, 0.0f, 10.0f);
inline Wrapper gWindSwirlSpeedHigh(2.0f, 0.0f, 10.0f);
inline Wrapper gWindVorticityConfinementLow(0.5f, 0.0f, 1.0f);
inline Wrapper gWindVorticityConfinementHigh(4.0f, 0.0f, 4.0f);
inline Wrapper gWindDecayLow(0.4f, 0.0f, 0.9f); // max < 1.0f load-bearing: keeps fDecayRate = 1 - mix(low, high, t) > 0 in WindSpreadCommon.h.
inline Wrapper gWindDecayHigh(0.99f, 0.5f, 0.999f); // max < 1.0f load-bearing: keeps fDecayRate = 1 - mix(low, high, t) > 0 in WindSpreadCommon.h.
inline Wrapper gWindMomentumLow(0.75f, 0.0f, 1.0f);
inline Wrapper gWindMomentumHigh(0.6f, 0.0f, 1.0f);
inline Wrapper gWindDiffusionLow(100.0f, 0.0f, 100.0f);
inline Wrapper gWindDiffusionHigh(0.0f, 0.0f, 100.0f);

} // namespace engine
