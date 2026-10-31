#include "Omnis.h"

#include "Data/Audio.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"

#if defined(BT_CLIENT)
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Ui/SoundWrappers.h"
#endif

namespace game
{

constexpr std::chrono::duration<float> kShieldCooldown(2.0f);
constexpr std::chrono::duration<float> kShieldDownSoundCooldown(2.0f);
constexpr float kfArmorHitSoundDamageThreshold = 3.0f;

constexpr float kfBlastersSpawnBarrelOffset = kfOmniRadius * 0.4667f;
constexpr float kfBlastersSpawnPreMove = 0.0f;
constexpr float kfBlasterAngleJitter = 0.03f;

constexpr float kfExplosionsRadius = kfOmniRadius * 20.0f;
constexpr float kfExplosionParticleCount = 16.0f;
constexpr float kfExplosionSizeStart = kfOmniRadius * 1.3333f;
constexpr float kfExplosionSizeEnd = kfOmniRadius * 0.3333f;
constexpr float kfExplosionSmoke = 0.25f;
constexpr float kfDeathRadialPower = 0.3f;
constexpr int64_t kiDeathTrailCount = 2;

void XM_CALLCONV OmnisPostRender::UpdateFacing(FXMVECTOR vecPosition, FXMVECTOR vecLookPosition, XMVECTOR& rVecWantedDirection)
{
	// The reticle uses this unsmoothed lead-intercept direction; OmnisInterpolate::Update smooths hull rotation with kfRotateTowardsSpeed.
	rVecWantedDirection = XMVector3Normalize(XMVectorSubtract(vecLookPosition, vecPosition));
}


static void XM_CALLCONV ApplyDamage([[maybe_unused]] const Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, [[maybe_unused]] OmnisInterpolate& rOmniInterpolate, OmnisPostRender& rOmni, int64_t i, float fDamage, [[maybe_unused]] FXMVECTOR vecDamagePosition)
{
	if (rOmni.pfShields[i] > 0.0f)
	{
#if defined(BT_CLIENT)
		engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioShieldArmor465540__steaq__scifishieldhitwavwavCrc, emitterCoordinate, vecDamagePosition, gShieldHitVolumeBase.mfCurrent + gShieldHitVolumeScale.mfCurrent * (1.0f - rOmni.pfShields[i] / kfOmniShield));
#endif

#if defined(BT_CLIENT)
		int64_t iLowestIntensityIndex = 0;
		for (int64_t k = 1; k < shaders::kiHexShieldDirections; ++k)
		{
			if (rOmniInterpolate.pHexShieldFragmentIntensities[i].pfData[k] < rOmniInterpolate.pHexShieldFragmentIntensities[i].pfData[iLowestIntensityIndex])
			{
				iLowestIntensityIndex = k;
			}
		}
		XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorSubtract(vecDamagePosition, rOmniInterpolate.pVecPositions[i]));
		XMStoreFloat4A(&rOmniInterpolate.pHexShieldDirections[i].pf4Data[iLowestIntensityIndex], vecDamageDirection);
		rOmniInterpolate.pHexShieldVertexIntensities[i].pfData[iLowestIntensityIndex] = 1.0f;
		rOmniInterpolate.pHexShieldFragmentIntensities[i].pfData[iLowestIntensityIndex] = 1.0f;
#endif // BT_CLIENT

		float fShieldDamage = std::min(rOmni.pfShields[i], fDamage);
		rOmni.pfShields[i] -= fShieldDamage;
		fDamage -= fShieldDamage;

		if (rOmni.pfShields[i] <= 0.0f)
		{
			rOmni.pShieldCooldowns[i] = kShieldCooldown;

			// Play shield down sound with cooldown to prevent spam
			if (rOmni.pShieldDownSoundCooldowns[i].count() <= 0.0f)
			{
				rOmni.pShieldDownSoundCooldowns[i] = kShieldDownSoundCooldown;
#if defined(BT_CLIENT)
				engine::gpAudioManager->PlayOneShot(rFrame, data::kAudioShieldArmor570852__rafaelzimrp__magicshielddownwavCrc, gShieldDownVolume.mfCurrent);
#endif
			}
		}
	}

	if (fDamage > 0.0f)
	{
#if defined(BT_CLIENT)
		if (fDamage > kfArmorHitSoundDamageThreshold)
		{
			engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioShieldArmor330629__stormwaveaudio__scififorcefieldimpact15wavCrc, emitterCoordinate, vecDamagePosition, gArmorHitVolumeBase.mfCurrent + gArmorHitVolumeScale.mfCurrent * (1.0f - rOmni.pfArmors[i] / kfOmniArmor));
		}
#endif

		rOmni.pfArmors[i] -= fDamage;
	}
}

static void BeginExplosion(Frame& rFrame, OmnisInterpolate& rCurrentInterpolate, OmnisPostRender& rCurrentPostRender, int64_t i)
{
	rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDying);
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.pPushers[i]);
	rCurrentInterpolate.pDestroyedTimes[i] = kDestroyTime;
	rCurrentPostRender.pDestroyedExplosionTimes[i] = kDestroyExplosionInterval;

	// Clear the registry id so missiles stop tracking
	rCurrentInterpolate.pRegistryIds[i] = {};
}

// At zero armor a random-spawn omni begins its explosion. A fleet omni never dies: its armor holds at 0 and it heals.
static void HandleArmorDepleted(Frame& rFrame, OmnisInterpolate& rCurrentInterpolate, OmnisPostRender& rCurrentPostRender, int64_t i)
{
	if (rCurrentPostRender.pController[i] == ShipController::kFleet)
	{
		rCurrentPostRender.pfArmors[i] = 0.0f;
		rCurrentPostRender.pFlags[i].Set(ShipFlags::kHealing);
		return;
	}
	BeginExplosion(rFrame, rCurrentInterpolate, rCurrentPostRender, i);
}

void OmnisPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			continue;
		}

		// Missile damage arrives through the area-damage phase, so a missile contact applies none here.
		for (const engine::CollisionResult& rResult : engine::Collision::GetCollisions(siCollisionLayerIndex, i))
		{
			if (rResult.uiOtherCategory == CollisionCategory::kuiBlaster)
			{
				ApplyDamage(rFrame, rStaticData.coordinate, rCurrentInterpolate, rCurrentPostRender, i, rResult.fDamageReceived, rResult.vecContactPoint);

#if defined(BT_CLIENT)
				engine::PuffsPostRender::AddControlled(rFrame, engine::TickToSimulationTime(rFrame.interpolate.iTick), OmnisInterpolate::siImpactPuffControllerTypeIndex, rResult.vecContactPoint);
				engine::PointLightsPostRender::AddControlled(rFrame, engine::TickToSimulationTime(rFrame.interpolate.iTick), OmnisInterpolate::siImpactPointLightControllerTypeIndex, rResult.vecContactPoint, 0.0f);
#endif
			}
		}

		if (rCurrentPostRender.pfArmors[i] <= 0.0f)
		{
			HandleArmorDepleted(rFrame, rCurrentInterpolate, rCurrentPostRender, i);
		}
		if (!(rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && !engine::IsInsideCell(rCurrentInterpolate.pVecPositions[i])) [[unlikely]]
		{
			// Entity candidates at or beyond cell exit were filtered during PreCollision.
			rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kTransfer);
		}
	}
}

void XM_CALLCONV OmnisPostRender::ApplyBeamDamage(Frame& rFrame, engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecShooterPosition, float fDamage)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	ApplyDamage(rFrame, emitterCoordinate, rCurrentInterpolate, rCurrentPostRender, i, fDamage, vecShooterPosition);

	if (rCurrentPostRender.pfArmors[i] <= 0.0f)
	{
		HandleArmorDepleted(rFrame, rCurrentInterpolate, rCurrentPostRender, i);
	}
}

void OmnisPostRender::AreaDamage([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			continue;
		}

		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kTransfer)
		{
			continue;
		}

		XMVECTOR vecClosestSource {};
		float fDamage = engine::AreaDamage::Get(rCurrentInterpolate.pVecPositions[i], OpposingBlastCategory(rCurrentPostRender.pAlignments[i], rFrame.postRender.fleetAlignment), vecClosestSource);

		if (fDamage <= 0.0f)
		{
			continue;
		}

		ApplyDamage(rFrame, rStaticData.coordinate, rCurrentInterpolate, rCurrentPostRender, i, fDamage, vecClosestSource);

		if (rCurrentPostRender.pfArmors[i] <= 0.0f)
		{
			HandleArmorDepleted(rFrame, rCurrentInterpolate, rCurrentPostRender, i);
		}
	}
}


void OmnisPostRender::SpawnBlasters(Frame& __restrict rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (!(rCurrentPostRender.pFlags[i] & ShipFlags::kFireBlaster))
		{
			continue;
		}
		rCurrentPostRender.pFlags[i].Set(ShipFlags::kFireBlaster, false);

		// Dying omnis stop firing through the death-explosion animation, and healing omnis until fully healed.
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			continue;
		}

		if (rCurrentPostRender.pFlags[i] & ShipFlags::kHealing)
		{
			continue;
		}

		XMVECTOR vecBaseDirection = rCurrentInterpolate.pVecDirections[i];
		XMVECTOR vecLeftNormal = XMVector3Normalize(XMVector3Cross(vecBaseDirection, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)));

		ASSERT(IsBlasterFireTimeInRange(rCurrentPostRender.pNextBlasterFireTimes[i].count()));
		rCurrentPostRender.pNextBlasterFireTimes[i] -= deltaTime;

		while (rCurrentPostRender.pNextBlasterFireTimes[i].count() <= 0.0f)
		{
			// Timer went negative by this amount when it crossed zero inside this tick, which equals
			// the elapsed time from the fire moment to the end of the tick (kfDeltaTime - fire_offset).
			std::chrono::duration<float> interFrameTime = -rCurrentPostRender.pNextBlasterFireTimes[i];

			// Rewind omni to the fire-moment position (end-of-tick position minus elapsed time times velocity).
			XMVECTOR vecOmniPositionAtSpawn = XMVectorSubtract(rCurrentInterpolate.pVecPositions[i], XMVectorScale(rCurrentPostRender.pVecVelocities[i], interFrameTime.count()));

			rCurrentPostRender.pFlags[i].Toggle(ShipFlags::kBlasterSpawnLeft);
			float fBarrelOffset = (rCurrentPostRender.pFlags[i] & ShipFlags::kBlasterSpawnLeft) ? kfBlastersSpawnBarrelOffset : -kfBlastersSpawnBarrelOffset;

			XMVECTOR vecFireDirection = common::RandomAngleJitter(vecBaseDirection, kfBlasterAngleJitter, rFrame.postRender.randomEngine);
			XMVECTOR vecBlasterVelocity = XMVectorScale(vecFireDirection, kfOmniBlastersSpeed);

			XMVECTOR vecSpawnPosition = XMVectorAdd(XMVectorAdd(vecOmniPositionAtSpawn, XMVectorScale(vecLeftNormal, fBarrelOffset)), XMVectorScale(vecFireDirection, kfBlastersSpawnPreMove));

			// Advance by the shot's age to store its position at the frame's reference time; sub-tick shots remain evenly spaced.
			XMVECTOR vecFinalPosition = XMVectorAdd(vecSpawnPosition, XMVectorScale(vecBlasterVelocity, interFrameTime.count()));

			[[maybe_unused]] bool bSpawned = BlastersPostRender::Spawn(rFrame,
			{
				.vecPosition = vecFinalPosition,
				.vecVelocity = vecBlasterVelocity,
				.iTypeIndex = OmnisInterpolate::siBlasterTypeIndex,
				.alignment = rCurrentPostRender.pAlignments[i],
			});

#if defined(BT_CLIENT)
			if (bSpawned)
			{
				engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster514039__newlocknew__blastershot6sytrusrsmplmultiprcsngsinglewavCrc, emitterCoordinate, vecFinalPosition, gOmniBlasterVolume.mfCurrent, gOmniBlasterPitchMinimum.mfCurrent, gOmniBlasterPitchRandom.mfCurrent);
			}
#endif

			rCurrentPostRender.pNextBlasterFireTimes[i] += kBlasterFireInterval;
		}
	}
}

void OmnisPostRender::SpawnDeathExplosions([[maybe_unused]] Frame& __restrict rFrame, engine::GridCoord emitterCoordinate)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (!((rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && rCurrentPostRender.pDestroyedExplosionTimes[i].count() <= 0.0f && rCurrentInterpolate.pDestroyedTimes[i] > std::chrono::duration<float>::zero()))
		{
			continue;
		}

		rCurrentPostRender.pDestroyedExplosionTimes[i] = kDestroyExplosionInterval;

		float fPercent = rCurrentInterpolate.pDestroyedTimes[i] / kDestroyTime;

		XMVECTOR vecDirection = XMVector4Transform(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(common::Random<XM_2PI>(rFrame.postRender.randomEngine)));

		XMVECTOR vecJitteredPosition = common::RandomPositionJitter<1.0f>(rCurrentInterpolate.pVecPositions[i], rFrame.postRender.randomEngine);
		XMVECTOR vecJitteredDirection = common::RandomDirectionJitter<0.5f>(vecDirection, rFrame.postRender.randomEngine);

		float fAdjustedPercent = (std::pow((1.0f - fPercent) + 1.0f, kfDeathRadialPower) - 1.0f) * kfExplosionsRadius;
		vecJitteredPosition = XMVectorMultiplyAdd(vecJitteredDirection, XMVectorReplicate(fAdjustedPercent), vecJitteredPosition);

		engine::ExplosionsPostRender::Spawn(rFrame, emitterCoordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick),
		{
			.iTypeIndex = OmnisInterpolate::siExplosionTypeIndex,
			.vecPosition = vecJitteredPosition,
			.vecDirection = vecJitteredDirection,
			.flags = {engine::ExplosionFlags::kYellow},
			.lifecycleFlags = {engine::LifecycleFlags::kDying},
			.iTrailCount = kiDeathTrailCount,
			.fTrailAngle = fPercent * XM_PIDIV2,
			.iParticleCount = static_cast<int64_t>(fPercent * kfExplosionParticleCount),
			.fParticleAngle = fPercent * XM_PIDIV2,
			.fSizePercent = fPercent * kfExplosionSizeStart + (1.0f - fPercent) * kfExplosionSizeEnd,
			.fSmokePercent = fPercent * kfExplosionSmoke,
			.fTimePercent = fPercent,
		});
	}
}

} // namespace game
