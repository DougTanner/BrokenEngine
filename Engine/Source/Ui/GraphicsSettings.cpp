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
	uint8_t uiPadding[3] {};
	VkPresentModeKHR vkPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	VkSampleCountFlagBits vkSampleCount = VK_SAMPLE_COUNT_4_BIT;
	float fMaximumAnisotropy = 0.0f;
	float fMinimumSampleShading = 0.0f;
	float fMipmapLevelOfDetailBias = 0.0f;
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
	uint8_t uiQualityPadding[3] {};
};
static_assert(std::is_trivially_copyable_v<GraphicsSettings>, "GraphicsSettings must stay trivially copyable — WriteVersionedFile stamps sizeof");
static_assert(std::is_standard_layout_v<GraphicsSettings>, "GraphicsSettings must stay standard-layout — BT_OFFSETOF below is only well-defined for standard-layout types");
static_assert(BT_OFFSETOF(GraphicsSettings, flags) == 0, "GraphicsSettings::flags offset changed — the flag byte leads the file");
static_assert(BT_OFFSETOF(GraphicsSettings, uiPadding) == 1, "GraphicsSettings padding changed — common::Flags must stay one byte");
static_assert(BT_OFFSETOF(GraphicsSettings, vkPresentMode) == 4, "GraphicsSettings::vkPresentMode offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, vkSampleCount) == 8, "GraphicsSettings::vkSampleCount offset changed — VkPresentModeKHR must stay four bytes");
static_assert(BT_OFFSETOF(GraphicsSettings, fMaximumAnisotropy) == 12, "GraphicsSettings::fMaximumAnisotropy offset changed — VkSampleCountFlagBits must stay four bytes");
static_assert(BT_OFFSETOF(GraphicsSettings, fMinimumSampleShading) == 16, "GraphicsSettings::fMinimumSampleShading offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fMipmapLevelOfDetailBias) == 20, "GraphicsSettings::fMipmapLevelOfDetailBias offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fSmokeSimulationArea) == 24, "GraphicsSettings::fSmokeSimulationArea offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fMinimumAmbient) == 28, "GraphicsSettings::fMinimumAmbient offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, fLightingUpdateCadence) == 32, "GraphicsSettings::fLightingUpdateCadence offset changed — existing setting bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiWaterLevel) == 36, "GraphicsSettings::uiWaterLevel offset changed — the quality bytes are appended after the floats");
static_assert(BT_OFFSETOF(GraphicsSettings, uiTerrainShadowsLevel) == 37, "GraphicsSettings::uiTerrainShadowsLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiObjectShadowsLevel) == 38, "GraphicsSettings::uiObjectShadowsLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiLightingLevel) == 39, "GraphicsSettings::uiLightingLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiSmokeDetailLevel) == 40, "GraphicsSettings::uiSmokeDetailLevel offset changed — existing quality bytes move");
static_assert(BT_OFFSETOF(GraphicsSettings, uiQualityPadding) == 41, "GraphicsSettings padding changed — GraphicsSettings must remain 44 bytes");
static_assert(sizeof(GraphicsSettings) == 44, "GraphicsSettings::kiVersion must be bumped with this layout");
constexpr char kpcGraphicsSettingsPath[] = "GraphicsSettings.bin";

void SaveGraphicsSettings()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	GraphicsSettings graphicsSettings
	{
		.vkPresentMode = gPresentMode.Get<VkPresentModeKHR>(),
		.vkSampleCount = gSampleCount.Get<VkSampleCountFlagBits>(),
		.fMaximumAnisotropy = gMaximumAnisotropy.mfCurrent,
		.fMinimumSampleShading = gMinimumSampleShading.mfCurrent,
		.fMipmapLevelOfDetailBias = gMipmapLevelOfDetailBias.mfCurrent,
		.fSmokeSimulationArea = gSmokeSimulationArea.mfCurrent,
		.fMinimumAmbient = gSunMoonMinimumAmbient.mfCurrent,
		.fLightingUpdateCadence = gLightingUpdateCadence.mfCurrent,
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

static bool IsValidGraphicsQualityLevel(uint8_t uiLevel)
{
	return uiLevel < static_cast<uint8_t>(GraphicsQualityLevel::kCount);
}

// Returns the first invalid field's name, or nullptr when every checked field is valid.
static const char* FindInvalidGraphicsSetting(const GraphicsSettings& rGraphicsSettings)
{
	if (!std::ranges::contains(gSampleCount.mAllowed, static_cast<float>(rGraphicsSettings.vkSampleCount)))
	{
		return "eSampleCount";
	}

	if (!gMaximumAnisotropy.IsInRange(rGraphicsSettings.fMaximumAnisotropy))
	{
		return "fMaxAnisotropy";
	}

	if (!gMinimumSampleShading.IsInRange(rGraphicsSettings.fMinimumSampleShading))
	{
		return "fMinSampleShading";
	}

	if (!gMipmapLevelOfDetailBias.IsInRange(rGraphicsSettings.fMipmapLevelOfDetailBias))
	{
		return "fMipLodBias";
	}

	if (!gSmokeSimulationArea.IsInRange(rGraphicsSettings.fSmokeSimulationArea))
	{
		return "fSmokeSimulationArea";
	}

	if (!gSunMoonMinimumAmbient.IsInRange(rGraphicsSettings.fMinimumAmbient))
	{
		return "fMinimumAmbient";
	}

	if (!gLightingUpdateCadence.IsInRange(rGraphicsSettings.fLightingUpdateCadence))
	{
		return "fLightingUpdateCadence";
	}

	if (!IsValidGraphicsQualityLevel(rGraphicsSettings.uiWaterLevel))
	{
		return "uiWaterLevel";
	}

	if (!IsValidGraphicsQualityLevel(rGraphicsSettings.uiTerrainShadowsLevel))
	{
		return "uiTerrainShadowsLevel";
	}

	if (!IsValidGraphicsQualityLevel(rGraphicsSettings.uiObjectShadowsLevel))
	{
		return "uiObjectShadowsLevel";
	}

	if (!IsValidGraphicsQualityLevel(rGraphicsSettings.uiLightingLevel))
	{
		return "uiLightingLevel";
	}

	if (!IsValidGraphicsQualityLevel(rGraphicsSettings.uiSmokeDetailLevel))
	{
		return "uiSmokeDetailLevel";
	}

	return nullptr;
}

static void LoadGraphicsQualityLevel(Wrapper& rLevel, uint8_t uiLevel)
{
	// Reset, not Set: loading is initialization, so no consumer should see this as a pending change.
	rLevel.Reset<int64_t>(uiLevel);
}

bool LoadGraphicsSettings()
{
	GraphicsSettings graphicsSettings {};
	gWaterShapeDetail.mfCurrent = gWaterShapeDetail.mfDefault;

	bool bRead = ReadVersionedFile({FileFlags::kAppDataDirectory, FileFlags::kRead}, kpcGraphicsSettingsPath, graphicsSettings);
	if (bRead)
	{
		const char* pcInvalidField = FindInvalidGraphicsSetting(graphicsSettings);
		if (pcInvalidField != nullptr)
		{
			LOG(kLoading, kWarning, "{} rejected: invalid {}", kpcGraphicsSettingsPath, pcInvalidField);
			bRead = false;
		}
	}

	if (bRead)
	{
		gFullscreen.Set(graphicsSettings.flags & GraphicsSettingsFlags::kFullscreen);
		gPresentMode.Set<VkPresentModeKHR>(graphicsSettings.vkPresentMode);
		gMultisampling.Set(graphicsSettings.flags & GraphicsSettingsFlags::kMultisampling);
		gSampleCount.Set<VkSampleCountFlagBits>(graphicsSettings.vkSampleCount);
		gAnisotropy.Set(graphicsSettings.flags & GraphicsSettingsFlags::kAnisotropy);
		gMaximumAnisotropy.Set(graphicsSettings.fMaximumAnisotropy);
		gSampleShading.Set(graphicsSettings.flags & GraphicsSettingsFlags::kSampleShading);
		gMinimumSampleShading.Set(graphicsSettings.fMinimumSampleShading);
		gMipmapLevelOfDetailBias.Set(graphicsSettings.fMipmapLevelOfDetailBias);
		gSmokeEnabled.Set(graphicsSettings.flags & GraphicsSettingsFlags::kSmoke);
		gSmokeSimulationArea.Set(graphicsSettings.fSmokeSimulationArea);
		gSunMoonMinimumAmbient.Set(graphicsSettings.fMinimumAmbient);
		gLightingUpdateCadence.Set(graphicsSettings.fLightingUpdateCadence);
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
	gFullscreen.mfCurrent = gFullscreen.mfDefault;
	gPresentMode.mfCurrent = gPresentMode.mfDefault;
	gMultisampling.mfCurrent = gMultisampling.mfDefault;
	gSampleCount.mfCurrent = gSampleCount.mfDefault;
	gAnisotropy.mfCurrent = gAnisotropy.mfDefault;
	gMaximumAnisotropy.mfCurrent = gMaximumAnisotropy.mfDefault;
	gSampleShading.mfCurrent = gSampleShading.mfDefault;
	gMinimumSampleShading.mfCurrent = gMinimumSampleShading.mfDefault;
	gMipmapLevelOfDetailBias.mfCurrent = gMipmapLevelOfDetailBias.mfDefault;
	gWaterShapeDetail.mfCurrent = gWaterShapeDetail.mfDefault;
	gSmokeEnabled.mfCurrent = gSmokeEnabled.mfDefault;
	gSmokeSimulationArea.mfCurrent = gSmokeSimulationArea.mfDefault;
	gSunMoonMinimumAmbient.mfCurrent = gSunMoonMinimumAmbient.mfDefault;
	gLightingUpdateCadence.mfCurrent = gLightingUpdateCadence.mfDefault;
	gWindEnabled.mfCurrent = gWindEnabled.mfDefault;
	gLightingEnabled.mfCurrent = gLightingEnabled.mfDefault;

	gWaterLevel.mfCurrent = gWaterLevel.mfDefault;
	gTerrainShadowsLevel.mfCurrent = gTerrainShadowsLevel.mfDefault;
	gObjectShadowsLevel.mfCurrent = gObjectShadowsLevel.mfDefault;
	gLightingLevel.mfCurrent = gLightingLevel.mfDefault;
	gSmokeDetailLevel.mfCurrent = gSmokeDetailLevel.mfDefault;
	ApplyAllGraphicsQualityLevels();

	SaveGraphicsSettings();
}

} // namespace engine

#endif // defined(BT_CLIENT)
