#include "SoundMenuScreen.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "Ui/MenuUtils.h"
#include "Ui/Localization.h"
#include "Ui/SoundSettingsWrappersBase.h"

namespace game
{

namespace
{

constexpr float kfSoundSliderWidthPixels = 640.0f;
constexpr std::string_view kMuteInBackgroundLabel = "Mute in background";

} // namespace

void SoundMenuScreen::Render()
{
	if (gpGame->meUiState != engine::UiState::kSound)
	{
		return;
	}

	ImGuiIO& rIo = ImGui::GetIO();
	engine::ScopedMenuScale menuScale;

	// Centered via the pivot convention (UserInterfaceDesign.txt section 5), matching Pause/Graphics
	ImGui::SetNextWindowPos(ImVec2(rIo.DisplaySize.x * 0.5f, rIo.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	engine::ScopedMenuFont menuFont;
	ImGui::Begin("SoundMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	engine::gpImGuiManager->RegisterOpaqueRect(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRect occlusion
	ImVec2 vPanelPos = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	engine::DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPos, ImVec2(vPanelPos.x + vPanelSize.x, vPanelPos.y + vPanelSize.y));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	engine::MenuHeading(engine::AppendUtf8(rWorkbuffer, TranslatedString(kStringAudio)), engine::kfMainMenuHeadingScale);

	const float fSliderWidth = kfSoundSliderWidthPixels * engine::UiScale();
	ImGui::SetNextItemWidth(fSliderWidth);
	engine::WrapperSlider("Master Volume", &engine::gMasterVolume);
	ImGui::SetNextItemWidth(fSliderWidth);
	engine::WrapperSlider("Music Volume", &engine::gMusicVolume);
	ImGui::SetNextItemWidth(fSliderWidth);
	engine::WrapperSlider("Sound Volume", &engine::gSoundVolume);
	const float fMuteRowWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(kMuteInBackgroundLabel.data()).x;
	ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, 0.5f * (ImGui::GetContentRegionAvail().x - fMuteRowWidth)));
	engine::WrapperToggle(kMuteInBackgroundLabel, &engine::gMuteInBackground);

	ImGui::Separator();

	// One themed width shared by both buttons (measured under the live menu font)
	float fButtonWidth = engine::MenuButtonsWidth({TranslatedString(kStringDefaults), U"Back"});

	// Defaults button
	if (engine::MenuButton(engine::AppendUtf8(rWorkbuffer, TranslatedString(kStringDefaults)), ImVec2(fButtonWidth, 0.0f), mfDefaultsHoverAnim))
	{
		ResetSoundSettings();
	}

	ImGui::SameLine();

	// Back button
	if (engine::MenuButton("Back", ImVec2(fButtonWidth, 0.0f), mfBackHoverAnim))
	{
		gpGame->meUiState = engine::UiState::kPause;
	}

	ImGui::End();
}

} // namespace game

#endif // BT_CLIENT
