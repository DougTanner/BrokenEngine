#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/MenuUtils.h"
#include "Ui/ShadowWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gShadowRegistrar
{
	{"Resolution", &gShadowRenderMultiplier},
	{"Shadow Texel Ramp Speed", &gShadowTexelRampMetersPerSecond},
	{"Shadow Temporal Blend", &gShadowTemporalBlend},
	{"Shadow Update Cadence", &gShadowUpdateCadence},
	{"Feather Noon", &gShadowFeatherNoon},
	{"Feather Noon Offset", &gShadowFeatherNoonOffset},
	{"Feather Sunset", &gShadowFeatherSunset},
	{"Feather Sunset Offset", &gShadowFeatherSunsetOffset},
	{"Feather Power", &gShadowFeatherPower},
	{"Distance Falloff", &gShadowDistanceFalloff},
	{"Blur Sigma", &gShadowBlurSigma},
	{"Affect Ambient", &gShadowAffectAmbient},
	{"Height Fade Top", &gShadowHeightFadeTop},
	{"Height Fade Bottom", &gShadowHeightFadeBottom},
	{"Render Multiplier", &gObjectShadowsRenderMultiplier},
	{"Blur Multiplier", &gObjectShadowsBlurMultiplier},
	{"Shadow Noon", &gObjectShadowsNoon},
	{"Shadow Sunset", &gObjectShadowsSunset},
	{"Sunset Stretch", &gObjectShadowsSunsetStretch},
	{"Stretch Power", &gObjectShadowsStretchPower},
	{"Object Blur Sigma Low", &gObjectShadowsBlurSigmaLow},
	{"Object Blur Sigma High", &gObjectShadowsBlurSigmaHigh},
	{"Object Blur Radius Noon", &gObjectShadowsBlurRadiusNoon},
	{"Object Blur Radius Sunset", &gObjectShadowsBlurRadiusSunset},
	{"Terrain Shadow Height Fade", &gObjectShadowsTerrainShadowFade},
	{"Underwater Fade", &gObjectShadowsUnderwaterFade},
	{"Bottom Smoke Shadow Intensity", &gSmokeShadowIntensityBottom},
	{"Top Smoke Shadow Intensity", &gSmokeShadowIntensityTop},
};

void RenderShadowSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionShadow;

	rScreen.WrapperSeparatorText("Quality / Perf");
	rScreen.WrapperSlider("Shadow Render Multiplier", iSection, 2.0f, "Resolution");
	rScreen.WrapperSlider("Shadow Texel Ramp Meters Per Second", iSection, 2.0f, "Shadow Texel Ramp Speed");
	rScreen.WrapperSlider("Shadow Temporal Blend", iSection, 2.0f, "Shadow Temporal Blend");
	rScreen.WrapperSlider("Shadow Update Cadence", iSection, 2.0f, "Shadow Update Cadence");

	rScreen.WrapperSeparatorText("Feather");
	rScreen.WrapperSlider("Shadow Feather Noon", iSection, 2.0f, "Feather Noon");
	rScreen.WrapperSlider("Shadow Feather Noon Offset", iSection, 2.0f, "Feather Noon Offset");
	rScreen.WrapperSlider("Shadow Feather Sunset", iSection, 2.0f, "Feather Sunset");
	rScreen.WrapperSlider("Shadow Feather Sunset Offset", iSection, 2.0f, "Feather Sunset Offset");
	rScreen.WrapperSlider("Shadow Feather Power", iSection, 2.0f, "Feather Power");
	rScreen.WrapperSlider("Shadow Distance Falloff", iSection, 2.0f, "Distance Falloff");
	rScreen.WrapperSlider("Shadow Blur Sigma", iSection, 2.0f, "Blur Sigma");
	rScreen.WrapperSlider("Shadow Affect Ambient", iSection, 2.0f, "Affect Ambient");
	rScreen.WrapperSlider("Shadow Height Fade Top", iSection, 2.0f, "Height Fade Top");
	rScreen.WrapperSlider("Shadow Height Fade Bottom", iSection, 2.0f, "Height Fade Bottom");

	rScreen.WrapperSeparatorText("Object Shadows");
	rScreen.WrapperSlider("Object Shadows Render Multiplier", iSection, 2.0f, "Render Multiplier");
	rScreen.WrapperSlider("Object Shadows Blur Multiplier", iSection, 2.0f, "Blur Multiplier");
	rScreen.WrapperSlider("Object Shadows Noon", iSection, 2.0f, "Shadow Noon");
	rScreen.WrapperSlider("Object Shadows Sunset", iSection, 2.0f, "Shadow Sunset");
	rScreen.WrapperSlider("Object Shadows Sunset Stretch", iSection, 2.0f, "Sunset Stretch");
	rScreen.WrapperSlider("Object Shadows Stretch Power", iSection, 2.0f, "Stretch Power");
	rScreen.WrapperSlider("Object Shadows Blur Sigma Low", iSection, 2.0f, "Object Blur Sigma Low");
	rScreen.WrapperSlider("Object Shadows Blur Sigma High", iSection, 2.0f, "Object Blur Sigma High");
	rScreen.WrapperSlider("Object Shadows Blur Radius Noon", iSection, 2.0f, "Object Blur Radius Noon");
	rScreen.WrapperSlider("Object Shadows Blur Radius Sunset", iSection, 2.0f, "Object Blur Radius Sunset");
	rScreen.WrapperSlider("Object Shadows Terrain Shadow Fade", iSection, 2.0f, "Terrain Shadow Height Fade");
	rScreen.WrapperSlider("Object Shadows Underwater Fade", iSection, 2.0f, "Underwater Fade");
	rScreen.WrapperSlider("Smoke Shadow Intensity Bottom", iSection, 2.0f, "Bottom Smoke Shadow Intensity");
	rScreen.WrapperSlider("Smoke Shadow Intensity Top", iSection, 2.0f, "Top Smoke Shadow Intensity");
}

} // namespace engine

#endif // BT_CLIENT
