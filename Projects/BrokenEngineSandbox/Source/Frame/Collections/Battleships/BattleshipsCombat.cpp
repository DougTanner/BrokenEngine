#include "Battleships.h"

#include "Data/Audio.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"

#if defined(BT_CLIENT)
#include "Frame/Collections/PointLights/PointLights.h"
#include "Ui/SoundWrappers.h"
#endif

namespace game
{

// PreCollision sets the collision layer index when battleships are present.
// thread_local: parallel per-Frame tick via Dispatch
static thread_local int64_t siCollisionLayerIndex = 0;

// Shared type indices (defined in Battleships.cpp, set during Register())
extern int64_t giBattleshipExplosionTypeIndex;
#if defined(BT_CLIENT)
extern int64_t giBattleshipHitFlashControllerTypeIndex;
extern int64_t giBattleshipBeamAreaLightTypeIndex;
#endif

// Defined in Battleships.cpp.
void SpawnBattleshipExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent);
#if defined(BT_CLIENT)
void XM_CALLCONV SyncBattleshipBeam(FrameInterpolate& rFrameInterpolate, engine::area_lights_t uiAreaLight, FXMVECTOR vecStart, FXMVECTOR vecEnd, float fIntensityMultiplier);
#endif

static void XM_CALLCONV BeginExplosion(Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecDamageDirection)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDying);
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.pPushers[i]);
	rCurrentInterpolate.pDestroyedTimes[i] = kBattleshipDestroyTime;
	rCurrentPostRender.pDestroyedExplosionTimes[i] = kBattleshipDestroyExplosionInterval;

	// Store damage direction for knockback
	rCurrentPostRender.pVecDamageDirections[i] = vecDamageDirection;

	// Clear the registry id so missiles stop tracking
	rCurrentInterpolate.pRegistryIds[i] = {};

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioExplosions80401__steveygos93__explosion2wavCrc, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], gFighterDeathVolume.mfCurrent, gFighterDeathPitchMinimum.mfCurrent, gFighterDeathPitchRandom.mfCurrent);
#endif

	XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
	SpawnBattleshipExplosion(rFrame, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], vecDirection, 1.0f);
}

static void XM_CALLCONV ApplyBeamDamage(Frame& rFrame, engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecShooterPosition)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	rCurrentPostRender.pfHealths[i] -= kfBeamDamage;

	if (rCurrentPostRender.pfHealths[i] <= 0.0f)
	{
		// A fleet Battleship never dies: its health holds at 0.
		if (rCurrentPostRender.pController[i] == ShipController::kFleet)
		{
			rCurrentPostRender.pfHealths[i] = 0.0f;
			return;
		}
		XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorSubtract(vecShooterPosition, rCurrentInterpolate.pVecPositions[i]));
		BeginExplosion(rFrame, emitterCoordinate, i, vecDamageDirection);
	}
}

// Hits the enemy on flight plane 0 within beam range that comes first by type, Battleships, then Omnis, then Fighters,
// the nearest of that type, and only when no ship qualifies, the nearest flying missile. Rows arrive in ascending
// order and only a strictly nearer row replaces the best, so an exact distance tie keeps the lowest row. Returns false
// when nothing qualifies.
static bool FireBeam(Frame& rFrame, const engine::CellStaticData& rStaticData, int64_t i)
{
	static constexpr float kfBeamRangeSquared = kfBattleshipBeamRange * kfBattleshipBeamRange;
	static constexpr int64_t kiShipTypeCount = std::to_underlying(ShipType::kCount);

	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	XMVECTOR vecShooterPosition = rCurrentInterpolate.pVecPositions[i];
	engine::AlignmentIdentifier alignment = rFrame.postRender.pBattleships->pAlignments[i];

	// Indexed by ShipType; a row of -1 means no candidate of that type.
	std::array<int64_t, kiShipTypeCount> nearestRows {-1, -1, -1};
	std::array<float, kiShipTypeCount> nearestDistancesSquared {};
	std::array<XMVECTOR, kiShipTypeCount> nearestPositions {};
	ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecPosition, [[maybe_unused]] FXMVECTOR vecVelocity)
	{
		bool bTransfer = VisitShip(rFrame, locator, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
		{
			return rPostRender.pLifecycleFlags[iRow] & engine::LifecycleFlags::kTransfer;
		});
		if (bTransfer)
		{
			return;
		}
		if (NearestFlightPlane(vecPosition) != 0)
		{
			return;
		}
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, vecShooterPosition)));
		if (fDistanceSquared > kfBeamRangeSquared)
		{
			return;
		}
		int64_t iType = std::to_underlying(locator.eType);
		if (nearestRows[iType] < 0 || fDistanceSquared < nearestDistancesSquared[iType])
		{
			nearestRows[iType] = locator.iRow;
			nearestDistancesSquared[iType] = fDistanceSquared;
			nearestPositions[iType] = vecPosition;
		}
	});

	int64_t iBattleshipRow = nearestRows[std::to_underlying(ShipType::kBattleship)];
	int64_t iOmniRow = nearestRows[std::to_underlying(ShipType::kOmni)];
	int64_t iFighterRow = nearestRows[std::to_underlying(ShipType::kFighter)];
	XMVECTOR vecEnd = XMVectorZero();
	if (iBattleshipRow >= 0)
	{
		vecEnd = nearestPositions[std::to_underlying(ShipType::kBattleship)];
		ApplyBeamDamage(rFrame, rStaticData.coordinate, iBattleshipRow, vecShooterPosition);
	}
	else if (iOmniRow >= 0)
	{
		vecEnd = nearestPositions[std::to_underlying(ShipType::kOmni)];
		OmnisPostRender::ApplyBeamDamage(rFrame, rStaticData.coordinate, iOmniRow, vecShooterPosition);
	}
	else if (iFighterRow >= 0)
	{
		vecEnd = nearestPositions[std::to_underlying(ShipType::kFighter)];
		FightersPostRender::ApplyBeamDamage(rFrame, rStaticData.coordinate, iFighterRow, vecShooterPosition);
	}
	else
	{
		const MissilesInterpolate& rMissilesInterpolate = *rFrame.interpolate.pMissiles;
		const MissilesPostRender& rMissilesPostRender = *rFrame.postRender.pMissiles;
		int64_t iMissile = -1;
		float fNearestDistanceSquared = 0.0f;
		for (int64_t j = 0; j < rMissilesPostRender.iCount; ++j)
		{
			engine::LifecycleFlags_t lifecycleFlags = rMissilesPostRender.pLifecycleFlags[j];
			if (lifecycleFlags & engine::LifecycleFlags::kDying)
			{
				continue;
			}
			if (lifecycleFlags & engine::LifecycleFlags::kDestroy)
			{
				continue;
			}
			// A missile leaving the cell this tick is skipped too: its Transfer would carry it on as a flying missile.
			if (lifecycleFlags & engine::LifecycleFlags::kTransfer)
			{
				continue;
			}
			if (rMissilesPostRender.pFlags[j] & MissileFlags::kFalling)
			{
				continue;
			}
			if (!rFrame.postRender.alignments.CanCollide(alignment, rMissilesPostRender.pAlignments[j]))
			{
				continue;
			}
			XMVECTOR vecPosition = rMissilesInterpolate.pVecPositions[j];
			if (NearestFlightPlane(vecPosition) != 0)
			{
				continue;
			}
			float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, vecShooterPosition)));
			if (fDistanceSquared > kfBeamRangeSquared)
			{
				continue;
			}
			if (iMissile < 0 || fDistanceSquared < fNearestDistanceSquared)
			{
				iMissile = j;
				fNearestDistanceSquared = fDistanceSquared;
			}
		}
		if (iMissile < 0)
		{
			return false;
		}
		vecEnd = rMissilesInterpolate.pVecPositions[iMissile];
		// Non-directional, as for an entity impact; the blast registers inside this PostCollision's produce window,
		// tagged by the missile's firing side.
		MissilesPostRender::Explode(rFrame, rStaticData, iMissile, false);
	}

#if defined(BT_CLIENT)
	engine::AreaLightsPostRender::Add(rFrame, rCurrentInterpolate.pBeamAreaLights[i], giBattleshipBeamAreaLightTypeIndex);
	rCurrentInterpolate.pVecBeamEnds[i] = vecEnd;
	SyncBattleshipBeam(rFrame.interpolate, rCurrentInterpolate.pBeamAreaLights[i], vecShooterPosition, vecEnd, 1.0f);
#endif
	return true;
}

void BattleshipsPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pBattleships->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.fRadius = kfBattleshipRadius,
		.uiCategory = CollisionCategory::kuiBattleship,
		.uiCollidesWith = CollidesWith::kuiBattleship,
		.pAlignments = rCurrentPostRender.pAlignments,
	});

	std::span<engine::CollisionFlags_t> flags = engine::Collision::GetLayerFlags(siCollisionLayerIndex);
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		flags[static_cast<size_t>(i)] = (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {};
	}
}

void BattleshipsPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

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

		// Check collision results - battleships take damage from blasters
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
				engine::PointLightsPostRender::AddControlled(rFrame, engine::TickToSimulationTime(rFrame.interpolate.iTick), giBattleshipHitFlashControllerTypeIndex, rResult.vecContactPoint, 0.0f);
#endif

				if (rCurrentPostRender.pfHealths[i] <= 0.0f)
				{
					// A fleet Battleship never dies: its health holds at 0.
					if (rCurrentPostRender.pController[i] == ShipController::kFleet)
					{
						rCurrentPostRender.pfHealths[i] = 0.0f;
						continue;
					}
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

	// The beam fires here, after every other collection's PostCollision, in one sequential pass over the current frame:
	// a death transition completes before Destroy and a missile blast registers inside the area-damage produce window,
	// and a later shooter sees an earlier shooter's kill.
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
		if (rCurrentInterpolate.puiFlightPlanes[i] == kuiNoFlightPlane)
		{
			continue;
		}

		if (!(rCurrentPostRender.pNextBeamFireTimes[i] < std::chrono::duration<float>::zero()))
		{
			continue;
		}

		if (FireBeam(rFrame, rStaticData, i))
		{
			rCurrentPostRender.pNextBeamFireTimes[i] = kBattleshipBeamCooldown;
		}
	}
}

void BattleshipsPostRender::AreaDamage([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

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
			// A fleet Battleship never dies: its health holds at 0.
			if (rCurrentPostRender.pController[i] == ShipController::kFleet)
			{
				rCurrentPostRender.pfHealths[i] = 0.0f;
				continue;
			}
			XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorSubtract(vecClosestSource, rCurrentInterpolate.pVecPositions[i]));
			BeginExplosion(rFrame, rStaticData.coordinate, i, vecDamageDirection);
		}
	}
}

} // namespace game
