#include "SoundMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"
#include "Ui/SoundSettings.h"
#include "Ui/SoundSettingsWrappersBase.h"

namespace engine
{

namespace
{

constexpr float kfSoundSliderWidthPixels = 640.0f;
constexpr std::string_view kMuteInBackgroundLabel = "Mute in background";

} // namespace

void SoundMenuScreen::Render(GameBase& rGame)
{
	using enum StandardString;

	if (rGame.meUiState != UiState::kSound)
	{
		return;
	}

	ImGuiIO& rIo = ImGui::GetIO();
	ScopedMenuScale menuScale;

	// Centered via the pivot convention (UserInterfaceDesign.txt section 5), matching Pause/Graphics
	ImGui::SetNextWindowPos(ImVec2(rIo.DisplaySize.x * 0.5f, rIo.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ScopedMenuFont menuFont;
	ImGui::Begin("SoundMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	gpImGuiManager->RegisterOpaqueRect(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRect occlusion
	ImVec2 vPanelPos = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPos, ImVec2(vPanelPos.x + vPanelSize.x, vPanelPos.y + vPanelSize.y));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	MenuHeading(AppendUtf8(rWorkbuffer, TranslatedString(kStringAudio)), kfMainMenuHeadingScale);

	const float fSliderWidth = kfSoundSliderWidthPixels * UiScale();
	ImGui::SetNextItemWidth(fSliderWidth);
	WrapperSlider("Master Volume", &gMasterVolume);
	ImGui::SetNextItemWidth(fSliderWidth);
	WrapperSlider("Music Volume", &gMusicVolume);
	ImGui::SetNextItemWidth(fSliderWidth);
	WrapperSlider("Sound Volume", &gSoundVolume);
	const float fMuteRowWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(kMuteInBackgroundLabel.data()).x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, 0.5f * (ImGui::GetContentRegionAvail().x - fMuteRowWidth)));
	WrapperToggle(kMuteInBackgroundLabel, &gMuteInBackground);

	ImGui::Separator();

	// One themed width shared by both buttons (measured under the live menu font)
	float fButtonWidth = MenuButtonsWidth({TranslatedString(kStringDefaults), U"Back"});

	// Defaults button
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringDefaults)), ImVec2(fButtonWidth, 0.0f), mfDefaultsHoverAnim))
	{
		ResetSoundSettings();
	}

	ImGui::SameLine();

	// Back button
	if (MenuButton("Back", ImVec2(fButtonWidth, 0.0f), mfBackHoverAnim))
	{
		rGame.meUiState = UiState::kPause;
	}

	ImGui::End();
}

} // namespace engine

#endif // BT_CLIENT
