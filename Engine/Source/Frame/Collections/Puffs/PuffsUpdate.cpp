#include "Puffs.h"

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace engine
{

void PuffsInterpolate::Update([[maybe_unused]] game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
	PuffsInterpolate& __restrict rCurrent = rFrameInterpolate.puffs;
	const PuffsInterpolate& rPrevious = rPreviousFrame.interpolate.puffs;
	std::chrono::duration<float> currentTime(rPreviousFrame.interpolate.fCurrentTime + rFrameInterpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		float fIntensity = rPrevious.pfIntensities[i];
		float fArea = rPrevious.pfAreas[i];
		float fRotation = rPrevious.pfRotations[i];

		// Load controller fields (copied in AllocateAndCopy)
		int64_t iControllerTypeIndex = rCurrent.puiControllerTypeIndices[i];
		std::chrono::duration<float> startTime(rCurrent.pfStartTimes[i]);

		if (iControllerTypeIndex != kiInvalidControllerType)
		{
			std::chrono::duration<float> elapsedTime = currentTime - startTime;
			const PuffControllerType& rController = PuffsInterpolate::sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));

			PuffKeyframe interpolated = InterpolateScaledKeyframes(rController, elapsedTime.count(), [](PuffControllerType& rScaledController, const PuffControllerType& rOriginalController, int64_t j)
			{
				if (rOriginalController.ppAreaScales[j] != nullptr)
				{
					rScaledController.keyframes[j].fArea *= rOriginalController.ppAreaScales[j]->mfCurrent;
				}
				if (rOriginalController.ppIntensityScales[j] != nullptr)
				{
					rScaledController.keyframes[j].fIntensity *= rOriginalController.ppIntensityScales[j]->mfCurrent;
				}
			});

			fArea = interpolated.fArea;
			fIntensity = interpolated.fIntensity;
			fRotation = interpolated.fRotation;
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pfIntensities[i] = fIntensity;
		rCurrent.pfAreas[i] = fArea;
		rCurrent.pfRotations[i] = fRotation;
	}
}

void PuffsPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
}

void XM_CALLCONV PuffsPostRender::AddControlled(game::Frame& __restrict rFrame, std::chrono::duration<float> currentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition)
{
	PuffsInterpolate& rInterpolate = rFrame.interpolate.puffs;
	PuffsPostRender& rPostRender = rFrame.postRender.puffs;

	const PuffControllerType& rController = PuffsInterpolate::sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));

	AddControlledElement(rInterpolate, currentTime.count(), iControllerTypeIndex, vecPosition, [&rInterpolate, &rPostRender]()
	{
		GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	}, [&rInterpolate, &rPostRender]()
	{
		return AddElement(rInterpolate, rPostRender);
	}, [&rInterpolate, &rController](int64_t iSpawnIndex)
	{
		rInterpolate.puiTypeIndices[iSpawnIndex] = static_cast<uint8_t>(rController.iBaseTypeIndex);
		rInterpolate.pfAreas[iSpawnIndex] = rController.keyframes[0].fArea * (rController.ppAreaScales[0] != nullptr ? rController.ppAreaScales[0]->mfCurrent : 1.0f);
		rInterpolate.pfIntensities[iSpawnIndex] = rController.keyframes[0].fIntensity * (rController.ppIntensityScales[0] != nullptr ? rController.ppIntensityScales[0]->mfCurrent : 1.0f);
		rInterpolate.pfRotations[iSpawnIndex] = rController.keyframes[0].fRotation;
	});
}

} // namespace engine

#endif // BT_CLIENT
