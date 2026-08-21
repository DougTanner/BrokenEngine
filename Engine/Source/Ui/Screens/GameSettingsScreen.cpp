#include "GameSettingsScreen.h"

#if defined(BT_CLIENT)

#include "Ui/GameSettings.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"

namespace engine
{

namespace
{

constexpr float kfUiOpacitySliderWidthPixels = 640.0f;

} // namespace

void GameSettingsScreen::Render(GameBase& rGame)
{
	using enum StandardString;

	if (rGame.meUiState != UiState::kGameSettings)
	{
		return;
	}

	ImGuiIO& rIo = ImGui::GetIO();
	ScopedMenuScale menuScale;

	// Centered via the pivot convention (UserInterfaceDesign.txt section 5), matching Pause/Sound
	ImGui::SetNextWindowPos(ImVec2(rIo.DisplaySize.x * 0.5f, rIo.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ScopedMenuFont menuFont;
	ImGui::Begin("GameSettingsMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	gpImGuiManager->RegisterOpaqueRect(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRect occlusion
	ImVec2 vPanelPos = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPos, ImVec2(vPanelPos.x + vPanelSize.x, vPanelPos.y + vPanelSize.y));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	MenuHeading(AppendUtf8(rWorkbuffer, TranslatedString(kStringGameSettings)), kfMainMenuHeadingScale);

	// Language row: the six labels stay in their own language, so they are never localized through the string table.
	// Font and geometry are stepped down to the utility tier so a six-button row reads below the primary controls.
	{
		const ImGuiStyle& rStyle = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(rStyle.FramePadding.x * kfLanguageMenuGeometryScale, rStyle.FramePadding.y * kfLanguageMenuGeometryScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(rStyle.ItemSpacing.x * kfLanguageMenuGeometryScale, rStyle.ItemSpacing.y * kfLanguageMenuGeometryScale));

		ScopedMenuFont languageFont(kfLanguageMenuFontScale);

		// Uniform text-driven width = max CalcTextSize over the 6 labels under the shared Latin/CJK font,
		// plus frame padding to match the button chrome. Buttons then sit one-per-row via SameLine (default spacing).
		float fLanguageButtonWidth = 0.0f;
		for (int64_t i = 0; i < kLanguageCount; ++i)
		{
			fLanguageButtonWidth = std::max(fLanguageButtonWidth, ImGui::CalcTextSize(kLanguageOptions[i].pcLabel).x);
		}
		fLanguageButtonWidth += ImGui::GetStyle().FramePadding.x * 4.0f;

		for (int64_t i = 0; i < kLanguageCount; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}

			bool bSelected = (geLanguage == kLanguageOptions[i].eLanguage);

			if (MenuButton(kLanguageOptions[i].pcLabel, ImVec2(fLanguageButtonWidth, 0.0f), mfLanguageHoverAnims[i], bSelected))
			{
				geLanguage = kLanguageOptions[i].eLanguage;
			}
		}

		ImGui::PopStyleVar(2);
	}

	WrapperPlusMinus("Font Size", &gUiFontScale, 0.1f);

	ImGui::Separator();

	WrapperToggle("Opaque UI", &gOpaqueUi);
	ImGui::SetNextItemWidth(kfUiOpacitySliderWidthPixels * UiScale());
	WrapperSlider("UI Opacity", &gUiOpacity);

	RadioRow("Theme", &gUiTheme, static_cast<float>(GetUiTheme()),
		{{"Naval Steel", static_cast<float>(UiTheme::kNavalSteel)}, {"Dark Amber", static_cast<float>(UiTheme::kDarkAmber)}, {"Midnight Mauve", static_cast<float>(UiTheme::kMidnightMauve)}});

	ImGui::Separator();

	// One themed width shared by both buttons (measured under the live menu font)
	float fButtonWidth = MenuButtonsWidth({TranslatedString(kStringDefaults), U"Back"});

	// Defaults button
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringDefaults)), ImVec2(fButtonWidth, 0.0f), mfDefaultsHoverAnim))
	{
		ResetGameSettings();
	}

	ImGui::SameLine();

	// Back button
	if (MenuButton("Back", ImVec2(fButtonWidth, 0.0f), mfBackHoverAnim))
	{
		SaveGameSettings();
		rGame.meUiState = UiState::kPause;
	}

	ImGui::End();
}

} // namespace engine

#endif // BT_CLIENT
