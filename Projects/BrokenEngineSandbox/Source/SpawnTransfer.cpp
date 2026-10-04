#include "SpawnTransfer.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Frame/HealthDamage.h"
#include "Ui/WindDepositsWrappers.h"

namespace game
{

void SpawnTransfer(Frame& rFrame, StatusChangeType eType, const TransferData& rData, engine::AlignmentIdentifier playerAlignment)
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
				.fNextBlasterSpawnTime = rData.nextBlasterSpawnTimeSeconds.count(),
				.fArrivalGracePeriod = kArrivalGracePeriod.count(),
				.fDeltaRotation = rData.fDeltaRotation,
			});
			break;

		case StatusChangeType::kTransferBlaster:
			BlastersPostRender::Spawn(rFrame,
			{
				.vecPosition = rData.vecPosition,
				.vecVelocity = rData.vecVelocity,
				.uiTypeIndex = rData.uiTypeIndex,
				.alignment = rData.alignment,
				// Wind-trail tuning is client-only visual debug state; reset to canonical defaults on server-authored transfer.
				.fWindTrailIntensity = rData.alignment == playerAlignment ? gWindDepositPlayerBlastersIntensity.mfDefault : gWindDepositSpaceshipsBlastersIntensity.mfDefault,
				.fWindTrailWidth = rData.alignment == playerAlignment ? gWindDepositPlayerBlastersWidth.mfDefault : gWindDepositSpaceshipsBlastersWidth.mfDefault,
				.fWindTrailLengthMultiplier = rData.alignment == playerAlignment ? gWindDepositPlayerBlastersLengthMultiplier.mfDefault : gWindDepositSpaceshipsBlastersLengthMultiplier.mfDefault,
			});
			break;

		case StatusChangeType::kTransferMissile:
		{
			MissileFlags_t missileFlags;
			if (rData.alignment == playerAlignment)
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
				.fNextBlasterFireTime = rData.nextBlasterFireTimeSeconds.count(),
				.fNextSecondarySpawnTime = rData.nextSecondarySpawnTimeSeconds.count(),
				.fShieldCooldown = rData.shieldCooldownSeconds.count(),
				.fShieldDownSoundCooldown = rData.shieldDownSoundCooldownSeconds.count(),
				.fAnimationTime = rData.animationTimeSeconds.count(),
				.flags = PlayerFlags_t {static_cast<PlayerFlags>(rData.uiPlayerFlags)},
				.fTransferLockTimer = 1.0f,
				.fArrivalGracePeriod = kArrivalGracePeriod.count(),
				.fNavigationDelay = rData.navigationDelaySeconds.count(),
				.globalPlayerId = rData.globalPlayerId,
				.fleetWantedCoordinate = rData.fleetWantedCoordinate,
				.uiPendingFleetWantedCoordinateTicks = rData.uiPendingFleetWantedCoordinateTicks,
				.uiPendingWeaponModeTicks = rData.uiPendingWeaponModeTicks,
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
