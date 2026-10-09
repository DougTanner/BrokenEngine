#include "Spaceships.h"

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

using enum SpaceshipFlags;

constexpr float kfHealthRegeneration = 0.1f;
constexpr float kfHealthRegenerationDistance = 60.0f;

// PreCollision sets the collision layer index when spaceships are present.
// thread_local: parallel per-Frame tick via Dispatch
static thread_local int64_t siCollisionLayerIndex = 0;
static thread_local std::vector<engine::CollisionFlags_t> sCollisionFlags;

struct SpaceshipCollisionIntervalScratch
{
	std::vector<float> maximumTimes;
};

static SpaceshipCollisionIntervalScratch& GetSpaceshipCollisionIntervalScratch()
{
	// Function-local TLS defers construction until first use; empty vectors require no allocation before allocator startup.
	// Vector growth suppresses allocation tracking.
	static thread_local SpaceshipCollisionIntervalScratch sScratch;
	return sScratch;
}

// Shared type indices (defined in Spaceships.cpp, set during Register())
extern int64_t giSpaceshipExplosionTypeIndex;
#if defined(BT_CLIENT)
extern int64_t giSpaceshipHitFlashControllerTypeIndex;
#endif

// Defined in Spaceships.cpp.
void SpawnSpaceshipExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent);

static void XM_CALLCONV BeginExplosion(Frame& rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecDamageDirection)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	rCurrentPostRender.pFlags[i].Set(kExploding);
	rCurrentInterpolate.pfDestroyedTimes[i] = kfSpaceshipDestroyTime;
	rCurrentPostRender.pfDestroyedExplosionTimes[i] = kfSpaceshipDestroyExplosionInterval;

	// Store damage direction for knockback
	rCurrentPostRender.pVecDamageDirections[i] = vecDamageDirection;

	// Clear the registry id so missiles stop tracking
	rCurrentInterpolate.puiRegistryIds[i] = {};

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioExplosions80401__steveygos93__explosion2wavCrc, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], gSpaceshipDeathVolume.mfCurrent, gSpaceshipDeathPitchMinimum.mfCurrent, gSpaceshipDeathPitchRandom.mfCurrent);
#endif

	XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
	SpawnSpaceshipExplosion(rFrame, emitterCoordinate, rCurrentInterpolate.pVecPositions[i], vecDirection, 1.0f);
}

void XM_CALLCONV SpaceshipsPostRender::RegenerateHealth(FXMVECTOR vecPosition, bool bPlayerAlive, FXMVECTOR vecNearestPlayer, SpaceshipFlags_t flags, std::chrono::duration<float> deltaTime, float& rfHealth)
{
	if (!(flags & kExploding) && bPlayerAlive && common::Distance(vecPosition, vecNearestPlayer) > kfHealthRegenerationDistance) [[unlikely]]
	{
		rfHealth = std::min(rfHealth + deltaTime.count() * kfHealthRegeneration, kfSpaceshipHealth);
	}
}

void SpaceshipsPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	SpaceshipCollisionIntervalScratch& rCollisionScratch = GetSpaceshipCollisionIntervalScratch();
	// Heap: static vectors resized each frame, only allocates on first call or when count grows (capacity retained).
	// .data() pointers are passed to AddLayer and must survive until PostCollision, so workbuffer can't be used
	ScopedSuppressAllocationTracking suppress;

	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	int64_t iCount = rCurrentInterpolate.iCount;
	sCollisionFlags.resize(static_cast<size_t>(iCount));
	rCollisionScratch.maximumTimes.resize(static_cast<size_t>(iCount));
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		sCollisionFlags.at(static_cast<size_t>(i)) = (rCurrentPostRender.pFlags[i] & kExploding) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {};
		engine::SegmentHit boundaryHit = engine::TracePointToCellExit(engine::LocalCellArea(), rPreviousFrame.interpolate.pSpaceships->pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		rCollisionScratch.maximumTimes.at(static_cast<size_t>(i)) = boundaryHit.bHit ? boundaryHit.fTime : std::numeric_limits<float>::max();
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pSpaceships->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pfMaxTimes = rCollisionScratch.maximumTimes.data(),
		.pFlags = sCollisionFlags.data(),
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.fRadius = kfSpaceshipRadius,
		.fDamage = kfSpaceshipCollisionDamage,
		.uiCategory = CollisionCategory::kuiSpaceship,
		.uiCollidesWith = CollidesWith::kuiSpaceship,
		.pAlignments = rCurrentPostRender.pAlignments,
	});
}

void SpaceshipsPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	engine::CellBounds bounds = engine::ComputeCellBounds(engine::LocalCellArea());

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pFlags[i] & kExploding) [[unlikely]]
		{
			continue;
		}

		// Check collision results - spaceships take damage from player blasters only
		// Note: Missile damage is handled via area damage system in AreaDamage phase
		if ((engine::Collision::sResultSpans[engine::Collision::sLayerBaseOffsets[siCollisionLayerIndex] + i].iCount > 0))
		{
			std::span<const engine::CollisionResult> collisions = engine::Collision::GetCollisions(siCollisionLayerIndex, i);
			for (const engine::CollisionResult& rResult : collisions)
			{
				if (rResult.uiOtherCategory == CollisionCategory::kuiBlaster)
				{
					rCurrentPostRender.pfHealths[i] -= rResult.fDamageReceived;

#if defined(BT_CLIENT)
					engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster793907__cvltiv8r__snaresbycvltiv8r301wavCrc, rStaticData.coordinate, rCurrentInterpolate.pVecPositions[i], gSpaceshipHitVolume.mfCurrent);
#endif

#if defined(BT_CLIENT)
					engine::PointLightsPostRender::AddControlled(rFrame, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime), giSpaceshipHitFlashControllerTypeIndex, rResult.vecContactPoint, 0.0f);
#endif

					if (rCurrentPostRender.pfHealths[i] <= 0.0f)
					{
						XMVECTOR vecDamageDirection = XMVector3Normalize(XMVectorNegate(rResult.vecOtherVelocity));
						BeginExplosion(rFrame, rStaticData.coordinate, i, vecDamageDirection);
						break;
					}
				}
			}
		}

		if (!(rCurrentPostRender.pFlags[i] & kExploding) && engine::IsOutOfBounds(bounds, rCurrentInterpolate.pVecPositions[i])) [[unlikely]]
		{
			// PreCollision's exclusive maximum-time cutoff excludes entity contacts at or beyond cell exit.
			rCurrentPostRender.pFlags[i].Set(kTransfer);
		}
	}
}

void SpaceshipsPostRender::AreaDamage([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (rCurrentPostRender.pFlags[i] & kExploding)
		{
			continue;
		}

		if (rCurrentPostRender.pFlags[i] & kTransfer)
		{
			continue;
		}

		XMVECTOR vecClosestSource {};
		float fDamage = engine::AreaDamage::Get(rCurrentInterpolate.pVecPositions[i], CollisionCategory::kuiMissile, vecClosestSource);

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
