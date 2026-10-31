#include "Fighters.h"

#include "Data/Audio.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"

#if defined(BT_CLIENT)
#include "Frame/Collections/PointLights/PointLights.h"
#include "Ui/SoundWrappers.h"
#endif

namespace game
{

// PreCollision sets the collision layer index when fighters are present.
// thread_local: parallel per-Frame tick via Dispatch
static thread_local int64_t siCollisionLayerIndex = 0;

// The most the heading may differ from the horizontal offset to a target for the beam to fire.
constexpr float kfFighterBeamAimAngle = 0.1f;

// Shared type indices (defined in Fighters.cpp, set during Register())
extern int64_t giFighterExplosionTypeIndex;
#if defined(BT_CLIENT)
extern int64_t giFighterHitFlashControllerTypeIndex;
extern int64_t giFighterBeamAreaLightTypeIndex;
#endif

// Defined in Fighters.cpp.
void SpawnFighterExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent);
#if defined(BT_CLIENT)
void XM_CALLCONV SyncFighterBeam(FrameInterpolate& rFrameInterpolate, engine::area_lights_t uiAreaLight, int64_t iTypeIndex, FXMVECTOR vecStart, FXMVECTOR vecEnd, float fIntensityMultiplier);
#endif

static void XM_CALLCONV BeginExplosion(Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecDamageDirection)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDying);
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.pPushers[i]);
	rCurrentInterpolate.pDestroyedTimes[i] = kFighterDestroyTime;
	rCurrentPostRender.pDestroyedExplosionTimes[i] = kFighterDestroyExplosionInterval;

	// Store damage direction for knockback
	rCurrentPostRender.pVecDamageDirections[i] = vecDamageDirection;

	// Clear the registry id so missiles stop tracking
	rCurrentInterpolate.pRegistryIds[i] = {};

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioExplosions80401__steveygos93__explosion2wavCrc, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], gFighterDeathVolume.mfCurrent, gFighterDeathPitchMinimum.mfCurrent, gFighterDeathPitchRandom.mfCurrent);
#endif

	XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
	SpawnFighterExplosion(rFrame, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], vecDirection, 1.0f);
}

// Hits the nearest enemy ship whose nearest flight plane is the shooter's stored plane, that is inside the cell, within
// beam range, and lined up with the heading. Only a strictly nearer row replaces the best, so an exact distance tie keeps
// the first row in ForEachShipType order. Returns false when no ship qualifies.
static bool FireBeam(Frame& rFrame, const engine::CellStaticData& rStaticData, int64_t i, uint8_t uiFlightPlane)
{
	static constexpr float kfBeamRangeSquared = kfFighterBeamRange * kfFighterBeamRange;

	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	XMVECTOR vecShooterPosition = rCurrentInterpolate.pVecPositions[i];
	XMVECTOR vecShooterDirection = rCurrentInterpolate.pVecDirections[i];
	engine::AlignmentIdentifier alignment = rFrame.postRender.pFighters->pAlignments[i];

	std::optional<ShipLocator> optionalTarget;
	float fNearestDistanceSquared = 0.0f;
	XMVECTOR vecEnd = XMVectorZero();
	ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		if (NearestFlightPlane(vecPosition) != uiFlightPlane)
		{
			return;
		}
		// A row outside the cell is leaving it this tick, whether or not its own PostCollision has flagged it yet.
		if (!engine::IsInsideCell(vecPosition))
		{
			return;
		}
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, vecShooterPosition)));
		if (fDistanceSquared > kfBeamRangeSquared)
		{
			return;
		}
		// Horizontal offset: the beam fires along the flat heading, so only the heading-plane angle decides a hit.
		XMVECTOR vecToEnemyNormal = XMVector3Normalize(XMVectorSetZ(XMVectorSubtract(vecPosition, vecShooterPosition), 0.0f));
		float fAngleToEnemy = XMVectorGetX(XMVector3AngleBetweenNormals(vecShooterDirection, vecToEnemyNormal));
		if (!(fAngleToEnemy <= kfFighterBeamAimAngle))
		{
			return;
		}
		if (!optionalTarget || fDistanceSquared < fNearestDistanceSquared)
		{
			optionalTarget = locator;
			fNearestDistanceSquared = fDistanceSquared;
			vecEnd = vecPosition;
		}
	});

	if (!optionalTarget)
	{
		return false;
	}

	VisitShip(rFrame, *optionalTarget, [&]([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
	{
		std::remove_cvref_t<decltype(rPostRender)>::ApplyBeamDamage(rFrame, rStaticData.coordinate, iRow, vecShooterPosition, kfFighterBeamDamage);
	});

#if defined(BT_CLIENT)
	engine::AreaLightsPostRender::Add(rFrame, rCurrentInterpolate.pBeamAreaLights[i], giFighterBeamAreaLightTypeIndex);
	rCurrentInterpolate.pVecBeamEnds[i] = vecEnd;
	SyncFighterBeam(rFrame.interpolate, rCurrentInterpolate.pBeamAreaLights[i], giFighterBeamAreaLightTypeIndex, vecShooterPosition, vecEnd, 1.0f);
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster514039__newlocknew__blastershot6sytrusrsmplmultiprcsngsinglewavCrc, rStaticData.coordinate, vecShooterPosition, gFighterBlasterVolume.mfCurrent, gFighterBlasterPitchMinimum.mfCurrent, gFighterBlasterPitchRandom.mfCurrent);
	engine::PointLightsPostRender::AddControlled(rFrame, engine::TickToSimulationTime(rFrame.interpolate.iTick), giFighterHitFlashControllerTypeIndex, vecEnd, 0.0f);
#endif
	return true;
}

void FightersPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pFighters->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.fRadius = kfFighterRadius,
		.uiCategory = CollisionCategory::kuiFighter,
		.uiCollidesWith = CollidesWith::kuiFighter,
		.pAlignments = rCurrentPostRender.pAlignments,
	});

	std::span<engine::CollisionFlags_t> flags = engine::Collision::GetLayerFlags(siCollisionLayerIndex);
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		flags[static_cast<size_t>(i)] = (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {};
	}
}

void FightersPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) [[unlikely]]
		{
			continue;
		}

		// Check collision results - fighters take damage from blasters
		// Note: Missile damage is handled via area damage system in AreaDamage phase
		for (const engine::CollisionResult& rResult : engine::Collision::GetCollisions(siCollisionLayerIndex, i))
		{
			if (rResult.uiOtherCategory & CollisionCategory::kuiBlaster)
			{
				rCurrentPostRender.pfHealths[i] -= rResult.fDamageReceived;

#if defined(BT_CLIENT)
				engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc, rStaticData.coordinate, rCurrentInterpolate.pVecPositions[i], gFighterHitVolume.mfCurrent);
#endif

#if defined(BT_CLIENT)
				engine::PointLightsPostRender::AddControlled(rFrame, engine::TickToSimulationTime(rFrame.interpolate.iTick), giFighterHitFlashControllerTypeIndex, rResult.vecContactPoint, 0.0f);
#endif

				if (rCurrentPostRender.pfHealths[i] <= 0.0f)
				{
					XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorNegate(rResult.vecOtherVelocity));
					BeginExplosion(rFrame, rStaticData.coordinate, i, vecDamageDirection);
					break;
				}
			}
		}

		if (!(rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && !engine::IsInsideCell(rCurrentInterpolate.pVecPositions[i])) [[unlikely]]
		{
			// PreCollision's exclusive maximum-time cutoff excludes entity contacts at or beyond cell exit.
			rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kTransfer);
		}
	}

	// The beam fires here, after the collision results, in one sequential pass over the current frame: a death transition
	// completes before Destroy, and a later shooter sees an earlier shooter's kill.
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

		if (rCurrentPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		// A ship climbing between planes stores no plane and holds fire.
		uint8_t uiFlightPlane = rCurrentInterpolate.puiFlightPlanes[i];
		if (uiFlightPlane == kuiNoFlightPlane)
		{
			continue;
		}

		if (!(rCurrentPostRender.pNextBeamFireTimes[i] < std::chrono::duration<float>::zero()))
		{
			continue;
		}

		if (FireBeam(rFrame, rStaticData, i, uiFlightPlane))
		{
			rCurrentPostRender.pNextBeamFireTimes[i] = kFighterBeamCooldown;
		}
	}
}

void XM_CALLCONV FightersPostRender::ApplyBeamDamage(Frame& rFrame, engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecShooterPosition, float fDamage)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	rCurrentPostRender.pfHealths[i] -= fDamage;

	if (rCurrentPostRender.pfHealths[i] <= 0.0f)
	{
		XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorSubtract(vecShooterPosition, rCurrentInterpolate.pVecPositions[i]));
		BeginExplosion(rFrame, emitterCoordinate, i, vecDamageDirection);
	}
}

void FightersPostRender::AreaDamage([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

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

		rCurrentPostRender.pfHealths[i] -= fDamage;

		if (rCurrentPostRender.pfHealths[i] <= 0.0f)
		{
			XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorSubtract(vecClosestSource, rCurrentInterpolate.pVecPositions[i]));
			BeginExplosion(rFrame, rStaticData.coordinate, i, vecDamageDirection);
		}
	}
}

} // namespace game
