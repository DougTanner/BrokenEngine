#include "Billboards.h"

#if defined(BT_CLIENT)

namespace engine
{

using enum BillboardFlags;

void BillboardsInterpolate::Update([[maybe_unused]] const game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
}

void BillboardsInterpolate::Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData)
{
	BillboardsInterpolate& rBillboards = rFrameInterpolate.billboards;
	int64_t iIndex = rBillboards.idToIndexMap.at(id);

	rBillboards.pVecPositions[iIndex] = XMVectorSetW(rData.vecPosition, 1.0f);
	rBillboards.puiTypeIndices[iIndex] = rData.uiTypeIndex;
	rBillboards.pFlags[iIndex] = rData.flags;
	rBillboards.pfRotations[iIndex] = rData.fRotation;
	rBillboards.pfExtra[iIndex] = rData.fExtra;
}

void BillboardsPostRender::Update([[maybe_unused]] const game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const FrameStaticData& rStaticData)
{
}

void BillboardsPostRender::Add(game::Frame& __restrict rFrame, billboard_t& rId, uint8_t uiTypeIndex)
{
	ASSERT(!(rId.uuid.iValue != 0));

	BillboardsInterpolate& rInterpolate = rFrame.interpolate.billboards;
	BillboardsPostRender& rPostRender = rFrame.postRender.billboards;

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	auto [iSpawnIndex, newId] = AddVisualIndexableElement(rInterpolate, rPostRender, rFrame.postRender);
	rId = newId;
	rPostRender.pIds[iSpawnIndex] = newId;
	ZeroMemberRow(iSpawnIndex, rInterpolate.Members());
	rInterpolate.puiTypeIndices[iSpawnIndex] = uiTypeIndex;
}

} // namespace engine

#endif // BT_CLIENT
