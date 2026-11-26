#include "KeybindsScreen.h"

#if defined(BT_CLIENT)

#include "Input/Keybinds.h"
#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"

#include "Input/GameKeybinds.h"
#include "Game.h"

namespace engine
{

constexpr float kfKeybindsMaximumHeightFraction = 0.9f;
constexpr float kfKeybindColumnGutterPixels = 80.0f;
constexpr float kfKeybindButtonMinimumWidthPixels = 480.0f;

static_assert(static_cast<int64_t>(KeyAction::kCount) + static_cast<int64_t>(game::GameKeyAction::kCount) <= KeybindsScreen::kiMaxRowCount, "KeybindsScreen::kiMaxRowCount too small");

static void RenderKeybindRow(const KeybindRowInfo& rRow, float fBindButtonWidth, float& rfHoverAnimation, common::Workbuffer& rWorkbuffer)
{
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(AppendUtf8(rWorkbuffer, rRow.label).mpData);

	ImGui::TableNextColumn();

	bool bCapturing = gpInput->IsCapturingKeybind(rRow.slot);
	char pcButtonLabel[128] {};
	if (bCapturing)
	{
		common::ScopedWorkbufferAllocation<char*> pressKeyUtf8 = AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringPressKey));
		std::snprintf(pcButtonLabel, sizeof(pcButtonLabel), "%s##%s", pressKeyUtf8.mpData, rRow.pcId);
	}
	else
	{
		char pcKeyName[64] {};
		std::string_view keyName = FormatVirtualKeyName(rRow.key, pcKeyName);
		std::snprintf(pcButtonLabel, sizeof(pcButtonLabel), "%.*s##%s", static_cast<int>(keyName.size()), keyName.data(), rRow.pcId);
	}

	if (MenuButton(pcButtonLabel, ImVec2(fBindButtonWidth, 0.0f), rfHoverAnimation, bCapturing))
	{
		gpInput->BeginKeybindCapture(rRow.slot);
	}
}

void KeybindsScreen::Render()
{
	if (game::gpGame->meUiState != UiState::kKeybinds)
	{
		return;
	}

	ImGuiIO& rInputOutput = ImGui::GetIO();
	ScopedMenuScale menuScale;

	ImGui::SetNextWindowPos(ImVec2(rInputOutput.DisplaySize.x * 0.5f, rInputOutput.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	// Cap root-window height without BeginChild so Input's root-window scroll gate stays intact.
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(rInputOutput.DisplaySize.x, rInputOutput.DisplaySize.y * kfKeybindsMaximumHeightFraction));

	ScopedMenuFont menuFont;
	ImGui::Begin("KeybindsMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	gpImGuiManager->RegisterOpaqueRectangle(ImGui::GetWindowPos(), ImGui::GetWindowSize());

	ImVec2 panelPosition = ImGui::GetWindowPos();
	ImVec2 panelSize = ImGui::GetWindowSize();
	DrawPanelAccents(ImGui::GetWindowDrawList(), panelPosition, ImVec2(panelPosition.x + panelSize.x, panelPosition.y + panelSize.y));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	MenuHeading(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringKeybinds)).mpData);

	std::span<const KeyAction> engineActions = VisibleEngineKeyActions();
	std::span<const game::GameKeyAction> gameActions = game::VisibleGameKeyActions();

	float fActionColumnWidth = 0.0f;
	for (KeyAction eAction : engineActions)
	{
		fActionColumnWidth = std::max(fActionColumnWidth, ImGui::CalcTextSize(AppendUtf8(rWorkbuffer, DescribeKeybind(eAction).label).mpData).x);
	}
	for (game::GameKeyAction eAction : gameActions)
	{
		fActionColumnWidth = std::max(fActionColumnWidth, ImGui::CalcTextSize(AppendUtf8(rWorkbuffer, game::DescribeKeybind(eAction).label).mpData).x);
	}
	fActionColumnWidth += kfKeybindColumnGutterPixels * UiScale();

	// Fixed bind-button width independent of the current key text so clicking or rebinding never reflows the column.
	float fBindButtonWidth = std::max(MenuButtonsWidth({TranslatedString(StandardString::kStringPressKey), TranslatedString(StandardString::kStringUnbound)}), kfKeybindButtonMinimumWidthPixels * UiScale());

	if (ImGui::BeginTable("KeybindsTable", 2, ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("KeybindAction", ImGuiTableColumnFlags_WidthFixed, fActionColumnWidth);
		ImGui::TableSetupColumn("KeybindKey", ImGuiTableColumnFlags_WidthFixed, fBindButtonWidth);

		int64_t iRowIndex = 0;
		for (KeyAction eAction : engineActions)
		{
			RenderKeybindRow(DescribeKeybind(eAction), fBindButtonWidth, mpfBindButtonHoverAnimations[iRowIndex++], rWorkbuffer);
		}
		for (game::GameKeyAction eAction : gameActions)
		{
			RenderKeybindRow(game::DescribeKeybind(eAction), fBindButtonWidth, mpfBindButtonHoverAnimations[iRowIndex++], rWorkbuffer);
		}

		ImGui::EndTable();
	}

	ImGui::Separator();

	float fFooterButtonWidth = MenuButtonsWidth({TranslatedString(StandardString::kStringDefaults), U"Back"});

	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringDefaults)).mpData, ImVec2(fFooterButtonWidth, 0.0f), mfDefaultsHoverAnimation))
	{
		gpInput->ResetAllKeybinds();
	}

	ImGui::SameLine();

	if (MenuButton("Back", ImVec2(fFooterButtonWidth, 0.0f), mfBackHoverAnimation))
	{
		game::gpGame->LeaveSettingsScreen();
	}

	ImGui::End();
}

} // namespace engine

#endif // BT_CLIENT
