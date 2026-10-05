#include "FrameStaticData.h"

#include "Frame/IslandTerrain.h"

namespace engine
{

void FrameStaticData::Write(std::ostream& rStream, bool bIncludeNavigationData) const
{
	common::Write(rStream, static_cast<int32_t>(std::ssize(islands)));
	for (const IslandPlacement& rPlacement : islands)
	{
		common::Write(rStream, rPlacement.islandCrc);
		common::Write(rStream, rPlacement.f2WorldPosition);
		common::Write(rStream, rPlacement.fRotation);
	}
	if (bIncludeNavigationData)
	{
		navigationData.Write(rStream);
	}
}

void FrameStaticData::Read(std::istream& rStream, bool bIncludeNavigationData)
{
	int32_t iCount = 0;
	common::Read(rStream, iCount);
	// Trust boundary (save / network full-state): enforce the generated per-cell contract and bound the count
	// against the stream before resize.
	common::ValidateDeserializedCountCapacity(iCount, kiMaximumIslandsPerCell, sizeof(IslandPlacement::islandCrc) + sizeof(IslandPlacement::f2WorldPosition) + sizeof(IslandPlacement::fRotation), rStream, "FrameStaticData::Read");
	islands.resize(iCount);
	for (IslandPlacement& rPlacement : islands)
	{
		common::Read(rStream, rPlacement.islandCrc);
		common::Read(rStream, rPlacement.f2WorldPosition);
		common::Read(rStream, rPlacement.fRotation);
	}
	if (bIncludeNavigationData)
	{
		navigationData.Read(rStream);
		// Wire-received navigationData is authoritative (client never rebuilds — server-only NavContour), so mark it
		// built regardless of vertex count.
		bNavigationDataBuilt = true;
	}
	else
	{
		navigationData = {};
		// Save-load only (the network-receive path takes the if-branch above with bIncludeNavigationData true): clear so
		// RunFrameTick (server) rebuilds navigationData via BuildCellNavigationData from the freshly-read placements.
		bNavigationDataBuilt = false;
	}
	// Never serialized — purely local derived data. Clear so a network resend / save-load
	// forces RunFrameTick to rebuild from the freshly-read placements.
	elevationGrid = {};
	islandRenderQueries = {};
}

void FrameStaticData::BuildRenderPlacementCache(const IslandTerrain& rIslandTerrain) const
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
