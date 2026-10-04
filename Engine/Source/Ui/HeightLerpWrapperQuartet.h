#pragma once

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

constexpr float LerpAtHeight(float fEyeHeight, float fStartHeight, float fEndHeight, float fLow, float fHigh)
{
	float fSpan = std::max(fEndHeight - fStartHeight, 0.001f);
	float fInterpolationFactor = std::clamp((fEyeHeight - fStartHeight) / fSpan, 0.0f, 1.0f);
	return std::lerp(fLow, fHigh, fInterpolationFactor);
}

} // namespace engine
