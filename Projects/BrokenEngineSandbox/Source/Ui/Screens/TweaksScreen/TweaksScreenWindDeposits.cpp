#include "TweaksScreen.h"

#include "Ui/Screens/TweaksScreen/TweaksSliderMap.h"

#include "Ui/WindDepositsWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

const engine::TweaksSliderMapRegistrar gWindDepositsRegistrar
{
	{"Player Deposit Width", &gWindDepositPlayerWidth},
	{"Player Deposit Intensity", &gWindDepositPlayerIntensity},
	{"Player Deposit Length Multiplier", &gWindDepositPlayerLengthMultiplier},
	{"Spaceships Deposit Width", &gWindDepositSpaceshipsWidth},
	{"Spaceships Deposit Intensity", &gWindDepositSpaceshipsIntensity},
	{"Spaceships Deposit Length Multiplier", &gWindDepositSpaceshipsLengthMultiplier},
	{"Player Blasters Deposit Width", &gWindDepositPlayerBlastersWidth},
	{"Player Blasters Deposit Intensity", &gWindDepositPlayerBlastersIntensity},
	{"Blasters Deposit Length Multiplier", &gWindDepositPlayerBlastersLengthMultiplier},
	{"Spaceships Blasters Deposit Width", &gWindDepositSpaceshipsBlastersWidth},
	{"Spaceships Blasters Deposit Intensity", &gWindDepositSpaceshipsBlastersIntensity},
	{"Explosions Deposit Width", &gWindDepositExplosionsWidth},
	{"Explosions Deposit Intensity", &gWindDepositExplosionsIntensity},
};

void RenderWindDepositsTab(engine::TweaksScreenBase& rScreen)
{
	int64_t iSection = engine::giTweakSectionWind;

	rScreen.WrapperSeparatorText("Player");
	rScreen.WrapperSlider("Player Deposit Width", iSection);
	rScreen.WrapperSlider("Player Deposit Intensity", iSection);
	rScreen.WrapperSlider("Player Deposit Length Multiplier", iSection);

	rScreen.WrapperSeparatorText("Spaceships");
	rScreen.WrapperSlider("Spaceships Deposit Width", iSection);
	rScreen.WrapperSlider("Spaceships Deposit Intensity", iSection);
	rScreen.WrapperSlider("Spaceships Deposit Length Multiplier", iSection);

	rScreen.WrapperSeparatorText("Player Blasters");
	rScreen.WrapperSlider("Player Blasters Deposit Width", iSection);
	rScreen.WrapperSlider("Player Blasters Deposit Intensity", iSection);
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
