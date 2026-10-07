#pragma once

#if defined(BT_CLIENT)

#include "Frame/Collections/Collection.h"
#include "Frame/GridCoord.h"

namespace engine
{

struct CellStaticData;

enum class BillboardFlags : uint8_t
{
	kOffscreenOnly   = 0x01,
	kOffscreenRotate = 0x02,
};
using BillboardFlags_t = common::Flags<BillboardFlags>;

struct BillboardsType
{
	common::crc_t uiCrc = 0;
	float fSize = 1.0f;
	float fAlpha = 1.0f;
	int64_t iGameType = 0;
};

struct BillboardsInterpolate : public Collection<BillboardsInterpolate, CollectionFlags::kIdToIndex>,
	public TypeRegistry<BillboardsType>
{
	static constexpr const char* kpcName = "Billboards";
	static constexpr common::crc_t kuiCrc = common::CrcConsteval("Billboards");

	// SyncData for parent-provided values
	struct SyncData
	{
		XMVECTOR vecPosition {};
		int64_t iTypeIndex = 0;
		BillboardFlags_t flags;
		float fRotation = 0.0f;
		float fExtra = 0.0f;
	};

	// Sync owned billboard with parent-provided data
	static void Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData);

	static void Update(const game::FrameInterpolate& __restrict rFrameInterpolate, const game::Frame& __restrict rPreviousFrame);

	uint8_t* __restrict puiTypeIndices = nullptr;
	BillboardFlags_t* __restrict pFlags = nullptr;
	float* __restrict pfRotations = nullptr;
	float* __restrict pfExtra = nullptr;
	XMVECTOR* __restrict pVecPositions = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.puiTypeIndices, rSelf.pFlags, rSelf.pfRotations, rSelf.pfExtra, rSelf.pVecPositions);
	}

	static void GraphicsResources();

	static void BeginRender(int64_t iCommandBuffer, const std::unordered_map<GridCoord, game::FrameInterpolate>& rRenderInterpolates, const std::vector<GridCoord>& rActiveCoordinates);
	static void Render(const game::FrameInterpolate& __restrict rFrameInterpolate, int64_t iCommandBuffer);
	static void EndRender(int64_t iCommandBuffer);
};
using billboard_t = BillboardsInterpolate::id_t;

struct BillboardsPostRender : public Collection<BillboardsPostRender>
{
	static void Update(const game::Frame& __restrict rFrame, const game::Frame& __restrict rPreviousFrame, const CellStaticData& rStaticData);
	static void Add(game::Frame& __restrict rFrame, billboard_t&& rId, int64_t) = delete;
	static void Add(game::Frame& __restrict rFrame, billboard_t& rId, int64_t iTypeIndex);

	billboard_t* __restrict pIds = nullptr;
	auto Members(this auto&& rSelf)
	{
		return std::tie(rSelf.pIds);
	}

};

extern template struct Collection<BillboardsInterpolate, CollectionFlags::kIdToIndex>;
extern template struct Collection<BillboardsPostRender>;

} // namespace engine

#endif // BT_CLIENT
