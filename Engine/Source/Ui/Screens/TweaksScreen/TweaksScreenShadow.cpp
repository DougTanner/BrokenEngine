#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/MenuUtils.h"
#include "Ui/ShadowWrappersBase.h"

namespace engine
{

void RenderShadowSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionShadow;

	rScreen.WrapperSeparatorText("Quality / Perf");
	rScreen.WrapperSlider("Shadow Render Multiplier", gShadowRenderMultiplier, iSection);
	rScreen.WrapperSlider("Shadow Texel Ramp Meters Per Second", gShadowTexelRampMetersPerSecond, iSection);
	rScreen.WrapperSlider("Shadow Temporal Blend", gShadowTemporalBlend, iSection);
	rScreen.WrapperSlider("Shadow Update Cadence", gShadowUpdateCadence, iSection);

	rScreen.WrapperSeparatorText("Feather");
	rScreen.WrapperSlider("Shadow Feather Noon", gShadowFeatherNoon, iSection);
	rScreen.WrapperSlider("Shadow Feather Noon Offset", gShadowFeatherNoonOffset, iSection);
	rScreen.WrapperSlider("Shadow Feather Sunset", gShadowFeatherSunset, iSection);
	rScreen.WrapperSlider("Shadow Feather Sunset Offset", gShadowFeatherSunsetOffset, iSection);
	rScreen.WrapperSlider("Shadow Feather Power", gShadowFeatherPower, iSection);
	rScreen.WrapperSlider("Shadow Distance Falloff", gShadowDistanceFalloff, iSection);
	rScreen.WrapperSlider("Shadow Blur Sigma", gShadowBlurSigma, iSection);
	rScreen.WrapperSlider("Shadow Affect Ambient", gShadowAffectAmbient, iSection);
	rScreen.WrapperSlider("Shadow Height Fade Top", gShadowHeightFadeTop, iSection);
	rScreen.WrapperSlider("Shadow Height Fade Bottom", gShadowHeightFadeBottom, iSection);

	rScreen.WrapperSeparatorText("Object Shadows");
	rScreen.WrapperSlider("Object Shadows Render Multiplier", gObjectShadowsRenderMultiplier, iSection);
	rScreen.WrapperSlider("Object Shadows Blur Multiplier", gObjectShadowsBlurMultiplier, iSection);
	rScreen.WrapperSlider("Object Shadow Cadence", gObjectShadowCadence, iSection);
	rScreen.WrapperSlider("Object Shadows Noon", gObjectShadowsNoon, iSection);
	rScreen.WrapperSlider("Object Shadows Sunset", gObjectShadowsSunset, iSection);
	rScreen.WrapperSlider("Object Shadows Sunset Stretch", gObjectShadowsSunsetStretch, iSection);
	rScreen.WrapperSlider("Object Shadows Stretch Power", gObjectShadowsStretchPower, iSection);
	rScreen.WrapperSlider("Object Shadows Blur Sigma Low", gObjectShadowsBlurSigmaLow, iSection);
	rScreen.WrapperSlider("Object Shadows Blur Sigma High", gObjectShadowsBlurSigmaHigh, iSection);
	rScreen.WrapperSlider("Object Shadows Blur Radius Noon", gObjectShadowsBlurRadiusNoon, iSection);
	rScreen.WrapperSlider("Object Shadows Blur Radius Sunset", gObjectShadowsBlurRadiusSunset, iSection);
	rScreen.WrapperSlider("Object Shadows Terrain Shadow Fade", gObjectShadowsTerrainShadowFade, iSection);
	rScreen.WrapperSlider("Object Shadows Underwater Fade", gObjectShadowsUnderwaterFade, iSection);
	rScreen.WrapperSlider("Smoke Shadow Intensity Bottom", gSmokeShadowIntensityBottom, iSection);
	rScreen.WrapperSlider("Smoke Shadow Intensity Top", gSmokeShadowIntensityTop, iSection);
}

} // namespace engine

#endif // BT_CLIENT
