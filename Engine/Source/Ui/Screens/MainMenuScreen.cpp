#include "MainMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"

namespace engine
{

namespace
{

void CenterMenuItem(float fContentStartX, float fContentWidth, float fItemWidth)
{
	ImGui::SetCursorPosX(fContentStartX + (fContentWidth - fItemWidth) * 0.5f);
}

} // namespace

void MainMenuScreen::Render(GameBase& rGame)
{
	using enum StandardString;
	using enum StandardMenuState;

	enum class AutoConnectState
	{
		kReady,
		kAttempted,
		kSucceeded,
	};
	static AutoConnectState seAutoConnectState = AutoConnectState::kReady;

	StandardMenuModel model = rGame.GetStandardMenuModel();

	if (model.state & kConnectionAccepted)
	{
		seAutoConnectState = AutoConnectState::kSucceeded;
	}

	if (rGame.meUiState != UiState::kPause || !rGame.InMainMenu())
	{
		return;
	}

	if (seAutoConnectState == AutoConnectState::kAttempted && !(model.state & kClientPresent))
	{
		seAutoConnectState = AutoConnectState::kReady;
	}

	ImGuiIO& rIo = ImGui::GetIO();
	ScopedMenuScale menuScale;

	// Invisible windows let the menu compose directly over the live scene
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

	// Center the title/action group vertically and place it on the screen's first vertical third.
	ImVec2 vMenuCenter(rIo.DisplaySize.x * kfMainMenuCenterFractionX, rIo.DisplaySize.y * kfMainMenuCenterFractionY);
	vMenuCenter.y -= kfMainMenuOpticalOffsetYPixels * UiScale() * ImGui::GetStyle().FontScaleMain;
	ImGui::SetNextWindowPos(vMenuCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

	ScopedMenuFont menuFont;
	ImGui::Begin("MainMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	// Shared text-driven width over every button label (including the SCANNING... discovery state), floored to the
	// primary-button minimum. Measured over the full label set so a disabled feature cannot reflow the column.
	// Height auto-sizes per button (0.0f).
	float fButtonWidth = std::max(MenuButtonsWidth({TranslatedString(kStringLocalServer), TranslatedString(kStringRemoteServer), TranslatedString(kStringGraphics), TranslatedString(kStringAudio), TranslatedString(kStringGameSettings), TranslatedString(kStringQuit), U"SCANNING..."}), kfPrimaryButtonMinWidthPixels * UiScale());
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
		// Auto-start discovery when main menu is shown
		if (!(model.state & kClientPresent) && !(model.state & kDiscoveryScannerPresent) && !(model.state & kServerDiscovered))
		{
			rGame.ApplyStandardMenuAction(StandardMenuAction::kStartDiscovery);
			// Discovery changes the live state the auto-connect decision below reads this same pass.
			model = rGame.GetStandardMenuModel();
		}

		// Auto-connect
		if ((model.state & kAutoConnect) && seAutoConnectState == AutoConnectState::kReady && !(model.state & kClientPresent) && (model.state & kServerDiscovered))
		{
			seAutoConnectState = AutoConnectState::kAttempted;
			rGame.ApplyStandardMenuAction(StandardMenuAction::kConnectToDiscoveredServer);
			// Connecting changes the live state the Local Server entry below reads this same pass.
			model = rGame.GetStandardMenuModel();
		}

		// Local Server button (discovers localhost + LAN)
		if (model.state & kServerDiscovered)
		{
			CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
			if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringLocalServer)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[0]))
			{
				rGame.ApplyStandardMenuAction(StandardMenuAction::kConnectToDiscoveredServer);
			}
		}
		else
		{
			ImGui::BeginDisabled();
			CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
			MenuButton("SCANNING...", ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[0]);
			ImGui::EndDisabled();
		}
	}

	// Remote Server button (placeholder for future Internet servers)
	if (model.features & StandardMenuFeature::kRemoteServer)
	{
		ImGui::BeginDisabled();
		CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
		MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringRemoteServer)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[1]);
		ImGui::EndDisabled();
	}

	// Graphics button
	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringGraphics)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[2]))
	{
		rGame.meUiState = UiState::kGraphicsSettings;
		gSunAngleOverride.Set(gpCamera->RawSunAngle());
	}

	// Audio button
	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringAudio)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[3]))
	{
		rGame.meUiState = UiState::kSound;
	}

	// Game Settings button
	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringGameSettings)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[4]))
	{
		rGame.meUiState = UiState::kGameSettings;
	}

	// Quit button
	CenterMenuItem(fContentStartX, fContentWidth, fButtonWidth);
	if (MenuButton(AppendUtf8(rWorkbuffer, TranslatedString(kStringQuit)), ImVec2(fButtonWidth, 0.0f), mfButtonHoverAnims[5]))
	{
		rGame.mGameFlags.Set(GameFlags::kQuit);
	}

	ImGui::End();

	ImGui::PopStyleColor(2);
}

} // namespace engine

#endif // BT_CLIENT
