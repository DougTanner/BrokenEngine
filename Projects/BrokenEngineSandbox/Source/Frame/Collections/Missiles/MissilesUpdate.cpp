#include "Missiles.h"

#include "Frame/Collections/Collection.h"
#include "Frame/FrameStaticData.h"

#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"


namespace game
{

using enum MissileFlags;

// Collision storage is thread-local because frame ticks run in parallel.
static thread_local int64_t siCollisionLayerIndex = 0;
static thread_local std::vector<engine::CollisionFlags_t> sCollisionFlags;
static thread_local std::vector<float> sCollisionRadii;
static thread_local std::vector<float> sCollisionDamages;

struct MissileCollisionIntervalScratch
{
	std::vector<float> startTimes;
	std::vector<float> endTimes;
	std::vector<float> maximumTimes;
	std::vector<engine::SegmentHit> terrainHits;
	std::vector<engine::SegmentHit> boundaryHits;
};

static MissileCollisionIntervalScratch& GetMissileCollisionIntervalScratch()
{
	// Function-local thread-local storage defers allocation-free vector construction until first use; growth sites suppress allocation tracking.
	static thread_local MissileCollisionIntervalScratch sScratch;
	return sScratch;
}

constexpr float kfAccelerationAtMaximumDeltaAngle = 0.9f;
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
// Forward declaration of SynchronizeMissile (defined in Missiles.cpp, also used by ClientInit)
void XM_CALLCONV SynchronizeMissile(FrameInterpolate& rFrameInterpolate, engine::area_lights_t uiAreaLight, engine::smoke_trails_t uiSmokeTrail, engine::sound_t uiSound, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecVelocity, GXMVECTOR vecPreviousPosition, MissileFlags_t flags, float fPitch, float fDeltaRotation, float fExhaustLength);
void XM_CALLCONV SynchronizeMissileTrail(FrameInterpolate& rFrameInterpolate, engine::smoke_trails_t uiSmokeTrail, FXMVECTOR vecPosition);
#endif

void MissilesInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rCurrentFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	MissilesInterpolate& rCurrent = *rCurrentFrameInterpolate.pMissiles;
	const MissilesInterpolate& rPrevious = *rPreviousFrame.interpolate.pMissiles;
	const MissilesPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pMissiles;
	float fDeltaTime = rCurrentFrameInterpolate.fDeltaTime;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
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
				// PostRender::Update already applies the rotation delay percentage.
				vecDirection = XMVector3Normalize(XMVector4Transform(vecDirection, XMMatrixRotationZ(fDeltaTime * rPreviousPostRender.pfDeltaRotations[i])));
			}
		}

		if (fDestroyedTime > 0.0f)
		{
			fDestroyedTime = std::max(fDestroyedTime - fDeltaTime, 0.0f);
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pfDestroyedTimes[i] = fDestroyedTime;

		// Sync owned objects (IDs copied in AllocateAndCopy)
#if defined(BT_CLIENT)
		SynchronizeMissile(rCurrentFrameInterpolate, rCurrent.puiAreaLights[i], rCurrent.puiSmokeTrails[i], rPreviousPostRender.puiSounds[i], vecPosition, vecDirection, rPreviousPostRender.pVecVelocities[i], vecPreviousPosition, flags, rPreviousPostRender.pfPitches[i], rPreviousPostRender.pfDeltaRotations[i], rPreviousPostRender.pfExhaustLengths[i]);
#endif // BT_CLIENT
	}
}

void MissilesPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	MissilesPostRender& __restrict rCurrent = *rFrame.postRender.pMissiles;
	const MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	const MissilesPostRender& rPrevious = *rPreviousFrame.postRender.pMissiles;
	float fDeltaTime = rFrame.interpolate.fDeltaTime;

	// Before Spaceships Update, current registry ids and interpolated positions still align with previous rows.
	// Bind previous positions for retained-target homing, previous arrival grace for eligibility, and previous
	// missile handles while current handles are being updated.
	RegistryWindow window = BuildSpaceshipRegistryWindow(rFrame, rPreviousFrame.interpolate.pSpaceships->pVecPositions, rPreviousFrame.postRender.pSpaceships->pfArrivalGracePeriods, *rPreviousFrame.postRender.pMissiles);

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

		if (!(rCurrent.pFlags[i] & kExploding) && !(rCurrent.pFlags[i] & kFalling) && fTime < kMissileLifetime.count()) [[likely]]
		{
			fNextJitter -= fDeltaTime;

			vecVelocity = XMVectorMultiply(XMVectorReplicate(1.0f - fDeltaTime * kfVelocityDecay), vecVelocity);

			float fDeltaAnglePercent = std::abs(fDeltaRotation) / rCurrent.pfDeltaRotationMaximum[i];
			fDeltaAnglePercent = std::clamp(fDeltaAnglePercent, 0.0f, 1.0f);
			float fAdjustedAcceleration = (1.0f - fDeltaAnglePercent) * rCurrent.pfAccelerations[i] + fDeltaAnglePercent * kfAccelerationAtMaximumDeltaAngle * rCurrent.pfAccelerations[i];
			vecVelocity = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime * fAdjustedAcceleration), rCurrentInterpolate.pVecDirections[i], vecVelocity);

			float fVelocityToDirectionPercent = 1.0f - fDeltaTime * kfVelocityToDirection;
			XMVECTOR vecVelocityComponent = XMVectorMultiply(XMVectorReplicate(fVelocityToDirectionPercent), XMVector3Normalize(vecVelocity));
			XMVECTOR vecDirectionComponent = XMVectorMultiply(XMVectorReplicate(1.0f - fVelocityToDirectionPercent), rCurrentInterpolate.pVecDirections[i]);
			vecVelocity = XMVectorMultiply(XMVector3Length(vecVelocity), XMVector3Normalize(XMVectorAdd(vecVelocityComponent, vecDirectionComponent)));

			fDeltaRotation = (1.0f - fDeltaTime * kfDeltaRotationDecay) * fDeltaRotation;

			if (fNextJitter < 0.0f)
			{
				fNextJitter = common::Random<kfJitterIntervalRandom>(rFrame.postRender.randomEngine);

				int64_t iRandom = common::Random(2i64, rFrame.postRender.randomEngine);
				float fDeltaAnglePercentExtra = 1.0f + 3.0f * fDeltaAnglePercent;
				float fDeltaAngleJitter = !(uiTarget.uuid.iValue != 0) ? kfDeltaAngleJitterRandom : kfDeltaAngleJitterRandomWithTarget;
				if (iRandom == 0)
				{
					vecVelocity = XMVector3RotateSafe(vecVelocity, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, fDeltaAnglePercentExtra * (-kfDirectionJitterRandom + common::Random(2.0f * kfDirectionJitterRandom, rFrame.postRender.randomEngine))));
					if constexpr (kbDebugBreak)
					{
						common::ValidateVector<false>(vecVelocity);
					}
				}
				if (iRandom == 1)
				{
					fDeltaRotation += fDeltaAnglePercentExtra * (-fDeltaAngleJitter + fDeltaAngleJitter * common::Random<2.0f>(rFrame.postRender.randomEngine));
				}
			}

			fExhaustLength = kfMissileExhaustLength + common::Random<kfMissileExhaustLengthRandom>(rFrame.postRender.randomEngine);

			// Validate the retained handle against the rows the registry currently considers eligible; a source
			// that died, transferred, or is still in arrival grace drops the handle.
			engine::RegistryResult retained {};
			bool bRetained = false;
			if ((uiTarget.uuid.iValue != 0))
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

			// Retained targets steer toward the source's previous-frame position.
			if (bRetained)
			{
				fDeltaRotationDelay -= fDeltaTime;

				XMVECTOR vecTargetPosition = retained.vecPreviousPosition;
				XMVECTOR vecToTargetNormal = XMVector3Normalize(XMVectorSubtract(vecTargetPosition, rCurrentInterpolate.pVecPositions[i]));
				float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(rCurrentInterpolate.pVecDirections[i], vecToTargetNormal));
				float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? kfDeltaRotationTowardsTarget : -kfDeltaRotationTowardsTarget;

				float fDelayPercent = std::clamp(1.0f - fDeltaRotationDelay / kMissileDeltaRotationDelay.count(), 0.0f, 1.0f);
				fWantedDeltaRotation *= fDelayPercent;

				fDeltaRotation = kfDeltaRotationChange * fDeltaRotation + (1.0f - kfDeltaRotationChange) * fWantedDeltaRotation;
			}
			else
			{
				fDeltaRotationDelay -= fDeltaTime;

				XMVECTOR vecCurrentDirection = rCurrentInterpolate.pVecDirections[i];
				float fDirectionCrossZ = XMVectorGetZ(XMVector3Cross(vecCurrentDirection, vecStoredDirection));
				float fWantedDeltaRotation = fDirectionCrossZ > 0.0f ? kfDeltaRotationTowardsStored : -kfDeltaRotationTowardsStored;

				float fDelayPercent = std::clamp(1.0f - fDeltaRotationDelay / kMissileDeltaRotationDelay.count(), 0.0f, 1.0f);
				fWantedDeltaRotation *= fDelayPercent;

				fDeltaRotation = kfDeltaRotationChange * fDeltaRotation + (1.0f - kfDeltaRotationChange) * fWantedDeltaRotation;
			}

			fDeltaRotation = common::ClampMagnitude(fDeltaRotation, rCurrent.pfDeltaRotationMaximum[i]);

			vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);
		}
		else if (rCurrent.pFlags[i] & kFalling)
		{
			vecVelocity = XMVectorSetZ(vecVelocity, XMVectorGetZ(vecVelocity) - kfMissileGravity * fDeltaTime);
		}

		// Save dynamic fields (static fields copied via memcpy in AllocateAndCopy)
		if constexpr (kbDebugBreak)
		{
			common::ValidateVector<false>(vecVelocity);
		}
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pVecStoredDirections[i] = vecStoredDirection;
		rCurrent.puiRegistryTargets[i] = uiTarget;
		rCurrent.pfTimes[i] = fTime;
		rCurrent.pfDeltaRotationDelays[i] = fDeltaRotationDelay;
		rCurrent.pfDeltaRotations[i] = fDeltaRotation;
		rCurrent.pfExhaustLengths[i] = fExhaustLength;
		rCurrent.pfNextJitter[i] = fNextJitter;

		if (!(rCurrent.pFlags[i] & kExploding) && !(rCurrent.pFlags[i] & kFalling) && fTime >= kMissileLifetime.count())
		{
			// The only release site: this is the one missile transition that happens while the window is live.
			engine::ReleaseRegistryTarget(window.context, rCurrent.puiRegistryTargets[i]);
			Fall(rFrame, i, std::chrono::duration<float>(fDeltaTime));
		}
	}

	// Acquire targetless flying missiles in ascending row order after all handles and expiry releases are published.
	// Fixed-size stack arrays bound acquisition scratch storage; each chunk updates shared subscription counts before the next.
	static constexpr int64_t kiAcquireChunk = 64;
	int64_t piAcquireRows[kiAcquireChunk] {};
	engine::RegistryResult pAcquireResults[kiAcquireChunk] {};
	int64_t iAcquireCount = 0;

	auto AcquireChunk = [&]()
	{
		engine::AcquireRegistryTargets(window.context,
		{
			.pTargets = rCurrent.puiRegistryTargets,
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
		if (rCurrent.pFlags[i] & kExploding)
		{
			continue;
		}

		if (rCurrent.pFlags[i] & kFalling)
		{
			continue;
		}

		if (rCurrent.pfTimes[i] >= kMissileLifetime.count())
		{
			continue;
		}

		if ((rCurrent.puiRegistryTargets[i].uuid.iValue != 0))
		{
			continue;
		}

		// /analyze loses the iAcquireCount reset because it happens inside AcquireChunk: the flush below runs on
		// every write, so the counter is always below the chunk size here.
		_Analysis_assume_(iAcquireCount >= 0 && iAcquireCount < kiAcquireChunk);
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
	// Collision array pointers passed to AddLayer must remain valid through PostCollision; thread-local vectors retain capacity.
	ScopedSuppressAllocationTracking suppress;

	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	int64_t iCount = rCurrentInterpolate.iCount;
	sCollisionFlags.resize(static_cast<size_t>(iCount));
	sCollisionRadii.resize(static_cast<size_t>(iCount));
	sCollisionDamages.resize(static_cast<size_t>(iCount));
	rCollisionScratch.startTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.endTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.maximumTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.terrainHits.resize(static_cast<size_t>(iCount));
	rCollisionScratch.boundaryHits.resize(static_cast<size_t>(iCount));
	const MissilesInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pMissiles;
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		int64_t iIndex = i;
		sCollisionFlags.at(static_cast<size_t>(iIndex)) = (rCurrentPostRender.pFlags[i] & kExploding) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {engine::CollisionFlags::kDestroyOnCollide};
		sCollisionRadii.at(static_cast<size_t>(iIndex)) = kfMissileCollisionRadius;
		sCollisionDamages.at(static_cast<size_t>(iIndex)) = 0.0f;  // Damage via area damage system
		rCollisionScratch.startTimes.at(static_cast<size_t>(iIndex)) = 0.0f;
		rCollisionScratch.endTimes.at(static_cast<size_t>(iIndex)) = 1.0f;
		rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex)) = engine::TracePointAgainstTerrain(rStaticData, rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex)) = engine::TracePointToFrameExit(engine::LocalFrameArea(), rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		float fMaximumTime = std::numeric_limits<float>::max();
		if (rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex)).bHit)
		{
			fMaximumTime = rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex)).fTime;
		}
		if (rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex)).bHit)
		{
			fMaximumTime = std::min(fMaximumTime, rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex)).fTime);
		}
		rCollisionScratch.maximumTimes.at(static_cast<size_t>(iIndex)) = fMaximumTime;
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousInterpolate.pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pfStartTimes = rCollisionScratch.startTimes.data(),
		.pfEndTimes = rCollisionScratch.endTimes.data(),
		.pfMaxTimes = rCollisionScratch.maximumTimes.data(),
		.pfRadii = sCollisionRadii.data(),
		.pfDamages = sCollisionDamages.data(),
		.pFlags = sCollisionFlags.data(),
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.bSweptTest = true,
		.uiCategory = CollisionCategory::kuiMissile,
		.uiCollidesWith = CollidesWith::kuiMissile,
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
		if (rCurrentPostRender.pFlags[i] & kExploding) [[unlikely]]
		{
			continue;
		}

		if (rCurrentPostRender.pFlags[i] & kSilentDespawn) [[unlikely]]
		{
			continue;
		}

		int64_t iIndex = i;
		// Entity results are pre-filtered against terrain and frame-exit cutoffs.
		if ((engine::Collision::sResultSpans[engine::Collision::sLayerBaseOffsets[siCollisionLayerIndex] + i].iCount > 0))
		{
			const engine::CollisionResult& rResult = engine::Collision::GetCollisions(siCollisionLayerIndex, i).front();
			rCurrentInterpolate.pVecPositions[i] = rResult.vecSelfPosition;
#if defined(BT_CLIENT)
			SynchronizeMissileTrail(rFrame.interpolate, rCurrentInterpolate.puiSmokeTrails[i], rResult.vecSelfPosition);
#endif
			Explode(rFrame, rStaticData, i, false);
			continue;
		}

		const engine::SegmentHit& rTerrainHit = rCollisionScratch.terrainHits.at(static_cast<size_t>(iIndex));
		const engine::SegmentHit& rBoundaryHit = rCollisionScratch.boundaryHits.at(static_cast<size_t>(iIndex));
		if (rTerrainHit.bHit && (!rBoundaryHit.bHit || rTerrainHit.fTime <= rBoundaryHit.fTime))
		{
			rCurrentInterpolate.pVecPositions[i] = rTerrainHit.vecPosition;
#if defined(BT_CLIENT)
			SynchronizeMissileTrail(rFrame.interpolate, rCurrentInterpolate.puiSmokeTrails[i], rTerrainHit.vecPosition);
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
