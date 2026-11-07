#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/SunMoonWrappersBase.h"

namespace engine
{

void RenderSunMoonSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionSunMoon;

	rScreen.WrapperSeparatorText("Color Phase Boundaries (radians)");
	rScreen.WrapperSlider("Sun Moon Morning", gSunMoonMorning, iSection);
	rScreen.WrapperSlider("Sun Moon Noon Start", gSunMoonNoonStart, iSection);
	rScreen.WrapperSlider("Sun Moon Noon End", gSunMoonNoonEnd, iSection);
	rScreen.WrapperSlider("Sun Moon Evening", gSunMoonEvening, iSection);
	rScreen.WrapperSlider("Sun Moon Night Start", gSunMoonNightStart, iSection);

	rScreen.WrapperSeparatorText("Sun Intensity");
	rScreen.WrapperSlider("Sun Moon Sun Intensity Terrain", gSunMoonSunIntensityTerrain, iSection);
	rScreen.WrapperSlider("Sun Moon Sun Intensity Water", gSunMoonSunIntensityWater, iSection);
	rScreen.WrapperSlider("Sun Moon Sun Intensity Objects", gSunMoonSunIntensityObjects, iSection);
	rScreen.WrapperSlider("Sun Moon Sun Intensity Smoke", gSunMoonSunIntensitySmoke, iSection);

	rScreen.WrapperSeparatorText("Moon Intensity");
	rScreen.WrapperSlider("Sun Moon Moon Intensity Terrain", gSunMoonMoonIntensityTerrain, iSection);
	rScreen.WrapperSlider("Sun Moon Moon Intensity Water", gSunMoonMoonIntensityWater, iSection);
	rScreen.WrapperSlider("Sun Moon Moon Intensity Objects", gSunMoonMoonIntensityObjects, iSection);
	rScreen.WrapperSlider("Sun Moon Moon Intensity Smoke", gSunMoonMoonIntensitySmoke, iSection);
	rScreen.WrapperSlider("Sun Moon Moon Blue Tint", gSunMoonMoonBlueTint, iSection);

	rScreen.WrapperSeparatorText("Moon Timing (radians)");
	rScreen.WrapperSlider("Sun Moon Moonrise Start", gSunMoonMoonriseStart, iSection);
	rScreen.WrapperSlider("Sun Moon Moonrise End", gSunMoonMoonriseEnd, iSection);
	rScreen.WrapperSlider("Sun Moon Moonset Start", gSunMoonMoonsetStart, iSection);
	rScreen.WrapperSlider("Sun Moon Moonset End", gSunMoonMoonsetEnd, iSection);

	rScreen.WrapperSeparatorText("Ambient");
	rScreen.WrapperSlider("Sun Moon Minimum Ambient", gSunMoonMinimumAmbient, iSection);
	rScreen.WrapperSlider("Sun Moon Ambient Multiplier", gSunMoonAmbientMultiplier, iSection);

	rScreen.WrapperSeparatorText("Normal Tilt (radians)");
	rScreen.WrapperSlider("Sun Moon Normal Tilt", gSunMoonNormalTilt, iSection);

	rScreen.WrapperSeparatorText("Shadow Night-Gate (radians)");
	rScreen.WrapperSlider("Sun Moon Shadow Night Multiplier", gSunMoonShadowNightMultiplier, iSection);
	rScreen.WrapperSlider("Sun Moon Shadow Sunset Start", gSunMoonShadowSunsetStart, iSection);
	rScreen.WrapperSlider("Sun Moon Shadow Sunset End", gSunMoonShadowSunsetEnd, iSection);
	rScreen.WrapperSlider("Sun Moon Shadow Sunrise Start", gSunMoonShadowSunriseStart, iSection);
	rScreen.WrapperSlider("Sun Moon Shadow Sunrise End", gSunMoonShadowSunriseEnd, iSection);
}

} // namespace engine

#endif // BT_CLIENT
