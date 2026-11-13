#pragma once

#include "WrapperBase.h"

namespace engine
{

// Color Phase Boundaries (radians)
inline Wrapper gSunMoonMorning(XM_PIDIV16, 0.0f, XM_PIDIV4);
inline Wrapper gSunMoonNoonStart(XM_PIDIV8, 0.0f, XM_PIDIV2);
inline Wrapper gSunMoonNoonEnd(XM_PIDIV2 + XM_PIDIV8, XM_PIDIV2, XM_PI - XM_PIDIV16);
inline Wrapper gSunMoonEvening(XM_PI - XM_PIDIV16, XM_PIDIV2, XM_PI);
inline Wrapper gSunMoonNightStart(XM_PI, XM_PIDIV2, XM_PI + XM_PIDIV2);

// Sun Intensity (per target)
inline Wrapper gSunMoonSunIntensityTerrain(1.1f, 0.0f, 4.0f);
inline Wrapper gSunMoonSunIntensityWater(0.4f, 0.0f, 4.0f);
inline Wrapper gSunMoonSunIntensityObjects(1.0f, 0.0f, 4.0f);
inline Wrapper gSunMoonSunIntensitySmoke(1.0f, 0.0f, 4.0f);

// Moon Intensity (per target) + Blue Tint
inline Wrapper gSunMoonMoonIntensityTerrain(0.06f, 0.0f, 0.1f);
inline Wrapper gSunMoonMoonIntensityWater(0.05f, 0.0f, 0.1f);
inline Wrapper gSunMoonMoonIntensityObjects(0.03f, 0.0f, 1.0f);
inline Wrapper gSunMoonMoonIntensitySmoke(0.05f, 0.0f, 1.0f);
inline Wrapper gSunMoonMoonBlueTint(1.0f, 0.0f, 4.0f);

// Moon Timing (radians)
inline Wrapper gSunMoonMoonriseStart(3.05f, XM_PIDIV2, XM_2PI);
inline Wrapper gSunMoonMoonriseEnd(3.1f, XM_PIDIV2, XM_2PI);
inline Wrapper gSunMoonMoonsetStart(XM_PIDIV128, 0.0f, XM_PIDIV2);
inline Wrapper gSunMoonMoonsetEnd(XM_PIDIV32, 0.0f, XM_PIDIV2);

// Ambient
inline Wrapper gSunMoonDayMinimumAmbient(0.0f, 0.0f, 0.2f);
inline Wrapper gSunMoonNightMinimumAmbient(0.02f, 0.0f, 0.2f);
inline Wrapper gSunMoonDayAmbientMultiplier(0.75f, 0.0f, 4.0f);
inline Wrapper gSunMoonNightAmbientMultiplier(1.75f, 0.0f, 4.0f);

// Tilt in radians affects f4SunMoonNormal and the terrain/water lighting derived from it; it leaves the normal's X component, and so the object-shadow away-from-light sign, unchanged.
inline Wrapper gSunMoonNormalTilt(-0.45f, -XM_PIDIV2, XM_PIDIV2);

// Shadow night multiplier is dimensionless; timing boundaries are in radians.
inline Wrapper gSunMoonShadowNightMultiplier(0.2f, 0.0f, 1.0f);
inline Wrapper gSunMoonShadowSunsetStart(XM_PI - XM_PIDIV32, XM_PIDIV2, XM_PI);
inline Wrapper gSunMoonShadowSunsetEnd(XM_PI - XM_PIDIV128, XM_PIDIV2, XM_PI);
inline Wrapper gSunMoonShadowSunriseStart(XM_PIDIV128, 0.0f, XM_PIDIV2);
inline Wrapper gSunMoonShadowSunriseEnd(XM_PIDIV32, 0.0f, XM_PIDIV2);

} // namespace engine
