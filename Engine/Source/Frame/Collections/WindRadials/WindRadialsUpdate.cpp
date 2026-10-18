#include "WindRadials.h"

#if defined(BT_CLIENT)

#include "Ui/GraphicsSettingsWrappersBase.h"
#include "Ui/WrapperBase.h"

namespace engine
{

void WindRadialsInterpolate::Update([[maybe_unused]] game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
	WindRadialsInterpolate& __restrict rCurrent = rFrameInterpolate.windRadials;
	const WindRadialsInterpolate& rPrevious = rPreviousFrame.interpolate.windRadials;

	if (!gWindEnabled.Get<bool>())
	{
		for (int64_t i = 0; i < rCurrent.iCount; ++i)
		{
			rCurrent.pVecPositions[i] = rPrevious.pVecPositions[i];
		}

		return;
	}

	SimulationTime currentTime = TickToSimulationTime(rPreviousFrame.interpolate.iTick) + common::FloatSecondsToNanoseconds(std::chrono::duration<float>(rFrameInterpolate.fDeltaTime));

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		float fBaseIntensity = rCurrent.pfBaseIntensities[i];
		float fBaseSize = rCurrent.pfBaseSizes[i];

		int64_t iControllerTypeIndex = rCurrent.puiControllerTypeIndices[i];
		SimulationTime startTime = rCurrent.pStartTimes[i];
		std::chrono::duration<float> elapsedTime(common::NanosecondsToFloatSeconds<float>(currentTime - startTime));
		const WindRadialControllerType& rController = WindRadialsInterpolate::sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));
		WindRadialKeyframe interpolated = InterpolateKeyframes(rController, elapsedTime);

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pfIntensities[i] = fBaseIntensity * interpolated.fIntensity;
		rCurrent.pfSizes[i] = fBaseSize * interpolated.fSize;
	}
}

void WindRadialsPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
}

void XM_CALLCONV WindRadialsPostRender::AddControlled(game::Frame& __restrict rFrame, SimulationTime currentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition, float fBaseIntensity, float fBaseSize)
{
	WindRadialsInterpolate& rInterpolate = rFrame.interpolate.windRadials;
	WindRadialsPostRender& rPostRender = rFrame.postRender.windRadials;

	const WindRadialControllerType& rController = WindRadialsInterpolate::sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));

	AddControlledElement(rInterpolate, currentTime, iControllerTypeIndex, vecPosition, [&rInterpolate, &rPostRender]()
	{
		GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	}, [&rInterpolate, &rPostRender]()
	{
		return AddElement(rInterpolate, rPostRender);
	}, [&rInterpolate, &rController, fBaseIntensity, fBaseSize](int64_t iSpawnIndex)
	{
		rInterpolate.pfIntensities[iSpawnIndex] = fBaseIntensity * rController.pKeyframes[0].fIntensity;
		rInterpolate.pfSizes[iSpawnIndex] = fBaseSize * rController.pKeyframes[0].fSize;
		rInterpolate.pfBaseIntensities[iSpawnIndex] = fBaseIntensity;
		rInterpolate.pfBaseSizes[iSpawnIndex] = fBaseSize;
	});
}

} // namespace engine

#endif // BT_CLIENT
