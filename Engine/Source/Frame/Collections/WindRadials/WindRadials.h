#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"
#include "Frame/GridCoord.h"

namespace engine
{

struct FrameStaticData;

struct WindRadialKeyframe
{
	float fIntensity = 0.0f;
	float fSize = 0.0f;

	static WindRadialKeyframe Interpolate(const WindRadialKeyframe& rPreviousKeyframe, const WindRadialKeyframe& rNextKeyframe, float fPercent)
	{
		return
		{
			.fIntensity = std::lerp(rPreviousKeyframe.fIntensity, rNextKeyframe.fIntensity, fPercent),
			.fSize = std::lerp(rPreviousKeyframe.fSize, rNextKeyframe.fSize, fPercent),
		};
	}

	bool operator==(const WindRadialKeyframe& rOther) const = default;
};

struct WindRadialControllerType
{
	int64_t iKeyframeCount = 2;
	bool bDestroysSelf = true;
	std::chrono::duration<float> times[kiMaximumControllerKeyframes] {};
	WindRadialKeyframe keyframes[kiMaximumControllerKeyframes] {};

	bool operator==(const WindRadialControllerType& rOther) const = default;
};

struct WindRadialsInterpolate : public Collection<WindRadialsInterpolate>,
	public ControllerTypeRegistry<WindRadialsInterpolate, WindRadialControllerType>
{
	static constexpr const char* kpcName = "WindRadials";
	static constexpr common::crc_t kuiCrc = common::CrcConsteval("WindRadials");

	static void Update(game::FrameInterpolate& __restrict rFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	XMVECTOR* __restrict pVecPositions = nullptr;
	float* __restrict pfIntensities = nullptr;
	float* __restrict pfSizes = nullptr;

	uint8_t* __restrict puiControllerTypeIndices = nullptr;
	float* __restrict pfStartTimes = nullptr;
	float* __restrict pfBaseIntensities = nullptr;
	float* __restrict pfBaseSizes = nullptr;

	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pfIntensities, rSelf.pfSizes, rSelf.puiControllerTypeIndices, rSelf.pfStartTimes, rSelf.pfBaseIntensities, rSelf.pfBaseSizes);
	}
	auto PersistentMembers(this auto&& rSelf)
	{
		return std::tie(rSelf.puiControllerTypeIndices, rSelf.pfStartTimes, rSelf.pfBaseIntensities, rSelf.pfBaseSizes);
	}

	static void GraphicsResources();

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoords);
	static void Render(const game::FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
};

struct WindRadialsPostRender : public Collection<WindRadialsPostRender>
{
	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const FrameStaticData& rStaticData);
	static void Destroy(game::Frame& __restrict rFrame, const FrameStaticData& rStaticData);

	// Add controlled wind radial (fire-and-forget, auto-destroys when animation ends)
	static void XM_CALLCONV AddControlled(game::Frame& __restrict rFrame, float fCurrentTime, int64_t iControllerTypeIndex, FXMVECTOR vecPosition, float fBaseIntensity, float fBaseSize);

	auto Members([[maybe_unused]] this auto&& rSelf)
	{
		return std::tie();
	}

};

extern template struct Collection<WindRadialsInterpolate>;
extern template struct Collection<WindRadialsPostRender>;

} // namespace engine

#endif // BT_CLIENT
