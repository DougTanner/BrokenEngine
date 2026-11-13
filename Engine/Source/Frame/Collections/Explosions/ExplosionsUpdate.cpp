#include "Explosions.h"

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

#include "Ui/SmokeWrappers.h"

namespace engine
{

// Shared helper defined in Explosions.cpp.
void XM_CALLCONV SyncExplosionTrail(game::FrameInterpolate& rFrameInterpolate, smoke_trails_t trailId, FXMVECTOR vecPosition, float fIntensity);

void ExplosionsInterpolate::Update(game::FrameInterpolate& __restrict rCurrentFrameInterpolate, const game::Frame& __restrict rPreviousFrame)
{
	ExplosionsInterpolate& rCurrent = rCurrentFrameInterpolate.explosions;
	const ExplosionsInterpolate& rPrevious = rPreviousFrame.interpolate.explosions;

	SimulationTime currentTime = TickToSimulationTime(rPreviousFrame.interpolate.iTick) + common::FloatSecondsToNanoseconds(std::chrono::duration<float>(rCurrentFrameInterpolate.fDeltaTime));

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		int64_t iTypeIndex = rPrevious.puiTypeIndices[i];
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];

		float fTimePercent = rPrevious.pfTimePercents[i];

		int64_t iTrailCount = rPrevious.piTrailCounts[i];

		rCurrent.puiTypeIndices[i] = static_cast<uint8_t>(iTypeIndex);
		rCurrent.pStartTimes[i] = rPrevious.pStartTimes[i];
		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;

		rCurrent.pfTimePercents[i] = fTimePercent;

		rCurrent.piTrailCounts[i] = static_cast<int32_t>(iTrailCount);

		// Copy trail data (not IDs - those are copied in AllocateAndCopy)
		for (int64_t j = 0; j < kiMaxExplosionTrails; ++j)
		{
			if (j < iTrailCount)
			{
				rCurrent.pTrailTimes[j][i] = rPrevious.pTrailTimes[j][i];
				rCurrent.pfTrailIntensities[j][i] = rPrevious.pfTrailIntensities[j][i];
				rCurrent.pVecTrailStartPositions[j][i] = rPrevious.pVecTrailStartPositions[j][i];
				rCurrent.pVecTrailEndPositions[j][i] = rPrevious.pVecTrailEndPositions[j][i];
			}
			else
			{
				rCurrent.pTrailTimes[j][i] = std::chrono::duration<float>::zero();
				rCurrent.pfTrailIntensities[j][i] = 0.0f;
				rCurrent.pVecTrailStartPositions[j][i] = XMVectorZero();
				rCurrent.pVecTrailEndPositions[j][i] = XMVectorZero();
			}
		}

		const ExplosionType& rType = sTypes.at(static_cast<size_t>(iTypeIndex));
		std::chrono::nanoseconds explosionTime = currentTime - rPrevious.pStartTimes[i];

		for (int64_t j = 0; j < iTrailCount; ++j)
		{
			smoke_trails_t trailId = rCurrent.pTrails[j][i];
			if (!(trailId.uuid.iValue != 0))
			{
				continue;
			}

			// j == 0 is the central trail along the explosion direction; j > 0 are angle-jittered side trails
			float fDurationMultiplier = (j == 0) ? game::gExplosionPrimaryTrailDuration.mfCurrent : game::gExplosionSecondaryTrailDuration.mfCurrent;
			std::chrono::duration<float> effectiveTrailTime = rCurrent.pTrailTimes[j][i] * fDurationMultiplier;
			std::chrono::duration<float> trailEndTime = fTimePercent * rType.trailDelayTime + effectiveTrailTime;

			// For expired trails, sync with zero intensity (they'll be removed in Destroy phase)
			if (explosionTime >= common::FloatSecondsToNanoseconds(trailEndTime))
			{
				XMVECTOR vecTrailEnd = rCurrent.pVecTrailEndPositions[j][i];
				SyncExplosionTrail(rCurrentFrameInterpolate, trailId, vecTrailEnd, 0.0f);
				continue;
			}

			float fTrailPercent = (std::chrono::duration<float>(common::NanosecondsToFloatSeconds<float>(explosionTime)) - fTimePercent * rType.trailDelayTime) / effectiveTrailTime;
			fTrailPercent = std::clamp(fTrailPercent, 0.0f, 1.0f);

			XMVECTOR vecTrailStart = rCurrent.pVecTrailStartPositions[j][i];
			XMVECTOR vecTrailEnd = rCurrent.pVecTrailEndPositions[j][i];
			XMVECTOR vecGravityOffset = XMVectorSet(0.0f, 0.0f, fTrailPercent * rType.fTrailGravity, 0.0f);

			XMVECTOR vecTrailPosition = XMVectorLerp(vecTrailStart, XMVectorSubtract(vecTrailEnd, vecGravityOffset), fTrailPercent);
			float fTrailIntensity = (1.0f - fTrailPercent) * rCurrent.pfTrailIntensities[j][i];

			SyncExplosionTrail(rCurrentFrameInterpolate, trailId, vecTrailPosition, fTrailIntensity);
		}
	}
}

void ExplosionsPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
}

} // namespace engine

#endif // BT_CLIENT
