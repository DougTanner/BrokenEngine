#include "Tweaks.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/WindDepositsWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gWindDepositsRegistrar
{
	{"Omni Deposit Width", &gWindDepositOmniWidth},
	{"Omni Deposit Intensity", &gWindDepositOmniIntensity},
	{"Omni Deposit Length Multiplier", &gWindDepositOmniLengthMultiplier},
	{"Fighters Deposit Width", &gWindDepositFightersWidth},
	{"Fighters Deposit Intensity", &gWindDepositFightersIntensity},
	{"Fighters Deposit Length Multiplier", &gWindDepositFightersLengthMultiplier},
	{"Omni Blasters Deposit Width", &gWindDepositOmniBlastersWidth},
	{"Omni Blasters Deposit Intensity", &gWindDepositOmniBlastersIntensity},
	{"Blasters Deposit Length Multiplier", &gWindDepositOmniBlastersLengthMultiplier},
	{"Explosions Deposit Width", &gWindDepositExplosionsWidth},
	{"Explosions Deposit Intensity", &gWindDepositExplosionsIntensity},
};

void RenderWindDepositsTab(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionWind;

	rScreen.WrapperSeparatorText("Omni");
	rScreen.WrapperSlider("Wind Deposit Omni Width", iSection, 2.0f, "Omni Deposit Width");
	rScreen.WrapperSlider("Wind Deposit Omni Intensity", iSection, 2.0f, "Omni Deposit Intensity");
	rScreen.WrapperSlider("Wind Deposit Omni Length Multiplier", iSection, 2.0f, "Omni Deposit Length Multiplier");

	rScreen.WrapperSeparatorText("Fighters");
	rScreen.WrapperSlider("Wind Deposit Fighters Width", iSection, 2.0f, "Fighters Deposit Width");
	rScreen.WrapperSlider("Wind Deposit Fighters Intensity", iSection, 2.0f, "Fighters Deposit Intensity");
	rScreen.WrapperSlider("Wind Deposit Fighters Length Multiplier", iSection, 2.0f, "Fighters Deposit Length Multiplier");

	rScreen.WrapperSeparatorText("Omni Blasters");
	rScreen.WrapperSlider("Wind Deposit Omni Blasters Width", iSection, 2.0f, "Omni Blasters Deposit Width");
	rScreen.WrapperSlider("Wind Deposit Omni Blasters Intensity", iSection, 2.0f, "Omni Blasters Deposit Intensity");
	rScreen.WrapperSlider("Wind Deposit Omni Blasters Length Multiplier", iSection, 2.0f, "Blasters Deposit Length Multiplier");

	rScreen.WrapperSeparatorText("Explosions");
	rScreen.WrapperSlider("Wind Deposit Explosions Width", iSection, 2.0f, "Explosions Deposit Width");
	rScreen.WrapperSlider("Wind Deposit Explosions Intensity", iSection, 2.0f, "Explosions Deposit Intensity");
}

} // namespace game

#endif // BT_CLIENT
