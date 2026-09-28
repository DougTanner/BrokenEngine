#include "PauseMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"

#include "Game.h"

namespace engine
{

void PauseMenuScreen::Render()
{
	if (game::gpGame->meUiState != UiState::kPause || game::gpGame->InMainMenu())
	{
		return;
	}

	ImGuiIO& rIo = ImGui::GetIO();
	ScopedMenuScale menuScale;

	// Dim the game scene behind the pause overlay
	DrawFullScreenDim();

	// Invisible window lets the pause actions float directly over the dimmed scene
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	// Center window on screen
	ImGui::SetNextWindowPos(ImVec2(rIo.DisplaySize.x * 0.5f, rIo.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ScopedMenuFont menuFont;
	ImGui::Begin("PauseMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	// Share the Main Menu's restrained title scale and primary action tier.
	float fButtonWidth = std::max(MenuButtonsWidth({TranslatedString(StandardString::kStringResume), TranslatedString(StandardString::kStringGraphics), TranslatedString(StandardString::kStringAudio), TranslatedString(StandardString::kStringGameSettings), TranslatedString(StandardString::kStringMainMenu), TranslatedString(StandardString::kStringQuit)}), kfPrimaryButtonMinWidthPixels * UiScale());
	float fHeadingWidth = 0.0f;
	{
		ScopedMenuFont headingFont(kfMenuUiScale * kfMainMenuHeadingScale);
		fHeadingWidth = ImGui::CalcTextSize(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringPaused))).x;
	}

	float fContentStartX = ImGui::GetCursorPosX();
	ImGui::SetCursorPosX(fContentStartX + (fButtonWidth - fHeadingWidth) * 0.5f);
	MenuHeading(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringPaused)), kfMainMenuHeadingScale);
	ImGui::SetCursorPosX(fContentStartX);

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringResume)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[0]))
	{
		game::gpGame->meUiState = UiState::kNone;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringGraphics)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[1]))
	{
		game::gpGame->meUiState = UiState::kGraphicsSettings;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringAudio)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[2]))
	{
		game::gpGame->meUiState = UiState::kSound;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringGameSettings)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[3]))
	{
		game::gpGame->meUiState = UiState::kGameSettings;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringMainMenu)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[4]))
	{
		game::gpGame->ApplyStandardMenuAction(StandardMenuAction::kChangeFrameToMainMenu);
		game::gpGame->meUiState = UiState::kPause;
	}

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringQuit)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[5]))
	{
		game::gpGame->mGameFlags.Set(GameFlags::kQuit);
	}

	ImGui::End();

	ImGui::PopStyleColor(2);
}

} // namespace engine

#endif // BT_CLIENT
