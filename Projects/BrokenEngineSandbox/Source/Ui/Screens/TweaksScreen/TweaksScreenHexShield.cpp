#include "TweaksScreen.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"
#include "Ui/HexShieldWrappersBase.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gHexShieldRegistrar
{
	{"Grow", &engine::gHexShieldGrow},
	{"Edge Distance", &engine::gHexShieldEdgeDistance},
	{"Edge Power", &engine::gHexShieldEdgePower},
	{"Edge Multiplier", &engine::gHexShieldEdgeMultiplier},
	{"Wave Multiplier", &engine::gHexShieldWaveMultiplier},
	{"Wave Dot", &engine::gHexShieldWaveDotMultiplier},
	{"Wave Intensity", &engine::gHexShieldWaveIntensityMultiplier},
	{"Wave Intensity Power", &engine::gHexShieldWaveIntensityPower},
	{"Wave Falloff Power", &engine::gHexShieldWaveFalloffPower},
	{"Direction Falloff Power", &engine::gHexShieldDirectionFalloffPower},
	{"Direction Multiplier", &engine::gHexShieldDirectionMultiplier},
};

void RenderHexShieldSection(engine::TweaksScreenBase& rScreen)
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

} // namespace game

#endif // BT_CLIENT
