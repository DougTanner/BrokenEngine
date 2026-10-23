#include "Missiles.h"

#include "Frame/Collections/Collection.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"


namespace game
{

using enum MissileFlags;

// Collision storage is thread-local because frame ticks run in parallel.
static thread_local int64_t siCollisionLayerIndex = 0;

constexpr float kfAccelerationAtMaximumDeltaAngle = 0.9f;
constexpr float kfVelocityDecay = 1.0f;
constexpr float kfVelocityToDirection = 16.0f;
constexpr std::chrono::duration<float> kJitterIntervalRandom = std::chrono::duration<float>(0.0025f);
constexpr float kfDirectionJitterRandom = 0.06f;
constexpr float kfDeltaAngleJitterRandom = 0.5f;
constexpr float kfDeltaAngleJitterRandomWithTarget = 1.0f;
constexpr float kfDeltaRotationChange = 0.925f;
constexpr float kfDeltaRotationDecay = 8.0f;
constexpr float kfDeltaRotationTowardsTarget = 10.0f;
constexpr float kfDeltaRotationTowardsStored = 3.0f;

#if defined(BT_CLIENT)
// Defined in Missiles.cpp, where ClientInitialize also calls it.
void XM_CALLCONV SynchronizeMissile(FrameInterpolate& rFrameInterpolate, engine::area_lights_t uiAreaLight, engine::smoke_trails_t uiSmokeTrail, engine::sound_t uiSound, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecVelocity, GXMVECTOR vecPreviousPosition, MissileFlags_t flags, engine::LifecycleFlags_t lifecycleFlags, float fPitch, float fDeltaRotation, float fExhaustLength);
void XM_CALLCONV SynchronizeMissileTrail(FrameInterpolate& rFrameInterpolate, engine::smoke_trails_t uiSmokeTrail, FXMVECTOR vecPosition);
#endif

void MissilesInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rCurrentFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	MissilesInterpolate& rCurrent = *rCurrentFrameInterpolate.pMissiles;
	const MissilesInterpolate& rPrevious = *rPreviousFrame.interpolate.pMissiles;
	const MissilesPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pMissiles;
	std::chrono::duration<float> deltaTime(rCurrentFrameInterpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPreviousPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecPosition = vecPreviousPosition;
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		std::chrono::duration<float> destroyedTime = rPrevious.pDestroyedTimes[i];
		MissileFlags_t flags = rPreviousPostRender.pFlags[i];
		engine::LifecycleFlags_t lifecycleFlags = rPreviousPostRender.pLifecycleFlags[i];

		if (!(lifecycleFlags & engine::LifecycleFlags::kDying)) [[likely]]
		{
			vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count()), rPreviousPostRender.pVecVelocities[i], vecPosition);
			// Positions must always have W=1.0 — prevents W-lane drift via MultiplyAdd.
			vecPosition = XMVectorSetW(vecPosition, 1.0f);

			if (!(flags & kFalling))
			{
				// PostRender::Update already applies the rotation delay percentage.
				vecDirection = XMVector3Normalize(XMVector4Transform(vecDirection, XMMatrixRotationZ(deltaTime.count() * rPreviousPostRender.pfDeltaRotations[i])));
			}
		}

		if (destroyedTime > std::chrono::duration<float>::zero())
		{
			destroyedTime = std::max(destroyedTime - deltaTime, std::chrono::duration<float>::zero());
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pDestroyedTimes[i] = destroyedTime;

		// Sync owned objects (IDs copied in AllocateAndCopy)
#if defined(BT_CLIENT)
		SynchronizeMissile(rCurrentFrameInterpolate, rCurrent.puiAreaLights[i], rCurrent.puiSmokeTrails[i], rPreviousPostRender.puiSounds[i], vecPosition, vecDirection, rPreviousPostRender.pVecVelocities[i], vecPreviousPosition, flags, lifecycleFlags, rPreviousPostRender.pfPitches[i], rPreviousPostRender.pfDeltaRotations[i], rPreviousPostRender.pfExhaustLengths[i]);
#endif // BT_CLIENT
	}
}

void MissilesPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	MissilesPostRender& __restrict rCurrent = *rFrame.postRender.pMissiles;
	const MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	const MissilesPostRender& rPrevious = *rPreviousFrame.postRender.pMissiles;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	// Before the ship Updates, current registry ids and interpolated positions still align with previous rows.
	// Bind previous positions for retained-target homing, previous arrival grace for eligibility, and previous
	// missile handles while current handles are being updated. A missile reads only the context of the flight plane at
	// its height.
	RegistryWindow window = BuildShipRegistryWindow(rFrame, &rPreviousFrame, *rPreviousFrame.postRender.pMissiles);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Load dynamic fields (static fields copied via memcpy in AllocateAndCopy)
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		engine::registry_id_t uiTarget = rPrevious.puiRegistryTargets[i];
		std::chrono::duration<float> missileTime = rPrevious.pTimes[i] + deltaTime;
		float fDeltaRotation = rPrevious.pfDeltaRotations[i];
		std::chrono::duration<float> deltaRotationDelay = rPrevious.pDeltaRotationDelays[i];
		float fExhaustLength = rPrevious.pfExhaustLengths[i];
		std::chrono::duration<float> nextJitter = rPrevious.pNextJitter[i];
		XMVECTOR vecStoredDirection = rPrevious.pVecStoredDirections[i];
		std::optional<int64_t> optionalFlightPlane = FlightPlaneAtHeight(XMVectorGetZ(rCurrentInterpolate.pVecPositions[i]));

		if (!(rCurrent.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && !(rCurrent.pFlags[i] & kFalling) && missileTime < kMissileLifetime) [[likely]]
		{
			nextJitter -= deltaTime;

			vecVelocity = XMVectorMultiply(XMVectorReplicate(1.0f - deltaTime.count() * kfVelocityDecay), vecVelocity);

			float fDeltaAnglePercent = std::abs(fDeltaRotation) / rCurrent.pfDeltaRotationMaximum[i];
			fDeltaAnglePercent = std::clamp(fDeltaAnglePercent, 0.0f, 1.0f);
			float fAdjustedAcceleration = (1.0f - fDeltaAnglePercent) * rCurrent.pfAccelerations[i] + fDeltaAnglePercent * kfAccelerationAtMaximumDeltaAngle * rCurrent.pfAccelerations[i];
			vecVelocity = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count() * fAdjustedAcceleration), rCurrentInterpolate.pVecDirections[i], vecVelocity);

			float fVelocityToDirectionPercent = 1.0f - deltaTime.count() * kfVelocityToDirection;
			XMVECTOR vecVelocityComponent = XMVectorMultiply(XMVectorReplicate(fVelocityToDirectionPercent), XMVector3Normalize(vecVelocity));
			XMVECTOR vecDirectionComponent = XMVectorMultiply(XMVectorReplicate(1.0f - fVelocityToDirectionPercent), rCurrentInterpolate.pVecDirections[i]);
			vecVelocity = XMVectorMultiply(XMVector3Length(vecVelocity), XMVector3Normalize(XMVectorAdd(vecVelocityComponent, vecDirectionComponent)));

			fDeltaRotation = (1.0f - deltaTime.count() * kfDeltaRotationDecay) * fDeltaRotation;

			if (nextJitter < std::chrono::duration<float>::zero())
			{
				nextJitter = std::chrono::duration<float>(common::Random<kJitterIntervalRandom.count()>(rFrame.postRender.randomEngine));

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

			// Validate the retained handle against the rows the registry currently considers eligible on the missile's
			// plane; a source that died, transferred, is still in arrival grace, or left the missile's plane drops the
			// handle.
			engine::RegistryResult retained {};
			bool bRetained = false;
			if ((uiTarget.uuid.iValue != 0))
			{
				ASSERT(optionalFlightPlane.has_value());
				bRetained = engine::ResolveRegistryHandle(window.contexts[static_cast<size_t>(*optionalFlightPlane)], uiTarget, retained);
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
				deltaRotationDelay -= deltaTime;

				XMVECTOR vecTargetPosition = retained.vecPreviousPosition;
				XMVECTOR vecToTargetNormal = XMVector3Normalize(XMVectorSubtract(vecTargetPosition, rCurrentInterpolate.pVecPositions[i]));
				float fDirectionDestinationCrossZ = XMVectorGetZ(XMVector3Cross(rCurrentInterpolate.pVecDirections[i], vecToTargetNormal));
				float fWantedDeltaRotation = fDirectionDestinationCrossZ > 0.0f ? kfDeltaRotationTowardsTarget : -kfDeltaRotationTowardsTarget;

				float fDelayPercent = std::clamp(1.0f - deltaRotationDelay / kMissileDeltaRotationDelay, 0.0f, 1.0f);
				fWantedDeltaRotation *= fDelayPercent;

				fDeltaRotation = kfDeltaRotationChange * fDeltaRotation + (1.0f - kfDeltaRotationChange) * fWantedDeltaRotation;
			}
			else
			{
				deltaRotationDelay -= deltaTime;

				XMVECTOR vecCurrentDirection = rCurrentInterpolate.pVecDirections[i];
				float fDirectionCrossZ = XMVectorGetZ(XMVector3Cross(vecCurrentDirection, vecStoredDirection));
				float fWantedDeltaRotation = fDirectionCrossZ > 0.0f ? kfDeltaRotationTowardsStored : -kfDeltaRotationTowardsStored;

				float fDelayPercent = std::clamp(1.0f - deltaRotationDelay / kMissileDeltaRotationDelay, 0.0f, 1.0f);
				fWantedDeltaRotation *= fDelayPercent;

				fDeltaRotation = kfDeltaRotationChange * fDeltaRotation + (1.0f - kfDeltaRotationChange) * fWantedDeltaRotation;
			}

			fDeltaRotation = common::ClampMagnitude(fDeltaRotation, rCurrent.pfDeltaRotationMaximum[i]);

			vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);
		}
		else if (rCurrent.pFlags[i] & kFalling)
		{
			vecVelocity = XMVectorSetZ(vecVelocity, XMVectorGetZ(vecVelocity) - kfMissileGravity * deltaTime.count());
		}

		// Save dynamic fields (static fields copied via memcpy in AllocateAndCopy)
		if constexpr (kbDebugBreak)
		{
			common::ValidateVector<false>(vecVelocity);
		}
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pVecStoredDirections[i] = vecStoredDirection;
		rCurrent.puiRegistryTargets[i] = uiTarget;
		rCurrent.pTimes[i] = missileTime;
		rCurrent.pDeltaRotationDelays[i] = deltaRotationDelay;
		rCurrent.pfDeltaRotations[i] = fDeltaRotation;
		rCurrent.pfExhaustLengths[i] = fExhaustLength;
		rCurrent.pNextJitter[i] = nextJitter;

		if (!(rCurrent.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && !(rCurrent.pFlags[i] & kFalling) && missileTime.count() >= kMissileLifetime.count())
		{
			// The only release site: this is the one missile transition that happens while the window is live.
			ASSERT(optionalFlightPlane.has_value());
			engine::ReleaseRegistryTarget(window.contexts[static_cast<size_t>(*optionalFlightPlane)], rCurrent.puiRegistryTargets[i]);
			Fall(rFrame, i, deltaTime);
		}
	}

	// Acquire targetless flying missiles after all handles and expiry releases are published, one flight plane at a time
	// in its own context, in ascending row order within the plane. Fixed-size stack arrays bound acquisition scratch
	// storage; each chunk updates shared subscription counts before the next.
	static constexpr int64_t kiAcquireChunk = 64;
	int64_t piAcquireRows[kiAcquireChunk] {};
	engine::RegistryResult pAcquireResults[kiAcquireChunk] {};
	int64_t iAcquireCount = 0;

	auto AcquireChunk = [&](engine::RegistryQueryContext& rContext)
	{
		engine::AcquireRegistryTargets(rContext,
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

	for (int64_t j = 0; j < std::ssize(window.contexts); ++j)
	{
		engine::RegistryQueryContext& rContext = window.contexts[j];

		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			if (rCurrent.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
			{
				continue;
			}

			if (rCurrent.pFlags[i] & kFalling)
			{
				continue;
			}

			if (rCurrent.pTimes[i].count() >= kMissileLifetime.count())
			{
				continue;
			}

			if ((rCurrent.puiRegistryTargets[i].uuid.iValue != 0))
			{
				continue;
			}

			if (FlightPlaneAtHeight(XMVectorGetZ(rCurrentInterpolate.pVecPositions[i])) != j)
			{
				continue;
			}

			// /analyze loses the iAcquireCount reset because it happens inside AcquireChunk: the flush below runs on
			// every write, so the counter is always below the chunk size here.
			_Analysis_assume_(iAcquireCount >= 0 && iAcquireCount < kiAcquireChunk);
			piAcquireRows[iAcquireCount++] = i;
			if (iAcquireCount == kiAcquireChunk)
			{
				AcquireChunk(rContext);
			}
		}

		if (iAcquireCount > 0)
		{
			AcquireChunk(rContext);
		}
	}
}

void MissilesPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pMissiles->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.fRadius = kfMissileCollisionRadius,
		.fDamage = 0.0f,  // Damage via area damage system
		.bSweptTest = true,
		.uiCategory = CollisionCategory::kuiMissile,
		.uiCollidesWith = CollidesWith::kuiMissile,
		.pAlignments = rCurrentPostRender.pAlignments,
		.pTerrainStaticData = &rStaticData,
	});

	std::span<engine::CollisionFlags_t> flags = engine::Collision::GetLayerFlags(siCollisionLayerIndex);
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		flags[static_cast<size_t>(i)] = (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {engine::CollisionFlags::kDestroyOnCollide};
	}
}

void MissilesPostRender::PostCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

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

		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDestroy) [[unlikely]]
		{
			continue;
		}

		// Entity results are pre-filtered against terrain and cell-exit cutoffs.
		engine::CollisionOutcome outcome = engine::Collision::GetOutcome(siCollisionLayerIndex, i);
		if (outcome.eType == engine::CollisionOutcomeType::kEntity)
		{
			const engine::CollisionResult& rResult = engine::Collision::GetCollisions(siCollisionLayerIndex, i).front();
			rCurrentInterpolate.pVecPositions[i] = rResult.vecSelfPosition;
#if defined(BT_CLIENT)
			SynchronizeMissileTrail(rFrame.interpolate, rCurrentInterpolate.puiSmokeTrails[i], rResult.vecSelfPosition);
#endif
			Explode(rFrame, rStaticData, i, false);
			continue;
		}

		if (outcome.eType == engine::CollisionOutcomeType::kTerrain)
		{
			rCurrentInterpolate.pVecPositions[i] = outcome.vecTerrainPosition;
#if defined(BT_CLIENT)
			SynchronizeMissileTrail(rFrame.interpolate, rCurrentInterpolate.puiSmokeTrails[i], outcome.vecTerrainPosition);
#endif
			if ((rCurrentPostRender.pFlags[i] & kFalling) && XMVectorGetZ(outcome.vecTerrainPosition) <= 0.0f)
			{
				rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDestroy);
			}
			else
			{
				Explode(rFrame, rStaticData, i, true);
			}
		}
		else if (outcome.eType == engine::CollisionOutcomeType::kCellExit) [[unlikely]]
		{
			rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kTransfer);
		}
	}
}

} // namespace game
