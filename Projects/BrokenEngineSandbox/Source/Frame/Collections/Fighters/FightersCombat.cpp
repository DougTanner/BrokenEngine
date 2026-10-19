#include "Fighters.h"

#include "Data/Audio.h"
#include "Frame/CellStaticData.h"

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

// Shared type indices (defined in Fighters.cpp, set during Register())
extern int64_t giFighterExplosionTypeIndex;
#if defined(BT_CLIENT)
extern int64_t giFighterHitFlashControllerTypeIndex;
#endif

// Defined in Fighters.cpp.
void SpawnFighterExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent);

static void XM_CALLCONV BeginExplosion(Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecDamageDirection)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDying);
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
		.fDamage = kfShipCollisionDamage,
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

	engine::CellBounds bounds = engine::ComputeCellBounds(engine::LocalCellArea());

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) [[unlikely]]
		{
			continue;
		}

		// Check collision results - fighters take damage from blasters and ship rams
		// Note: Missile damage is handled via area damage system in AreaDamage phase
		for (const engine::CollisionResult& rResult : engine::Collision::GetCollisions(siCollisionLayerIndex, i))
		{
			if (rResult.uiOtherCategory & (CollisionCategory::kuiBlaster | CollisionCategory::kuiShips))
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

		if (!(rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && engine::IsOutOfBounds(bounds, rCurrentInterpolate.pVecPositions[i])) [[unlikely]]
		{
			// PreCollision's exclusive maximum-time cutoff excludes entity contacts at or beyond cell exit.
			rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kTransfer);
		}
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
