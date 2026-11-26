#pragma once

#if defined(BT_CLIENT)

#include "WrapperBase.h"

namespace engine
{

struct HeightLerpWrapperQuartet
{
	Wrapper startHeight;
	Wrapper endHeight;
	Wrapper low;
	Wrapper high;
};

constexpr float LerpAtHeight(float fFraming, float fStartHeight, float fEndHeight, float fLow, float fHigh)
{
	float fSpan = std::max(fEndHeight - fStartHeight, 0.001f);
	float fInterpolationFactor = std::clamp((fFraming - fStartHeight) / fSpan, 0.0f, 1.0f);
	return std::lerp(fLow, fHigh, fInterpolationFactor);
}

inline float LerpAtHeight(float fFraming, const HeightLerpWrapperQuartet& rQuartet)
{
	return LerpAtHeight(fFraming, rQuartet.startHeight.mfCurrent, rQuartet.endHeight.mfCurrent, rQuartet.low.mfCurrent, rQuartet.high.mfCurrent);
}

} // namespace engine

#endif // BT_CLIENT
