#include "Missiles.h"

#include "Frame/FrameStaticData.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"
#include "Frame/Collections/Collection.h"


namespace game
{

using enum MissileFlags;

// Collision layer index (set each frame in PreCollision)
// thread_local: parallel per-Frame tick via Dispatch
static thread_local size_t suiCollisionLayerIndex = 0;
static thread_local std::vector<engine::CollisionFlags_t> sCollisionFlags;
static thread_local std::vector<float> sCollisionRadii;
static thread_local std::vector<float> sCollisionDamages;

struct MissileCollisionIntervalScratch
{
	std::vector<float> startTimes;
	std::vector<float> endTimes;
	std::vector<float> maxTimes;
	std::vector<engine::SegmentHit> terrainHits;
	std::vector<engine::SegmentHit> boundaryHits;
};

static MissileCollisionIntervalScratch& GetMissileCollisionIntervalScratch()
{
	// Function-local TLS defers construction until first use; default construction is allocation-free
	// (empty vectors), so it is safe even before allocator startup completes. Growth sites suppress tracking.
	static thread_local MissileCollisionIntervalScratch sScratch;
	return sScratch;
}

// Missile AI
constexpr float kfAccelerationAtMaxDeltaAngle = 0.9f;
constexpr float kfVelocityDecay = 1.0f;
constexpr float kfVelocityToDirection = 16.0f;
constexpr float kfJitterIntervalRandom = 0.0025f;
constexpr float kfDirectionJitterRandom = 0.06f;
constexpr float kfDeltaAngleJitterRandom = 0.5f;
constexpr float kfDeltaAngleJitterRandomWithTarget = 1.0f;
constexpr float kfDeltaRotationChange = 0.925f;
constexpr float kfDeltaRotationDecay = 8.0f;
constexpr float kfDeltaRotationTowardsTarget = 10.0f;
constexpr float kfDeltaRotationTowardsStored = 3.0f;

#if defined(BT_CLIENT)
// Forward declaration of SyncMissile (defined in Missiles.cpp, also used by ClientInit)
void XM_CALLCONV SyncMissile(FrameInterpolate& rFrameInterpolate, engine::area_lights_t uiAreaLight, engine::smoke_trails_t uiSmokeTrail, engine::sound_t uiSound, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecVelocity, GXMVECTOR vecPreviousPosition, MissileFlags_t flags, float fPitch, float fDeltaRotation, float fExhaustLength);
void XM_CALLCONV SyncMissileTrail(FrameInterpolate& rFrameInterpolate, engine::smoke_trails_t uiSmokeTrail, FXMVECTOR vecPosition);
#endif

void MissilesInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rCurrentFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	MissilesInterpolate& rCurrent = *rCurrentFrameInterpolate.pMissiles;
	const MissilesInterpolate& rPrevious = *rPreviousFrame.interpolate.pMissiles;
	const MissilesPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pMissiles;
	float fDeltaTime = rCurrentFrameInterpolate.fDeltaTime;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Load
		XMVECTOR vecPreviousPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecPosition = vecPreviousPosition;
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		float fDestroyedTime = rPrevious.pfDestroyedTimes[i];
		MissileFlags_t flags = rPreviousPostRender.pFlags[i];

		if (!(flags & kExploding)) [[likely]]
		{
			vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime), rPreviousPostRender.pVecVelocities[i], vecPosition);
			// Positions must always have W=1.0 — prevents W-lane drift via MultiplyAdd.
			vecPosition = XMVectorSetW(vecPosition, 1.0f);

			if (!(flags & kFalling))
			{
				// Add delta rotation to direction (delay percentage already applied in PostRender::Update)
				vecDirection = XMVector3Normalize(XMVector4Transform(vecDirection, XMMatrixRotationZ(fDeltaTime * rPreviousPostRender.pfDeltaRotations[i])));
			}
		}

		// Decay destroyed time
		if (fDestroyedTime > 0.0f)
		{
			fDestroyedTime = std::max(fDestroyedTime - fDeltaTime, 0.0f);
		}

		// Save
		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pfDestroyedTimes[i] = fDestroyedTime;

		// Sync owned objects (IDs copied in AllocateAndCopy)
#if defined(BT_CLIENT)
		SyncMissile(rCurrentFrameInterpolate, rCurrent.puiAreaLights[i], rCurrent.puiSmokeTrails[i], rPreviousPostRender.puiSounds[i], vecPosition, vecDirection, rPreviousPostRender.pVecVelocities[i], vecPreviousPosition, flags, rPreviousPostRender.pfPitches[i], rPreviousPostRender.pfDeltaRotations[i], rPreviousPostRender.pfExhaustLengths[i]);
#endif // BT_CLIENT
	}
}

void MissilesPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	MissilesPostRender& __restrict rCurrent = *rFrame.postRender.pMissiles;
	const MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	const MissilesPostRender& rPrevious = *rPreviousFrame.postRender.pMissiles;
	float fDeltaTime = rFrame.interpolate.fDeltaTime;

	// One query window for the whole phase. Spaceships update after Missiles, so the source positions and
	// eligibility it binds are still this tick's Interpolate state, unchanged by any later phase.
	RegistryWindow window = Frame::MissileUpdateWindow(rFrame, rPreviousFrame);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Load dynamic fields (static fields copied via memcpy in AllocateAndCopy)
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		engine::registry_id_t uiTarget = rPrevious.puiRegistryTargets[i];
		float fTime = rPrevious.pfTimes[i] + fDeltaTime;
		float fDeltaRotation = rPrevious.pfDeltaRotations[i];
		float fDeltaRotationDelay = rPrevious.pfDeltaRotationDelays[i];
		float fExhaustLength = rPrevious.pfExhaustLengths[i];
		float fNextJitter = rPrevious.pfNextJitter[i];
		XMVECTOR vecStoredDirection = rPrevious.pVecStoredDirections[i];

		if (!(rCurrent.pFlags[i] & kExploding) && !(rCurrent.pFlags[i] & kFalling) && fTime < kfMissileLifetime) [[likely]]
		{
			fNextJitter -= fDeltaTime;

			// Decay velocity
			vecVelocity = XMVectorMultiply(XMVectorReplicate(1.0f - fDeltaTime * kfVelocityDecay), vecVelocity);

			// Accelerate
			float fDeltaAnglePercent = std::abs(fDeltaRotation) / rCurrent.pfDeltaRotationMax[i];
			fDeltaAnglePercent = std::clamp(fDeltaAnglePercent, 0.0f, 1.0f);
			float fAdjustedAcceleration = (1.0f - fDeltaAnglePercent) * rCurrent.pfAccelerations[i] + fDeltaAnglePercent * kfAccelerationAtMaxDeltaAngle * rCurrent.pfAccelerations[i];
			vecVelocity = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime * fAdjustedAcceleration), rCurrentInterpolate.pVecDirections[i], vecVelocity);

			// Rotate velocity towards direction
			float fVelocityToDirectionPercent = 1.0f - fDeltaTime * kfVelocityToDirection;
			XMVECTOR vecVelocityComponent = XMVectorMultiply(XMVectorReplicate(fVelocityToDirectionPercent), XMVector3Normalize(vecVelocity));
			XMVECTOR vecDirectionComponent = XMVectorMultiply(XMVectorReplicate(1.0f - fVelocityToDirectionPercent), rCurrentInterpolate.pVecDirections[i]);
			vecVelocity = XMVectorMultiply(XMVector3Length(vecVelocity), XMVector3Normalize(XMVectorAdd(vecVelocityComponent, vecDirectionComponent)));

			// Decay delta rotation
			fDeltaRotation = (1.0f - fDeltaTime * kfDeltaRotationDecay) * fDeltaRotation;

			// Jitter direction
			if (fNextJitter < 0.0f)
			{
				fNextJitter = common::Random<kfJitterIntervalRandom>(rFrame.postRender.randomEngine);

				uint32_t uiRandom = common::Random(2u, rFrame.postRender.randomEngine);
				float fDeltaAnglePercentExtra = 1.0f + 3.0f * fDeltaAnglePercent;
				float fDeltaAngleJitter = !uiTarget.IsValid() ? kfDeltaAngleJitterRandom : kfDeltaAngleJitterRandomWithTarget;
				if (uiRandom == 0)
				{
					vecVelocity = XMVector3RotateSafe(vecVelocity, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, fDeltaAnglePercentExtra * (-kfDirectionJitterRandom + common::Random(2.0f * kfDirectionJitterRandom, rFrame.postRender.randomEngine))));
#if defined(BT_DEBUG)
					common::ValidateVector<false>(vecVelocity);
#endif
				}
				if (uiRandom == 1) { fDeltaRotation += fDeltaAnglePercentExtra * (-fDeltaAngleJitter + fDeltaAngleJitter * common::Random<2.0f>(rFrame.postRender.randomEngine)); }
			}

			// Generate new random exhaust length every frame
			fExhaustLength = kfMissileExhaustLength + common::Random<kfMissileExhaustLengthRandom>(rFrame.postRender.randomEngine);

			// Validate the retained handle against the rows the registry currently considers eligible; a source
			// that died, transferred, or is still in arrival grace drops the handle.
			engine::RegistryResult retained {};
			bool bRetained = false;
			if (uiTarget.IsValid())
			{
				bRetained = engine::ResolveRegistryHandle(window.context, uiTarget, retained);
				if (!bRetained)
				{
					uiTarget = {};
					vecStoredDirection = rCurrentInterpolate.pVecDirections[i];
				}
			}

			// A targetless missile re-acquires every tick, in the post-loop pass, so the handle it produces is
			// stored but does not steer until the next tick.

			// Home only on a target that was already valid coming into this tick, steering toward where that
			// source stood last tick (matches engine's read-previous / write-current pattern)
			if (bRetained)
			{
				fDeltaRotationDelay -= fDeltaTime;

				XMVECTOR vecTargetPosition = retained.vecPreviousPosition;
				XMVECTOR vecToTargetNormal = XMVector3Normalize(XMVectorSubtract(vecTargetPosition, rCurrentInterpolate.pVecPositions[i]));
				float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(rCurrentInterpolate.pVecDirections[i], vecToTargetNormal));
				float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? kfDeltaRotationTowardsTarget : -kfDeltaRotationTowardsTarget;

				float fDelayPercent = std::clamp(1.0f - fDeltaRotationDelay / kfMissileDeltaRotationDelay, 0.0f, 1.0f);
				fWantedDeltaRotation *= fDelayPercent;

				fDeltaRotation = kfDeltaRotationChange * fDeltaRotation + (1.0f - kfDeltaRotationChange) * fWantedDeltaRotation;
			}
			else
			{
				fDeltaRotationDelay -= fDeltaTime;

				XMVECTOR vecCurrentDirection = rCurrentInterpolate.pVecDirections[i];
				float fDirectionCrossZ = XMVectorGetZ(XMVector3Cross(vecCurrentDirection, vecStoredDirection));
				float fWantedDeltaRotation = fDirectionCrossZ > 0.0f ? kfDeltaRotationTowardsStored : -kfDeltaRotationTowardsStored;

				float fDelayPercent = std::clamp(1.0f - fDeltaRotationDelay / kfMissileDeltaRotationDelay, 0.0f, 1.0f);
				fWantedDeltaRotation *= fDelayPercent;

				fDeltaRotation = kfDeltaRotationChange * fDeltaRotation + (1.0f - kfDeltaRotationChange) * fWantedDeltaRotation;
			}

			// Clamp delta rotation
			fDeltaRotation = common::MinAbs(fDeltaRotation, rCurrent.pfDeltaRotationMax[i]);

			// Keep velocity in XY plane
			vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);
		}
		else if (rCurrent.pFlags[i] & kFalling)
		{
			vecVelocity = XMVectorSetZ(vecVelocity, XMVectorGetZ(vecVelocity) - kfMissileGravity * fDeltaTime);
		}

		// Save dynamic fields (static fields copied via memcpy in AllocateAndCopy)
#if defined(BT_DEBUG)
		common::ValidateVector<false>(vecVelocity);
#endif
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pVecStoredDirections[i] = vecStoredDirection;
		rCurrent.puiRegistryTargets[i] = uiTarget;
		rCurrent.pfTimes[i] = fTime;
		rCurrent.pfDeltaRotationDelays[i] = fDeltaRotationDelay;
		rCurrent.pfDeltaRotations[i] = fDeltaRotation;
		rCurrent.pfExhaustLengths[i] = fExhaustLength;
		rCurrent.pfNextJitter[i] = fNextJitter;

		if (!(rCurrent.pFlags[i] & kExploding) && !(rCurrent.pFlags[i] & kFalling) && fTime >= kfMissileLifetime)
		{
			// The only release site: this is the one missile transition that happens while the window is live.
			engine::ReleaseRegistryTarget(window.context, rCurrent.puiRegistryTargets[i]);
			Fall(rFrame, i, fDeltaTime);
		}
	}

	// Acquisition runs after the loop so every missile has published this tick's handle and released the
	// subscription of every missile that expired this tick, and so it runs in ascending missile order. A row that
	// is still flying and holds no handle is exactly the targetless set the loop decided on. Fixed-size chunks keep
	// both spans off the workbuffer, which a per-missile span would overrun on a worker thread; each chunk folds its
	// subscriptions into the shared counts before the next one starts, so chunked ascending order assigns exactly
	// what one whole-collection batch assigns.
	constexpr int64_t kiAcquireChunk = 64;
	int64_t piAcquireRows[kiAcquireChunk] {};
	engine::RegistryResult pAcquireResults[kiAcquireChunk] {};
	int64_t iAcquireCount = 0;

	auto AcquireChunk = [&]()
	{
		engine::AcquireRegistryTargets(window.context,
		{
			.puiTargets = rCurrent.puiRegistryTargets,
			.pVecOrigins = rCurrentInterpolate.pVecPositions,
			.pVecDirections = rCurrentInterpolate.pVecDirections,
			.pAlignments = rCurrent.pAlignments,
			.rows = std::span<const int64_t>(piAcquireRows, static_cast<size_t>(iAcquireCount)),
			.results = std::span<engine::RegistryResult>(pAcquireResults, static_cast<size_t>(iAcquireCount)),
			.iSourceCount = rCurrent.iCount,
		}, kfMissileTargetAcquireRange);
		iAcquireCount = 0;
	};

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		if ((rCurrent.pFlags[i] & kExploding) || (rCurrent.pFlags[i] & kFalling) || rCurrent.pfTimes[i] >= kfMissileLifetime || rCurrent.puiRegistryTargets[i].IsValid())
		{
			continue;
		}

		piAcquireRows[iAcquireCount++] = i;
		if (iAcquireCount == kiAcquireChunk)
		{
			AcquireChunk();
		}
	}

	if (iAcquireCount > 0)
	{
		AcquireChunk();
	}
}

void MissilesPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	MissileCollisionIntervalScratch& rCollisionScratch = GetMissileCollisionIntervalScratch();
	// Heap: static vectors resized each frame, only allocates on first call or when count grows (capacity retained).
	// .data() pointers are passed to AddLayer and must survive until PostCollision, so workbuffer can't be used
	ScopedSuppressAllocationTracking suppress;

	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	// Build collision arrays
	size_t uiCount = static_cast<size_t>(rCurrentInterpolate.iCount);
	sCollisionFlags.resize(uiCount);
	sCollisionRadii.resize(uiCount);
	sCollisionDamages.resize(uiCount);
	rCollisionScratch.startTimes.resize(uiCount);
	rCollisionScratch.endTimes.resize(uiCount);
	rCollisionScratch.maxTimes.resize(uiCount);
	rCollisionScratch.terrainHits.resize(uiCount);
	rCollisionScratch.boundaryHits.resize(uiCount);
	const MissilesInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pMissiles;
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		size_t uiIndex = static_cast<size_t>(i);
		sCollisionFlags.at(uiIndex) = (rCurrentPostRender.pFlags[i] & kExploding) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {engine::CollisionFlags::kDestroyOnCollide};
		sCollisionRadii.at(uiIndex) = kfMissileCollisionRadius;
		sCollisionDamages.at(uiIndex) = 0.0f;  // Damage via area damage system
		rCollisionScratch.startTimes.at(uiIndex) = 0.0f;
		rCollisionScratch.endTimes.at(uiIndex) = 1.0f;
		rCollisionScratch.terrainHits.at(uiIndex) = engine::TracePointAgainstTerrain(rStaticData, rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		rCollisionScratch.boundaryHits.at(uiIndex) = engine::TracePointToFrameExit(rStaticData.vecArea, rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		float fMaxTime = std::numeric_limits<float>::max();
		if (rCollisionScratch.terrainHits.at(uiIndex).bHit)
		{
			fMaxTime = rCollisionScratch.terrainHits.at(uiIndex).fTime;
		}
		if (rCollisionScratch.boundaryHits.at(uiIndex).bHit)
		{
			fMaxTime = std::min(fMaxTime, rCollisionScratch.boundaryHits.at(uiIndex).fTime);
		}
		rCollisionScratch.maxTimes.at(uiIndex) = fMaxTime;
	}

	// Note: Damage is applied via area damage system, not direct collision
	suiCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousInterpolate.pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pfStartTimes = rCollisionScratch.startTimes.data(),
		.pfEndTimes = rCollisionScratch.endTimes.data(),
		.pfMaxTimes = rCollisionScratch.maxTimes.data(),
		.pfRadii = sCollisionRadii.data(),
		.pfDamages = sCollisionDamages.data(),
		.pFlags = sCollisionFlags.data(),
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.bSweptTest = true,
		.uiCategory = CollisionCategory::kMissile,
		.uiCollidesWith = CollidesWith::kMissile,
		.pAlignments = rCurrentPostRender.pAlignments,
	});
}

void MissilesPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	MissileCollisionIntervalScratch& rCollisionScratch = GetMissileCollisionIntervalScratch();
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if ((rCurrentPostRender.pFlags[i] & kExploding) || (rCurrentPostRender.pFlags[i] & kSilentDespawn)) [[unlikely]]
		{
			continue;
		}

		size_t uiIndex = static_cast<size_t>(i);
		// Entity results are pre-filtered against terrain and frame-exit cutoffs.
		if (engine::Collision::HasCollision(suiCollisionLayerIndex, i))
		{
			const engine::CollisionResult& rResult = engine::Collision::GetCollisions(suiCollisionLayerIndex, i).front();
			rCurrentInterpolate.pVecPositions[i] = rResult.vecSelfPosition;
#if defined(BT_CLIENT)
			SyncMissileTrail(rFrame.interpolate, rCurrentInterpolate.puiSmokeTrails[i], rResult.vecSelfPosition);
#endif
			Explode(rFrame, rStaticData, i, false);
			continue;
		}

		const engine::SegmentHit& rTerrainHit = rCollisionScratch.terrainHits.at(uiIndex);
		const engine::SegmentHit& rBoundaryHit = rCollisionScratch.boundaryHits.at(uiIndex);
		if (rTerrainHit.bHit && (!rBoundaryHit.bHit || rTerrainHit.fTime <= rBoundaryHit.fTime))
		{
			rCurrentInterpolate.pVecPositions[i] = rTerrainHit.vecPosition;
#if defined(BT_CLIENT)
			SyncMissileTrail(rFrame.interpolate, rCurrentInterpolate.puiSmokeTrails[i], rTerrainHit.vecPosition);
#endif
			if ((rCurrentPostRender.pFlags[i] & kFalling) && XMVectorGetZ(rTerrainHit.vecPosition) <= 0.0f)
			{
				rCurrentPostRender.pFlags[i].Set(kSilentDespawn);
			}
			else
			{
				Explode(rFrame, rStaticData, i, true);
			}
		}
		else if (rBoundaryHit.bHit) [[unlikely]]
		{
			rCurrentPostRender.pFlags[i].Set(kTransfer);
		}
	}
}

} // namespace game
