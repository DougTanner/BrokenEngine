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
	// Listener Distance Start
	{"Listener Distance Start Start Height", &gListenerDistanceStart.StartHeight},
	{"Listener Distance Start End Height", &gListenerDistanceStart.EndHeight},
	{"Listener Distance Start Low", &gListenerDistanceStart.Low},
	{"Listener Distance Start High", &gListenerDistanceStart.High},
	// Listener Distance End
	{"Listener Distance End Start Height", &gListenerDistanceEnd.StartHeight},
	{"Listener Distance End End Height", &gListenerDistanceEnd.EndHeight},
	{"Listener Distance End Low", &gListenerDistanceEnd.Low},
	{"Listener Distance End High", &gListenerDistanceEnd.High},
	// Listener Curve
	{"Listener Curve Start Height", &gListenerCurve.StartHeight},
	{"Listener Curve End Height", &gListenerCurve.EndHeight},
	{"Listener Curve Low", &gListenerCurve.Low},
	{"Listener Curve High", &gListenerCurve.High},
	// Listener Audible Floor
	{"Listener Audible Floor Start Height", &gListenerAudibleFloor.StartHeight},
	{"Listener Audible Floor End Height", &gListenerAudibleFloor.EndHeight},
	{"Listener Audible Floor Low", &gListenerAudibleFloor.Low},
	{"Listener Audible Floor High", &gListenerAudibleFloor.High},
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
