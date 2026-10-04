#include "SoundMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"
#include "Ui/SoundSettings.h"
#include "Ui/SoundSettingsWrappersBase.h"

#include "Game.h"

namespace engine
{

constexpr float kfSoundSliderWidthPixels = 640.0f;
constexpr std::string_view kMuteInBackgroundLabel = "Mute in background";

void SoundMenuScreen::Render()
{
	if (game::gpGame->meUiState != UiState::kSound)
	{
		return;
	}

	ImGuiIO& rInputOutput = ImGui::GetIO();
	ScopedMenuScale menuScale;

	// Centered via the pivot convention (UserInterfaceDesign.txt section 5), matching Pause/Graphics
	ImGui::SetNextWindowPos(ImVec2(rInputOutput.DisplaySize.x * 0.5f, rInputOutput.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ScopedMenuFont menuFont;
	ImGui::Begin("SoundMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRectangle occlusion
	ImVec2 vPanelPosition = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPosition, ImVec2(vPanelPosition.x + vPanelSize.x, vPanelPosition.y + vPanelSize.y));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	MenuHeading(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringAudio)).mpData, kfMainMenuHeadingScale);

	float fSliderWidth = kfSoundSliderWidthPixels * UiScale();
	ImGui::SetNextItemWidth(fSliderWidth);
	WrapperSlider("Master Volume", &gMasterVolume);
	ImGui::SetNextItemWidth(fSliderWidth);
	WrapperSlider("Music Volume", &gMusicVolume);
	ImGui::SetNextItemWidth(fSliderWidth);
	WrapperSlider("Sound Volume", &gSoundVolume);
	float fMuteRowWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(kMuteInBackgroundLabel.data()).x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, 0.5f * (ImGui::GetContentRegionAvail().x - fMuteRowWidth)));
	WrapperToggle(kMuteInBackgroundLabel, &gMuteInBackground);

	ImGui::Separator();

	// One themed width shared by both buttons (measured under the live menu font)
	float fButtonWidth = MenuButtonsWidth({TranslatedString(StandardString::kStringDefaults), U"Back"});

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringDefaults)).mpData, ImVec2(fButtonWidth, 0.0f), mfDefaultsHoverAnimation))
	{
		ResetSoundSettings();
	}

	ImGui::SameLine();

	if (MenuButton("Back", ImVec2(fButtonWidth, 0.0f), mfBackHoverAnimation))
	{
		game::gpGame->meUiState = UiState::kPause;
	}

	ImGui::End();
}

} // namespace engine

#endif // BT_CLIENT
