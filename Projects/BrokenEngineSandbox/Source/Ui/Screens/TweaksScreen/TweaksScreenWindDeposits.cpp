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
	{"Spaceships Deposit Width", &gWindDepositSpaceshipsWidth},
	{"Spaceships Deposit Intensity", &gWindDepositSpaceshipsIntensity},
	{"Spaceships Deposit Length Multiplier", &gWindDepositSpaceshipsLengthMultiplier},
	{"Omni Blasters Deposit Width", &gWindDepositOmniBlastersWidth},
	{"Omni Blasters Deposit Intensity", &gWindDepositOmniBlastersIntensity},
	{"Blasters Deposit Length Multiplier", &gWindDepositOmniBlastersLengthMultiplier},
	{"Spaceships Blasters Deposit Width", &gWindDepositSpaceshipsBlastersWidth},
	{"Spaceships Blasters Deposit Intensity", &gWindDepositSpaceshipsBlastersIntensity},
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

	rScreen.WrapperSeparatorText("Spaceships");
	rScreen.WrapperSlider("Spaceships Deposit Width", iSection);
	rScreen.WrapperSlider("Spaceships Deposit Intensity", iSection);
	rScreen.WrapperSlider("Spaceships Deposit Length Multiplier", iSection);

	rScreen.WrapperSeparatorText("Omni Blasters");
	rScreen.WrapperSlider("Omni Blasters Deposit Width", iSection);
	rScreen.WrapperSlider("Omni Blasters Deposit Intensity", iSection);
	rScreen.WrapperSlider("Blasters Deposit Length Multiplier", iSection);

	rScreen.WrapperSeparatorText("Spaceships Blasters");
	rScreen.WrapperSlider("Spaceships Blasters Deposit Width", iSection);
	rScreen.WrapperSlider("Spaceships Blasters Deposit Intensity", iSection);

	rScreen.WrapperSeparatorText("Explosions");
	rScreen.WrapperSlider("Explosions Deposit Width", iSection);
	rScreen.WrapperSlider("Explosions Deposit Intensity", iSection);
}

} // namespace game

#endif // BT_CLIENT
