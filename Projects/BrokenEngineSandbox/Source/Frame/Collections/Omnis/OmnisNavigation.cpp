#include "Omnis.h"

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"

#include "Frame/HealthDamage.h"

namespace game
{

void OmnisPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kTransfer;
	}, [&](int64_t i)
	{
		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];

		TransferRequest request
		{
			.change = MakeTransfer(SpawnInfo
			{
				.vecPosition = vecPosition,
				.vecDirection = rCurrentInterpolate.pVecDirections[i],
				.vecVelocity = rCurrentPostRender.pVecVelocities[i],
				.alignment = rCurrentPostRender.pAlignments[i],
				.eController = rCurrentPostRender.pController[i],
				.orders = rCurrentPostRender.pOrders[i],
				.fArmor = rCurrentPostRender.pfArmors[i],
				.fShield = rCurrentPostRender.pfShields[i],
				.nextBlasterFireTime = rCurrentPostRender.pNextBlasterFireTimes[i],
				.nextSecondarySpawnTime = rCurrentPostRender.pNextSecondarySpawnTimes[i],
				.shieldCooldown = rCurrentPostRender.pShieldCooldowns[i],
				.shieldDownSoundCooldown = rCurrentPostRender.pShieldDownSoundCooldowns[i],
				.animationTime = rCurrentInterpolate.pAnimationTimes[i],
				.flags = rCurrentPostRender.pFlags[i],
				.transferLockTimer = std::chrono::duration<float>(1.0f),
				.arrivalGracePeriod = kArrivalGracePeriod,
				.navigationDelay = rCurrentPostRender.pNavigationDelays[i],
				.globalId = rCurrentPostRender.pGlobalIds[i],
				.clientGuid = rCurrentPostRender.pClientGuids[i],
				.flagshipGlobalId = rCurrentPostRender.pFlagshipGlobalIds[i],
				.fleetWantedCoordinate = rCurrentPostRender.pFleetWantedCoordinates[i],
				.uiPendingFleetWantedCoordinateTicks = rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[i],
				.uiPendingWeaponModeTicks = rCurrentPostRender.puiPendingWeaponModeTicks[i],
				.uiFlightPlane = rCurrentInterpolate.puiFlightPlanes[i],
				.bTransfer = true,
			}),
		};
		if (!rFrame.postRender.PrepareTransferRequest(request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Omni Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Direction: {} GlobalId: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iQ, rStaticData.coordinate.iR, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDirection), rCurrentPostRender.pGlobalIds[i], rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, std::ssize(rFrame.postRender.transferRequests), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		rFrame.postRender.PushTransferRequest(request);
	});
}

} // namespace game
