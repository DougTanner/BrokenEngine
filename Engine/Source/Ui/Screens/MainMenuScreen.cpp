#include "MainMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"

#include "Game.h"

namespace engine
{

static void CenterMenuItem(float fContentStartX, float fContentWidth, float fItemWidth)
{
	ImGui::SetCursorPosX(fContentStartX + (fContentWidth - fItemWidth) * 0.5f);
}

void MainMenuScreen::Render()
{
	enum class AutoConnectState
	{
		kReady,
		kAttempted,
		kSucceeded,
	};
	static AutoConnectState seAutoConnectState = AutoConnectState::kReady;

	StandardMenuModel model = game::gpGame->GetStandardMenuModel();

	if (model.state & StandardMenuState::kConnectionAccepted)
	{
		seAutoConnectState = AutoConnectState::kSucceeded;
	}

	// The model reports only client presence, so the connection-lost modal is what tells a real loss from a
	// deliberate return to the main menu; only a real loss rearms. Runs before the gate below, which returns
	// while the modal is up.
	if (seAutoConnectState == AutoConnectState::kSucceeded && !(model.state & StandardMenuState::kClientPresent) && game::gpGame->meUiState == UiState::kModal)
	{
		seAutoConnectState = AutoConnectState::kReady;
	}

	if (game::gpGame->meUiState != UiState::kPause || !(game::gpGame->mGameFlags & engine::GameFlags::kMainMenu))
	{
		return;
	}

	if (seAutoConnectState == AutoConnectState::kAttempted && !(model.state & StandardMenuState::kClientPresent))
	{
		seAutoConnectState = AutoConnectState::kReady;
	}

	ImGuiIO& rInputOutput = ImGui::GetIO();
	ScopedMenuScale menuScale;

	// Invisible windows let the menu compose directly over the live scene
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	// Center the title/action group vertically and place it on the screen's first vertical third.
	ImVec2 vMenuCenter(rInputOutput.DisplaySize.x * kfMainMenuCenterFractionX, rInputOutput.DisplaySize.y * kfMainMenuCenterFractionY);
	vMenuCenter.y -= kfMainMenuOpticalOffsetYPixels * UiScale() * ImGui::GetStyle().FontScaleMain;
	ImGui::SetNextWindowPos(vMenuCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ScopedMenuFont menuFont;
	ImGui::Begin("MainMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	// Shared text-driven width over every button label (including the SCANNING... discovery state), floored to the
	// primary-button minimum. Measured over the full label set so a disabled feature cannot reflow the column.
	// Height auto-sizes per button (0.0f).
	float fButtonWidth = std::max(MenuButtonsWidth({TranslatedString(StandardString::kStringLocalServer), TranslatedString(StandardString::kStringRemoteServer), TranslatedString(StandardString::kStringGraphics), TranslatedString(StandardString::kStringAudio), TranslatedString(StandardString::kStringGameSettings), TranslatedString(StandardString::kStringQuit), U"SCANNING..."}), kfPrimaryButtonMinimumWidthPixels * UiScale());
	float fHeadingWidth = 0.0f;
	{
		ScopedMenuFont headingFont(kfMenuUiScale * kfMainMenuHeadingScale);
		fHeadingWidth = ImGui::CalcTextSize(model.pcTitle).x;
	}

	// Title and actions share one measured content width and center line, while the block remains scene-anchored.
	float fContentStartX = ImGui::GetCursorPosX();
	float fContentWidth = std::max(fHeadingWidth, fButtonWidth);
	CenterMenuItem(fContentStartX, fContentWidth, fHeadingWidth);
	MenuHeading(model.pcTitle, kfMainMenuHeadingScale);

	if (model.features & StandardMenuFeature::kLocalServer)
	{
		if (!(model.state & StandardMenuState::kClientPresent) && !(model.state & StandardMenuState::kDiscoveryScannerPresent) && !(model.state & StandardMenuState::kServerDiscovered))
		{
			game::gpGame->ApplyStandardMenuAction(StandardMenuAction::kStartDiscovery);
			// Discovery changes the live state the auto-connect decision below reads this same pass.
			model = game::gpGame->GetStandardMenuModel();
		}

		if ((model.state & StandardMenuState::kAutoConnect) && seAutoConnectState == AutoConnectState::kReady && !(model.state & StandardMenuState::kClientPresent) && (model.state & StandardMenuState::kServerDiscovered))
		{
			seAutoConnectState = AutoConnectState::kAttempted;
			game::gpGame->ApplyStandardMenuAction(StandardMenuAction::kConnectToDiscoveredServer);
			// Connecting changes the live state the Local Server entry below reads this same pass.
			model = game::gpGame->GetStandardMenuModel();
		}

		// The Local Server entry connects to the discovered localhost or LAN server.
		if (model.state & StandardMenuState::kServerDiscovered)
		{
			CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
			if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringLocalServer)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[0]))
			{
				game::gpGame->ApplyStandardMenuAction(StandardMenuAction::kConnectToDiscoveredServer);
			}
		}
		else
		{
			ImGui::BeginDisabled();
			CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
			MenuButton("SCANNING...", ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[0]);
			ImGui::EndDisabled();
		}
	}

	// The Remote Server button is displayed disabled because no connection action is available.
	if (model.features & StandardMenuFeature::kRemoteServer)
	{
		ImGui::BeginDisabled();
		CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
		MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringRemoteServer)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[1]);
		ImGui::EndDisabled();
	}

	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringGraphics)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[2]))
	{
		game::gpGame->meUiState = UiState::kGraphicsSettings;
		gSunAngleOverride.Set(gpCamera->mfSunAngle);
	}

	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringAudio)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[3]))
	{
		game::gpGame->meUiState = UiState::kSound;
	}

	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringGameSettings)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[4]))
	{
		game::gpGame->meUiState = UiState::kGameSettings;
	}

	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(StandardString::kStringQuit)).mpData, ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnimations[5]))
	{
		game::gpGame->mGameFlags.Set(GameFlags::kQuit);
	}

	ImGui::End();

	ImGui::PopStyleColor(2);
}

} // namespace engine

#endif // BT_CLIENT
