#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/HexShieldWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gHexShieldRegistrar
{
	{"Grow", &gHexShieldGrow},
	{"Edge Distance", &gHexShieldEdgeDistance},
	{"Edge Multiplier", &gHexShieldEdgeMultiplier},
	{"Wave Multiplier", &gHexShieldWaveMultiplier},
	{"Wave Dot", &gHexShieldWaveDotMultiplier},
	{"Wave Intensity", &gHexShieldWaveIntensityMultiplier},
	{"Wave Intensity Power", &gHexShieldWaveIntensityPower},
	{"Wave Falloff Power", &gHexShieldWaveFalloffPower},
	{"Direction Falloff Power", &gHexShieldDirectionFalloffPower},
	{"Direction Multiplier", &gHexShieldDirectionMultiplier},
};

void RenderHexShieldSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionHexShield;

	rScreen.WrapperSeparatorText("Edge");
	rScreen.WrapperSlider("Hex Shield Grow", iSection, 2.0f, "Grow");
	rScreen.WrapperSlider("Hex Shield Edge Distance", iSection, 2.0f, "Edge Distance");
	rScreen.WrapperSlider("Hex Shield Edge Multiplier", iSection, 2.0f, "Edge Multiplier");

	rScreen.WrapperSeparatorText("Wave");
	rScreen.WrapperSlider("Hex Shield Wave Multiplier", iSection, 2.0f, "Wave Multiplier");
	rScreen.WrapperSlider("Hex Shield Wave Dot Multiplier", iSection, 2.0f, "Wave Dot");
	rScreen.WrapperSlider("Hex Shield Wave Intensity Multiplier", iSection, 2.0f, "Wave Intensity");
	rScreen.WrapperSlider("Hex Shield Wave Intensity Power", iSection, 2.0f, "Wave Intensity Power");
	rScreen.WrapperSlider("Hex Shield Wave Falloff Power", iSection, 2.0f, "Wave Falloff Power");

	rScreen.WrapperSeparatorText("Direction");
	rScreen.WrapperSlider("Hex Shield Direction Falloff Power", iSection, 2.0f, "Direction Falloff Power");
	rScreen.WrapperSlider("Hex Shield Direction Multiplier", iSection, 2.0f, "Direction Multiplier");
}

} // namespace engine

#endif // BT_CLIENT
