#include "Battleships.h"

#include "Data/Audio.h"
#include "File/PackChunks.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Ships/ShipHitboxes.h"
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

static void XM_CALLCONV BeginExplosion(Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecDamageDirection)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDying);
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.pPushers[i]);
	rCurrentInterpolate.pDestroyedTimes[i] = kBattleshipDestroyTime;
	rCurrentPostRender.pDestroyedExplosionTimes[i] = kBattleshipDestroyExplosionInterval;

	// The staggered death explosions in the Spawn hook fly away from this direction.
	rCurrentPostRender.pVecDamageDirections[i] = vecDamageDirection;

	// Clear the registry id so missiles stop tracking
	rCurrentInterpolate.pRegistryIds[i] = {};

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioExplosions80401__steveygos93__explosion2wavCrc, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], gFighterDeathVolume.mfCurrent, gFighterDeathPitchMinimum.mfCurrent, gFighterDeathPitchRandom.mfCurrent);
#endif

	XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
	// The dying ship holds still from its death tick on.
	rCurrentPostRender.pVecVelocities[i] = XMVectorZero();
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
		.layerFlags = engine::CollisionLayerFlags::kHitboxes,
		.uiCategory = CollisionCategory::kuiBattleship,
		.uiCollidesWith = CollidesWith::kuiBattleship,
		.pAlignments = rCurrentPostRender.pAlignments,
	});

	std::span<engine::CollisionFlags_t> flags = engine::Collision::GetLayerFlags(siCollisionLayerIndex);
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		flags[static_cast<size_t>(i)] = (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {};
	}

	std::span<float> radii = engine::Collision::GetLayerRadii(siCollisionLayerIndex);
	std::span<engine::CollisionHitbox> hitboxes = engine::Collision::GetLayerHitboxes(siCollisionLayerIndex);
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		radii[static_cast<size_t>(i)] = gShipHitboxShapes[std::to_underlying(ShipType::kBattleship)][std::to_underlying(rCurrentPostRender.pController[i])].fRadius;
		BuildShipHitbox(ShipType::kBattleship, rCurrentPostRender.pController[i], rCurrentInterpolate.pVecPositions[i], rCurrentInterpolate.pVecDirections[i], hitboxes[static_cast<size_t>(i)]);
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
		float fDamage = engine::AreaDamage::Get(rCurrentInterpolate.pVecPositions[i], engine::Collision::GetLayerHitboxes(siCollisionLayerIndex)[static_cast<size_t>(i)], OpposingBlastCategory(rCurrentPostRender.pAlignments[i], rFrame.postRender.fleetAlignment), vecClosestSource);

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

void BattleshipsPostRender::SpawnMissiles(Frame& __restrict rFrame, const engine::CellStaticData& rStaticData)
{
	static constexpr float kfMissileRangeSquared = kfBattleshipMissileRange * kfBattleshipMissileRange;

	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	// One sequential pass in row order, each row launching at most one missile: Spawn appends each missile at once, so a
	// later count includes every earlier launch of this tick.
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

		// Launch only while settled at the controller's effective height on its stored plane.
		XMVECTOR vecShipPosition = rCurrentInterpolate.pVecPositions[i];
		float fShipHeight = XMVectorGetZ(vecShipPosition);
		uint8_t uiFlightPlane = rCurrentInterpolate.puiFlightPlanes[i];
		if (uiFlightPlane == kuiNoFlightPlane)
		{
			continue;
		}

		if (fShipHeight != EffectiveShipFlightHeight(ShipType::kBattleship, rCurrentPostRender.pController[i], uiFlightPlane))
		{
			continue;
		}

		// At or below zero, unlike the Fighter beam's strictly below, so launches come exactly the cooldown apart.
		if (rCurrentPostRender.pNextMissileFireTimes[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		engine::AlignmentIdentifier alignment = rCurrentPostRender.pAlignments[i];

		std::optional<engine::registry_id_t> optionalTarget;
		float fNearestDistanceSquared = 0.0f;
		XMVECTOR vecTargetPosition = XMVectorZero();
		ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecEnemyPosition, [[maybe_unused]] FXMVECTOR vecEnemyVelocity)
		{
			if (NearestFlightPlane(vecEnemyPosition) != uiFlightPlane)
			{
				return;
			}
			float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecEnemyPosition, vecShipPosition)));
			if (fDistanceSquared > kfMissileRangeSquared)
			{
				return;
			}
			if (optionalTarget && !(fDistanceSquared < fNearestDistanceSquared))
			{
				return;
			}

			engine::registry_id_t target = VisitShip(rFrame, locator, [](const auto& rInterpolate, [[maybe_unused]] const auto& rPostRender, int64_t iRow)
			{
				return rInterpolate.pRegistryIds[iRow];
			});

			const MissilesPostRender& rMissiles = *rFrame.postRender.pMissiles;
			int64_t iTargetingCount = 0;
			for (int64_t j = 0; j < rMissiles.iCount; ++j)
			{
				if (rMissiles.puiRegistryTargets[j] == target)
				{
					++iTargetingCount;
				}
			}
			if (iTargetingCount >= kiMissilesPerTarget[std::to_underlying(locator.eType)])
			{
				return;
			}
			if (!HasLineOfSight(rStaticData, kiFiringClearanceTier, uiFlightPlane, vecShipPosition, vecEnemyPosition))
			{
				return;
			}

			optionalTarget = target;
			fNearestDistanceSquared = fDistanceSquared;
			vecTargetPosition = vecEnemyPosition;
		});

		if (!optionalTarget)
		{
			continue;
		}

		common::crc_t uiModel = kShipHitboxTypes[std::to_underlying(ShipType::kBattleship)].models[std::to_underlying(rCurrentPostRender.pController[i])];
		const engine::EagerChunk& rChunk = engine::gpFileManager->mpPackChunks->GetEagerChunkMap().at(uiModel);
		const common::ModelHeader& rModelHeader = rChunk.pHeader->modelHeader;
		ASSERT(rChunk.pHeader->iSize <= std::numeric_limits<int64_t>::max() - (common::kiAlignmentBytes - 1) - static_cast<int64_t>(sizeof(common::ModelOutline)));
		int64_t iTubesOffset = common::ModelHeader::MissileTubesOffset(rChunk.pHeader->iSize);
		ASSERT(iTubesOffset <= rChunk.iDataSize);
		ASSERT(rModelHeader.uiMissileTubeCount > 0 && static_cast<int64_t>(rModelHeader.uiMissileTubeCount) <= (rChunk.iDataSize - iTubesOffset) / static_cast<int64_t>(sizeof(common::PackedModelMissileTube)));
		std::span<const common::PackedModelMissileTube> tubes(reinterpret_cast<const common::PackedModelMissileTube*>(rChunk.pData + iTubesOffset), rModelHeader.uiMissileTubeCount);

		XMVECTOR vecHullDirection = rCurrentInterpolate.pVecDirections[i];
		float fCos = XMVectorGetX(vecHullDirection);
		float fSin = XMVectorGetY(vecHullDirection);
		int64_t iNearestTube = 0;
		float fNearestTubeDistanceSquared = 0.0f;
		XMVECTOR vecTubePosition = XMVectorZero();
		for (int64_t j = 0; j < std::ssize(tubes); ++j)
		{
			const XMFLOAT3& rf3Position = tubes[static_cast<size_t>(j)].f3Position;
			XMVECTOR vecPosition = XMVectorSet((fCos * rf3Position.x - fSin * rf3Position.y) + XMVectorGetX(vecShipPosition), (fSin * rf3Position.x + fCos * rf3Position.y) + XMVectorGetY(vecShipPosition), fShipHeight + rf3Position.z, 1.0f);
			float fDistanceSquared = XMVectorGetX(XMVector2LengthSq(XMVectorSubtract(vecTargetPosition, vecPosition)));
			if (j == 0 || fDistanceSquared < fNearestTubeDistanceSquared)
			{
				iNearestTube = j;
				fNearestTubeDistanceSquared = fDistanceSquared;
				vecTubePosition = vecPosition;
			}
		}
		XMVECTOR vecAlignedDirection = XMVector2Normalize(XMVectorSetZ(XMLoadFloat3(&tubes[static_cast<size_t>(iNearestTube)].f3Direction), 0.0f));
		float fDirectionX = XMVectorGetX(vecAlignedDirection);
		float fDirectionY = XMVectorGetY(vecAlignedDirection);
		XMVECTOR vecTubeDirection = XMVectorSet(fCos * fDirectionX - fSin * fDirectionY, fSin * fDirectionX + fCos * fDirectionY, 0.0f, 0.0f);

		MissileFlags_t missileFlags = (alignment == rFrame.postRender.fleetAlignment) ? MissileFlags::kTargetRandom : MissileFlags::kTargetFleet;
		missileFlags.Set(MissileFlags::kUpperFlightPlane, uiFlightPlane == 1);
		bool bSpawned = MissilesPostRender::Spawn(rFrame,
		{
			.vecPosition = vecTubePosition,
			.vecDirection = vecTubeDirection,
			.vecVelocity = XMVectorScale(vecTubeDirection, kfBattleshipMissileInitialVelocity),
			.vecStoredDirection = common::DirectionTo(vecTubePosition, XMVectorSetZ(vecTargetPosition, XMVectorGetZ(vecTubePosition))),
			.uiTarget = *optionalTarget,
			.fAcceleration = kfBattleshipMissileAcceleration,
			.flags = missileFlags,
			.alignment = alignment,
		});
		// A refused launch leaves the cooldown unchanged.
		if (!bSpawned)
		{
			continue;
		}
		rCurrentPostRender.pNextMissileFireTimes[i] = kBattleshipMissileCooldown;

#if defined(BT_CLIENT)
		engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioMissile182794__qubodup__rocketlaunch_start_2wavCrc, rStaticData.coordinate, vecTubePosition, gMissileLaunchVolume.mfCurrent, gMissilePitchMin.mfCurrent, gMissilePitchRandom.mfCurrent);
#endif
	}
}

} // namespace game
