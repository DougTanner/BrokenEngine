#include "Players.h"

#include "Data/Audio.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Frame/HealthDamage.h"
#include "Ui/WindDepositsWrappers.h"

#if defined(BT_CLIENT)
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Ui/SoundWrappers.h"
#endif

namespace game
{

using enum PlayerFlags;

constexpr float kfBlasterTargetRange = 120.0f;
constexpr float kfMissileTargetRange = 160.0f;

constexpr std::chrono::duration<float> kShieldCooldown(2.0f);
constexpr std::chrono::duration<float> kShieldDownSoundCooldown(2.0f);
constexpr float kfArmorHitSoundDamageThreshold = 3.0f;

constexpr float kfBlastersSpawnBarrelOffset = kfPlayerRadius * 0.4667f;
constexpr float kfBlastersSpawnPreMove = 0.0f;
constexpr float kfBlasterAngleJitter = 0.03f;

constexpr std::chrono::duration<float> kMissileSpawnInterval(0.2f);
constexpr float kfMissileInitialVelocity = 30.0f;
constexpr float kfMissileAcceleration = 40.0f;
constexpr float kfMissileSpawnBarrelOffset = kfPlayerRadius * 0.7333f;
constexpr float kfMissileSpawnPreMove = kfPlayerRadius * 1.0f;
constexpr float kfMissileSpawnAngle = XM_PIDIV16;
constexpr float kfMissileAngleJitter = XM_PIDIV16;

constexpr float kfExplosionsRadius = kfPlayerRadius * 20.0f;
constexpr float kfExplosionParticleCount = 16.0f;
constexpr float kfExplosionSizeStart = kfPlayerRadius * 1.3333f;
constexpr float kfExplosionSizeEnd = kfPlayerRadius * 0.3333f;
constexpr float kfExplosionSmoke = 0.25f;
constexpr float kfDeathRadialPower = 0.3f;
constexpr int64_t kiDeathTrailCount = 2;

// Firing, look targets, and missile aim require a living, visible spaceship past arrival grace.
static bool XM_CALLCONV IsAcquireCandidate(const SpaceshipsInterpolate& __restrict rSpaceshipsInterpolate, const SpaceshipsPostRender& __restrict rSpaceshipsPostRender, FXMVECTOR vecPlayerPosition, int64_t j)
{
	if (rSpaceshipsInterpolate.pfDestroyedTimes[j] != -1.0f)
	{
		return false;
	}
	if (rSpaceshipsPostRender.pfArrivalGracePeriods[j] > 0.0f)
	{
		return false;
	}
	if (!FrameInterpolate::IsVisible(vecPlayerPosition, rSpaceshipsInterpolate.pVecPositions[j]))
	{
		return false;
	}
	return true;
}

// Missile aim searches current-frame data; AcquireTarget selects firing and look targets from previous-frame data.
static int64_t XM_CALLCONV FindTargetSpaceshipIndex(const SpaceshipsInterpolate& __restrict rSpaceshipsInterpolate, const SpaceshipsPostRender& __restrict rSpaceshipsPostRender, FXMVECTOR vecPlayerPosition, float fMaximumRange)
{
	float fClosestDistance = fMaximumRange;
	int64_t iClosestSpaceship = -1;
	for (int64_t j = 0; j < rSpaceshipsPostRender.iCount; ++j)
	{
		if (!IsAcquireCandidate(rSpaceshipsInterpolate, rSpaceshipsPostRender, vecPlayerPosition, j))
		{
			continue;
		}
		float fDistance = common::Distance(vecPlayerPosition, rSpaceshipsInterpolate.pVecPositions[j]);
		if (fDistance < fClosestDistance)
		{
			fClosestDistance = fDistance;
			iClosestSpaceship = j;
		}
	}
	return iClosestSpaceship;
}


void XM_CALLCONV PlayersPostRender::AcquireTarget(const Frame& __restrict rPreviousFrame, FXMVECTOR vecPosition, PlayerFlags_t& rFlags, bool& rbLookTargetFound, XMVECTOR& rVecLookPosition)
{
	const SpaceshipsInterpolate& rSpaceshipsInterpolate = *rPreviousFrame.interpolate.pSpaceships;
	const SpaceshipsPostRender& rSpaceshipsPostRender = *rPreviousFrame.postRender.pSpaceships;
	int64_t iSpaceshipCount = rSpaceshipsPostRender.iCount;

	int64_t iTargetInRange = -1;
	int64_t iTargetForLook = -1;
	float fBestInRange = kfMissileTargetRange;
	float fBestForLook = std::numeric_limits<float>::max();
	for (int64_t j = 0; j < iSpaceshipCount; ++j)
	{
		if (!IsAcquireCandidate(rSpaceshipsInterpolate, rSpaceshipsPostRender, vecPosition, j))
		{
			continue;
		}
		float fDistance = common::Distance(vecPosition, rSpaceshipsInterpolate.pVecPositions[j]);
		if (fDistance < fBestInRange)
		{
			fBestInRange = fDistance;
			iTargetInRange = j;
		}
		if (fDistance < fBestForLook)
		{
			fBestForLook = fDistance;
			iTargetForLook = j;
		}
	}

	rbLookTargetFound = false;
	rVecLookPosition = XMVectorZero();
	if (iTargetInRange >= 0)
	{
		rVecLookPosition = common::ComputeLeadPosition(vecPosition, rSpaceshipsInterpolate.pVecPositions[iTargetInRange], rSpaceshipsPostRender.pVecVelocities[iTargetInRange], kfPlayerBlastersSpeed);
		rbLookTargetFound = true;
	}
	else if (iTargetForLook >= 0)
	{
		rVecLookPosition = rSpaceshipsInterpolate.pVecPositions[iTargetForLook];
		rbLookTargetFound = true;
	}

	// Weapon spawn timers rate-limit continuous fire; blasters require the shorter target range.
	if (iTargetInRange >= 0)
	{
		rFlags.Set(kFireMissile);
		if (common::Distance(vecPosition, rSpaceshipsInterpolate.pVecPositions[iTargetInRange]) <= kfBlasterTargetRange)
		{
			rFlags.Set(kFireBlaster);
		}
	}
}

void XM_CALLCONV PlayersPostRender::UpdateFacing(FXMVECTOR vecPosition, FXMVECTOR vecLookPosition, XMVECTOR& rVecWantedDirection)
{
	// The reticle uses this unsmoothed lead-intercept direction; PlayersInterpolate::Update smooths hull rotation with kfRotateTowardsSpeed.
	rVecWantedDirection = XMVector3Normalize(XMVectorSubtract(vecLookPosition, vecPosition));
}

void PlayersPostRender::RegenerateShield(std::chrono::duration<float> deltaTime, std::chrono::duration<float> shieldCooldown, float& rfShield)
{
	if (shieldCooldown.count() <= 0.0f)
	{
		rfShield = std::min(rfShield + deltaTime.count() * kfPlayerShieldRegeneration, kfPlayerShield);
	}
}


static void XM_CALLCONV ApplyDamage([[maybe_unused]] const Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, [[maybe_unused]] PlayersInterpolate& rPlayerInterpolate, PlayersPostRender& rPlayer, int64_t i, float fDamage, [[maybe_unused]] FXMVECTOR vecDamagePosition, [[maybe_unused]] float fHexShieldIntensity = 1.0f)
{
	if (rPlayer.pfShields[i] > 0.0f)
	{
#if defined(BT_CLIENT)
		engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioShieldArmor465540__steaq__scifishieldhitwavwavCrc, emitterCoordinate, vecDamagePosition, gShieldHitVolumeBase.mfCurrent + gShieldHitVolumeScale.mfCurrent * (1.0f - rPlayer.pfShields[i] / kfPlayerShield));
#endif

#if defined(BT_CLIENT)
		int64_t iLowestIntensityIndex = 0;
		for (int64_t k = 1; k < shaders::kiHexShieldDirections; ++k)
		{
			if (rPlayerInterpolate.pHexShieldFragmentIntensities[i].data[k] < rPlayerInterpolate.pHexShieldFragmentIntensities[i].data[iLowestIntensityIndex])
			{
				iLowestIntensityIndex = k;
			}
		}
		XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorSubtract(vecDamagePosition, rPlayerInterpolate.pVecPositions[i]));
		XMStoreFloat4A(&rPlayerInterpolate.pHexShieldDirections[i].data[iLowestIntensityIndex], vecDamageDirection);
		rPlayerInterpolate.pHexShieldVertexIntensities[i].data[iLowestIntensityIndex] = fHexShieldIntensity;
		rPlayerInterpolate.pHexShieldFragmentIntensities[i].data[iLowestIntensityIndex] = fHexShieldIntensity;
#endif // BT_CLIENT

		float fShieldDamage = std::min(rPlayer.pfShields[i], fDamage);
		rPlayer.pfShields[i] -= fShieldDamage;
		fDamage -= fShieldDamage;

		if (rPlayer.pfShields[i] <= 0.0f)
		{
			rPlayer.pfShieldCooldowns[i] = kShieldCooldown.count();

			// Play shield down sound with cooldown to prevent spam
			if (rPlayer.pfShieldDownSoundCooldowns[i] <= 0.0f)
			{
				rPlayer.pfShieldDownSoundCooldowns[i] = kShieldDownSoundCooldown.count();
#if defined(BT_CLIENT)
				engine::gpAudioManager->PlayOneShot(rFrame, data::kAudioShieldArmor570852__rafaelzimrp__magicshielddownwavCrc, false, gShieldDownVolume.mfCurrent);
#endif
			}
		}
	}

	if (fDamage > 0.0f)
	{
#if defined(BT_CLIENT)
		if (fDamage > kfArmorHitSoundDamageThreshold)
		{
			engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioShieldArmor330629__stormwaveaudio__scififorcefieldimpact15wavCrc, emitterCoordinate, vecDamagePosition, gArmorHitVolumeBase.mfCurrent + gArmorHitVolumeScale.mfCurrent * (1.0f - rPlayer.pfArmors[i] / kfPlayerArmor));
		}
#endif

		if constexpr (!kbInvincibility)
		{
			rPlayer.pfArmors[i] -= fDamage;
		}
	}
}

void PlayersPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	engine::CellBounds bounds = engine::ComputeCellBounds(engine::LocalCellArea());

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pFlags[i] & kExploding)
		{
			continue;
		}

		if ((engine::Collision::sResultSpans[engine::Collision::sLayerBaseOffsets[siCollisionLayerIndex] + i].iCount > 0))
		{
			std::span<const engine::CollisionResult> collisions = engine::Collision::GetCollisions(siCollisionLayerIndex, i);
			for (const engine::CollisionResult& rResult : collisions)
			{
				if (rResult.uiOtherCategory == CollisionCategory::kuiSpaceship)
				{
					ApplyDamage(rFrame, rStaticData.coordinate, rCurrentInterpolate, rCurrentPostRender, i, kfSpaceshipCollisionDamage, rResult.vecContactPoint);
				}
				else if (rResult.uiOtherCategory == CollisionCategory::kuiBlaster)
				{
					ApplyDamage(rFrame, rStaticData.coordinate, rCurrentInterpolate, rCurrentPostRender, i, rResult.fDamageReceived, rResult.vecContactPoint);

#if defined(BT_CLIENT)
					engine::PuffsPostRender::AddControlled(rFrame, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime), PlayersInterpolate::suiImpactPuffControllerTypeIndex, rResult.vecContactPoint);
					engine::PointLightsPostRender::AddControlled(rFrame, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime), PlayersInterpolate::suiImpactPointLightControllerTypeIndex, rResult.vecContactPoint, 0.0f);
#endif
				}
			}
		}

		if (rCurrentPostRender.pfArmors[i] <= 0.0f)
		{
			rCurrentPostRender.pFlags[i].Set(kExploding);
			rCurrentInterpolate.pfDestroyedTimes[i] = kfDestroyTime;
			rCurrentPostRender.pfDestroyedExplosionTimes[i] = kfDestroyExplosionInterval;
		}
		else if (engine::IsOutOfBounds(bounds, rCurrentInterpolate.pVecPositions[i])) [[unlikely]]
		{
			// Entity candidates at or beyond cell exit were filtered during PreCollision.
			rCurrentPostRender.pFlags[i].Set(kTransfer);
		}
	}
}


void PlayersPostRender::SpawnBlasters(Frame& __restrict rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (!(rCurrentPostRender.pFlags[i] & kFireBlaster))
		{
			continue;
		}
		rCurrentPostRender.pFlags[i].Set(kFireBlaster, false);

		if (rCurrentPostRender.pFlags[i] & kUseMissiles)
		{
			continue;
		}

		// Dying players stop firing blasters through the death-explosion animation (mirrors SpawnMissiles gate)
		if (rCurrentPostRender.pFlags[i] & kExploding)
		{
			continue;
		}

		XMVECTOR vecBaseDirection = rCurrentInterpolate.pVecDirections[i];
		XMVECTOR vecLeftNormal = XMVector3Normalize(XMVector3Cross(vecBaseDirection, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)));

		ASSERT(IsBlasterFireTimeInRange(rCurrentPostRender.pfNextBlasterFireTimes[i]));
		rCurrentPostRender.pfNextBlasterFireTimes[i] -= deltaTime.count();

		while (rCurrentPostRender.pfNextBlasterFireTimes[i] <= 0.0f)
		{
			// Timer went negative by this amount when it crossed zero inside this tick, which equals
			// the elapsed time from the fire moment to the end of the tick (kfDeltaTime - fire_offset).
			std::chrono::duration<float> interFrameTime(-rCurrentPostRender.pfNextBlasterFireTimes[i]);

			// Rewind player to the fire-moment position (end-of-tick position minus elapsed time times velocity).
			XMVECTOR vecPlayerPositionAtSpawn = XMVectorSubtract(rCurrentInterpolate.pVecPositions[i], XMVectorScale(rCurrentPostRender.pVecVelocities[i], interFrameTime.count()));

			rCurrentPostRender.pFlags[i].Toggle(kBlasterSpawnLeft);
			float fBarrelOffset = (rCurrentPostRender.pFlags[i] & kBlasterSpawnLeft) ? kfBlastersSpawnBarrelOffset : -kfBlastersSpawnBarrelOffset;

			XMVECTOR vecJitteredDirection = common::RandomAngleJitter(vecBaseDirection, kfBlasterAngleJitter, rFrame.postRender.randomEngine);
			XMVECTOR vecBlasterVelocity = XMVectorScale(vecJitteredDirection, kfPlayerBlastersSpeed);

			XMVECTOR vecSpawnPosition = XMVectorAdd(XMVectorAdd(vecPlayerPositionAtSpawn, XMVectorScale(vecLeftNormal, fBarrelOffset)), XMVectorScale(vecJitteredDirection, kfBlastersSpawnPreMove));

			// Advance by the shot's age to store its position at the frame's reference time; sub-tick shots remain evenly spaced.
			XMVECTOR vecFinalPosition = XMVectorAdd(vecSpawnPosition, XMVectorScale(vecBlasterVelocity, interFrameTime.count()));

			[[maybe_unused]] bool bSpawned = BlastersPostRender::Spawn(rFrame,
			{
				.vecPosition = vecFinalPosition,
				.vecVelocity = vecBlasterVelocity,
				.iTypeIndex = PlayersInterpolate::siBlasterTypeIndex,
				.alignment = rCurrentPostRender.pAlignments[i],
				.fWindTrailIntensity = game::gWindDepositPlayerBlastersIntensity.mfCurrent,
				.fWindTrailWidth = game::gWindDepositPlayerBlastersWidth.mfCurrent,
				.fWindTrailLengthMultiplier = game::gWindDepositPlayerBlastersLengthMultiplier.mfCurrent,
			});

#if defined(BT_CLIENT)
			if (bSpawned)
			{
				engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster514039__newlocknew__blastershot6sytrusrsmplmultiprcsngsinglewavCrc, emitterCoordinate, vecFinalPosition, gPlayerBlasterVolume.mfCurrent, gPlayerBlasterPitchMinimum.mfCurrent, gPlayerBlasterPitchRandom.mfCurrent);
			}
#endif

			rCurrentPostRender.pfNextBlasterFireTimes[i] += kfBlasterFireInterval;
		}
	}
}

void PlayersPostRender::SpawnMissiles([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	// Built once for the whole spawn loop, after Update, Transfer, and Destroy have settled this tick's spaceship
	// rows and every current missile handle. Missiles spawned below may grow their collection, which the context
	// tolerates because its subscription binding was already consumed. Previous rows no longer align after
	// Transfer and Destroy, so no previous positions are bound; new missiles do not home until the next tick.
	RegistryWindow window = BuildSpaceshipRegistryWindow(rFrame, nullptr, rFrame.postRender.pSpaceships->pfArrivalGracePeriods, *rFrame.postRender.pMissiles);

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		// Always decrement timer so releasing and re-pressing fires immediately after cooldown
		rCurrentPostRender.pfNextSecondarySpawnTimes[i] -= deltaTime.count();

		if (!(rCurrentPostRender.pFlags[i] & kFireMissile))
		{
			continue;
		}
		rCurrentPostRender.pFlags[i].Set(kFireMissile, false);

		if (!(rCurrentPostRender.pFlags[i] & kUseMissiles))
		{
			continue;
		}

		if (rCurrentPostRender.pfNextSecondarySpawnTimes[i] >= 0.0f)
		{
			continue;
		}

		if (rCurrentPostRender.pFlags[i] & kExploding)
		{
			continue;
		}

		rCurrentPostRender.pfNextSecondarySpawnTimes[i] = kMissileSpawnInterval.count();

		rCurrentPostRender.pFlags[i].Toggle(kMissileSpawnLeft);
		bool bLeftSide = rCurrentPostRender.pFlags[i] & kMissileSpawnLeft;

		// Hull direction drives the barrel-offset normal: missiles spawn from the visible left/right
		// barrel positions on the ship even when the hull is rotated toward the lead point.
		XMVECTOR vecHullDirection = rCurrentInterpolate.pVecDirections[i];
		XMVECTOR vecLeftNormal = XMVector3Normalize(XMVector3Cross(vecHullDirection, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)));
		float fBarrelOffset = bLeftSide ? kfMissileSpawnBarrelOffset : -kfMissileSpawnBarrelOffset;

		// Missiles aim at current positions; hull direction is the fallback when no eligible spaceship is in range.
		int64_t iTargetSpaceship = FindTargetSpaceshipIndex(*rFrame.interpolate.pSpaceships, *rFrame.postRender.pSpaceships, rCurrentInterpolate.pVecPositions[i], kfMissileTargetRange);
		XMVECTOR vecAimDirection = (iTargetSpaceship >= 0)
			? common::DirectionTo(rCurrentInterpolate.pVecPositions[i], rFrame.interpolate.pSpaceships->pVecPositions[iTargetSpaceship])
			: vecHullDirection;

		float fAngleOffset = bLeftSide ? -kfMissileSpawnAngle : kfMissileSpawnAngle;
		XMVECTOR vecAngledDirection = XMVector3TransformNormal(vecAimDirection, XMMatrixRotationZ(fAngleOffset));
		XMVECTOR vecJitteredDirection = common::RandomAngleJitter(vecAngledDirection, kfMissileAngleJitter, rFrame.postRender.randomEngine);

		XMVECTOR vecSpawnPosition = XMVectorAdd(rCurrentInterpolate.pVecPositions[i], XMVectorScale(vecLeftNormal, fBarrelOffset));
		XMVECTOR vecMissilePosition = XMVectorAdd(vecSpawnPosition, XMVectorScale(vecJitteredDirection, kfMissileSpawnPreMove));
		XMVECTOR vecMissileVelocity = XMVectorScale(vecJitteredDirection, kfMissileInitialVelocity);

		// Refuse before acquisition: a subscription taken for a row that is never appended is never released.
		if (!common::InsideArea(vecMissilePosition, engine::LocalCellArea()))
		{
			continue;
		}

		// The one-row batch acquires the next-tick homing target from the missile spawn position, aim direction, and player alignment.
		engine::registry_id_t registryTarget {};
		int64_t iAcquireRow = 0;
		engine::RegistryResult acquireResult {};
		engine::AcquireRegistryTargets(window.context,
		{
			.pTargets = &registryTarget,
			.pVecOrigins = &vecMissilePosition,
			.pVecDirections = &vecAimDirection,
			.pAlignments = &rCurrentPostRender.pAlignments[i],
			.rows = std::span<const int64_t>(&iAcquireRow, 1),
			.results = std::span<engine::RegistryResult>(&acquireResult, 1),
			.iSourceCount = 1,
		}, kfMissileTargetAcquireRange);

		MissilesPostRender::Spawn(rFrame,
		{
			.vecPosition = vecMissilePosition,
			.vecDirection = vecJitteredDirection,
			.vecVelocity = vecMissileVelocity,
			.vecStoredDirection = vecAimDirection,
			.uiTarget = registryTarget,
			.fAcceleration = kfMissileAcceleration,
			.flags = MissileFlags::kTargetEnemy,
			.alignment = rCurrentPostRender.pAlignments[i],
		});

#if defined(BT_CLIENT)
		engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioMissile182794__qubodup__rocketlaunch_start_2wavCrc, emitterCoordinate, vecMissilePosition, gMissileLaunchVolume.mfCurrent, gMissilePitchMin.mfCurrent, gMissilePitchRandom.mfCurrent);
#endif
	}
}

void PlayersPostRender::SpawnDeathExplosions([[maybe_unused]] Frame& __restrict rFrame, engine::GridCoord emitterCoordinate)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (!((rCurrentPostRender.pFlags[i] & kExploding) && rCurrentPostRender.pfDestroyedExplosionTimes[i] <= 0.0f && rCurrentInterpolate.pfDestroyedTimes[i] > 0.0f))
		{
			continue;
		}

		rCurrentPostRender.pfDestroyedExplosionTimes[i] = kfDestroyExplosionInterval;

		float fPercent = rCurrentInterpolate.pfDestroyedTimes[i] / kfDestroyTime;

		XMVECTOR vecDirection = XMVector4Transform(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(common::Random<XM_2PI>(rFrame.postRender.randomEngine)));

		XMVECTOR vecJitteredPosition = common::RandomPositionJitter<1.0f>(rCurrentInterpolate.pVecPositions[i], rFrame.postRender.randomEngine);
		XMVECTOR vecJitteredDirection = common::RandomDirectionJitter<0.5f>(vecDirection, rFrame.postRender.randomEngine);

		float fAdjustedPercent = (std::pow((1.0f - fPercent) + 1.0f, kfDeathRadialPower) - 1.0f) * kfExplosionsRadius;
		vecJitteredPosition = XMVectorMultiplyAdd(vecJitteredDirection, XMVectorReplicate(fAdjustedPercent), vecJitteredPosition);

		engine::ExplosionsPostRender::Spawn(rFrame, emitterCoordinate, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime),
		{
			.iTypeIndex = PlayersInterpolate::siExplosionTypeIndex,
			.vecPosition = vecJitteredPosition,
			.vecDirection = vecJitteredDirection,
			.flags = {engine::ExplosionFlags::kDestroysSelf, engine::ExplosionFlags::kYellow},
			.iTrailCount = kiDeathTrailCount,
			.fTrailAngle = fPercent * XM_PIDIV2,
			.uiParticleCount = static_cast<uint32_t>(fPercent * kfExplosionParticleCount),
			.fParticleAngle = fPercent * XM_PIDIV2,
			.fSizePercent = fPercent * kfExplosionSizeStart + (1.0f - fPercent) * kfExplosionSizeEnd,
			.fSmokePercent = fPercent * kfExplosionSmoke,
			.fTimePercent = fPercent,
		});
	}
}

} // namespace game
