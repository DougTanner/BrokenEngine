#pragma once

namespace engine
{

#if defined(BT_CLIENT)
class Wrapper;
#endif


inline constexpr int64_t kiMaximumControllerKeyframes = 4;
inline constexpr int64_t kiInvalidTypeIndex = 0xFF;
inline constexpr int64_t kiInvalidControllerType = kiInvalidTypeIndex;

struct ControllerKeyframe
{
	float fVisibleArea = 0.0f;
	float fVisibleIntensity = 0.0f;
	float fLightingArea = 0.0f;
	float fLightingIntensity = 0.0f;
	float fRotation = 0.0f;

	static ControllerKeyframe Interpolate(const ControllerKeyframe& rA, const ControllerKeyframe& rB, float fPercent)
	{
		return
		{
			.fVisibleArea = std::lerp(rA.fVisibleArea, rB.fVisibleArea, fPercent),
			.fVisibleIntensity = std::lerp(rA.fVisibleIntensity, rB.fVisibleIntensity, fPercent),
			.fLightingArea = std::lerp(rA.fLightingArea, rB.fLightingArea, fPercent),
			.fLightingIntensity = std::lerp(rA.fLightingIntensity, rB.fLightingIntensity, fPercent),
			.fRotation = std::lerp(rA.fRotation, rB.fRotation, fPercent),
		};
	}

	bool operator==(const ControllerKeyframe& rOther) const = default;
};

struct ControllerType
{
	int64_t iBaseTypeIndex = 0;                                // Base Type for color/texture
	int64_t iKeyframeCount = 2;                                // Actual keyframes used (2-4)
	bool bDestroysSelf = true;                                  // Auto-remove when animation ends
	std::chrono::duration<float> pTimes[kiMaximumControllerKeyframes] {};                  // Keyframe times (relative to start)
	ControllerKeyframe pKeyframes[kiMaximumControllerKeyframes] {};   // Keyframe states (normalized when wrappers present)

#if defined(BT_CLIENT)
	// Per-keyframe wrapper scaling: keyframe values are multiplied by wrapper.mfCurrent at interpolation time
	Wrapper* ppVisibleAreaScales[kiMaximumControllerKeyframes] {};
	Wrapper* ppVisibleIntensityScales[kiMaximumControllerKeyframes] {};
	Wrapper* ppLightingAreaScales[kiMaximumControllerKeyframes] {};
	Wrapper* ppLightingIntensityScales[kiMaximumControllerKeyframes] {};
#endif

	bool operator==(const ControllerType& rOther) const = default;
};

template <typename CONTROLLER_TYPE>
inline std::remove_extent_t<decltype(CONTROLLER_TYPE::pKeyframes)> InterpolateKeyframes(const CONTROLLER_TYPE& rController, std::chrono::duration<float> elapsedTime)
{
	using KeyframeType = std::remove_extent_t<decltype(CONTROLLER_TYPE::pKeyframes)>;
	int64_t iKeyframeCount = rController.iKeyframeCount;

	if (elapsedTime.count() <= rController.pTimes[0].count())
	{
		return rController.pKeyframes[0];
	}
	if (elapsedTime.count() >= rController.pTimes[iKeyframeCount - 1].count())
	{
		return rController.pKeyframes[iKeyframeCount - 1];
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
template <typename INTERPOLATE, typename GROW_FUNCTION, typename ADD_FUNCTION, typename SEED_FUNCTION>
void XM_CALLCONV AddControlledElement(INTERPOLATE& rInterpolate, SimulationTime currentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition, GROW_FUNCTION GrowFunction, ADD_FUNCTION AddFunction, SEED_FUNCTION SeedFunction)
{
	GrowFunction();
	int64_t iSpawnIndex = AddFunction();

	rInterpolate.pVecPositions[iSpawnIndex] = XMVectorSetW(vecPosition, 1.0f);
	SeedFunction(iSpawnIndex);
	rInterpolate.puiControllerTypeIndices[iSpawnIndex] = static_cast<uint8_t>(iControllerTypeIndex);
	rInterpolate.pStartTimes[iSpawnIndex] = currentTime;
}

// PointLights use ControllerType; Puffs use a custom controller type.
// Registration occurs on one thread before Dispatch() workers start; controller types remain immutable during frame ticks.
template <typename COLLECTION, typename CONTROLLER_TYPE = ControllerType>
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

// The removal callback takes the loop index by reference.
template <typename INTERPOLATE, typename POST_RENDER, typename REMOVE_FUNCTION>
void DestroyExpiredControlled(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, SimulationTime currentTime, REMOVE_FUNCTION RemoveFunction)
{
	for (int64_t i = 0; i < rInterpolate.iCount; ++i)
	{
		int64_t iControllerTypeIndex = rInterpolate.puiControllerTypeIndices[i];
		if (iControllerTypeIndex == kiInvalidControllerType)
		{
			continue;
		}

		const typename decltype(INTERPOLATE::sControllerTypes)::value_type& rController = INTERPOLATE::sControllerTypes.at(static_cast<size_t>(iControllerTypeIndex));
		if (!rController.bDestroysSelf)
		{
			continue;
		}

		std::chrono::nanoseconds elapsedTime = currentTime - rInterpolate.pStartTimes[i];
		if (elapsedTime > common::FloatSecondsToNanoseconds(rController.pTimes[rController.iKeyframeCount - 1])) [[unlikely]]
		{
			RemoveFunction(rInterpolate, rPostRender, i);
		}
	}
}

} // namespace engine
