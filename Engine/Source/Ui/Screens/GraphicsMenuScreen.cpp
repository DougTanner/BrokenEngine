#include "GraphicsMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/GraphicsSettings.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"
#include "Ui/SmokeWrappersBase.h"
#include "Ui/SunMoonWrappersBase.h"

#include "Game.h"

namespace engine
{

constexpr float kfGraphicsMinimumWidthFraction = 0.5f;
constexpr float kfGraphicsMaximumWidthFraction = 0.9f;
constexpr float kfGraphicsFontScaleAtMinimum = 2.0f;
constexpr float kfGraphicsFontScaleAtMaximum = 1.2f;
constexpr float kfGraphicsHeadingScale = 1.15f;
constexpr float kfGraphicsColumnGutterPixels = 80.0f;

// Draw pcLabel separately on the left so WrapperSlider fills the remaining column (-FLT_MIN). Its hidden ## ID is
// recorded by the agent snapshot; AgentUiRegistry::ResolveLabel maps a human-readable query through case-insensitive
// substring matching.
static void ColumnSlider(const char* pcLabel, Wrapper* pWrapper, std::string_view format = "%.2f")
{
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(pcLabel);
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

	char pcSliderIdentifier[64];
	std::snprintf(pcSliderIdentifier, sizeof(pcSliderIdentifier), "##%s", pcLabel);
	ImGui::SetNextItemWidth(-FLT_MIN);
	WrapperSlider(pcSliderIdentifier, pWrapper, format);
}

void GraphicsMenuScreen::Render()
{
	if (game::gpGame->meUiState != UiState::kGraphicsSettings)
	{
		return;
	}

	ImGuiIO& rInputOutput = ImGui::GetIO();
	float fFontScaleRange = gUiFontScale.mfMax - gUiFontScale.mfMin;
	float fFontScalePosition = (gUiFontScale.mfCurrent - gUiFontScale.mfMin) / fFontScaleRange;
	float fPanelWidthFraction = std::lerp(kfGraphicsMinimumWidthFraction, kfGraphicsMaximumWidthFraction, fFontScalePosition);
	// The dense graphics menu compresses its base font scale as Font Size increases to keep the overall adjustment monotonic.
	// Padding and spacing use the theme's base geometry.
	float fMenuFontScale = std::lerp(kfGraphicsFontScaleAtMinimum, kfGraphicsFontScaleAtMaximum, fFontScalePosition);

	ImGui::SetNextWindowPos(ImVec2(rInputOutput.DisplaySize.x * 0.5f, rInputOutput.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(rInputOutput.DisplaySize.x * fPanelWidthFraction, 0.0f));
	// Window auto-resizes to its content (content can exceed a 4K screen); cap the height so the whole panel stays on
	// screen.
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(rInputOutput.DisplaySize.x, rInputOutput.DisplaySize.y * kfGraphicsMaximumHeightFraction));

	// The graphics menu uses gUiOpacity even when Opaque UI is enabled.
	ImVec4 f4WindowBackground = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
	f4WindowBackground.w = gUiOpacity.mfCurrent;
	ImGui::PushStyleColor(ImGuiCol_WindowBg, f4WindowBackground);

	ScopedMenuFont menuFont(fMenuFontScale);
	ImGui::Begin("GraphicsMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	// Border + accent strip only — the themed WindowBg already fills the panel
	ImVec2 vPanelPosition = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPosition, ImVec2(vPanelPosition.x + vPanelSize.x, vPanelPosition.y + vPanelSize.y));

	float fHeaderButtonWidth = MenuButtonsWidth({TranslatedString(StandardString::kStringDefaults), U"Back"});
	bool bBackPressed = false;
	if (ImGui::BeginTable("GraphicsHeader", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("GraphicsTitle", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("GraphicsFps", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("GraphicsDefaults", ImGuiTableColumnFlags_WidthFixed, fHeaderButtonWidth);
		ImGui::TableSetupColumn("GraphicsBack", ImGuiTableColumnFlags_WidthFixed, fHeaderButtonWidth);
		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		float fHeaderHeight = 0.0f;
		{
			ScopedMenuFont headingFont(fMenuFontScale * kfGraphicsHeadingScale);
			fHeaderHeight = ImGui::GetTextLineHeight();
			ImGui::TextUnformatted(AppendUtf8(common::gpThreadLocal->mWorkbuffer, TranslatedString(StandardString::kStringGraphics)).mpData);
		}

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (fHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f));
		ImGui::Text("FPS: %lld", gpGraphics->mRendersInTheLastSecond.Get());

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (fHeaderHeight - ImGui::GetFrameHeight()) * 0.5f));
		if (MenuButton(AppendUtf8(common::gpThreadLocal->mWorkbuffer, TranslatedString(StandardString::kStringDefaults)).mpData, ImVec2(fHeaderButtonWidth, 0.0f), mfDefaultsHoverAnimation))
		{
			ResetGraphicsSettings();
		}

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (fHeaderHeight - ImGui::GetFrameHeight()) * 0.5f));
		bBackPressed = MenuButton("Back", ImVec2(fHeaderButtonWidth, 0.0f), mfBackHoverAnimation);

		ImGui::EndTable();
	}

	ImGui::Separator();

	const ImGuiStyle& rStyle = ImGui::GetStyle();
	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(kfGraphicsColumnGutterPixels * UiScale() * 0.5f, rStyle.CellPadding.y));
	if (ImGui::BeginTable("GraphicsColumns", 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX))
	{
		// Left column: Display
		ImGui::TableNextColumn();

		ColumnSlider("Time of Day", &gSunAngleOverride);
		ColumnSlider("Minimum Ambient", &gSunMoonMinimumAmbient, "%.4f");

		ImGui::Separator();

		WrapperToggle("Fullscreen", &gFullscreen);

		RadioRow("Presentation Mode", &gPresentMode, gPresentMode.mfCurrent, {{"Immediate", static_cast<float>(VK_PRESENT_MODE_IMMEDIATE_KHR)}, {"Mailbox", static_cast<float>(VK_PRESENT_MODE_MAILBOX_KHR)}, {"FIFO", static_cast<float>(VK_PRESENT_MODE_FIFO_KHR)}});

		ImGui::Separator();

		WrapperToggle("Multisampling", &gMultisampling);
		ImGui::BeginDisabled(!gMultisampling.Get<bool>());
		RadioRow(nullptr, &gSampleCount, gSampleCount.mfCurrent, {{"2x", static_cast<float>(VK_SAMPLE_COUNT_2_BIT)}, {"4x", static_cast<float>(VK_SAMPLE_COUNT_4_BIT)}, {"8x", static_cast<float>(VK_SAMPLE_COUNT_8_BIT)}, {"16x", static_cast<float>(VK_SAMPLE_COUNT_16_BIT)}});
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Sample Shading", &gSampleShading);
		ImGui::BeginDisabled(!gSampleShading.Get<bool>());
		ColumnSlider("Min Sample Shading", &gMinimumSampleShading);
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Anisotropy", &gAnisotropy);
		ImGui::BeginDisabled(!gAnisotropy.Get<bool>());
		ColumnSlider("Max Anisotropy", &gMaximumAnisotropy);
		ImGui::EndDisabled();
		ColumnSlider("Mip Lod Bias", &gMipmapLevelOfDetailBias);

		ImGui::TableNextColumn();

		RadioRow("Water", &gWaterLevel, gWaterLevel.mfCurrent, {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}});

		ImGui::Separator();

		if (RadioRow("Terrain Shadows", &gTerrainShadowsLevel, gTerrainShadowsLevel.mfCurrent, {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}}))
		{
			ApplyTerrainShadowsLevel();
		}

		ImGui::Separator();

		WrapperToggle("Lighting", &gLightingEnabled);
		ImGui::BeginDisabled(!gLightingEnabled.Get<bool>());
		if (RadioRow("Lighting", &gLightingLevel, gLightingLevel.mfCurrent, {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}}))
		{
			ApplyLightingLevel();
		}
		ColumnSlider("Lighting Update Cadence", &gLightingUpdateCadence, "%.0f");
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Smoke", &gSmokeEnabled);
		ImGui::BeginDisabled(!gSmokeEnabled.Get<bool>());
		if (RadioRow("Smoke Detail", &gSmokeDetailLevel, gSmokeDetailLevel.mfCurrent, {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}}))
		{
			ApplySmokeDetailLevel();
		}
		ColumnSlider("Smoke Area", &gSmokeSimulationArea);
		ColumnSlider("Smoke Update Cadence", &gSmokeUpdateCadence, "%.0f");
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Wind", &gWindEnabled);

		ImGui::EndTable();
	}
	ImGui::PopStyleVar();

	if (bBackPressed)
	{
		SaveGraphicsSettings();
		game::gpGame->meUiState = UiState::kPause;
	}

	ImGui::End();
	ImGui::PopStyleColor();
}

} // namespace engine

#endif // BT_CLIENT
