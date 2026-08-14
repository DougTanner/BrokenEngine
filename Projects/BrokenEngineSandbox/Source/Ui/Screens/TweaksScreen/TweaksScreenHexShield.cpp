#include "TweaksScreen.h"
#include "Ui/HexShieldWrappersBase.h"

#if defined(BT_CLIENT)

namespace game
{

namespace
{
const engine::TweaksSliderMapRegistrar gHexShieldRegistrar
{
	// Edge
	{"Grow", &engine::gHexShieldGrow},
	{"Edge Distance", &engine::gHexShieldEdgeDistance},
	{"Edge Power", &engine::gHexShieldEdgePower},
	{"Edge Multiplier", &engine::gHexShieldEdgeMultiplier},
	// Wave
	{"Wave Multiplier", &engine::gHexShieldWaveMultiplier},
	{"Wave Dot", &engine::gHexShieldWaveDotMultiplier},
	{"Wave Intensity", &engine::gHexShieldWaveIntensityMultiplier},
	{"Wave Intensity Power", &engine::gHexShieldWaveIntensityPower},
	{"Wave Falloff Power", &engine::gHexShieldWaveFalloffPower},
	// Direction
	{"Direction Falloff Power", &engine::gHexShieldDirectionFalloffPower},
	{"Direction Multiplier", &engine::gHexShieldDirectionMultiplier},
};
}

void TweaksScreen::RenderHexShieldSection()
{
	const int64_t iSection = giTweakSectionHexShield;

	WrapperSeparatorText("Edge");
	WrapperSlider("Grow", iSection);
	WrapperSlider("Edge Distance", iSection);
	WrapperSlider("Edge Power", iSection);
	WrapperSlider("Edge Multiplier", iSection);

	WrapperSeparatorText("Wave");
	WrapperSlider("Wave Multiplier", iSection);
	WrapperSlider("Wave Dot", iSection);
	WrapperSlider("Wave Intensity", iSection);
	WrapperSlider("Wave Intensity Power", iSection);
	WrapperSlider("Wave Falloff Power", iSection);

	WrapperSeparatorText("Direction");
	WrapperSlider("Direction Falloff Power", iSection);
	WrapperSlider("Direction Multiplier", iSection);
}

} // namespace game

#endif // BT_CLIENT
