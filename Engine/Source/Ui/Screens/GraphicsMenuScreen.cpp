#include "GraphicsMenuScreen.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/GraphicsSettings.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/LocalizationBase.h"
#include "Ui/MenuUtils.h"
#include "Ui/SunMoonWrappersBase.h"

namespace engine
{

namespace
{

constexpr float kfGraphicsMinWidthFraction = 0.5f;
constexpr float kfGraphicsMaxWidthFraction = 0.9f;
constexpr float kfGraphicsFontScaleAtMinimum = 2.0f;
constexpr float kfGraphicsFontScaleAtMaximum = 1.2f;
constexpr float kfGraphicsHeadingScale = 1.15f;
constexpr float kfGraphicsColumnGutterPixels = 80.0f;

// WrapperSlider with its label drawn to the left of the bar instead of trailing it, the bar filling the rest of the
// table column (-FLT_MIN item width). ImGui only ever draws a widget's own label after the frame, so the visible text
// is emitted separately and the slider carries a hidden-label id ("##" prefix, display portion empty). That id is what
// the agent UI snapshot records, so describe_ui reports "##Minimum Ambient" and a harness query for the human-readable
// name resolves through AgentUiRegistry::ResolveLabel's case-insensitive substring tier.
void ColumnSlider(const char* pcLabel, Wrapper* pWrapper, std::string_view format = "%.2f")
{
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(pcLabel);
	ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);

	char pcSliderId[64];
	std::snprintf(pcSliderId, sizeof(pcSliderId), "##%s", pcLabel);
	ImGui::SetNextItemWidth(-FLT_MIN);
	WrapperSlider(pcSliderId, pWrapper, format);
}

} // namespace

void GraphicsMenuScreen::Render(GameBase& rGame)
{
	using enum StandardString;

	if (rGame.meUiState != UiState::kGraphicsSettings)
	{
		return;
	}

	ImGuiIO& rIo = ImGui::GetIO();
	float fFontScaleRange = gUiFontScale.GetMax() - gUiFontScale.GetMin();
	float fFontScalePosition = (gUiFontScale.Get() - gUiFontScale.GetMin()) / fFontScaleRange;
	float fPanelWidthFraction = std::lerp(kfGraphicsMinWidthFraction, kfGraphicsMaxWidthFraction, fFontScalePosition);
	// Graphics is the densest player-facing menu. Preserve the user's monotonic Font Size adjustment while
	// compressing its local base scale toward the high end, and retain the theme's base geometry instead of
	// doubling padding and spacing a second time.
	float fMenuFontScale = std::lerp(kfGraphicsFontScaleAtMinimum, kfGraphicsFontScaleAtMaximum, fFontScalePosition);

	ImGui::SetNextWindowPos(ImVec2(rIo.DisplaySize.x * 0.5f, rIo.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(rIo.DisplaySize.x * fPanelWidthFraction, 0.0f));
	// Window auto-resizes to its content (content can exceed a 4K screen); cap the height so the whole panel stays on
	// screen.
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(rIo.DisplaySize.x, rIo.DisplaySize.y * kfGraphicsMaxHeightFraction));

	// Always transparent regardless of Opaque UI, so the FPS readout below reflects worst-case cost
	ImVec4 f4WindowBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
	f4WindowBg.w = gUiOpacity.Get();
	ImGui::PushStyleColor(ImGuiCol_WindowBg, f4WindowBg);

	ScopedMenuFont menuFont(fMenuFontScale);
	ImGui::Begin("GraphicsMenu", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

	// Border + accent strip only — the themed WindowBg already fills the panel
	ImVec2 vPanelPos = ImGui::GetWindowPos();
	ImVec2 vPanelSize = ImGui::GetWindowSize();
	DrawPanelAccents(ImGui::GetWindowDrawList(), vPanelPos, ImVec2(vPanelPos.x + vPanelSize.x, vPanelPos.y + vPanelSize.y));

	float fHeaderButtonWidth = MenuButtonsWidth({TranslatedString(kStringDefaults), U"Back"});
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
			ImGui::TextUnformatted(AppendUtf8(common::gpThreadLocal->mWorkbuffer, TranslatedString(kStringGraphics)));
		}

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (fHeaderHeight - ImGui::GetTextLineHeight()) * 0.5f));
		ImGui::Text("FPS: %lld", gpGraphics->mRendersInTheLastSecond.Get());

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (fHeaderHeight - ImGui::GetFrameHeight()) * 0.5f));
		if (MenuButton(AppendUtf8(common::gpThreadLocal->mWorkbuffer, TranslatedString(kStringDefaults)), ImVec2(fHeaderButtonWidth, 0.0f), mfDefaultsHoverAnim))
		{
			ResetGraphicsSettings();
		}

		ImGui::TableNextColumn();
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (fHeaderHeight - ImGui::GetFrameHeight()) * 0.5f));
		bBackPressed = MenuButton("Back", ImVec2(fHeaderButtonWidth, 0.0f), mfBackHoverAnim);

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

		RadioRow("Presentation Mode", &gPresentMode, gPresentMode.Get(),
			{{"Immediate", static_cast<float>(VK_PRESENT_MODE_IMMEDIATE_KHR)}, {"Mailbox", static_cast<float>(VK_PRESENT_MODE_MAILBOX_KHR)}, {"FIFO", static_cast<float>(VK_PRESENT_MODE_FIFO_KHR)}});

		ImGui::Separator();

		WrapperToggle("Multisampling", &gMultisampling);
		ImGui::BeginDisabled(!gMultisampling.Get<bool>());
		RadioRow(nullptr, &gSampleCount, gSampleCount.Get(),
			{{"2x", static_cast<float>(VK_SAMPLE_COUNT_2_BIT)}, {"4x", static_cast<float>(VK_SAMPLE_COUNT_4_BIT)}, {"8x", static_cast<float>(VK_SAMPLE_COUNT_8_BIT)}, {"16x", static_cast<float>(VK_SAMPLE_COUNT_16_BIT)}});
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Sample Shading", &gSampleShading);
		ImGui::BeginDisabled(!gSampleShading.Get<bool>());
		ColumnSlider("Min Sample Shading", &gMinSampleShading);
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Anisotropy", &gAnisotropy);
		ImGui::BeginDisabled(!gAnisotropy.Get<bool>());
		ColumnSlider("Max Anisotropy", &gMaxAnisotropy);
		ImGui::EndDisabled();
		ColumnSlider("Mip Lod Bias", &gMipLodBias);

		// Right column: Effects & UI
		ImGui::TableNextColumn();

		RadioRow("Water", &gWaterLevel, gWaterLevel.Get(), {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}});

		ImGui::Separator();

		if (RadioRow("Terrain Shadows", &gTerrainShadowsLevel, gTerrainShadowsLevel.Get(), {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}}))
		{
			ApplyTerrainShadowsLevel();
		}

		ImGui::Separator();

		WrapperToggle("Lighting", &gLightingEnabled);
		ImGui::BeginDisabled(!gLightingEnabled.Get<bool>());
		if (RadioRow("Lighting", &gLightingLevel, gLightingLevel.Get(), {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}}))
		{
			ApplyLightingLevel();
		}
		ColumnSlider("Lighting Update Cadence", &gLightingUpdateCadence, "%.0f");
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Smoke", &gSmokeEnabled);
		ImGui::BeginDisabled(!gSmokeEnabled.Get<bool>());
		if (RadioRow("Smoke Detail", &gSmokeDetailLevel, gSmokeDetailLevel.Get(), {{"Low", 0.0f}, {"Medium", 1.0f}, {"High", 2.0f}}))
		{
			ApplySmokeDetailLevel();
		}
		ColumnSlider("Smoke Area", &gSmokeSimulationArea);
		ImGui::EndDisabled();

		ImGui::Separator();

		WrapperToggle("Wind", &gWindEnabled);

		ImGui::EndTable();
	}
	ImGui::PopStyleVar();

	if (bBackPressed)
	{
		SaveGraphicsSettings();
		rGame.meUiState = UiState::kPause;
	}

	ImGui::End();
	ImGui::PopStyleColor();
}

} // namespace engine

#endif // BT_CLIENT
