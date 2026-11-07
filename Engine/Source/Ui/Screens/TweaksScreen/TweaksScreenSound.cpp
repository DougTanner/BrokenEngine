#include "TweaksScreen.h"

#if defined(BT_CLIENT)

#include "Ui/SoundSettingsWrappersBase.h"

#include "Ui/Screens/TweaksScreen/Tweaks.h"

namespace engine
{

void RenderSoundSection(TweaksScreen& rScreen)
{
	int64_t iSection = giTweakSectionSound;

	if (ImGui::BeginTabBar("SoundTabs"))
	{
		if (rScreen.BeginSubtab("Volumes", iSection, 0))
		{
			// Engine settings rendered inline (no outer table) so the game's RenderSoundTweakExtensions, called by name below, can own its own 2-column table at full sub-tab width.
			rScreen.WrapperSeparatorText("Settings");
			rScreen.WrapperSlider("Master Volume", gMasterVolume, iSection, 1.0f);
			rScreen.WrapperSlider("Music Volume", gMusicVolume, iSection, 1.0f);
			rScreen.WrapperSlider("Sound Volume", gSoundVolume, iSection, 1.0f);

			game::RenderSoundTweakExtensions(rScreen);

			ImGui::EndTabItem();
		}
		if (rScreen.BeginSubtab("Tweaks", iSection, 1))
		{
			rScreen.WrapperSeparatorText("Listener Distance Start");
			rScreen.WrapperSlider("Listener Distance Start Start Height", gListenerDistanceStart.startHeight, iSection);
			rScreen.WrapperSlider("Listener Distance Start End Height", gListenerDistanceStart.endHeight, iSection);
			rScreen.WrapperSlider("Listener Distance Start Low", gListenerDistanceStart.low, iSection);
			rScreen.WrapperSlider("Listener Distance Start High", gListenerDistanceStart.high, iSection);

			rScreen.WrapperSeparatorText("Listener Distance End");
			rScreen.WrapperSlider("Listener Distance End Start Height", gListenerDistanceEnd.startHeight, iSection);
			rScreen.WrapperSlider("Listener Distance End End Height", gListenerDistanceEnd.endHeight, iSection);
			rScreen.WrapperSlider("Listener Distance End Low", gListenerDistanceEnd.low, iSection);
			rScreen.WrapperSlider("Listener Distance End High", gListenerDistanceEnd.high, iSection);

			rScreen.WrapperSeparatorText("Listener Curve");
			rScreen.WrapperSlider("Listener Curve Start Height", gListenerCurve.startHeight, iSection);
			rScreen.WrapperSlider("Listener Curve End Height", gListenerCurve.endHeight, iSection);
			rScreen.WrapperSlider("Listener Curve Low", gListenerCurve.low, iSection);
			rScreen.WrapperSlider("Listener Curve High", gListenerCurve.high, iSection);

			rScreen.WrapperSeparatorText("Listener Audible Floor");
			rScreen.WrapperSlider("Listener Audible Floor Start Height", gListenerAudibleFloor.startHeight, iSection);
			rScreen.WrapperSlider("Listener Audible Floor End Height", gListenerAudibleFloor.endHeight, iSection);
			rScreen.WrapperSlider("Listener Audible Floor Low", gListenerAudibleFloor.low, iSection);
			rScreen.WrapperSlider("Listener Audible Floor High", gListenerAudibleFloor.high, iSection);

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
}

} // namespace engine

#endif // BT_CLIENT
