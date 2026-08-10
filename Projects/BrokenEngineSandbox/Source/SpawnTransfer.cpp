#include "SpawnTransfer.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Frame/HealthDamage.h"

namespace game
{

void SpawnTransfer(Frame& rFrame, StatusChangeType eType, const TransferData& rData, engine::alignment_t playerAlignment)
{
	switch (eType)
	{
		case StatusChangeType::kTransferSpaceship:
			SpaceshipsPostRender::Spawn(rFrame, {
				.vecPosition = rData.vecPosition,
				.vecDirection = rData.vecDirection,
				.vecVelocity = rData.vecVelocity,
				.alignment = rData.alignment,
				.fHealth = rData.fHealth,
				.fNextBlasterSpawnTime = rData.fNextBlasterSpawnTime,
				.fArrivalGracePeriod = kfArrivalGracePeriod,
				.fDeltaRotation = rData.fDeltaRotation,
			});
			break;

		case StatusChangeType::kTransferBlaster:
			BlastersPostRender::Spawn(rFrame, {
				.vecPosition = rData.vecPosition,
				.vecVelocity = rData.vecVelocity,
				.uiTypeIndex = rData.uiTypeIndex,
				.alignment = rData.alignment,
				// Wind-trail tuning is client-only visual debug state; reset to canonical defaults on server-authored transfer.
				.fWindTrailIntensity = rData.alignment == playerAlignment ? gWindDepositPlayerBlastersIntensity.GetDefault() : gWindDepositSpaceshipsBlastersIntensity.GetDefault(),
				.fWindTrailWidth = rData.alignment == playerAlignment ? gWindDepositPlayerBlastersWidth.GetDefault() : gWindDepositSpaceshipsBlastersWidth.GetDefault(),
				.fWindTrailLengthMultiplier = rData.alignment == playerAlignment ? gWindDepositPlayerBlastersLengthMultiplier.GetDefault() : gWindDepositSpaceshipsBlastersLengthMultiplier.GetDefault(),
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

			MissilesPostRender::Spawn(rFrame, {
				.vecPosition = rData.vecPosition,
				.vecDirection = rData.vecDirection,
				.vecVelocity = rData.vecVelocity,
				.vecStoredDirection = rData.vecDirection,
				.uiTarget = {},
				.fAcceleration = rData.fAcceleration,
				.flags = missileFlags,
				.alignment = rData.alignment,
				.fDeltaRotationDelay = rData.fDeltaRotationDelay,
				.fDeltaRotation = rData.fDeltaRotation,
				.fDeltaRotationMax = rData.fDeltaRotationMax,
				.fPitch = rData.fPitch,
				.fTime = rData.fTime,
				.fNextJitter = rData.fNextJitter,
				.bTransfer = true,
#if defined(BT_CLIENT)
				.smokeTrailId = rData.smokeTrailId,
#endif
			});
			break;
		}

		case StatusChangeType::kTransferPlayer:
			PlayersPostRender::Spawn(rFrame, {
				.vecPosition = rData.vecPosition,
				.vecDirection = rData.vecDirection,
				.vecVelocity = rData.vecVelocity,
				.alignment = rData.alignment,
				.fArmor = rData.fHealth,
				.fShield = rData.fShield,
				.fNextBlasterFireTime = rData.fNextBlasterFireTime,
				.fNextSecondarySpawnTime = rData.fNextSecondarySpawnTime,
				.fShieldCooldown = rData.fShieldCooldown,
				.fShieldDownSoundCooldown = rData.fShieldDownSoundCooldown,
				.fAnimationTime = rData.fAnimationTime,
				// Shield rotation/shrink are client-only visual animation state; reset to canonical defaults while gameplay transfer fields restore verbatim.
				.flags = PlayerFlags_t {static_cast<PlayerFlags>(rData.uiPlayerFlags)},
				.fTransferLockTimer = 1.0f,
				.fArrivalGracePeriod = kfArrivalGracePeriod,
				.fNavigationDelay = rData.fNavigationDelay,
				.globalPlayerId = rData.globalPlayerId,
				.fleetWantedCoord = rData.fleetWantedCoord,
				.uiPendingFleetWantedCoordTicks = rData.uiPendingFleetWantedCoordTicks,
				.uiPendingWeaponModeTicks = rData.uiPendingWeaponModeTicks,
				.bTransfer = true,
			});
			break;

		default:
			break;
	}
}

} // namespace game
