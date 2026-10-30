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
	float fCameraFramingTarget = engine::CameraBase::kfCameraFramingInitial;
	uint8_t puiPadding[4] {};
};
constexpr char kpcClientStatePath[] = "ClientState.bin";

void SaveClientState()
{
	// Heap: engine::WriteVersionedFile file I/O
	ScopedSuppressAllocationTracking suppress;

	ClientStateSettings settings
	{
		.fleetGuid            = gpGame->mRememberedFleetGuid,
		.iFocusedShipId       = gpGame->mRememberedFocusedShipIdentifier.iValue,
		.fCameraFramingTarget = engine::gpCamera->mfCameraFramingTarget,
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

	if (!std::isfinite(settings.fCameraFramingTarget) || settings.fCameraFramingTarget < engine::kfMinimumFraming || settings.fCameraFramingTarget > engine::CameraBase::kfFramingMaximum)
	{
		LOG(kDefault, kWarning, "LoadClientState rejected {}: camera framing target {} is outside [{}, {}]", kpcClientStatePath, settings.fCameraFramingTarget, engine::kfMinimumFraming, engine::CameraBase::kfFramingMaximum);
		return;
	}

	gpGame->mRememberedFleetGuid = settings.fleetGuid;
	gpGame->mRememberedFocusedShipIdentifier = engine::GlobalId {.iValue = settings.iFocusedShipId};

	// Apply zoom directly so the camera starts AT the saved zoom rather than easing from the default.
	engine::gpCamera->RestoreFraming(settings.fCameraFramingTarget);
}

} // namespace game

#endif // BT_CLIENT
