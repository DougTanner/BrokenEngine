#include "ClientSettings.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "Ui/GraphicsQualityWrappersBase.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/LightingWrappersBase.h"
#include "Ui/Localization.h"
#include "Ui/MiscWrappersBase.h"
#include "Ui/SoundSettingsWrappersBase.h"
#include "Ui/SunMoonWrappersBase.h"
#include "Ui/Screens/TweaksScreen/TweaksScreen.h"

namespace game
{

struct GameSettings
{
	static constexpr int64_t kiVersion = 2;

	// Fixed-width on disk: the Language enum's underlying type must not decide the file layout.
	int32_t iLanguage = kEnglish;
	float fUiFontScale = 1.0f;
	float fUiOpacity = 0.5f;
	// uint8_t, not bool: the file is opaque input and a non-0/1 byte read into a bool is an invalid object representation
	uint8_t uiOpaqueUi = 0;
	engine::UiTheme eUiTheme = engine::UiTheme::kNavalSteel;
	uint8_t uiPad[2] {};
};
static_assert(std::is_trivially_copyable_v<GameSettings>);
static constexpr char kpcGameSettingsPath[] = "GameSettings.bin";

void SaveGameSettings()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	GameSettings gameSettings
	{
		.iLanguage = static_cast<int32_t>(geLanguage),
		.fUiFontScale = engine::gUiFontScale.Get(),
		.fUiOpacity = engine::gUiOpacity.Get(),
		.uiOpaqueUi = static_cast<uint8_t>(engine::gOpaqueUi.Get<bool>()),
		.eUiTheme = engine::GetUiTheme(),
	};

	engine::WriteVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, kpcGameSettingsPath, gameSettings);
}

void LoadGameSettings()
{
	GameSettings gameSettings {};

	if (engine::ReadVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, kpcGameSettingsPath, gameSettings))
	{
		// The file is opaque input: an out-of-range index would read past the translation table's language extent.
		geLanguage = (gameSettings.iLanguage >= kEnglish && gameSettings.iLanguage < kLanguageCount) ? static_cast<Language>(gameSettings.iLanguage) : kEnglish;
		// Wrapper::Set clamps to the wrapper range, but std::clamp passes a NaN through unchanged.
		if (std::isfinite(gameSettings.fUiFontScale))
		{
			engine::gUiFontScale.Set(gameSettings.fUiFontScale);
		}
		else
		{
			engine::gUiFontScale.ResetToDefault();
		}
		if (std::isfinite(gameSettings.fUiOpacity))
		{
			engine::gUiOpacity.Set(gameSettings.fUiOpacity);
		}
		else
		{
			engine::gUiOpacity.ResetToDefault();
		}
		engine::gOpaqueUi.Set(gameSettings.uiOpaqueUi != 0);
		engine::gUiTheme.Set<engine::UiTheme>(gameSettings.eUiTheme);
	}
}

void ResetGameSettings()
{
	geLanguage = kEnglish;
	engine::gUiFontScale.ResetToDefault();
	engine::gOpaqueUi.ResetToDefault();
	engine::gUiOpacity.ResetToDefault();
	engine::gUiTheme.ResetToDefault();

	SaveGameSettings();
}

struct SoundSettings
{
	static constexpr int64_t kiVersion = 3;

	float fMasterVolume = 0.0f;
	float fMusicVolume = 0.0f;
	float fSoundVolume = 0.0f;
	uint8_t uiMuteInBackground = 0;
	uint8_t uiPad[3] {};
};
static_assert(std::is_trivially_copyable_v<SoundSettings>, "SoundSettings must stay trivially copyable — WriteVersionedFile stamps sizeof");
static_assert(std::is_standard_layout_v<SoundSettings>, "SoundSettings must stay standard-layout — BT_OFFSETOF above is only well-defined for standard-layout types");
static_assert(BT_OFFSETOF(SoundSettings, fMasterVolume) == 0, "SoundSettings::fMasterVolume offset changed — existing volume bytes move");
static_assert(BT_OFFSETOF(SoundSettings, fMusicVolume) == 4, "SoundSettings::fMusicVolume offset changed — existing volume bytes move");
static_assert(BT_OFFSETOF(SoundSettings, fSoundVolume) == 8, "SoundSettings::fSoundVolume offset changed — existing volume bytes move");
static_assert(BT_OFFSETOF(SoundSettings, uiMuteInBackground) == 12, "SoundSettings::uiMuteInBackground offset changed — mute byte is appended after volumes");
static_assert(BT_OFFSETOF(SoundSettings, uiPad) == 13, "SoundSettings padding changed — SoundSettings must remain 16 bytes");
static_assert(sizeof(SoundSettings) == 16, "SoundSettings::kiVersion must be bumped with this layout");
static constexpr char kpcSoundSettingsPath[] = "SoundSettings.bin";

void SaveSoundSettings()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	SoundSettings soundSettings
	{
		.fMasterVolume = engine::gMasterVolume.Get(),
		.fMusicVolume = engine::gMusicVolume.Get(),
		.fSoundVolume = engine::gSoundVolume.Get(),
		.uiMuteInBackground = static_cast<uint8_t>(engine::gMuteInBackground.Get<bool>()),
	};

	engine::WriteVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, kpcSoundSettingsPath, soundSettings);
}

void LoadSoundSettings()
{
	SoundSettings soundSettings {};

	if (engine::ReadVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, kpcSoundSettingsPath, soundSettings))
	{
		engine::gMasterVolume.Set(soundSettings.fMasterVolume);
		engine::gMusicVolume.Set(soundSettings.fMusicVolume);
		engine::gSoundVolume.Set(soundSettings.fSoundVolume);
		engine::gMuteInBackground.Set(soundSettings.uiMuteInBackground != 0);
	}

	if constexpr (kbRecording)
	{
		engine::gMusicVolume.Set(0.0f);
	}
}

void ResetSoundSettings()
{
	engine::gMasterVolume.ResetToDefault();
	engine::gMusicVolume.ResetToDefault();
	engine::gSoundVolume.ResetToDefault();
	engine::gMuteInBackground.ResetToDefault();

	SaveSoundSettings();
}

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
static constexpr char kpcGraphicsSettingsPath[] = "GraphicsSettings.bin";

void SaveGraphicsSettings()
{
	// Heap: file I/O allocates
	ScopedSuppressAllocationTracking suppress;

	GraphicsSettings graphicsSettings
	{
		.ePresentMode = engine::gPresentMode.Get<VkPresentModeKHR>(),
		.eSampleCount = engine::gSampleCount.Get<VkSampleCountFlagBits>(),
		.fMaxAnisotropy = engine::gMaxAnisotropy.Get(),
		.fMinSampleShading = engine::gMinSampleShading.Get(),
		.fMipLodBias = engine::gMipLodBias.Get(),
		.fSmokeSimulationArea = engine::gSmokeSimulationArea.Get(),
		.fMinimumAmbient = engine::gSunMoonMinimumAmbient.Get(),
		.fLightingUpdateCadence = engine::gLightingUpdateCadence.Get(),
		.uiWaterLevel = engine::gWaterLevel.Get<uint8_t>(),
		.uiTerrainShadowsLevel = engine::gTerrainShadowsLevel.Get<uint8_t>(),
		.uiObjectShadowsLevel = engine::gObjectShadowsLevel.Get<uint8_t>(),
		.uiLightingLevel = engine::gLightingLevel.Get<uint8_t>(),
		.uiSmokeDetailLevel = engine::gSmokeDetailLevel.Get<uint8_t>(),
	};

	graphicsSettings.flags.Set(GraphicsSettingsFlags::kFullscreen, engine::gFullscreen.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kMultisampling, engine::gMultisampling.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kAnisotropy, engine::gAnisotropy.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kSampleShading, engine::gSampleShading.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kSmoke, engine::gSmokeEnabled.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kWind, engine::gWindEnabled.Get<bool>());
	graphicsSettings.flags.Set(GraphicsSettingsFlags::kLighting, engine::gLightingEnabled.Get<bool>());

	engine::WriteVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, kpcGraphicsSettingsPath, graphicsSettings);
}

namespace
{

// The file is opaque input: an out-of-range byte would index past every quality-level table.
void LoadGraphicsQualityLevel(engine::Wrapper& rLevel, uint8_t uiLevel)
{
	// Reset, not Set: loading is initialization, so no consumer should see this as a pending change.
	rLevel.Reset<int64_t>(std::min<int64_t>(uiLevel, static_cast<int64_t>(engine::GraphicsQualityLevel::kCount) - 1));
}

} // namespace

bool LoadGraphicsSettings()
{
	GraphicsSettings graphicsSettings {};
	engine::gWaterShapeDetail.ResetToDefault();

	bool bRead = engine::ReadVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, kpcGraphicsSettingsPath, graphicsSettings);
	if (bRead)
	{
		engine::gFullscreen.Set(graphicsSettings.flags & GraphicsSettingsFlags::kFullscreen);
		engine::gPresentMode.Set<VkPresentModeKHR>(graphicsSettings.ePresentMode);
		engine::gMultisampling.Set(graphicsSettings.flags & GraphicsSettingsFlags::kMultisampling);
		engine::gSampleCount.Set<VkSampleCountFlagBits>(graphicsSettings.eSampleCount);
		engine::gAnisotropy.Set(graphicsSettings.flags & GraphicsSettingsFlags::kAnisotropy);
		engine::gMaxAnisotropy.Set(graphicsSettings.fMaxAnisotropy);
		engine::gSampleShading.Set(graphicsSettings.flags & GraphicsSettingsFlags::kSampleShading);
		engine::gMinSampleShading.Set(graphicsSettings.fMinSampleShading);
		engine::gMipLodBias.Set(graphicsSettings.fMipLodBias);
		engine::gSmokeEnabled.Set(graphicsSettings.flags & GraphicsSettingsFlags::kSmoke);
		engine::gSmokeSimulationArea.Set(graphicsSettings.fSmokeSimulationArea);
		engine::gSunMoonMinimumAmbient.Set(graphicsSettings.fMinimumAmbient);
		if (std::isfinite(graphicsSettings.fLightingUpdateCadence))
		{
			engine::gLightingUpdateCadence.Set(graphicsSettings.fLightingUpdateCadence);
		}
		else
		{
			engine::gLightingUpdateCadence.ResetToDefault();
		}
		engine::gWindEnabled.Set(graphicsSettings.flags & GraphicsSettingsFlags::kWind);
		engine::gLightingEnabled.Set(graphicsSettings.flags & GraphicsSettingsFlags::kLighting);
		LoadGraphicsQualityLevel(engine::gWaterLevel, graphicsSettings.uiWaterLevel);
		LoadGraphicsQualityLevel(engine::gTerrainShadowsLevel, graphicsSettings.uiTerrainShadowsLevel);
		LoadGraphicsQualityLevel(engine::gObjectShadowsLevel, graphicsSettings.uiObjectShadowsLevel);
		LoadGraphicsQualityLevel(engine::gLightingLevel, graphicsSettings.uiLightingLevel);
		LoadGraphicsQualityLevel(engine::gSmokeDetailLevel, graphicsSettings.uiSmokeDetailLevel);
	}

	// Also on a failed read: the default levels still have to drive their underlying wrappers, otherwise a fresh
	// install shows a level the rendering values do not match.
	engine::ApplyAllGraphicsQualityLevels();

	return bRead;
}

void ResetGraphicsSettings()
{
	engine::gFullscreen.ResetToDefault();
	engine::gPresentMode.ResetToDefault();
	engine::gMultisampling.ResetToDefault();
	engine::gSampleCount.ResetToDefault();
	engine::gAnisotropy.ResetToDefault();
	engine::gMaxAnisotropy.ResetToDefault();
	engine::gSampleShading.ResetToDefault();
	engine::gMinSampleShading.ResetToDefault();
	engine::gMipLodBias.ResetToDefault();
	engine::gWaterShapeDetail.ResetToDefault();
	engine::gSmokeEnabled.ResetToDefault();
	engine::gSmokeSimulationArea.ResetToDefault();
	engine::gSunMoonMinimumAmbient.ResetToDefault();
	engine::gLightingUpdateCadence.ResetToDefault();
	engine::gWindEnabled.ResetToDefault();
	engine::gLightingEnabled.ResetToDefault();

	engine::gWaterLevel.ResetToDefault();
	engine::gTerrainShadowsLevel.ResetToDefault();
	engine::gObjectShadowsLevel.ResetToDefault();
	engine::gLightingLevel.ResetToDefault();
	engine::gSmokeDetailLevel.ResetToDefault();
	engine::ApplyAllGraphicsQualityLevels();

	SaveGraphicsSettings();
}

struct TweaksSettings
{
	static constexpr int64_t kiVersion = 14;

	bool bShowImGui = false;
	uint8_t uiPad[3] {};
	float fSunAngle = 1.15f;
	// Engine-owned layout POD, embedded by value: one array bound and one sizeof for the whole program.
	engine::TweakSectionState sectionState {};
};
static_assert(std::is_trivially_copyable_v<TweaksSettings>);
static_assert(sizeof(TweaksSettings) == 304, "kiVersion must be bumped with this layout");
static constexpr char kpcTweaksSettingsPath[] = "TweaksSettings.bin";

void SaveTweaksSettings()
{
	if constexpr (!kbDebugInput)
	{
		return;
	}

	TweaksSettings settings {};
	settings.bShowImGui = gpGame->mbShowImGui;
	settings.fSunAngle = engine::gSunAngleOverride.Get();
	engine::gpImGuiManager->mpTweaksScreen->SaveState(settings.sectionState);

	engine::WriteVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, kpcTweaksSettingsPath, settings);
}

void LoadTweaksSettings()
{
	if constexpr (!kbDebugInput)
	{
		return;
	}

	TweaksSettings settings {};
	if (engine::ReadVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, kpcTweaksSettingsPath, settings))
	{
		gpGame->mbShowImGui = settings.bShowImGui;
		engine::gpImGuiManager->mpTweaksScreen->LoadState(settings.sectionState);
		engine::gSunAngleOverride.Set(settings.fSunAngle);
	}
	else
	{
		LOG(kDefault, kWarning, "LoadTweaks FAILED to read file");
	}
}

struct ClientStateSettings
{
	static constexpr int64_t kiVersion = 3;

	game::FleetGuid fleetGuid {};
	int64_t iFocusedShipId = 0;
	float fCameraEyeHeightTarget = game::Camera::kfCameraEyeHeightInitial;
	uint8_t uiPad[4] {};
};
static constexpr char kpcClientStatePath[] = "ClientState.bin";

void SaveClientState()
{
	// Heap: engine::WriteVersionedFile file I/O
	ScopedSuppressAllocationTracking suppress;

	ClientStateSettings settings
	{
		.fleetGuid              = gpGame->mRememberedFleetGuid,
		.iFocusedShipId         = gpGame->mRememberedFocusedShipId.iValue,
		.fCameraEyeHeightTarget = gpGame->mfRememberedCameraEyeHeightTarget,
	};
	engine::WriteVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kWrite}, kpcClientStatePath, settings);
}

void LoadClientState()
{
	// Heap: engine::ReadVersionedFile file I/O
	ScopedSuppressAllocationTracking suppress;

	ClientStateSettings settings {};
	if (!engine::ReadVersionedFile({engine::FileFlags::kAppDataDirectory, engine::FileFlags::kRead}, kpcClientStatePath, settings))
	{
		return;
	}

	gpGame->mRememberedFleetGuid = settings.fleetGuid;
	gpGame->mRememberedFocusedShipId = engine::global_id_t {settings.iFocusedShipId};
	gpGame->mfRememberedCameraEyeHeightTarget = settings.fCameraEyeHeightTarget;

	// Apply zoom directly so the camera starts AT the saved zoom rather than easing from the default.
	gpCamera->mfCameraEyeHeight = settings.fCameraEyeHeightTarget;
	gpCamera->mfCameraEyeHeightTarget = settings.fCameraEyeHeightTarget;
}

} // namespace game

#endif // BT_CLIENT
