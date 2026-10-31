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
	rScreen.WrapperSlider("Omni Deposit Width", iSection);
	rScreen.WrapperSlider("Omni Deposit Intensity", iSection);
	rScreen.WrapperSlider("Omni Deposit Length Multiplier", iSection);

	rScreen.WrapperSeparatorText("Fighters");
	rScreen.WrapperSlider("Fighters Deposit Width", iSection);
	rScreen.WrapperSlider("Fighters Deposit Intensity", iSection);
	rScreen.WrapperSlider("Fighters Deposit Length Multiplier", iSection);

	rScreen.WrapperSeparatorText("Omni Blasters");
	rScreen.WrapperSlider("Omni Blasters Deposit Width", iSection);
	rScreen.WrapperSlider("Omni Blasters Deposit Intensity", iSection);
	rScreen.WrapperSlider("Blasters Deposit Length Multiplier", iSection);

	rScreen.WrapperSeparatorText("Explosions");
	rScreen.WrapperSlider("Explosions Deposit Width", iSection);
	rScreen.WrapperSlider("Explosions Deposit Intensity", iSection);
}

} // namespace game

#endif // BT_CLIENT
