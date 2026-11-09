#include "ClientSettings.h"

#if defined(BT_CLIENT)

#include "Graphics/Camera.h"
#include "Game.h"

namespace game
{

struct ClientStateSettings
{
	static constexpr int64_t kiVersion = 4;

	game::FleetGuid fleetGuid {};
	int64_t iFocusedShipId = 0;
	float fCameraFramingTarget = engine::CameraBase::kfCameraFramingInitial;
	float fMenuCameraFraming = Camera::kfMenuCameraFraming;
	XMFLOAT2 f2MenuCameraPosition {};
};
constexpr char kpcClientStatePath[] = "ClientState.bin";

static bool ValidFraming(float fFraming)
{
	return std::isfinite(fFraming) && fFraming >= engine::kfMinimumFraming && fFraming <= engine::CameraBase::kfFramingMaximum;
}

void SaveClientState()
{
	// Heap: engine::WriteVersionedFile file I/O
	ScopedSuppressAllocationTracking suppress;

	Camera& rCamera = static_cast<Camera&>(*engine::gpCamera);
	rCamera.StorePose(static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kMainMenu));
	ClientStateSettings settings
	{
		.fleetGuid            = gpGame->mRememberedFleetGuid,
		.iFocusedShipId       = gpGame->mRememberedFocusedShipIdentifier.iValue,
		.fCameraFramingTarget = rCamera.mfGameFraming,
		.fMenuCameraFraming   = rCamera.mfMenuFraming,
		.f2MenuCameraPosition = rCamera.mf2MenuPosition,
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

	if (!ValidFraming(settings.fCameraFramingTarget) || !ValidFraming(settings.fMenuCameraFraming))
	{
		LOG(kDefault, kWarning, "LoadClientState rejected {}: camera framing target {} or menu framing {} is outside [{}, {}]", kpcClientStatePath, settings.fCameraFramingTarget, settings.fMenuCameraFraming, engine::kfMinimumFraming, engine::CameraBase::kfFramingMaximum);
		return;
	}
	if (!std::isfinite(settings.f2MenuCameraPosition.x) || !std::isfinite(settings.f2MenuCameraPosition.y))
	{
		LOG(kDefault, kWarning, "LoadClientState rejected {}: menu camera position ({}, {}) is not finite", kpcClientStatePath, settings.f2MenuCameraPosition.x, settings.f2MenuCameraPosition.y);
		return;
	}

	gpGame->mRememberedFleetGuid = settings.fleetGuid;
	gpGame->mRememberedFocusedShipIdentifier = engine::GlobalId {.iValue = settings.iFocusedShipId};

	Camera& rCamera = static_cast<Camera&>(*engine::gpCamera);
	rCamera.mfGameFraming = settings.fCameraFramingTarget;
	if constexpr (kbFreeCamera)
	{
		rCamera.mfMenuFraming = settings.fMenuCameraFraming;
		rCamera.mf2MenuPosition = settings.f2MenuCameraPosition;
	}

	// The client starts in the main menu; apply its pose directly rather than easing from the default.
	rCamera.ApplyPose(true);
}

} // namespace game

#endif // BT_CLIENT
