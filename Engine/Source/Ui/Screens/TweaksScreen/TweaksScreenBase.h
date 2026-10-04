#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// Ceiling is 31, not 32: TweakSectionFlags is backed by uint32_t and AllSectionFlags() would need 1u << 32 (UB) at 32.
inline constexpr int64_t kiMaxTweakSections = 31;
inline constexpr int64_t kiInvalidTweakSection = -1;

// Enumerator-free by design. Section identity is assigned at startup by RegisterSection, so the engine names
// no section here and nothing has to be kept in lockstep. common::Flags only requires an unsigned underlying type.
enum class TweakSectionFlags : uint32_t
{
};

inline TweakSectionFlags SectionFlag(int64_t iSection)
{
	return static_cast<TweakSectionFlags>(1ui32 << iSection);
}

class TweaksScreenBase;

struct TweakSectionDescription
{
	// Toggle-bar label and ImGui window title/ID. Register with string literals only: consumed as a C string.
	std::string_view displayName;
	// Persisted identity, folded into the layout CRC. Kept distinct from displayName so relabeling a section
	// does not discard saved layout.
	std::string_view stableKey;
	void (*pRender)(TweaksScreenBase& rScreen) = nullptr;
};

struct TweakExtensionHook
{
	// The engine section this entry extends.
	const int64_t* piSection = nullptr;
	// Sub-tab label; nullptr draws the content inline at the section's inline position instead of as a sub-tab.
	const char* pcLabel = nullptr;
	void (*pRender)(TweaksScreenBase& rScreen) = nullptr;
};

// Persisted section layout, engine-owned and embedded by value in the game settings struct so exactly one
// array bound and one sizeof exist in the program. Padding is explicit (uiPadding) because the whole object is
// written verbatim and repeated saves of unchanged settings must be byte-identical.
struct TweakSectionState
{
	common::Flags<TweakSectionFlags> visible {};
	common::Flags<TweakSectionFlags> collapsed {};
	float fWindowPositionX[kiMaxTweakSections] {};
	float fWindowPositionY[kiMaxTweakSections] {};
	int8_t iActiveSubtab[kiMaxTweakSections] {};
	uint8_t uiPadding[1] {};
	// CRC of stable keys in registration order; on mismatch LoadState ignores saved layout and keeps defaults.
	common::crc_t crcLayout = 0;
};
static_assert(std::is_trivially_copyable_v<TweakSectionState>);
static_assert(sizeof(TweakSectionState) == 296, "TweaksSettings::kiVersion must be bumped with this layout");

class TweaksScreenBase
{
public:

	// Registry storage. Startup-only writes, immutable during rendering, and fixed-size so registration never allocates.
	inline static TweakSectionDescription msSectionDescriptions[kiMaxTweakSections] {};
	inline static int64_t msiSectionCount = 0;

	TweaksScreenBase();
	virtual ~TweaksScreenBase() = default;

	// RegisterEngineTweakSections and game::RegisterGameTweakSections register sections on one thread before the first UI frame; the registry is immutable during rendering.
	static void RegisterSection(int64_t& riSection, const TweakSectionDescription& rDescription);
	static common::Flags<TweakSectionFlags> AllSectionFlags();

	void SaveState(TweakSectionState& rState) const;
	void LoadState(const TweakSectionState& rState);

	void Render();

	void RenderToggleBar();
	void RenderSectionWindow(int64_t iSection);

	void RenderWaveCountRadioButtons(Wrapper& rCountWrapper);
	bool BeginSubtab(const char* pcLabel, int64_t iSection, int8_t iTab);
	void WrapperSlider(std::string_view label, int64_t iSection, float fWidthMultiplier = 2.0f, std::string_view mapKey = {});
	void WrapperSeparatorText(std::string_view label);
	// Chevron selection wraps; names must contain exactly the wrapper's allowed values.
	void ChevronIndexSelector(std::string_view label, Wrapper& rWrapper, std::span<const std::string_view> names);

	void RunSliderAuditFrame();

	std::string_view mActiveSlider;
	int64_t miActiveSliderSection = -1; // -1 for toggle bar, 0+ for sections

	common::Flags<TweakSectionFlags> mSectionVisible {};
	ImVec2 mWindowPositions[kiMaxTweakSections] {};
	int8_t miActiveSubtab[kiMaxTweakSections] {};
	common::Flags<TweakSectionFlags> mApplySubtab {};
	common::Flags<TweakSectionFlags> mSectionCollapsed {};
	float mfToggleBarBottom = 0.0f;

	// Slider-map drift audit runs once per TweaksScreen lifetime and is re-armed by graphics reconstruction. Cycles miActiveSubtab[] across 6 frames: frames 0-4 queue tabs and frame 5 settles the final selection.
	// miAuditFrame: 0..(kiAuditFrameCount-1) = audit running, -1 = audit complete.
	std::unordered_set<std::string_view> mAuditTouched;
	std::unordered_set<std::string_view> mAuditMissed;
	int8_t miPreAuditSubtab[kiMaxTweakSections] {};
	int8_t miAuditFrame = 0;
	bool mbAuditMode = false;
};

// Startup-assigned dense section indices, written by RegisterEngineTweakSections(). kiInvalidTweakSection until then.
inline int64_t giTweakSectionPbr = kiInvalidTweakSection;
inline int64_t giTweakSectionTerrain = kiInvalidTweakSection;
inline int64_t giTweakSectionWater = kiInvalidTweakSection;
inline int64_t giTweakSectionLighting = kiInvalidTweakSection;
inline int64_t giTweakSectionShadow = kiInvalidTweakSection;
inline int64_t giTweakSectionSunMoon = kiInvalidTweakSection;
inline int64_t giTweakSectionMiscellaneous = kiInvalidTweakSection;
inline int64_t giTweakSectionSound = kiInvalidTweakSection;
inline int64_t giTweakSectionSmoke = kiInvalidTweakSection;
inline int64_t giTweakSectionWind = kiInvalidTweakSection;

void RenderPhysicallyBasedRenderingSection(TweaksScreenBase& rScreen);
void RenderTerrainSection(TweaksScreenBase& rScreen);
void RenderWaterSection(TweaksScreenBase& rScreen);
void RenderLightingSection(TweaksScreenBase& rScreen);
void RenderShadowSection(TweaksScreenBase& rScreen);
void RenderSunMoonSection(TweaksScreenBase& rScreen);
void RenderMiscellaneousSection(TweaksScreenBase& rScreen);
void RenderSoundSection(TweaksScreenBase& rScreen);
void RenderSmokeSection(TweaksScreenBase& rScreen);
void RenderWindSection(TweaksScreenBase& rScreen);

void RenderTweakExtensionSubtabs(TweaksScreenBase& rScreen, int64_t iSection, int8_t iFirstSubtab);
void RenderTweakExtensionInline(TweaksScreenBase& rScreen, int64_t iSection);

// Registers the engine's sections. Called from Main.cpp immediately before the Graphics ctor (which builds
// ImGuiManager -> TweaksScreen) and therefore before game::LoadTweaksSettings(). Deliberately not in the
// TweaksScreenBase ctor: device loss recreates Graphics in place and would double-register.
void RegisterEngineTweakSections();

} // namespace engine

#endif // BT_CLIENT
