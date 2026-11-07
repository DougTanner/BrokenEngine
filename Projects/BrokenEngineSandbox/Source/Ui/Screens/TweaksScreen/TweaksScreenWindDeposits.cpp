#include "Tweaks.h"

#include "Ui/WindDepositsWrappers.h"

#if defined(BT_CLIENT)

namespace game
{

void RenderWindDepositsTab(engine::TweaksScreen& rScreen)
{
	int64_t iSection = engine::giTweakSectionWind;

	rScreen.WrapperSeparatorText("Omni");
	rScreen.WrapperSlider("Wind Deposit Omni Width", gWindDepositOmniWidth, iSection);
	rScreen.WrapperSlider("Wind Deposit Omni Intensity", gWindDepositOmniIntensity, iSection);
	rScreen.WrapperSlider("Wind Deposit Omni Length Multiplier", gWindDepositOmniLengthMultiplier, iSection);

	rScreen.WrapperSeparatorText("Fighters");
	rScreen.WrapperSlider("Wind Deposit Fighters Width", gWindDepositFightersWidth, iSection);
	rScreen.WrapperSlider("Wind Deposit Fighters Intensity", gWindDepositFightersIntensity, iSection);
	rScreen.WrapperSlider("Wind Deposit Fighters Length Multiplier", gWindDepositFightersLengthMultiplier, iSection);

	rScreen.WrapperSeparatorText("Omni Blasters");
	rScreen.WrapperSlider("Wind Deposit Omni Blasters Width", gWindDepositOmniBlastersWidth, iSection);
	rScreen.WrapperSlider("Wind Deposit Omni Blasters Intensity", gWindDepositOmniBlastersIntensity, iSection);
	rScreen.WrapperSlider("Wind Deposit Omni Blasters Length Multiplier", gWindDepositOmniBlastersLengthMultiplier, iSection);

	rScreen.WrapperSeparatorText("Explosions");
	rScreen.WrapperSlider("Wind Deposit Explosions Width", gWindDepositExplosionsWidth, iSection);
	rScreen.WrapperSlider("Wind Deposit Explosions Intensity", gWindDepositExplosionsIntensity, iSection);
}

} // namespace game

#endif // BT_CLIENT
