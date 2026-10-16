#include "ClientSettings.h"

#if defined(BT_CLIENT)

#include "Game.h"

namespace game
{

struct ClientStateSettings
{
	static constexpr int64_t kiVersion = 3;

	game::FleetGuid fleetGuid {};
	int64_t iFocusedShipId = 0;
	float fCameraEyeHeightTarget = engine::CameraBase::kfCameraEyeHeightInitial;
	uint8_t uiPadding[4] {};
};
constexpr char kpcClientStatePath[] = "ClientState.bin";

void SaveClientState()
{
	// Heap: engine::WriteVersionedFile file I/O
	ScopedSuppressAllocationTracking suppress;

	ClientStateSettings settings
	{
		.fleetGuid              = gpGame->mRememberedFleetGuid,
		.iFocusedShipId         = gpGame->mRememberedFocusedShipIdentifier.iValue,
		.fCameraEyeHeightTarget = engine::gpCamera->mfCameraEyeHeightTarget,
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

	if (!std::isfinite(settings.fCameraEyeHeightTarget) || settings.fCameraEyeHeightTarget < engine::kfMinimumEyeHeight || settings.fCameraEyeHeightTarget > engine::CameraBase::kfEyeHeightMaximum)
	{
		LOG(kDefault, kWarning, "LoadClientState rejected {}: camera eye-height target {} is outside [{}, {}]", kpcClientStatePath, settings.fCameraEyeHeightTarget, engine::kfMinimumEyeHeight, engine::CameraBase::kfEyeHeightMaximum);
		return;
	}

	gpGame->mRememberedFleetGuid = settings.fleetGuid;
	gpGame->mRememberedFocusedShipIdentifier = engine::GlobalId {.iValue = settings.iFocusedShipId};

	// Apply zoom directly so the camera starts AT the saved zoom rather than easing from the default.
	engine::gpCamera->RestoreEyeHeight(settings.fCameraEyeHeightTarget);
}

} // namespace game

#endif // BT_CLIENT
