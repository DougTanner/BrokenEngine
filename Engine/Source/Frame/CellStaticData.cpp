#include "CellStaticData.h"

#include "Frame/IslandTerrain.h"

namespace engine
{

static void WriteCompactInteger(std::ostream& rStream, int64_t iValue)
{
	ASSERT(iValue >= 0 && iValue <= INT32_MAX);
	uint32_t uiValue = static_cast<uint32_t>(iValue);
	do
	{
		uint8_t uiByte = static_cast<uint8_t>(uiValue & 0x7Fui32);
		uiValue >>= 7;
		if (uiValue != 0)
		{
			uiByte |= 0x80ui32;
		}
		common::Write(rStream, uiByte);
	}
	while (uiValue != 0);
}

static int64_t ReadCompactInteger(std::istream& rStream)
{
	uint32_t uiValue = 0;
	int64_t iShift = 0;
	uint8_t uiByte = 0;
	do
	{
		uiByte = 0;
		common::Read(rStream, uiByte);
		ASSERT(iShift != 28 || uiByte <= 0x07ui32);
		uiValue |= static_cast<uint32_t>(uiByte & 0x7Fui32) << iShift;
		iShift += 7;
	}
	while ((uiByte & 0x80ui32) != 0);
	return uiValue;
}

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
		common::Write(rStream, static_cast<int32_t>(std::ssize(rNavData.vertices)));
		for (const XMFLOAT2& rVertex : rNavData.vertices)
		{
			common::Write(rStream, rVertex);
		}
		common::Write(rStream, static_cast<int32_t>(std::ssize(rNavData.polygonOffsets)));
		int64_t iPreviousOffset = 0;
		for (int32_t iOffset : rNavData.polygonOffsets)
		{
			WriteCompactInteger(rStream, iOffset - iPreviousOffset);
			iPreviousOffset = iOffset;
		}
		int64_t iEdgeCount = std::ssize(rNavData.visibilityEdgeA);
		ASSERT(iEdgeCount == std::ssize(rNavData.visibilityEdgeB));
		common::Write(rStream, static_cast<int32_t>(iEdgeCount));
		int64_t iEdge = 0;
		for (int64_t iSource = 0; iSource < std::ssize(rNavData.vertices); ++iSource)
		{
			int64_t iStart = iEdge;
			while (iEdge < iEdgeCount && rNavData.visibilityEdgeA.at(iEdge) == iSource)
			{
				++iEdge;
			}
			WriteCompactInteger(rStream, iEdge - iStart);
			int64_t iPreviousTarget = 0;
			for (int64_t i = iStart; i < iEdge; ++i)
			{
				int64_t iTarget = rNavData.visibilityEdgeB.at(i);
				WriteCompactInteger(rStream, iTarget - iPreviousTarget);
				iPreviousTarget = iTarget;
			}
		}
		ASSERT(iEdge == iEdgeCount);
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
		int32_t iVertexCount = 0;
		common::Read(rStream, iVertexCount);
		rNavData.vertices.resize(iVertexCount);
		for (XMFLOAT2& rVertex : rNavData.vertices)
		{
			common::Read(rStream, rVertex);
		}
		int32_t iPolygonCount = 0;
		common::Read(rStream, iPolygonCount);
		rNavData.polygonOffsets.resize(iPolygonCount);
		int64_t iOffset = 0;
		for (int32_t& riOffset : rNavData.polygonOffsets)
		{
			iOffset += ReadCompactInteger(rStream);
			riOffset = static_cast<int32_t>(iOffset);
		}
		int32_t iEdgeCount = 0;
		common::Read(rStream, iEdgeCount);
		rNavData.visibilityEdgeA.resize(iEdgeCount);
		rNavData.visibilityEdgeB.resize(iEdgeCount);
		int64_t iEdge = 0;
		for (int64_t iSource = 0; iSource < iVertexCount; ++iSource)
		{
			int64_t iSourceEdgeCount = ReadCompactInteger(rStream);
			int64_t iTarget = 0;
			for (int64_t i = 0; i < iSourceEdgeCount; ++i)
			{
				iTarget += ReadCompactInteger(rStream);
				rNavData.visibilityEdgeA.at(iEdge) = static_cast<int32_t>(iSource);
				rNavData.visibilityEdgeB.at(iEdge) = static_cast<int32_t>(iTarget);
				++iEdge;
			}
		}
		BuildNavigationAcceleration(rNavData);
	}
	// Server visibility is authoritative; an empty completed graph must still be marked built.
	flags.Set(CellStaticDataFlags::kNavigationDataBuilt);
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
