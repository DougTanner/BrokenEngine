#include "AreaLights.h"

#if defined(BT_CLIENT)

namespace engine
{

void AreaLightsInterpolate::Update([[maybe_unused]] game::FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame)
{
}

void AreaLightsInterpolate::Sync(game::FrameInterpolate& rFrameInterpolate, id_t id, const SyncData& rData)
{
	AreaLightsInterpolate& rAreaLights = rFrameInterpolate.areaLights;
	int64_t iIndex = rAreaLights.idToIndexMap.at(id);

	rAreaLights.puiTypeIndices[iIndex] = static_cast<uint8_t>(rData.iTypeIndex);
	rAreaLights.pVecVisiblePositions[0][iIndex] = rData.vecVisiblePositions[0];
	rAreaLights.pVecVisiblePositions[1][iIndex] = rData.vecVisiblePositions[1];
	rAreaLights.pVecVisiblePositions[2][iIndex] = rData.vecVisiblePositions[2];
	rAreaLights.pVecVisiblePositions[3][iIndex] = rData.vecVisiblePositions[3];
	rAreaLights.pfIntensityMultipliers[iIndex] = rData.fIntensityMultiplier;

}

void AreaLightsPostRender::Update([[maybe_unused]] game::Frame& __restrict rFrame, [[maybe_unused]] const game::Frame& __restrict rPreviousFrame, [[maybe_unused]] const CellStaticData& rStaticData)
{
}

void AreaLightsPostRender::Add(game::Frame& __restrict rFrame, area_lights_t& rId, int64_t iTypeIndex)
{
	ASSERT(!(rId.uuid.iValue != 0));

	AreaLightsInterpolate& rInterpolate = rFrame.interpolate.areaLights;
	AreaLightsPostRender& rPostRender = rFrame.postRender.areaLights;

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	auto [iSpawnIndex, newId] = AddVisualIndexableElement(rInterpolate, rPostRender, rFrame.postRender);
	rId = newId;
	rPostRender.pIds[iSpawnIndex] = newId;
	ZeroMemberRow(iSpawnIndex, rInterpolate.Members());
	rInterpolate.puiTypeIndices[iSpawnIndex] = static_cast<uint8_t>(iTypeIndex);
	rInterpolate.pfIntensityMultipliers[iSpawnIndex] = 1.0f;
}

} // namespace engine

#endif // BT_CLIENT
