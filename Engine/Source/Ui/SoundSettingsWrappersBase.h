#pragma once

#include "HeightLerpWrapperQuartet.h"
#include "WrapperBase.h"

namespace engine
{

inline Wrapper gMasterVolume(1.0f, 0.0f, 1.0f);
inline Wrapper gMusicVolume(0.25f, 0.0f, 1.0f);
inline Wrapper gSoundVolume(1.0f, 0.0f, 1.0f);
inline Wrapper gMuteInBackground(true);

// Listener distance / curve / audible-floor are camera-height-lerped per the canonical
// "Camera-Height-Conditional Uniforms" pattern.
inline HeightLerpWrapperQuartet gListenerDistanceStart // mfEffectiveFadeStart
{
	.startHeight = Wrapper(150.0f, 0.0f, 1'000.0f),
	.endHeight = Wrapper(600.0f, 0.0f, 1'000.0f),
	.low = Wrapper(200.0f, 0.0f, 1'000.0f),
	.high = Wrapper(600.0f, 0.0f, 1'000.0f),
};

inline HeightLerpWrapperQuartet gListenerDistanceEnd // mfEffectiveFadeEnd
{
	.startHeight = Wrapper(150.0f, 0.0f, 1'000.0f),
	.endHeight = Wrapper(600.0f, 0.0f, 1'000.0f),
	.low = Wrapper(400.0f, 0.0f, 2'000.0f),
	.high = Wrapper(1'200.0f, 0.0f, 2'000.0f),
};

inline HeightLerpWrapperQuartet gListenerCurve // mfCurveDistanceScaler (X3DAudio)
{
	.startHeight = Wrapper(150.0f, 0.0f, 1'000.0f),
	.endHeight = Wrapper(600.0f, 0.0f, 1'000.0f),
	.low = Wrapper(10.0f, 1.0f, 100.0f),
	.high = Wrapper(10.0f, 1.0f, 100.0f),
};

inline HeightLerpWrapperQuartet gListenerAudibleFloor // mfManualFadeVolume
{
	.startHeight = Wrapper(150.0f, 0.0f, 1'000.0f),
	.endHeight = Wrapper(600.0f, 0.0f, 1'000.0f),
	.low = Wrapper(0.15f, 0.0f, 0.5f),
	.high = Wrapper(0.15f, 0.0f, 0.5f),
};

} // namespace engine
