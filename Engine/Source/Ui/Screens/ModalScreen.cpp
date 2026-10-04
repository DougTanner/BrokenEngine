#include "ModalScreen.h"

#if defined(BT_CLIENT)

#include "Ui/MenuUtils.h"

#include "Game.h"

namespace engine
{

constexpr float kfModalWindowWidthFraction = 0.24f;
constexpr float kfModalMessageFontScale = 1.15f;
constexpr float kfModalMessageActionGapPixels = 36.0f;

void ModalScreen::Render()
{
	if (game::gpGame->meUiState != UiState::kModal)
	{
		return;
	}

	ImGuiIO& rInputOutput = ImGui::GetIO();
	ScopedMenuScale menuScale;

	float fWindowWidth = rInputOutput.DisplaySize.x * kfModalWindowWidthFraction;
	ImGui::SetNextWindowPos(ImVec2(rInputOutput.DisplaySize.x * 0.5f, rInputOutput.DisplaySize.y * kfModalAnchorFractionY), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(fWindowWidth, 0.0f));

	ScopedMenuFont menuFont;
	ImGui::Begin("ModalDialog", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	// Border + accent strip only — the opaque themed WindowBg must stay intact for RegisterOpaqueRectangle occlusion
	ImVec2 vPanelPosition = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPosition, ImVec2(vPanelPosition.x + vPanelSize.x, vPanelPosition.y + vPanelSize.y));

	{
		ScopedMenuFont messageFont(kfMenuUiScale * kfModalMessageFontScale);
		ImGui::TextWrapped("%s", game::gpGame->mModalMessage);
	}

	ImGui::Dummy(ImVec2(0.0f, kfModalMessageActionGapPixels * UiScale()));

	float fButtonWidth = std::max(MenuButtonsWidth({U"OK"}), kfModalButtonMinimumWidthPixels * UiScale());
	ImGui::SetCursorPosX((fWindowWidth - fButtonWidth) / 2.0f);
	if (MenuButton("OK", ImVec2(fButtonWidth, 0.0f), mfOkHoverAnimation))
	{
		game::gpGame->meUiState = UiState::kPause;
		game::gpGame->mModalMessage[0] = '\0';
	}

	ImGui::End();
}

} // namespace engine

#endif // BT_CLIENT
