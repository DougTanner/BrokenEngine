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
	std::chrono::duration<float> times[kiMaximumControllerKeyframes] {};                  // Keyframe times (relative to start)
	ControllerKeyframe keyframes[kiMaximumControllerKeyframes] {};   // Keyframe states (normalized when wrappers present)

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
inline std::remove_extent_t<decltype(CONTROLLER_TYPE::keyframes)> InterpolateKeyframes(const CONTROLLER_TYPE& rController, float fElapsedTime)
{
	using KeyframeType = std::remove_extent_t<decltype(CONTROLLER_TYPE::keyframes)>;
	int64_t iKeyframeCount = rController.iKeyframeCount;

	if (fElapsedTime <= rController.times[0].count())
	{
		return rController.keyframes[0];
	}
	// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound) — registered controllers always have iKeyframeCount >= 2; the analyzer's count==0 path cannot occur
	if (fElapsedTime >= rController.times[iKeyframeCount - 1].count())
	{
		return rController.keyframes[iKeyframeCount - 1];
	}

	for (int64_t j = 1; j < iKeyframeCount; ++j)
	{
		if (fElapsedTime < rController.times[j].count())
		{
			float fPreviousTime = rController.times[j - 1].count();
			float fPercent = (fElapsedTime - fPreviousTime) / (rController.times[j].count() - fPreviousTime);
			return KeyframeType::Interpolate(rController.keyframes[j - 1], rController.keyframes[j], fPercent);
		}
	}

	return rController.keyframes[iKeyframeCount - 1];
}

template <typename CONTROLLER_TYPE, typename SCALE_FUNCTION>
inline std::remove_extent_t<decltype(CONTROLLER_TYPE::keyframes)> InterpolateScaledKeyframes(const CONTROLLER_TYPE& rController, float fElapsedTime, SCALE_FUNCTION ScaleFunction)
{
	CONTROLLER_TYPE scaledController = rController;
	for (int64_t j = 0; j < rController.iKeyframeCount; ++j)
	{
		ScaleFunction(scaledController, rController, j);
	}
	return InterpolateKeyframes(scaledController, fElapsedTime);
}

// Spawns a paired controlled element while leaving collection-specific seeding to the caller.
template <typename INTERPOLATE, typename POST_RENDER, typename GROW_FUNCTION, typename ADD_FUNCTION, typename SEED_FUNCTION>
void XM_CALLCONV AddControlledElement(INTERPOLATE& rInterpolate, [[maybe_unused]] const POST_RENDER& rPostRender, float fCurrentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition, GROW_FUNCTION GrowFunction, ADD_FUNCTION AddFunction, SEED_FUNCTION SeedFunction)
{
	GrowFunction();
	int64_t iSpawnIndex = AddFunction();

	rInterpolate.pVecPositions[iSpawnIndex] = XMVectorSetW(vecPosition, 1.0f);
	SeedFunction(iSpawnIndex);
	rInterpolate.puiControllerTypeIndices[iSpawnIndex] = static_cast<uint8_t>(iControllerTypeIndex);
	rInterpolate.pfStartTimes[iSpawnIndex] = fCurrentTime;
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
			ASSERT(rType.times[i] >= rType.times[i - 1]);
		}
		riIndex = std::ssize(sControllerTypes);
		sControllerTypes.push_back(rType);
	}

};

// The removal callback takes the loop index by reference.
template <typename INTERPOLATE, typename POST_RENDER, typename REMOVE_FUNCTION>
void DestroyExpiredControlled(INTERPOLATE& rInterpolate, POST_RENDER& rPostRender, float fCurrentTime, REMOVE_FUNCTION RemoveFunction)
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

		float fElapsedTime = fCurrentTime - rInterpolate.pfStartTimes[i];
		if (fElapsedTime > rController.times[rController.iKeyframeCount - 1].count()) [[unlikely]]
		{
			RemoveFunction(rInterpolate, rPostRender, i);
		}
	}
}

} // namespace engine
