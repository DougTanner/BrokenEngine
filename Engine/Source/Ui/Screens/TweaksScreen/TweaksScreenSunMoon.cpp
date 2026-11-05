#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/SunMoonWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gSunMoonRegistrar
{
	// Color Phase Boundaries (radians)
	{"Morning Start", &gSunMoonMorning},
	{"Noon Start", &gSunMoonNoonStart},
	{"Noon End", &gSunMoonNoonEnd},
	{"Evening Start", &gSunMoonEvening},
	{"Night Start", &gSunMoonNightStart},
	{"Sun Terrain", &gSunMoonSunIntensityTerrain},
	{"Sun Water", &gSunMoonSunIntensityWater},
	{"Sun Objects", &gSunMoonSunIntensityObjects},
	{"Sun Smoke", &gSunMoonSunIntensitySmoke},
	{"Moon Terrain", &gSunMoonMoonIntensityTerrain},
	{"Moon Water", &gSunMoonMoonIntensityWater},
	{"Moon Objects", &gSunMoonMoonIntensityObjects},
	{"Moon Smoke", &gSunMoonMoonIntensitySmoke},
	{"Moon Blue Tint", &gSunMoonMoonBlueTint},
	{"Moonrise Start", &gSunMoonMoonriseStart},
	{"Moonrise End", &gSunMoonMoonriseEnd},
	{"Moonset Start", &gSunMoonMoonsetStart},
	{"Moonset End", &gSunMoonMoonsetEnd},
	{"Minimum Ambient", &gSunMoonMinimumAmbient},
	{"Ambient Multiplier", &gSunMoonAmbientMultiplier},
	{"Normal Tilt", &gSunMoonNormalTilt},
	{"Shadow Night Multiplier", &gSunMoonShadowNightMultiplier},
	{"Shadow Sunset Start", &gSunMoonShadowSunsetStart},
	{"Shadow Sunset End", &gSunMoonShadowSunsetEnd},
	{"Shadow Sunrise Start", &gSunMoonShadowSunriseStart},
	{"Shadow Sunrise End", &gSunMoonShadowSunriseEnd},
};

void RenderSunMoonSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionSunMoon;

	rScreen.WrapperSeparatorText("Color Phase Boundaries (radians)");
	rScreen.WrapperSlider("Sun Moon Morning", iSection, 2.0f, "Morning Start");
	rScreen.WrapperSlider("Sun Moon Noon Start", iSection, 2.0f, "Noon Start");
	rScreen.WrapperSlider("Sun Moon Noon End", iSection, 2.0f, "Noon End");
	rScreen.WrapperSlider("Sun Moon Evening", iSection, 2.0f, "Evening Start");
	rScreen.WrapperSlider("Sun Moon Night Start", iSection, 2.0f, "Night Start");

	rScreen.WrapperSeparatorText("Sun Intensity");
	rScreen.WrapperSlider("Sun Moon Sun Intensity Terrain", iSection, 2.0f, "Sun Terrain");
	rScreen.WrapperSlider("Sun Moon Sun Intensity Water", iSection, 2.0f, "Sun Water");
	rScreen.WrapperSlider("Sun Moon Sun Intensity Objects", iSection, 2.0f, "Sun Objects");
	rScreen.WrapperSlider("Sun Moon Sun Intensity Smoke", iSection, 2.0f, "Sun Smoke");

	rScreen.WrapperSeparatorText("Moon Intensity");
	rScreen.WrapperSlider("Sun Moon Moon Intensity Terrain", iSection, 2.0f, "Moon Terrain");
	rScreen.WrapperSlider("Sun Moon Moon Intensity Water", iSection, 2.0f, "Moon Water");
	rScreen.WrapperSlider("Sun Moon Moon Intensity Objects", iSection, 2.0f, "Moon Objects");
	rScreen.WrapperSlider("Sun Moon Moon Intensity Smoke", iSection, 2.0f, "Moon Smoke");
	rScreen.WrapperSlider("Sun Moon Moon Blue Tint", iSection, 2.0f, "Moon Blue Tint");

	rScreen.WrapperSeparatorText("Moon Timing (radians)");
	rScreen.WrapperSlider("Sun Moon Moonrise Start", iSection, 2.0f, "Moonrise Start");
	rScreen.WrapperSlider("Sun Moon Moonrise End", iSection, 2.0f, "Moonrise End");
	rScreen.WrapperSlider("Sun Moon Moonset Start", iSection, 2.0f, "Moonset Start");
	rScreen.WrapperSlider("Sun Moon Moonset End", iSection, 2.0f, "Moonset End");

	rScreen.WrapperSeparatorText("Ambient");
	rScreen.WrapperSlider("Sun Moon Minimum Ambient", iSection, 2.0f, "Minimum Ambient");
	rScreen.WrapperSlider("Sun Moon Ambient Multiplier", iSection, 2.0f, "Ambient Multiplier");

	rScreen.WrapperSeparatorText("Normal Tilt (radians)");
	rScreen.WrapperSlider("Sun Moon Normal Tilt", iSection, 2.0f, "Normal Tilt");

	rScreen.WrapperSeparatorText("Shadow Night-Gate (radians)");
	rScreen.WrapperSlider("Sun Moon Shadow Night Multiplier", iSection, 2.0f, "Shadow Night Multiplier");
	rScreen.WrapperSlider("Sun Moon Shadow Sunset Start", iSection, 2.0f, "Shadow Sunset Start");
	rScreen.WrapperSlider("Sun Moon Shadow Sunset End", iSection, 2.0f, "Shadow Sunset End");
	rScreen.WrapperSlider("Sun Moon Shadow Sunrise Start", iSection, 2.0f, "Shadow Sunrise Start");
	rScreen.WrapperSlider("Sun Moon Shadow Sunrise End", iSection, 2.0f, "Shadow Sunrise End");
}

} // namespace engine

#endif // BT_CLIENT
