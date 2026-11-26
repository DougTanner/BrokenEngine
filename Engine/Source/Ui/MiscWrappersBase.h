#pragma once

#if defined(BT_CLIENT)

#include "WrapperBase.h"

namespace engine
{

inline Wrapper gMiscDebugTextureLinearRange(1.0f, 0.1f, 10.0f);
inline Wrapper gExtraArea(1.1f, 0.75f, 1.5f);

inline Wrapper gObserverPanSpeed(300.0f, 50.0f, 1'200.0f);
inline Wrapper gObserverPanAcceleration(10.0f, 1.0f, 30.0f);
inline Wrapper gObserverPanDeceleration(12.0f, 1.0f, 30.0f);
inline Wrapper gObserverEdgeScrollMargin(24.0f, 0.0f, 120.0f);
inline Wrapper gObserverZoomSpeedScaling(1.0f, 0.0f, 2.0f);

} // namespace engine

#endif // BT_CLIENT
