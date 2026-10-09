#include "PointLights.h"

#if defined(BT_CLIENT)

#include "Ui/WrapperBase.h"

namespace engine
{

void PointLightsInterpolate::Update([[maybe_unused]] game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
	PointLightsInterpolate& __restrict rCurrent = rFrameInterpolate.pointLights;
	const PointLightsInterpolate& rPrevious = rPreviousFrame.interpolate.pointLights;
	std::chrono::duration<float> currentTime(rPreviousFrame.interpolate.fCurrentTime + rFrameInterpolate.fDeltaTime);

	if (rCurrent.iCount == 0)
	{
		return;
	}

	// Note: Owner is responsible for writing position each frame via idToIndexMap.at()

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		float fRotation = rPrevious.pfRotations[i];
		float fVisibleArea = rPrevious.pfVisibleAreas[i];
		float fVisibleIntensity = rPrevious.pfVisibleIntensities[i];
		float fLightingArea = rPrevious.pfLightingAreas[i];
		float fLightingIntensity = rPrevious.pfLightingIntensities[i];

		// AllocateAndCopy preserves controller type, start time, and base rotation.
		int64_t iControllerTypeIndex = rCurrent.puiControllerTypeIndices[i];
		std::chrono::duration<float> startTime(rCurrent.pfStartTimes[i]);
		float fBaseRotation = rCurrent.pfBaseRotations[i];

		if (iControllerTypeIndex != kiInvalidControllerType)
		{
			std::chrono::duration<float> elapsedTime = currentTime - startTime;
			const ControllerType& rController = sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));

			ControllerKeyframe interpolated = InterpolateScaledKeyframes(rController, elapsedTime.count(), [](ControllerType& rScaledController, const ControllerType& rOriginalController, int64_t j)
			{
				if (rOriginalController.ppVisibleAreaScales[j] != nullptr)
				{
					rScaledController.keyframes[j].fVisibleArea *= rOriginalController.ppVisibleAreaScales[j]->mfCurrent;
				}
				if (rOriginalController.ppVisibleIntensityScales[j] != nullptr)
				{
					rScaledController.keyframes[j].fVisibleIntensity *= rOriginalController.ppVisibleIntensityScales[j]->mfCurrent;
				}
				if (rOriginalController.ppLightingAreaScales[j] != nullptr)
				{
					rScaledController.keyframes[j].fLightingArea *= rOriginalController.ppLightingAreaScales[j]->mfCurrent;
				}
				if (rOriginalController.ppLightingIntensityScales[j] != nullptr)
				{
					rScaledController.keyframes[j].fLightingIntensity *= rOriginalController.ppLightingIntensityScales[j]->mfCurrent;
				}
			});

			fVisibleArea = interpolated.fVisibleArea;
			fVisibleIntensity = interpolated.fVisibleIntensity;
			fLightingArea = interpolated.fLightingArea;
			fLightingIntensity = interpolated.fLightingIntensity;
			fRotation = fBaseRotation + interpolated.fRotation;
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pfRotations[i] = fRotation;
		rCurrent.pfVisibleAreas[i] = fVisibleArea;
		rCurrent.pfVisibleIntensities[i] = fVisibleIntensity;
		rCurrent.pfLightingAreas[i] = fLightingArea;
		rCurrent.pfLightingIntensities[i] = fLightingIntensity;
	}
}

void PointLightsInterpolate::Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData)
{
	PointLightsInterpolate& rPointLights = rFrameInterpolate.pointLights;
	int64_t iIndex = rPointLights.idToIndexMap.at(id);

	rPointLights.pVecPositions[iIndex] = XMVectorSetW(rData.vecPosition, 1.0f);
	rPointLights.pfVisibleAreas[iIndex] = rData.fVisibleArea;
	rPointLights.pfVisibleIntensities[iIndex] = rData.fVisibleIntensity;
	rPointLights.pfLightingAreas[iIndex] = rData.fLightingArea;
	rPointLights.pfLightingIntensities[iIndex] = rData.fLightingIntensity;
	rPointLights.pfRotations[iIndex] = rData.fRotation;
}

void PointLightsPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
}

void PointLightsPostRender::Add(game::Frame& __restrict rFrame, point_lights_t& rId, int64_t iTypeIndex)
{
	ASSERT(!(rId.uuid.iValue != 0));

	PointLightsInterpolate& rInterpolate = rFrame.interpolate.pointLights;
	PointLightsPostRender& rPostRender = rFrame.postRender.pointLights;

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	auto [iSpawnIndex, newId] = AddVisualIndexableElement(rInterpolate, rPostRender, rFrame.postRender);
	rId = newId;
	rPostRender.pIds[iSpawnIndex] = newId;

	ZeroMemberRow(iSpawnIndex, rInterpolate.Members());
	rInterpolate.pVecPositions[iSpawnIndex] = XMVectorSetW(XMVectorZero(), 1.0f);
	rInterpolate.puiTypeIndices[iSpawnIndex] = static_cast<uint8_t>(iTypeIndex);
	rInterpolate.puiControllerTypeIndices[iSpawnIndex] = static_cast<uint8_t>(kiInvalidControllerType);
}

void XM_CALLCONV PointLightsPostRender::AddControlled(game::Frame& __restrict rFrame, std::chrono::duration<float> currentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition, float fRotation)
{
	PointLightsInterpolate& rInterpolate = rFrame.interpolate.pointLights;
	PointLightsPostRender& rPostRender = rFrame.postRender.pointLights;

	const ControllerType& rController = PointLightsInterpolate::sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));

	AddControlledElement(rInterpolate, currentTime.count(), iControllerTypeIndex, vecPosition, [&rInterpolate, &rPostRender]()
	{
		GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	},
	[&rInterpolate, &rPostRender, &rFrame]()
	{
		auto [iSpawnIndex, newId] = AddVisualIndexableElement(rInterpolate, rPostRender, rFrame.postRender);
		rPostRender.pIds[iSpawnIndex] = newId;
		return iSpawnIndex;
	},
	[&rInterpolate, &rController, fRotation](int64_t iSpawnIndex)
	{
		rInterpolate.puiTypeIndices[iSpawnIndex] = static_cast<uint8_t>(rController.iBaseTypeIndex);
		rInterpolate.pfVisibleAreas[iSpawnIndex] = rController.keyframes[0].fVisibleArea * (rController.ppVisibleAreaScales[0] != nullptr ? rController.ppVisibleAreaScales[0]->mfCurrent : 1.0f);
		rInterpolate.pfVisibleIntensities[iSpawnIndex] = rController.keyframes[0].fVisibleIntensity * (rController.ppVisibleIntensityScales[0] != nullptr ? rController.ppVisibleIntensityScales[0]->mfCurrent : 1.0f);
		rInterpolate.pfLightingAreas[iSpawnIndex] = rController.keyframes[0].fLightingArea * (rController.ppLightingAreaScales[0] != nullptr ? rController.ppLightingAreaScales[0]->mfCurrent : 1.0f);
		rInterpolate.pfLightingIntensities[iSpawnIndex] = rController.keyframes[0].fLightingIntensity * (rController.ppLightingIntensityScales[0] != nullptr ? rController.ppLightingIntensityScales[0]->mfCurrent : 1.0f);
		rInterpolate.pfRotations[iSpawnIndex] = fRotation + rController.keyframes[0].fRotation;
		rInterpolate.pfBaseRotations[iSpawnIndex] = fRotation;
	});
}

} // namespace engine

#endif // BT_CLIENT
