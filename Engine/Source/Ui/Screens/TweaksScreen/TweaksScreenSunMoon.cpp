#include "TweaksScreenBase.h"

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

void RenderSunMoonSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionSunMoon;

	rScreen.WrapperSeparatorText("Color Phase Boundaries (radians)");
	rScreen.WrapperSlider("Morning Start", iSection);
	rScreen.WrapperSlider("Noon Start", iSection);
	rScreen.WrapperSlider("Noon End", iSection);
	rScreen.WrapperSlider("Evening Start", iSection);
	rScreen.WrapperSlider("Night Start", iSection);

	rScreen.WrapperSeparatorText("Sun Intensity");
	rScreen.WrapperSlider("Sun Terrain", iSection);
	rScreen.WrapperSlider("Sun Water", iSection);
	rScreen.WrapperSlider("Sun Objects", iSection);
	rScreen.WrapperSlider("Sun Smoke", iSection);

	rScreen.WrapperSeparatorText("Moon Intensity");
	rScreen.WrapperSlider("Moon Terrain", iSection);
	rScreen.WrapperSlider("Moon Water", iSection);
	rScreen.WrapperSlider("Moon Objects", iSection);
	rScreen.WrapperSlider("Moon Smoke", iSection);
	rScreen.WrapperSlider("Moon Blue Tint", iSection);

	rScreen.WrapperSeparatorText("Moon Timing (radians)");
	rScreen.WrapperSlider("Moonrise Start", iSection);
	rScreen.WrapperSlider("Moonrise End", iSection);
	rScreen.WrapperSlider("Moonset Start", iSection);
	rScreen.WrapperSlider("Moonset End", iSection);

	rScreen.WrapperSeparatorText("Ambient");
	rScreen.WrapperSlider("Minimum Ambient", iSection);
	rScreen.WrapperSlider("Ambient Multiplier", iSection);

	rScreen.WrapperSeparatorText("Normal Tilt (radians)");
	rScreen.WrapperSlider("Normal Tilt", iSection);

	rScreen.WrapperSeparatorText("Shadow Night-Gate (radians)");
	rScreen.WrapperSlider("Shadow Night Multiplier", iSection);
	rScreen.WrapperSlider("Shadow Sunset Start", iSection);
	rScreen.WrapperSlider("Shadow Sunset End", iSection);
	rScreen.WrapperSlider("Shadow Sunrise Start", iSection);
	rScreen.WrapperSlider("Shadow Sunrise End", iSection);
}

} // namespace engine

#endif // BT_CLIENT
