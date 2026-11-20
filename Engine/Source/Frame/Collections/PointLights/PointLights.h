#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"
#include "Frame/GridCoord.h"

namespace engine
{

class Wrapper;
struct CellStaticData;

struct PointLightsType
{
	common::crc_t uiCrc = 0;
	uint32_t uiColor = 0xFFFFFFFF;
	float fVisibleIntensity = 1.0f;
	float fLightingArea = 1.0f;
	float fLightingIntensity = 1.0f;
	bool bCameraAligned = false;

	Wrapper* pVisibleIntensityWrapper = nullptr;
	Wrapper* pLightingAreaWrapper = nullptr;
	Wrapper* pLightingIntensityWrapper = nullptr;
};

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
};

struct ControllerType
{
	int64_t iBaseTypeIndex = 0;                                // Base Type for color/texture
	int64_t iKeyframeCount = 2;                                // Actual keyframes used (2-4)
	std::chrono::duration<float> pTimes[kiMaximumControllerKeyframes] {};                  // Keyframe times (relative to start)
	ControllerKeyframe pKeyframes[kiMaximumControllerKeyframes] {};   // Keyframe states (normalized when wrappers present)

	// Per-keyframe wrapper scaling: keyframe values are multiplied by wrapper.mfCurrent at interpolation time
	Wrapper* ppVisibleAreaScales[kiMaximumControllerKeyframes] {};
	Wrapper* ppVisibleIntensityScales[kiMaximumControllerKeyframes] {};
	Wrapper* ppLightingAreaScales[kiMaximumControllerKeyframes] {};
	Wrapper* ppLightingIntensityScales[kiMaximumControllerKeyframes] {};
};

struct PointLightsInterpolate : public Collection<PointLightsInterpolate, CollectionFlags::kIdToIndex>,
	public TypeRegistry<PointLightsType>,
	public ControllerTypeRegistry<PointLightsInterpolate, ControllerType>
{
	static constexpr const char* kpcName = "PointLights";
	static constexpr common::crc_t kCrc = common::CrcConsteval("PointLights");

	static void Update(game::FrameInterpolate& __restrict rFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	struct SyncData
	{
		XMVECTOR vecPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		float fVisibleArea = 0.0f;
		float fRotation = 0.0f;
	};

	// Sync owned point light with parent-provided data; Render takes its intensities and lighting area from its type.
	static void Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData);

	uint8_t* __restrict puiTypeIndices = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	float* __restrict pfRotations = nullptr;

	// Owner Sync writes the visible area directly; controllers animate all four from their keyframes. Render uses the
	// other three only for controlled rows.
	float* __restrict pfVisibleAreas = nullptr;
	float* __restrict pfVisibleIntensities = nullptr;
	float* __restrict pfLightingAreas = nullptr;
	float* __restrict pfLightingIntensities = nullptr;

	// Controller fields (kiInvalidControllerType = not controlled)
	uint8_t* __restrict puiControllerTypeIndices = nullptr;
	SimulationTime* __restrict pStartTimes = nullptr;
	float* __restrict pfBaseRotations = nullptr;

	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.puiTypeIndices, rSelf.pVecPositions, rSelf.pfRotations, rSelf.pfVisibleAreas, rSelf.pfVisibleIntensities, rSelf.pfLightingAreas, rSelf.pfLightingIntensities, rSelf.puiControllerTypeIndices, rSelf.pStartTimes, rSelf.pfBaseRotations);
	}
	auto PersistentMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.puiTypeIndices, rSelf.puiControllerTypeIndices, rSelf.pStartTimes, rSelf.pfBaseRotations);
	}

	static void GraphicsResources();

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords);
	static void Render(const game::FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
};
using point_lights_t = PointLightsInterpolate::id_t;

struct PointLightsPostRender : public Collection<PointLightsPostRender>
{
	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData);

	// Add non-controlled point light
	static void Add(game::Frame& __restrict rFrame, point_lights_t& rId, int64_t iTypeIndex);

	// Fire-and-forget keyframe animation; the row is removed after the final keyframe.
	static void XM_CALLCONV AddControlled(game::Frame& __restrict rFrame, GridCoord coordinate, SimulationTime currentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition, float fRotation);


	// Destroy marks expired controlled lights for removal.
	static void Destroy(game::Frame& __restrict rFrame, const CellStaticData& rStaticData);

	point_lights_t* __restrict pIds = nullptr;
	LifecycleFlags_t* __restrict pLifecycleFlags = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds, rSelf.pLifecycleFlags);
	}

};

extern template struct Collection<PointLightsInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<PointLightsPostRender>;

} // namespace engine

#endif // BT_CLIENT
