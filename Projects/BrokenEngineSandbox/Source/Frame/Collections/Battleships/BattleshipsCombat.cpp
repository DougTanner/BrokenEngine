#include "Battleships.h"

#include "Data/Audio.h"
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

#if defined(BT_CLIENT)
// Shared type index (defined in Battleships.cpp, set during Register())
extern int64_t giBattleshipHitFlashControllerTypeIndex;
#endif

constexpr float kfBattleshipMissileInitialVelocity = 30.0f;
constexpr float kfBattleshipMissileAcceleration = 40.0f;

static void BeginExplosion(Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, int64_t i)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDying);
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.pPushers[i]);
	rCurrentInterpolate.pDestroyedTimes[i] = kBattleshipDestroyTime;

	// Clear the registry id so missiles stop tracking
	rCurrentInterpolate.pRegistryIds[i] = {};

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioExplosions80401__steveygos93__explosion2wavCrc, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], gFighterDeathVolume.mfCurrent, gFighterDeathPitchMinimum.mfCurrent, gFighterDeathPitchRandom.mfCurrent);
#endif
	// The dying ship holds still from its death tick on.
	rCurrentPostRender.pVecVelocities[i] = XMVectorZero();
}

// The Fighters beam calls every ship type's ApplyBeamDamage through one generic call, so the shooter position stays in the signature.
void XM_CALLCONV BattleshipsPostRender::ApplyBeamDamage(Frame& rFrame, engine::GridCoord emitterCoordinate, int64_t i, [[maybe_unused]] FXMVECTOR vecShooterPosition, float fDamage)
{
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
		BeginExplosion(rFrame, emitterCoordinate, i);
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
				engine::PointLightsPostRender::AddControlled(rFrame, rStaticData.coordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick), giBattleshipHitFlashControllerTypeIndex, rResult.vecContactPoint, 0.0f);
#endif

				if (rCurrentPostRender.pfHealths[i] <= 0.0f)
				{
					// A fleet Battleship never dies: its health holds at 0.
					if (rCurrentPostRender.pController[i] == ShipController::kFleet)
					{
						rCurrentPostRender.pfHealths[i] = 0.0f;
						continue;
					}
					BeginExplosion(rFrame, rStaticData.coordinate, i);
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
			BeginExplosion(rFrame, rStaticData.coordinate, i);
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

		std::span<const common::PackedModelHardpoint> tubes = gShipHitboxShapes[std::to_underlying(ShipType::kBattleship)][std::to_underlying(rCurrentPostRender.pController[i])].missileTubes;
		XMVECTOR vecHullDirection = rCurrentInterpolate.pVecDirections[i];

		// Every (enemy, tube) pair is a candidate, measured from the tube; an exact tie keeps the first enemy, then its first
		// authored tube.
		std::optional<engine::registry_id_t> optionalTarget;
		float fNearestDistanceSquared = 0.0f;
		XMVECTOR vecTargetPosition = XMVectorZero();
		int64_t iLaunchTube = 0;
		XMVECTOR vecTubePosition = XMVectorZero();
		ForEachEnemyShip(rFrame.interpolate, rFrame.postRender, alignment, [&](ShipLocator locator, FXMVECTOR vecEnemyPosition, [[maybe_unused]] FXMVECTOR vecEnemyVelocity)
		{
			int64_t iEnemyFlightPlane = NearestFlightPlane(vecEnemyPosition);
			// Set once the per-target cap has passed for this enemy.
			std::optional<engine::registry_id_t> optionalEnemy;
			for (const auto& [k, rTube] : std::views::enumerate(tubes))
			{
				const XMFLOAT3& rf3Position = rTube.f3Position;
				XMVECTOR vecPosition = XMVectorSetW(XMVectorSetZ(RotateByHeading(vecHullDirection, XMLoadFloat3(&rf3Position), vecShipPosition), fShipHeight + rf3Position.z), 1.0f);
				int64_t iTubeFlightPlane = NearestFlightPlane(vecPosition);
				if (iEnemyFlightPlane != iTubeFlightPlane)
				{
					continue;
				}
				float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecEnemyPosition, vecPosition)));
				if (fDistanceSquared > kfMissileRangeSquared)
				{
					continue;
				}
				if (optionalTarget && !(fDistanceSquared < fNearestDistanceSquared))
				{
					continue;
				}

				if (!optionalEnemy)
				{
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
					optionalEnemy = target;
				}
				if (!HasShipLineOfSight(rFrame, rStaticData, kiFiringClearanceTier, static_cast<uint8_t>(iTubeFlightPlane), vecPosition, vecEnemyPosition))
				{
					continue;
				}

				optionalTarget = optionalEnemy;
				fNearestDistanceSquared = fDistanceSquared;
				vecTargetPosition = vecEnemyPosition;
				iLaunchTube = k;
				vecTubePosition = vecPosition;
			}
		});

		if (!optionalTarget)
		{
			continue;
		}

		float fCos = XMVectorGetX(vecHullDirection);
		float fSin = XMVectorGetY(vecHullDirection);
		XMVECTOR vecAlignedDirection = XMVector2Normalize(XMVectorSetZ(XMLoadFloat3(&tubes[static_cast<size_t>(iLaunchTube)].f3Direction), 0.0f));
		XMVECTOR vecTubeDirection = common::TransformDirection(vecAlignedDirection, XMMATRIX(fCos, fSin, 0.0f, 0.0f, -fSin, fCos, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f));

		MissileFlags_t missileFlags = (alignment == rFrame.postRender.fleetAlignment) ? MissileFlags::kTargetRandom : MissileFlags::kTargetFleet;
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
