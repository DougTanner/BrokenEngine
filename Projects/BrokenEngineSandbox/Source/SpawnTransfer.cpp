#include "SpawnTransfer.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Frame/HealthDamage.h"
#include "Ui/WindDepositsWrappers.h"

namespace game
{

void SpawnTransfer(Frame& rFrame, StatusChangeType eType, const TransferData& rData)
{
	switch (eType)
	{
		case StatusChangeType::kTransferSpaceship:
			SpaceshipsPostRender::Spawn(rFrame,
			{
				.vecPosition = rData.vecPosition,
				.vecDirection = rData.vecDirection,
				.vecVelocity = rData.vecVelocity,
				.alignment = rData.alignment,
				.fHealth = rData.fHealth,
				.nextBlasterSpawnTime = rData.nextBlasterSpawnTimeSeconds,
				.arrivalGracePeriod = kArrivalGracePeriod,
				.fDeltaRotation = rData.fDeltaRotation,
			});
			break;

		case StatusChangeType::kTransferBlaster:
			BlastersPostRender::Spawn(rFrame,
			{
				.vecPosition = rData.vecPosition,
				.vecVelocity = rData.vecVelocity,
				.iTypeIndex = rData.uiTypeIndex,
				.alignment = rData.alignment,
				// Wind-trail tuning is client-only visual debug state; reset to canonical defaults on server-authored transfer.
				.fWindTrailIntensity = rData.alignment == rFrame.postRender.playerAlignment ? gWindDepositPlayerBlastersIntensity.mfDefault : gWindDepositSpaceshipsBlastersIntensity.mfDefault,
				.fWindTrailWidth = rData.alignment == rFrame.postRender.playerAlignment ? gWindDepositPlayerBlastersWidth.mfDefault : gWindDepositSpaceshipsBlastersWidth.mfDefault,
				.fWindTrailLengthMultiplier = rData.alignment == rFrame.postRender.playerAlignment ? gWindDepositPlayerBlastersLengthMultiplier.mfDefault : gWindDepositSpaceshipsBlastersLengthMultiplier.mfDefault,
			});
			break;

		case StatusChangeType::kTransferMissile:
		{
			MissileFlags_t missileFlags;
			if (rData.alignment == rFrame.postRender.playerAlignment)
			{
				missileFlags.Set(MissileFlags::kTargetEnemy);
			}
			else
			{
				missileFlags.Set(MissileFlags::kTargetPlayer);
			}

			MissilesPostRender::Spawn(rFrame,
			{
				.vecPosition = rData.vecPosition,
				.vecDirection = rData.vecDirection,
				.vecVelocity = rData.vecVelocity,
				.vecStoredDirection = rData.vecDirection,
				.uiTarget = {},
				.fAcceleration = rData.fAcceleration,
				.flags = missileFlags,
				.alignment = rData.alignment,
				.deltaRotationDelay = std::chrono::duration<float>(rData.deltaRotationDelaySeconds.count()),
				.fDeltaRotation = rData.fDeltaRotation,
				.fDeltaRotationMaximum = rData.fDeltaRotationMaximum,
				.fPitch = rData.fPitch,
				.time = std::chrono::duration<float>(rData.timeSeconds.count()),
				.nextJitter = std::chrono::duration<float>(rData.nextJitterSeconds.count()),
				.bTransfer = true,
			});
			break;
		}

		case StatusChangeType::kTransferPlayer:
			PlayersPostRender::Spawn(rFrame,
			{
				.vecPosition = rData.vecPosition,
				.vecDirection = rData.vecDirection,
				.vecVelocity = rData.vecVelocity,
				.alignment = rData.alignment,
				.fArmor = rData.fHealth,
				.fShield = rData.fShield,
				.nextBlasterFireTime = rData.nextBlasterFireTimeSeconds,
				.nextSecondarySpawnTime = rData.nextSecondarySpawnTimeSeconds,
				.shieldCooldown = rData.shieldCooldownSeconds,
				.shieldDownSoundCooldown = rData.shieldDownSoundCooldownSeconds,
				.animationTime = rData.animationTimeSeconds,
				.flags = PlayerFlags_t {static_cast<PlayerFlags>(rData.uiPlayerFlags)},
				.transferLockTimer = std::chrono::duration<float>(1.0f),
				.arrivalGracePeriod = kArrivalGracePeriod,
				.navigationDelay = rData.navigationDelaySeconds,
				.globalPlayerId = rData.globalId,
				.fleetWantedCoordinate = rData.fleetWantedCoordinate,
				.iPendingFleetWantedCoordinateTicks = rData.uiPendingFleetWantedCoordinateTicks,
				.iPendingWeaponModeTicks = rData.uiPendingWeaponModeTicks,
				.bTransfer = true,
			});
			break;

		default:
			break;
	}
}

bool IsAdoptableStatusChange(const StatusChange& rChange)
{
	if (rChange.eType != StatusChangeType::kTransferBlaster)
	{
		return true;
	}

	return std::get<TransferData>(rChange.data).uiTypeIndex < std::ssize(BlastersInterpolate::sTypes);
}

} // namespace game
