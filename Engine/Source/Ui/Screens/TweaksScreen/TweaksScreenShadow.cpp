#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/ShadowWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gShadowRegistrar
{
	{"Resolution", &gShadowRenderMultiplier},
	{"Shadow Texel Ramp Speed", &gShadowTexelRampMetersPerSecond},
	{"Shadow Temporal Blend", &gShadowTemporalBlend},
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
	{"Object Blur Sigma", &gObjectShadowsBlurSigma},
	{"Object Blur Radius", &gObjectShadowsBlurRadius},
	{"Object Shadow Grow", &gObjectShadowsGrow},
	{"Smoke Shadow Intensity", &gSmokeShadowIntensity},
};

void RenderShadowSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionShadow;

	rScreen.WrapperSeparatorText("Quality / Perf");
	rScreen.WrapperSlider("Resolution", iSection);
	rScreen.WrapperSlider("Texel Contraction Speed", iSection, 2.0f, "Shadow Texel Ramp Speed");
	rScreen.WrapperSlider("Temporal Blend", iSection, 2.0f, "Shadow Temporal Blend");

	rScreen.WrapperSeparatorText("Feather");
	rScreen.WrapperSlider("Feather Noon", iSection);
	rScreen.WrapperSlider("Feather Noon Offset", iSection);
	rScreen.WrapperSlider("Feather Sunset", iSection);
	rScreen.WrapperSlider("Feather Sunset Offset", iSection);
	rScreen.WrapperSlider("Feather Power", iSection);
	rScreen.WrapperSlider("Distance Falloff", iSection);
	rScreen.WrapperSlider("Blur Sigma", iSection);
	rScreen.WrapperSlider("Affect Ambient", iSection);
	rScreen.WrapperSlider("Height Fade Top", iSection);
	rScreen.WrapperSlider("Height Fade Bottom", iSection);

	rScreen.WrapperSeparatorText("Object Shadows");
	rScreen.WrapperSlider("Render Multiplier", iSection);
	rScreen.WrapperSlider("Blur Multiplier", iSection);
	rScreen.WrapperSlider("Shadow Noon", iSection);
	rScreen.WrapperSlider("Shadow Sunset", iSection);
	rScreen.WrapperSlider("Sunset Stretch", iSection);
	rScreen.WrapperSlider("Object Blur Sigma", iSection);
	rScreen.WrapperSlider("Object Blur Radius", iSection);
	rScreen.WrapperSlider("Grow", iSection, 2.0f, "Object Shadow Grow");
	rScreen.WrapperSlider("Smoke Shadow Intensity", iSection);
}

} // namespace engine

#endif // BT_CLIENT
