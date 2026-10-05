#include "SmokeTrails.h"

#if defined(BT_CLIENT)

namespace engine
{

constexpr float kfSmoothingRate = 20.0f;

void SmokeTrailsInterpolate::Update(game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
	SmokeTrailsInterpolate& rCurrent = rFrameInterpolate.smokeTrails;
	float fSmoothingInterpolant = common::ExponentialInterpolant(kfSmoothingRate, rFrameInterpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		if (XMVectorGetW(rCurrent.pVecSmoothedPositions[i]) == 0.0f)
		{
			rCurrent.pVecSmoothedPositions[i] = rCurrent.pVecPositions[i];
		}
		else
		{
			rCurrent.pVecSmoothedPositions[i] = XMVectorLerp(rCurrent.pVecSmoothedPositions[i], rCurrent.pVecPositions[i], fSmoothingInterpolant);
		}
	}
}

void SmokeTrailsInterpolate::Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData)
{
	SmokeTrailsInterpolate& rSmokeTrails = rFrameInterpolate.smokeTrails;
	int64_t iIndex = rSmokeTrails.idToIndexMap.at(id);

	rSmokeTrails.pVecPositions[iIndex] = XMVectorSetW(rData.vecPosition, 1.0f);
	rSmokeTrails.pfIntensities[iIndex] = rData.fIntensity;
}

void SmokeTrailsPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
}

void SmokeTrailsPostRender::Add(game::Frame& __restrict rFrame, smoke_trails_t& rId, int64_t iTypeIndex)
{
	ASSERT(!(rId.uuid.iValue != 0));

	SmokeTrailsInterpolate& rInterpolate = rFrame.interpolate.smokeTrails;
	SmokeTrailsPostRender& rPostRender = rFrame.postRender.smokeTrails;

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());

	auto [iSpawnIndex, id] = AddVisualIndexableElement(rInterpolate, rPostRender, rFrame.postRender);

	rId = id;
	rPostRender.pIds[iSpawnIndex] = id;
	ZeroMemberRow(iSpawnIndex, rInterpolate.Members());
	rInterpolate.puiTypeIndices[iSpawnIndex] = static_cast<uint8_t>(iTypeIndex);
	rInterpolate.pfStartTimes[iSpawnIndex] = rFrame.interpolate.fCurrentTime;
}

} // namespace engine

#endif // BT_CLIENT
