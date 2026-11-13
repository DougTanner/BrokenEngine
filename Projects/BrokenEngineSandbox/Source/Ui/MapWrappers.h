#pragma once

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace game
{

// The map's final framing: about 100 visible cells at the default, about 51 to 460 across the range.
inline engine::Wrapper gMapFraming(14'000.0f, 10'000.0f, 30'000.0f);
// The framing at which the map overlay is fully opaque. The minimum stays above CameraBase::kfFramingMaximum so the
// opacity's log denominator is positive, and the maximum at or below gMapFraming's minimum so the map always reaches
// full opacity.
inline engine::Wrapper gMapOpaqueFraming(10'000.0f, 1'000.0f, 10'000.0f);
// The TAB ease length in seconds; positive, because the framing ease divides by it.
inline engine::Wrapper gMapBlendSeconds(3.0f, 0.5f, 10.0f);

} // namespace game

#endif // defined(BT_CLIENT)
