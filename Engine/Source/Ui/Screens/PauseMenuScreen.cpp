#include "PauseMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"

#include "Game.h"

namespace engine
{

void PauseMenuScreen::Render()
{
	if (game::gpGame->meUiState != UiState::kPause || (game::gpGame->mGameFlags & engine::GameFlags::kMainMenu))
	{
		return;
	}

	ImGuiIO& rInputOutput = ImGui::GetIO();
	ScopedMenuScale menuScale;

	DrawFullScreenDim();

	// Invisible window lets the pause actions float directly over the dimmed scene
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	ImGui::SetNextWindowPos(ImVec2(rInputOutput.DisplaySize.x * 0.5f, rInputOutput.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ScopedMenuFont menuFont;
	ImGui::Begin("PauseMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	// Share the Main Menu's restrained title scale and primary action tier.
	float fButtonWidth = std::max(MenuButtonsWidth({TranslatedString(StandardString::kStringResume), TranslatedString(StandardString::kStringGraphics), TranslatedString(StandardString::kStringAudio), TranslatedString(StandardString::kStringGameSettings), TranslatedString(StandardString::kStringMainMenu), TranslatedString(StandardString::kStringQuit)}), kfPrimaryButtonMinimumWidthPixels * UiScale());
	float fHeadingWidth = 0.0f;
	{
		ScopedMenuFont headingFont(kfMenuUiScale * kfMainMenuHeadingScale);
		fHeadingWidth = ImGui::CalcTextSize(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringPaused)).mpData).x;
	}

	float fContentStartX = ImGui::GetCursorPosX();
	ImGui::SetCursorPosX(fContentStartX + (fButtonWidth - fHeadingWidth) * 0.5f);
	MenuHeading(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringPaused)).mpData, kfMainMenuHeadingScale);
	ImGui::SetCursorPosX(fContentStartX);

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringResume)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[0]))
	{
		game::gpGame->meUiState = UiState::kNone;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringGraphics)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[1]))
	{
		game::gpGame->meUiState = UiState::kGraphicsSettings;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringAudio)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[2]))
	{
		game::gpGame->meUiState = UiState::kSound;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringGameSettings)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[3]))
	{
		game::gpGame->meUiState = UiState::kGameSettings;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringMainMenu)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[4]))
	{
		game::gpGame->ApplyStandardMenuAction(StandardMenuAction::kChangeFrameToMainMenu);
		game::gpGame->meUiState = UiState::kPause;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringQuit)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[5]))
	{
		game::gpGame->mGameFlags.Set(GameFlags::kQuit);
	}

	ImGui::End();

	ImGui::PopStyleColor(2);
}

} // namespace engine

#endif // BT_CLIENT
