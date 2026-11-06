#include "CellStaticData.h"

#include "Frame/IslandTerrain.h"

namespace engine
{

void CellStaticData::Write(std::ostream& rStream) const
{
	common::Write(rStream, static_cast<int32_t>(std::ssize(islands)));
	for (const IslandPlacement& rPlacement : islands)
	{
		common::Write(rStream, rPlacement.islandCrc);
		common::Write(rStream, rPlacement.f2WorldPosition);
		common::Write(rStream, rPlacement.fRotation);
	}
	common::Write(rStream, static_cast<int32_t>(std::ssize(navigationData)));
	for (const NavData& rNavData : navigationData)
	{
		rNavData.Write(rStream);
	}
}

void CellStaticData::Read(std::istream& rStream)
{
	int32_t iCount = 0;
	common::Read(rStream, iCount);
	islands.resize(iCount);
	for (IslandPlacement& rPlacement : islands)
	{
		common::Read(rStream, rPlacement.islandCrc);
		common::Read(rStream, rPlacement.f2WorldPosition);
		common::Read(rStream, rPlacement.fRotation);
	}
	int32_t iGraphCount = 0;
	common::Read(rStream, iGraphCount);
	navigationData.resize(iGraphCount);
	for (NavData& rNavData : navigationData)
	{
		rNavData.Read(rStream);
	}
	// Wire-received navigationData is authoritative (client never rebuilds — server-only NavContour), so mark it
	// built regardless of vertex count.
	bNavigationDataBuilt = true;
}

void CellStaticData::BuildRenderPlacementCache(const IslandTerrain& rIslandTerrain) const
{
	islandRenderQueries.resize(static_cast<size_t>(std::ssize(islands)));
	for (int64_t i = 0; i < std::ssize(islands); ++i)
	{
		const IslandPlacement& rPlacement = islands.at(i);
		const IslandTemplate& rTemplate = rIslandTerrain.mIslands.at(rPlacement.islandCrc);
		IslandRenderQuery& rQuery = islandRenderQueries.at(i);

		// Negated rotation matches GlobalElevation's world-to-local convention and BlendPlacementIntoGrid.
		// The uncached render path uses library trigonometry, so its results can differ slightly from this cache.
		common::SinCos rotation = common::DeterministicSinCos(-rPlacement.fRotation);
		rQuery.fCosine = rotation.fCos;
		rQuery.fSine = rotation.fSin;
		rQuery.f2WorldPosition = rPlacement.f2WorldPosition;
		rQuery.fFootprintX = rTemplate.fQuadFootprintX;
		rQuery.fFootprintY = rTemplate.fQuadFootprintY;
		rQuery.puiHeightmapHalf = rTemplate.puiHeightmapHalf;
		rQuery.iHeightmapWidth = rTemplate.iHeightmapWidth;
		rQuery.iHeightmapHeight = rTemplate.iHeightmapHeight;
	}
}

} // namespace engine
