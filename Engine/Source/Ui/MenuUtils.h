#pragma once

namespace engine
{

inline constexpr float kfMenuUiScale = 2.0f;

// Heading scale multiplier on top of kfMenuUiScale for menu/panel titles
inline constexpr float kfMenuHeadingScale = 1.6f;
inline constexpr float kfMainMenuHeadingScale = 1.3f;

// Language-row utility tier: absolute font scale plus a multiplier on the already menu-scaled geometry
inline constexpr float kfLanguageMenuFontScale = 1.0f;
inline constexpr float kfLanguageMenuGeometryScale = 0.5f;

// Shared menu-layout constants (source of truth: Documents/UserInterfaceDesign.txt). Values shared by multiple
// screens live here; screen-specific dimensions remain named constants in their screen .cpp. Two kinds per the
// sizing standard: DisplaySize fractions (content-independent anchors/extents) and 4K-authored pixel constants
// multiplied by engine::UiScale() at each use site.
//
// Anchors / extents as DisplaySize fractions:
inline constexpr float kfMainMenuCenterFractionX = 1.0f / 3.0f; // MainMenu group center on first vertical third
inline constexpr float kfMainMenuCenterFractionY = 0.5f;        // MainMenu group anchored at semantic screen center
inline constexpr float kfGraphicsMaximumHeightFraction = 0.9f;     // Graphics settings-panel height cap
inline constexpr float kfModalAnchorFractionY = 0.4f;          // Modal center Y, seated slightly above screen center
// 4K-authored pixels (multiply by engine::UiScale() at use):
inline constexpr float kfHeadingGapPixels = 22.0f;             // Gap below a MenuHeading
inline constexpr float kfMainMenuOpticalOffsetYPixels = 20.0f; // Screenshot-derived upward correction for visible title/action composition
inline constexpr float kfPrimaryButtonMinimumWidthPixels = 760.0f; // Menu primary-button minimum width
inline constexpr float kfModalButtonMinimumWidthPixels = 380.0f;   // Modal button minimum width

class [[nodiscard]] ScopedMenuScale
{
public:
	ScopedMenuScale()
	{
		const ImGuiStyle& rStyle = ImGui::GetStyle();
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(rStyle.FramePadding.x * kfMenuUiScale, rStyle.FramePadding.y * kfMenuUiScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(rStyle.ItemSpacing.x * kfMenuUiScale, rStyle.ItemSpacing.y * kfMenuUiScale));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(rStyle.WindowPadding.x * kfMenuUiScale, rStyle.WindowPadding.y * kfMenuUiScale));
	}

	ScopedMenuScale(const ScopedMenuScale&) = delete;
	ScopedMenuScale& operator=(const ScopedMenuScale&) = delete;

	~ScopedMenuScale()
	{
		ImGui::PopStyleVar(3);
	}
};

// The returned handle owns the Workbuffer frame; its mpData points to null-terminated UTF-8 valid only
// for the handle's lifetime. Pass AppendUtf8(...).mpData inline to the consuming ImGui call.
common::ScopedWorkbufferAllocation<char*> AppendUtf8(common::Workbuffer& rWorkbuffer, std::u32string_view u32String);

bool WrapperToggle(std::string_view label, engine::Wrapper* pWrapper);
bool WrapperSlider(std::string_view label, engine::Wrapper* pWrapper, std::string_view format = "%.2f");
bool WrapperPlusMinus(std::string_view label, engine::Wrapper* pWrapper);

// RadioRow draws an optional header and wraps option buttons to the available width. Present mode, sample count, quality, and
// theme share this enum/index mapping; exact discrete float equality selects the checked option, and a click writes
// its value. Wrapper-address ID scopes keep repeated labels distinct. Header/button labels are the harness API and
// must stay stable; return true on the click frame.
bool RadioRow(const char* pcHeader, engine::Wrapper* pWrapper, float fCurrent, std::initializer_list<std::pair<const char*, float>> aOptions);

#if defined(BT_CLIENT)

struct SlidePanelState
{
	float fOpenness = 0.0f;
	ImVec2 vLastSize {};
};

float ComputeMouseOpennessTarget(ImVec2 vFixedExtent, ImVec2 vAnchor, float fPivotX);
float UpdateSlideAndGetEdgeX(SlidePanelState& rState, ImVec2 vAnchor, float fSidePivotSign, float fTarget);

// fScale multiplies the default Latin/CJK font's base size; gUiFontScale applies once on top.
// Construct before Begin() and destroy after End(), or construct and destroy inside the window.
// Leaving a font pushed inside Begin() on the stack at End() triggers a recovery pop and a second pop in the destructor.
class [[nodiscard]] ScopedMenuFont
{
public:
	explicit ScopedMenuFont(float fScale = kfMenuUiScale);
	ScopedMenuFont(const ScopedMenuFont&) = delete;
	ScopedMenuFont& operator=(const ScopedMenuFont&) = delete;
	~ScopedMenuFont();
};

// Call inside the window with the menu font pushed; TranslatedString(...) supplies UTF-32 label views.
// Labels are UTF-8 encoded through the Workbuffer and measured with CalcTextSize, with FramePadding.x * 4 added.
// Main-menu, pause, and modal callers clamp the width to their minimum-width pixel constant multiplied by UiScale().
float MenuButtonsWidth(std::initializer_list<std::u32string_view> aLabels);

// One menu/panel heading: pushes the heading font (kfMenuUiScale * fHeadingScale), emits pcLabel, then a
// standard kfHeadingGapPixels * UiScale() gap below it. Screens override the scale when their hierarchy requires it.
void MenuHeading(const char* pcLabel, float fHeadingScale = kfMenuHeadingScale);

// Full-screen dim behind pause-style overlays (background draw list, behind all windows)
void DrawFullScreenDim();

// Border + top accent strip only (no fill) — for panels whose own WindowBg must stay intact as the fill.
void DrawPanelAccents(ImDrawList* pDrawList, const ImVec2& rvMinimum, const ImVec2& rvMaximum);

// Custom-drawn menu button: InvisibleButton semantics (ID/click/keyboard/gamepad nav) with animated rounded
// fill, border, and left accent bar. rfHoverAnimation is per-button state owned by the caller (screen member — no
// heap). Pass rvSize x or y as 0.0f to auto-size that axis from the label.
bool MenuButton(const char* pcLabel, const ImVec2& rvSize, float& rfHoverAnimation, bool bSelected = false);

#endif // BT_CLIENT

} // namespace engine
