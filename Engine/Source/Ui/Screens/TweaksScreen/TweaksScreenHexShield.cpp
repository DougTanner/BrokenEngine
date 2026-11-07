#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/HexShieldWrappersBase.h"

namespace engine
{

void RenderHexShieldSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionHexShield;

	rScreen.WrapperSeparatorText("Edge");
	rScreen.WrapperSlider("Hex Shield Grow", gHexShieldGrow, iSection);
	rScreen.WrapperSlider("Hex Shield Edge Distance", gHexShieldEdgeDistance, iSection);
	rScreen.WrapperSlider("Hex Shield Edge Multiplier", gHexShieldEdgeMultiplier, iSection);

	rScreen.WrapperSeparatorText("Wave");
	rScreen.WrapperSlider("Hex Shield Wave Multiplier", gHexShieldWaveMultiplier, iSection);
	rScreen.WrapperSlider("Hex Shield Wave Dot Multiplier", gHexShieldWaveDotMultiplier, iSection);
	rScreen.WrapperSlider("Hex Shield Wave Intensity Multiplier", gHexShieldWaveIntensityMultiplier, iSection);
	rScreen.WrapperSlider("Hex Shield Wave Intensity Power", gHexShieldWaveIntensityPower, iSection);
	rScreen.WrapperSlider("Hex Shield Wave Falloff Power", gHexShieldWaveFalloffPower, iSection);

	rScreen.WrapperSeparatorText("Direction");
	rScreen.WrapperSlider("Hex Shield Direction Falloff Power", gHexShieldDirectionFalloffPower, iSection);
	rScreen.WrapperSlider("Hex Shield Direction Multiplier", gHexShieldDirectionMultiplier, iSection);
}

} // namespace engine

#endif // BT_CLIENT
