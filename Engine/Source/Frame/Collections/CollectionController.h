#pragma once

#include "CollectionLifecycle.h"

namespace engine
{

#if defined(BT_CLIENT)
class Wrapper;
#endif


inline constexpr int64_t kiMaximumControllerKeyframes = 4;
inline constexpr int64_t kiInvalidTypeIndex = 0xFF;
inline constexpr int64_t kiInvalidControllerType = kiInvalidTypeIndex;

template <typename CONTROLLER_TYPE>
inline std::remove_extent_t<decltype(CONTROLLER_TYPE::pKeyframes)> InterpolateKeyframes(const CONTROLLER_TYPE& rController, std::chrono::duration<float> elapsedTime)
{
	using KeyframeType = std::remove_extent_t<decltype(CONTROLLER_TYPE::pKeyframes)>;
	int64_t iKeyframeCount = rController.iKeyframeCount;

	if (elapsedTime.count() <= rController.pTimes[0].count())
	{
		return rController.pKeyframes[0];
	}

	for (int64_t j = 1; j < iKeyframeCount; ++j)
	{
		if (elapsedTime < rController.pTimes[j])
		{
			std::chrono::duration<float> previousTime = rController.pTimes[j - 1];
			float fPercent = (elapsedTime - previousTime) / (rController.pTimes[j] - previousTime);
			return KeyframeType::Interpolate(rController.pKeyframes[j - 1], rController.pKeyframes[j], fPercent);
		}
	}

	return rController.pKeyframes[iKeyframeCount - 1];
}

template <typename CONTROLLER_TYPE, typename SCALE_FUNCTION>
inline std::remove_extent_t<decltype(CONTROLLER_TYPE::pKeyframes)> InterpolateScaledKeyframes(const CONTROLLER_TYPE& rController, std::chrono::duration<float> elapsedTime, SCALE_FUNCTION ScaleFunction)
{
	CONTROLLER_TYPE scaledController = rController;
	for (int64_t j = 0; j < rController.iKeyframeCount; ++j)
	{
		ScaleFunction(scaledController, rController, j);
	}
	return InterpolateKeyframes(scaledController, elapsedTime);
}

// Spawns a paired controlled element while leaving collection-specific seeding to the caller.
template <typename INTERPOLATE, typename POST_RENDER, typename ADD_FUNCTION, typename SEED_FUNCTION>
void XM_CALLCONV AddControlledElement(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, SimulationTime currentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition, ADD_FUNCTION AddFunction, SEED_FUNCTION SeedFunction)
{
	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	int64_t iSpawnIndex = AddFunction();

	rInterpolate.pVecPositions[iSpawnIndex] = XMVectorSetW(vecPosition, 1.0f);
	SeedFunction(iSpawnIndex);
	rInterpolate.puiControllerTypeIndices[iSpawnIndex] = static_cast<uint8_t>(iControllerTypeIndex);
	rInterpolate.pStartTimes[iSpawnIndex] = currentTime;
}

// Registration occurs on one thread before Dispatch() workers start; controller types remain immutable during frame ticks.
template <typename COLLECTION, typename CONTROLLER_TYPE>
struct ControllerTypeRegistry
{
	static inline std::vector<CONTROLLER_TYPE> sControllerTypes;

	static void RegisterControllerType(int64_t& riIndex, const CONTROLLER_TYPE& rType)
	{
		ASSERT(riIndex == kiInvalidTypeIndex);
		ASSERT(std::ssize(sControllerTypes) < kiInvalidTypeIndex);
		ASSERT(rType.iKeyframeCount >= 2);
		ASSERT(rType.iKeyframeCount <= kiMaximumControllerKeyframes);
		for (int64_t i = 1; i < rType.iKeyframeCount; ++i)
		{
			ASSERT(rType.pTimes[i] >= rType.pTimes[i - 1]);
		}
		riIndex = std::ssize(sControllerTypes);
		sControllerTypes.push_back(rType);
	}

};

// Marks each row past its controller's final keyframe for the Destroy-phase compaction fold.
template <typename INTERPOLATE, typename POST_RENDER>
void DestroyExpiredControlled(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, SimulationTime currentTime)
{
	for (int64_t i = 0; i < rInterpolate.iCount; ++i)
	{
		int64_t iControllerTypeIndex = rInterpolate.puiControllerTypeIndices[i];
		if (iControllerTypeIndex == kiInvalidControllerType)
		{
			continue;
		}

		const typename decltype(INTERPOLATE::sControllerTypes)::value_type& rController = INTERPOLATE::sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));
		std::chrono::nanoseconds elapsedTime = currentTime - rInterpolate.pStartTimes[i];
		if (elapsedTime > common::FloatSecondsToNanoseconds(rController.pTimes[rController.iKeyframeCount - 1])) [[unlikely]]
		{
			MarkDestroy(rPostRender.pLifecycleFlags[i]);
		}
	}
}

} // namespace engine
