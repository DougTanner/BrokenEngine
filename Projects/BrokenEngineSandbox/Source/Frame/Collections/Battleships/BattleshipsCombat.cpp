#include "Battleships.h"

#include "Data/Audio.h"
#include "Frame/CellStaticData.h"

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
#endif

// Defined in Battleships.cpp.
void SpawnBattleshipExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent);

constexpr float kfBattleshipMissileInitialVelocity = 30.0f;
constexpr float kfBattleshipMissileAcceleration = 40.0f;
// The tubes sit on the hull, evenly spaced along it from the front tube to the rear tube on each side.
constexpr float kfBattleshipMissileTubeSideOffset = 0.5f * kfBattleshipRadius;
constexpr float kfBattleshipMissileTubeFrontOffset = 0.75f * kfBattleshipRadius;
constexpr float kfBattleshipMissileTubeSpacing = 2.0f * kfBattleshipMissileTubeFrontOffset / static_cast<float>(kiBattleshipMissileTubesPerSide - 1);

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

void XM_CALLCONV BattleshipsPostRender::ApplyBeamDamage(Frame& rFrame, engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecShooterPosition, float fDamage)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	rCurrentPostRender.pfHealths[i] -= fDamage;

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

void BattleshipsPostRender::SpawnMissiles(Frame& __restrict rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate)
{
	static constexpr float kfMissileRangeSquared = kfBattleshipMissileRange * kfBattleshipMissileRange;

	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	// One sequential pass, shooters in row order and each one's enemies in ForEachEnemyShip order: Spawn appends each
	// missile at once, so a later count includes every earlier launch of this tick.
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			continue;
		}

		if (rCurrentPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		// Missiles launch only while the ship is exactly at its stored flight plane's height, so every flying missile
		// starts on a plane, and only at a target whose nearest flight plane is that plane.
		XMVECTOR vecShipPosition = rCurrentInterpolate.pVecPositions[i];
		float fShipHeight = XMVectorGetZ(vecShipPosition);
		uint8_t uiFlightPlane = rCurrentInterpolate.puiFlightPlanes[i];
		if (FlightPlaneAtHeight(fShipHeight) != uiFlightPlane)
		{
			continue;
		}

		XMVECTOR vecHullDirection = rCurrentInterpolate.pVecDirections[i];
		XMVECTOR vecLeftNormal = XMVector3Normalize(XMVector3Cross(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), vecHullDirection));
		engine::AlignmentIdentifier alignment = rCurrentPostRender.pAlignments[i];

		ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecEnemyPosition, [[maybe_unused]] FXMVECTOR vecEnemyVelocity)
		{
			if (NearestFlightPlane(vecEnemyPosition) != uiFlightPlane)
			{
				return;
			}
			if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecEnemyPosition, vecShipPosition))) > kfMissileRangeSquared)
			{
				return;
			}

			engine::registry_id_t target = VisitShip(rFrame, locator, [](const auto& rInterpolate, [[maybe_unused]] const auto& rPostRender, int64_t iRow)
			{
				return rInterpolate.pRegistryIds[iRow];
			});

			// Read the columns per enemy: a launch may reallocate them.
			const MissilesPostRender& rMissiles = *rFrame.postRender.pMissiles;
			int64_t iTargetingCount = 0;
			for (int64_t j = 0; j < rMissiles.iCount; ++j)
			{
				if (rMissiles.puiRegistryTargets[j] == target)
				{
					++iTargetingCount;
				}
			}

			for (; iTargetingCount < kiMissilesPerTarget; ++iTargetingCount)
			{
				uint8_t uiSlot = rCurrentPostRender.puiNextMissileTubes[i];
				XMVECTOR vecOutwardNormal = (uiSlot % 2 == 0) ? vecLeftNormal : XMVectorNegate(vecLeftNormal);
				float fAlongOffset = kfBattleshipMissileTubeFrontOffset - static_cast<float>(uiSlot / 2) * kfBattleshipMissileTubeSpacing;
				XMVECTOR vecTubePosition = XMVectorMultiplyAdd(XMVectorReplicate(fAlongOffset), vecHullDirection, vecShipPosition);
				vecTubePosition = XMVectorMultiplyAdd(XMVectorReplicate(kfBattleshipMissileTubeSideOffset), vecOutwardNormal, vecTubePosition);
				vecTubePosition = XMVectorSetZ(vecTubePosition, fShipHeight);

				// Missiles Update zeroes a flying missile's velocity Z, so it holds the ship's plane height.
				bool bSpawned = MissilesPostRender::Spawn(rFrame,
				{
					.vecPosition = vecTubePosition,
					.vecDirection = vecOutwardNormal,
					.vecVelocity = XMVectorScale(vecOutwardNormal, kfBattleshipMissileInitialVelocity),
					.vecStoredDirection = common::DirectionTo(vecTubePosition, XMVectorSetZ(vecEnemyPosition, fShipHeight)),
					.uiTarget = target,
					.fAcceleration = kfBattleshipMissileAcceleration,
					.flags = (alignment == rFrame.postRender.fleetAlignment) ? MissileFlags::kTargetRandom : MissileFlags::kTargetFleet,
					.alignment = alignment,
				});
				// A refused launch takes no slot and ends this tick's launches at this enemy.
				if (!bSpawned)
				{
					return;
				}
				rCurrentPostRender.puiNextMissileTubes[i] = static_cast<uint8_t>((uiSlot + 1) % kiBattleshipMissileTubeSlots);

#if defined(BT_CLIENT)
				engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioMissile182794__qubodup__rocketlaunch_start_2wavCrc, emitterCoordinate, vecTubePosition, gMissileLaunchVolume.mfCurrent, gMissilePitchMin.mfCurrent, gMissilePitchRandom.mfCurrent);
#endif
			}
		});
	}
}

} // namespace game
