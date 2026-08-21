#if defined(BT_CLIENT)

#include "GraphicsSettings.h"

#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/SunMoonWrappersBase.h"

namespace engine
{

enum class GraphicsSettingsFlags : uint8_t
{
	kFullscreen    = 1 << 0,
	kMultisampling = 1 << 1,
	kAnisotropy    = 1 << 2,
	kSampleShading = 1 << 3,
	kSmoke         = 1 << 4,
	kWind          = 1 << 5,
	kLighting      = 1 << 6,
};

struct GraphicsSettings
{
	static constexpr int64_t kiVersion = 15;

	common::Flags<GraphicsSettingsFlags> flags {};
	uint8_t uiPad[3] {};
	VkPresentModeKHR ePresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkSampleCountFlagBits eSampleCount = VK_SAMPLE_COUNT_4_BIT;
	float fMaxAnisotropy = 0.0f;
	float fMinSampleShading = 0.0f;
	float fMipLodBias = 0.0f;
	float fSmokeSimulationArea = 0.0f;
	float fMinimumAmbient = 0.0f;
	float fLightingUpdateCadence = 1.0f;
	// The quality levels are the single source of truth for the wrappers they drive; those wrapper values are
	// never persisted directly. uint8_t, not the enum: the file layout must not follow GraphicsQualityLevel.
	uint8_t uiWaterLevel = 0;
	uint8_t uiTerrainShadowsLevel = 0;
	uint8_t uiObjectShadowsLevel = 0;
	uint8_t uiLightingLevel = 0;
	uint8_t uiSmokeDetailLevel = 0;
	uint8_t uiQualityPad[3] {};
};
static_assert(std::is_trivially_copyable_v<GraphicsSettings>, "GraphicsSettings must stay trivially copyable — WriteVersionedFile stamps sizeof");
static_assert(std::is_standard_layout_v<GraphicsSettings>, "GraphicsSettings must stay standard-layout — BT_OFFSETOF below is only well-defined for standard-layout types");
static_assert(BT_OFFSETOF(GraphicsSettings, flags) == 0, "GraphicsSettings::flags offset changed — the flag byte leads the file");
static_assert(BT_OFFSETOF(GraphicsSettings, uiPad) == 1, "GraphicsSettings padding changed — common::Flags must stay one byte");
static_assert(BT_OFFSETOF(GraphicsSettings, ePresentMode) == 4, "GraphicsSettings::ePresentMode offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, eSampleCount) == 8, "GraphicsSettings::eSampleCount offset changed — VkPresentModeKHR must stay four bytes");
static_assert(BT_OFFSETOF(GraphicsSettings, fMaxAnisotropy) == 12, "GraphicsSettings::fMaxAnisotropy offset changed — VkSampleCountFlagBits must stay four bytes");
static_assert(BT_OFFSETOF(GraphicsSettings, fMinSampleShading) == 16, "GraphicsSettings::fMinSampleShading offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fMipLodBias) == 20, "GraphicsSettings::fMipLodBias offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fSmokeSimulationArea) == 24, "GraphicsSettings::fSmokeSimulationArea offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fMinimumAmbient) == 28, "GraphicsSettings::fMinimumAmbient offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fLightingUpdateCadence) == 32, "GraphicsSettings::fLightingUpdateCadence offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiWaterLevel) == 36, "GraphicsSettings::uiWaterLevel offset changed — the quality bytes are appended after the floats");
static_assert(BT_OFFSETOF(GraphicsSettings, uiTerrainShadowsLevel) == 37, "GraphicsSettings::uiTerrainShadowsLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiObjectShadowsLevel) == 38, "GraphicsSettings::uiObjectShadowsLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiLightingLevel) == 39, "GraphicsSettings::uiLightingLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiSmokeDetailLevel) == 40, "GraphicsSettings::uiSmokeDetailLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiQualityPad) == 41, "GraphicsSettings padding changed — GraphicsSettings must remain 44 bytes");
static_assert(sizeof(GraphicsSettings) == 44, "GraphicsSettings::kiVersion must be bumped with this layout");
static constexpr char kpcGraphicsSettingsPath[] = "GraphicsSettings.bin";

void SaveGraphicsSettings()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	GraphicsSettings graphicsSettings
	{
		.ePresentMode = gPresentMode.Get<VkPresentModeKHR>(),
		.eSampleCount = gSampleCount.Get<VkSampleCountFlagBits>(),
		.fMaxAnisotropy = gMaxAnisotropy.Get(),
		.fMinSampleShading = gMinSampleShading.Get(),
		.fMipLodBias = gMipLodBias.Get(),
		.fSmokeSimulationArea = gSmokeSimulationArea.Get(),
		.fMinimumAmbient = gSunMoonMinimumAmbient.Get(),
		.fLightingUpdateCadence = gLightingUpdateCadence.Get(),
		.uiWaterLevel = gWaterLevel.Get<uint8_t>(),
		.uiTerrainShadowsLevel = gTerrainShadowsLevel.Get<uint8_t>(),
		.uiObjectShadowsLevel = gObjectShadowsLevel.Get<uint8_t>(),
		.uiLightingLevel = gLightingLevel.Get<uint8_t>(),
		.uiSmokeDetailLevel = gSmokeDetailLevel.Get<uint8_t>(),
	};

	graphicsSettings.flags.Set(GraphicsSettingsFlags::kFullscreen, gFullscreen.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kMultisampling, gMultisampling.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kAnisotropy, gAnisotropy.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kSampleShading, gSampleShading.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kSmoke, gSmokeEnabled.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kWind, gWindEnabled.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kLighting, gLightingEnabled.Get<bool>());

	WriteVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kWrite}, kpcGraphicsSettingsPath, graphicsSettings);
}

namespace
{

// The file is opaque input: an out-of-range byte would index past every quality-level table.
void LoadGraphicsQualityLevel(Wrapper& rLevel, uint8_t uiLevel)
{
	// Reset, not Set: loading is initialization, so no consumer should see this as a pending change.
	rLevel.Reset<int64_t>(std::min<int64_t>(uiLevel, static_cast<int64_t>(GraphicsQualityLevel::kCount) - 1));
}

} // namespace

bool LoadGraphicsSettings()
{
	GraphicsSettings graphicsSettings {};
	gWaterShapeDetail.ResetToDefault();

	bool bRead = ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, kpcGraphicsSettingsPath, graphicsSettings);
	if (bRead)
	{
		gFullscreen.Set(graphicsSettings.flags & GraphicsSettingsFlags::kFullscreen);
		gPresentMode.Set<VkPresentModeKHR>(graphicsSettings.ePresentMode);
		gMultisampling.Set(graphicsSettings.flags & GraphicsSettingsFlags::kMultisampling);
		gSampleCount.Set<VkSampleCountFlagBits>(graphicsSettings.eSampleCount);
		gAnisotropy.Set(graphicsSettings.flags & GraphicsSettingsFlags::kAnisotropy);
		gMaxAnisotropy.Set(graphicsSettings.fMaxAnisotropy);
		gSampleShading.Set(graphicsSettings.flags & GraphicsSettingsFlags::kSampleShading);
		gMinSampleShading.Set(graphicsSettings.fMinSampleShading);
		gMipLodBias.Set(graphicsSettings.fMipLodBias);
		gSmokeEnabled.Set(graphicsSettings.flags & GraphicsSettingsFlags::kSmoke);
		gSmokeSimulationArea.Set(graphicsSettings.fSmokeSimulationArea);
		gSunMoonMinimumAmbient.Set(graphicsSettings.fMinimumAmbient);
		if (std::isfinite(graphicsSettings.fLightingUpdateCadence))
		{
			gLightingUpdateCadence.Set(graphicsSettings.fLightingUpdateCadence);
		}
		else
		{
			gLightingUpdateCadence.ResetToDefault();
		}
		gWindEnabled.Set(graphicsSettings.flags & GraphicsSettingsFlags::kWind);
		gLightingEnabled.Set(graphicsSettings.flags & GraphicsSettingsFlags::kLighting);
		LoadGraphicsQualityLevel(gWaterLevel, graphicsSettings.uiWaterLevel);
		LoadGraphicsQualityLevel(gTerrainShadowsLevel, graphicsSettings.uiTerrainShadowsLevel);
		LoadGraphicsQualityLevel(gObjectShadowsLevel, graphicsSettings.uiObjectShadowsLevel);
		LoadGraphicsQualityLevel(gLightingLevel, graphicsSettings.uiLightingLevel);
		LoadGraphicsQualityLevel(gSmokeDetailLevel, graphicsSettings.uiSmokeDetailLevel);
	}

	// Also on a failed read: the default levels still have to drive their underlying wrappers, otherwise a fresh
	// install shows a level the rendering values do not match.
	ApplyAllGraphicsQualityLevels();

	return bRead;
}

void ResetGraphicsSettings()
{
	gFullscreen.ResetToDefault();
	gPresentMode.ResetToDefault();
	gMultisampling.ResetToDefault();
	gSampleCount.ResetToDefault();
	gAnisotropy.ResetToDefault();
	gMaxAnisotropy.ResetToDefault();
	gSampleShading.ResetToDefault();
	gMinSampleShading.ResetToDefault();
	gMipLodBias.ResetToDefault();
	gWaterShapeDetail.ResetToDefault();
	gSmokeEnabled.ResetToDefault();
	gSmokeSimulationArea.ResetToDefault();
	gSunMoonMinimumAmbient.ResetToDefault();
	gLightingUpdateCadence.ResetToDefault();
	gWindEnabled.ResetToDefault();
	gLightingEnabled.ResetToDefault();

	gWaterLevel.ResetToDefault();
	gTerrainShadowsLevel.ResetToDefault();
	gObjectShadowsLevel.ResetToDefault();
	gLightingLevel.ResetToDefault();
	gSmokeDetailLevel.ResetToDefault();
	ApplyAllGraphicsQualityLevels();

	SaveGraphicsSettings();
}

} // namespace engine

#endif // defined(BT_CLIENT)
