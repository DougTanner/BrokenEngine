#include "TweaksScreenBase.h"

#if defined(BT_CLIENT)

#include "Ui/SoundSettingsWrappersBase.h"
#include "TweaksSliderMap.h"

namespace engine
{

const TweaksSliderMapRegistrar gSoundRegistrar
{
	{"Master Volume", &gMasterVolume},
	{"Music Volume", &gMusicVolume},
	{"Sound Volume", &gSoundVolume},
	{"Listener Distance Start Start Height", &gListenerDistanceStart.startHeight},
	{"Listener Distance Start End Height", &gListenerDistanceStart.endHeight},
	{"Listener Distance Start Low", &gListenerDistanceStart.low},
	{"Listener Distance Start High", &gListenerDistanceStart.high},
	{"Listener Distance End Start Height", &gListenerDistanceEnd.startHeight},
	{"Listener Distance End End Height", &gListenerDistanceEnd.endHeight},
	{"Listener Distance End Low", &gListenerDistanceEnd.low},
	{"Listener Distance End High", &gListenerDistanceEnd.high},
	{"Listener Curve Start Height", &gListenerCurve.startHeight},
	{"Listener Curve End Height", &gListenerCurve.endHeight},
	{"Listener Curve Low", &gListenerCurve.low},
	{"Listener Curve High", &gListenerCurve.high},
	{"Listener Audible Floor Start Height", &gListenerAudibleFloor.startHeight},
	{"Listener Audible Floor End Height", &gListenerAudibleFloor.endHeight},
	{"Listener Audible Floor Low", &gListenerAudibleFloor.low},
	{"Listener Audible Floor High", &gListenerAudibleFloor.high},
};

void RenderSoundSection(TweaksScreenBase& rScreen)
{
	int64_t iSection = giTweakSectionSound;

	if (ImGui::BeginTabBar("SoundTabs"))
	{
		if (rScreen.BeginSubtab("Volumes", iSection, 0))
		{
			// Engine settings rendered inline (no outer table) so the game-side hook can own its own 2-column table at full sub-tab width.
			rScreen.WrapperSeparatorText("Settings");
			rScreen.WrapperSlider("Master", iSection, 1.0f, "Master Volume");
			rScreen.WrapperSlider("Music", iSection, 1.0f, "Music Volume");
			rScreen.WrapperSlider("Sound", iSection, 1.0f, "Sound Volume");

			rScreen.RenderSoundEffects();

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Tweaks", iSection, 1))
		{
			rScreen.WrapperSeparatorText("Listener Distance Start");
			rScreen.WrapperSlider("Start Height", iSection, 2.0f, "Listener Distance Start Start Height");
			rScreen.WrapperSlider("End Height", iSection, 2.0f, "Listener Distance Start End Height");
			rScreen.WrapperSlider("Low", iSection, 2.0f, "Listener Distance Start Low");
			rScreen.WrapperSlider("High", iSection, 2.0f, "Listener Distance Start High");

			rScreen.WrapperSeparatorText("Listener Distance End");
			rScreen.WrapperSlider("Start Height", iSection, 2.0f, "Listener Distance End Start Height");
			rScreen.WrapperSlider("End Height", iSection, 2.0f, "Listener Distance End End Height");
			rScreen.WrapperSlider("Low", iSection, 2.0f, "Listener Distance End Low");
			rScreen.WrapperSlider("High", iSection, 2.0f, "Listener Distance End High");

			rScreen.WrapperSeparatorText("Listener Curve");
			rScreen.WrapperSlider("Start Height", iSection, 2.0f, "Listener Curve Start Height");
			rScreen.WrapperSlider("End Height", iSection, 2.0f, "Listener Curve End Height");
			rScreen.WrapperSlider("Low", iSection, 2.0f, "Listener Curve Low");
			rScreen.WrapperSlider("High", iSection, 2.0f, "Listener Curve High");

			rScreen.WrapperSeparatorText("Listener Audible Floor");
			rScreen.WrapperSlider("Start Height", iSection, 2.0f, "Listener Audible Floor Start Height");
			rScreen.WrapperSlider("End Height", iSection, 2.0f, "Listener Audible Floor End Height");
			rScreen.WrapperSlider("Low", iSection, 2.0f, "Listener Audible Floor Low");
			rScreen.WrapperSlider("High", iSection, 2.0f, "Listener Audible Floor High");

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
