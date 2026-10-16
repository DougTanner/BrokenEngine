#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/HexShieldWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gHexShieldRegistrar
{
	{"Grow", &gHexShieldGrow},
	{"Edge Distance", &gHexShieldEdgeDistance},
	{"Edge Power", &gHexShieldEdgePower},
	{"Edge Multiplier", &gHexShieldEdgeMultiplier},
	{"Wave Multiplier", &gHexShieldWaveMultiplier},
	{"Wave Dot", &gHexShieldWaveDotMultiplier},
	{"Wave Intensity", &gHexShieldWaveIntensityMultiplier},
	{"Wave Intensity Power", &gHexShieldWaveIntensityPower},
	{"Wave Falloff Power", &gHexShieldWaveFalloffPower},
	{"Direction Falloff Power", &gHexShieldDirectionFalloffPower},
	{"Direction Multiplier", &gHexShieldDirectionMultiplier},
};

void RenderHexShieldSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionHexShield;

	rScreen.WrapperSeparatorText("Edge");
	rScreen.WrapperSlider("Grow", iSection);
	rScreen.WrapperSlider("Edge Distance", iSection);
	rScreen.WrapperSlider("Edge Power", iSection);
	rScreen.WrapperSlider("Edge Multiplier", iSection);

	rScreen.WrapperSeparatorText("Wave");
	rScreen.WrapperSlider("Wave Multiplier", iSection);
	rScreen.WrapperSlider("Wave Dot", iSection);
	rScreen.WrapperSlider("Wave Intensity", iSection);
	rScreen.WrapperSlider("Wave Intensity Power", iSection);
	rScreen.WrapperSlider("Wave Falloff Power", iSection);

	rScreen.WrapperSeparatorText("Direction");
	rScreen.WrapperSlider("Direction Falloff Power", iSection);
	rScreen.WrapperSlider("Direction Multiplier", iSection);
}

} // namespace engine

#endif // BT_CLIENT
