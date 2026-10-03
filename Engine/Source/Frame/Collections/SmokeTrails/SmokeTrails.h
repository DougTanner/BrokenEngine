#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"
#include "Frame/GridCoord.h"

namespace engine
{

struct FrameStaticData;

struct SmokeTrailsType
{
	common::crc_t uiCrc = 0;
	uint32_t uiColor = 0xFFFFFFFF;
	float fWidth = 1.0f;
};

struct SmokeTrailsInterpolate : public Collection<SmokeTrailsInterpolate, CollectionFlags::kIdToIndex>,
	public TypeRegistry<SmokeTrailsType>
{
	static constexpr const char* kpcName = "SmokeTrails";
	static constexpr common::crc_t kuiCrc = common::CrcConsteval("SmokeTrails");
	static constexpr bool kbManualRender = true;

	static void AllocateAndCopy(SmokeTrailsInterpolate& rCurrent, const SmokeTrailsInterpolate& rPrevious);

	// SyncData for parent-provided values
	struct SyncData
	{
		XMVECTOR vecPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		float fIntensity = 0.0f;
	};

	// Sync owned trail with parent-provided data
	static void Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData);

	static void Update(game::FrameInterpolate& __restrict rFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	uint8_t* __restrict puiTypeIndices = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	XMVECTOR* __restrict pVecSmoothedPositions = nullptr;
	float* __restrict pfIntensities = nullptr;
	float* __restrict pfStartTimes = nullptr;

	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.puiTypeIndices, rSelf.pVecPositions, rSelf.pVecSmoothedPositions, rSelf.pfIntensities, rSelf.pfStartTimes);
	}

	static void GraphicsResources();

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates);
	static void Render(const game::FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
};
using smoke_trails_t = SmokeTrailsInterpolate::id_t;

struct SmokeTrailsPostRender : public Collection<SmokeTrailsPostRender>
{
	static void AllocateAndCopy(SmokeTrailsPostRender& rCurrent, const SmokeTrailsPostRender& rPrevious);

	static void Update(game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const FrameStaticData& rStaticData);

	static void Add(game::Frame& __restrict rFrame, smoke_trails_t& rId, uint8_t uiTypeIndex);


	smoke_trails_t* __restrict pIds = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds);
	}

};

extern template struct Collection<SmokeTrailsInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<SmokeTrailsPostRender>;

} // namespace engine

#endif // BT_CLIENT
