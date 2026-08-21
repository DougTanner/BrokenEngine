#include "ClientSettings.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/Screens/TweaksScreen/TweaksScreen.h"

namespace game
{

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
	float fCameraEyeHeightTarget = engine::Camera::kfCameraEyeHeightInitial;
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
	engine::gpCamera->RestoreEyeHeight(settings.fCameraEyeHeightTarget);
}

} // namespace game

#endif // BT_CLIENT
