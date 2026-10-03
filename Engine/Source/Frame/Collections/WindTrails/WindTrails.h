#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"
#include "Frame/GridCoord.h"

namespace engine
{

struct FrameStaticData;

struct WindTrailsInterpolate : public Collection<WindTrailsInterpolate, CollectionFlags::kIdToIndex>
{
	static constexpr const char* kpcName = "WindTrails";
	static constexpr common::crc_t kuiCrc = common::CrcConsteval("WindTrails");
	static constexpr bool kbManualRender = true;

	static void AllocateAndCopy(WindTrailsInterpolate& rCurrent, const WindTrailsInterpolate& rPrevious);

	// SyncData for parent-provided values
	struct SyncData
	{
		XMVECTOR vecPosition {};
		float fIntensity = 0.0f;
		float fWidth = 0.0f;
		float fLengthMultiplier = 1.0f;
	};

	static void Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData);

	static void Update(game::FrameInterpolate& __restrict rFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	XMVECTOR* __restrict pVecPositions = nullptr;
	float* __restrict pfIntensities = nullptr;
	float* __restrict pfWidths = nullptr;
	float* __restrict pfLengthMultipliers = nullptr;

	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pVecPositions, rSelf.pfIntensities, rSelf.pfWidths, rSelf.pfLengthMultipliers);
	}

	static void GraphicsResources();

	inline static std::unordered_map<id_t, XMVECTOR> sPreviousPositions;

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates);
	static void Render(const game::FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
};
using wind_trail_t = WindTrailsInterpolate::id_t;

struct WindTrailsPostRender : public Collection<WindTrailsPostRender>
{
	static void AllocateAndCopy(WindTrailsPostRender& rCurrent, const WindTrailsPostRender& rPrevious);

	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const FrameStaticData& rStaticData);

	// Add wind trail (Sync pattern - owner manages lifetime)
	static void Add(game::Frame& __restrict rFrame, wind_trail_t& rId);


	wind_trail_t* __restrict pIds = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds);
	}

};

extern template struct Collection<WindTrailsInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<WindTrailsPostRender>;

} // namespace engine

#endif // BT_CLIENT
