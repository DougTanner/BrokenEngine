#include "Frame/IslandChainPlacement.h"
#include "Frame/IslandTerrain.h"
#include "NavBuild.h"
#include "NavBuildInternal.h"

namespace engine
{

// Proper intersections between non-adjacent edges indicate self-intersecting contours.
// Cross-placement overlap is allowed: visibility building and runtime queries test every polygon
// in the shared cell-local frame. This O(edges^2) diagnostic is enabled by kbDebugNavigationCrossingCheck.
static void DebugCheckCrossingEdges([[maybe_unused]] const NavData& rNavigationData)
{
	if constexpr (kbDebugNavigationCrossingCheck)
	{
		int64_t iVertexCount = std::ssize(rNavigationData.vertices);
		int64_t iPolygonCount = std::ssize(rNavigationData.polygonOffsets);
		for (int64_t i = 0; i < iPolygonCount; ++i)
		{
			auto [iStartA, iEndA] = std::pair<int64_t, int64_t>{PolygonRange(rNavigationData.polygonOffsets, i, iVertexCount)};
			int64_t iCountA = iEndA - iStartA;
			if (iCountA < 2)
			{
				continue;
			}

			for (int64_t j = 0; j < iCountA; ++j)
			{
				int64_t iA0 = iStartA + j;
				int64_t iA1 = iStartA + ((j + 1) % iCountA);
				XMFLOAT2 f2A0 = rNavigationData.vertices.at(static_cast<size_t>(iA0));
				XMFLOAT2 f2A1 = rNavigationData.vertices.at(static_cast<size_t>(iA1));

				for (int64_t k = i; k < iPolygonCount; ++k)
				{
					auto [iStartB, iEndB] = std::pair<int64_t, int64_t>{PolygonRange(rNavigationData.polygonOffsets, k, iVertexCount)};
					int64_t iCountB = iEndB - iStartB;
					if (iCountB < 2)
					{
						continue;
					}

					int64_t iFirstEdgeB = (k == i) ? (j + 1) : 0;
					for (int64_t l = iFirstEdgeB; l < iCountB; ++l)
					{
						int64_t iB0 = iStartB + l;
						int64_t iB1 = iStartB + ((l + 1) % iCountB);
						XMFLOAT2 f2B0 = rNavigationData.vertices.at(static_cast<size_t>(iB0));
						XMFLOAT2 f2B1 = rNavigationData.vertices.at(static_cast<size_t>(iB1));

						if (SegmentsIntersect(f2A0, f2A1, f2B0, f2B1))
						{
							LOG(kNavData, kError, "[DEBUG-nav-crossing] NavData crossing polygon edges: polyA={} edgeA=({}->{}) ({} {})-({} {}) | polyB={} edgeB=({}->{}) ({} {})-({} {})", i, iA0, iA1, common::Wb(f2A0.x, 4), common::Wb(f2A0.y, 4), common::Wb(f2A1.x, 4), common::Wb(f2A1.y, 4), k, iB0, iB1, common::Wb(f2B0.x, 4), common::Wb(f2B0.y, 4), common::Wb(f2B1.x, 4), common::Wb(f2B1.y, 4));
							DEBUG_BREAK();
						}
					}
				}
			}
		}
	}
}

// Edge grid (CSR): bucket each obstacle edge into every cell its AABB overlaps (conservative). Reads
// the global vertex AABB off rNavigationData.f2GridMinimum/f2GridMaximum (set by the caller). The gridEdges fill is
// cursor-driven, so its insertion order must be preserved verbatim — the rebuild is deterministic on
// both client and server, and the broad phase feeds A* steering CRC'd positions.
static void BuildNavigationEdgeGrid(NavData& rNavigationData)
{
	float fMinX = rNavigationData.f2GridMinimum.x;
	float fMinY = rNavigationData.f2GridMinimum.y;
	float fMaxX = rNavigationData.f2GridMaximum.x;
	float fMaxY = rNavigationData.f2GridMaximum.y;

	int64_t iEdgeCount = std::ssize(rNavigationData.edgeA);
	int64_t iCellCount = kiNavZonesX * kiNavZonesY;

	auto EdgeCellRange = [&](int64_t iEdge, int64_t& riCellXMin, int64_t& riCellXMax, int64_t& riCellYMin, int64_t& riCellYMax)
	{
		const XMFLOAT2& rA = rNavigationData.vertices.at(static_cast<size_t>(rNavigationData.edgeA.at(static_cast<size_t>(iEdge))));
		const XMFLOAT2& rB = rNavigationData.vertices.at(static_cast<size_t>(rNavigationData.edgeB.at(static_cast<size_t>(iEdge))));
		riCellXMin = NavGridCell(std::min(rA.x, rB.x), fMinX, fMaxX, kiNavZonesX);
		riCellXMax = NavGridCell(std::max(rA.x, rB.x), fMinX, fMaxX, kiNavZonesX);
		riCellYMin = NavGridCell(std::min(rA.y, rB.y), fMinY, fMaxY, kiNavZonesY);
		riCellYMax = NavGridCell(std::max(rA.y, rB.y), fMinY, fMaxY, kiNavZonesY);
	};

	std::vector<int64_t> gridCounts(static_cast<size_t>(iCellCount), 0);
	for (int64_t i = 0; i < iEdgeCount; ++i)
	{
		int64_t iCellXMin = 0;
		int64_t iCellXMax = 0;
		int64_t iCellYMin = 0;
		int64_t iCellYMax = 0;
		EdgeCellRange(i, iCellXMin, iCellXMax, iCellYMin, iCellYMax);
		for (int64_t j = iCellYMin; j <= iCellYMax; ++j)
		{
			for (int64_t k = iCellXMin; k <= iCellXMax; ++k)
			{
				++gridCounts.at(static_cast<size_t>(j * kiNavZonesX + k));
			}
		}
	}

	rNavigationData.gridEdgeOffsets.resize(static_cast<size_t>(iCellCount) + 1);
	rNavigationData.gridEdgeOffsets.at(0) = 0;
	for (int64_t i = 0; i < iCellCount; ++i)
	{
		rNavigationData.gridEdgeOffsets.at(static_cast<size_t>(i) + 1) = static_cast<int32_t>(rNavigationData.gridEdgeOffsets.at(static_cast<size_t>(i)) + gridCounts.at(static_cast<size_t>(i)));
	}

	rNavigationData.gridEdges.resize(static_cast<size_t>(rNavigationData.gridEdgeOffsets.at(static_cast<size_t>(iCellCount))));
	std::vector<int64_t> gridCursor(rNavigationData.gridEdgeOffsets.begin(), rNavigationData.gridEdgeOffsets.end() - 1);
	for (int64_t i = 0; i < iEdgeCount; ++i)
	{
		int64_t iCellXMin = 0;
		int64_t iCellXMax = 0;
		int64_t iCellYMin = 0;
		int64_t iCellYMax = 0;
		EdgeCellRange(i, iCellXMin, iCellXMax, iCellYMin, iCellYMax);
		for (int64_t j = iCellYMin; j <= iCellYMax; ++j)
		{
			for (int64_t k = iCellXMin; k <= iCellXMax; ++k)
			{
				rNavigationData.gridEdges.at(static_cast<size_t>(gridCursor.at(static_cast<size_t>(j * kiNavZonesX + k))++)) = i;
			}
		}
	}
}

// Per-vertex adjacency CSR (visibility-graph neighbors + polygon prev/next). Each vertex's neighbor
// span is sorted by index so A* neighbor iteration is order-stable -> deterministic across builds.
static void BuildNavigationAdjacency(NavData& rNavigationData)
{
	int64_t iVertexCount = std::ssize(rNavigationData.vertices);
	int64_t iPolygonCount = std::ssize(rNavigationData.polygonOffsets);

	int64_t iVisibilityEdgeCount = std::ssize(rNavigationData.visibilityEdgeA);
	std::vector<int64_t> adjacencyCounts(static_cast<size_t>(iVertexCount), 0);
	for (int64_t i = 0; i < iVisibilityEdgeCount; ++i)
	{
		++adjacencyCounts.at(static_cast<size_t>(rNavigationData.visibilityEdgeA.at(i)));
		++adjacencyCounts.at(static_cast<size_t>(rNavigationData.visibilityEdgeB.at(i)));
	}
	for (int64_t i = 0; i < iPolygonCount; ++i)
	{
		auto [iStart, iEnd] = std::pair<int64_t, int64_t>{PolygonRange(rNavigationData.polygonOffsets, i, iVertexCount)};
		if (iEnd - iStart < 2)
		{
			continue;
		}
		for (int64_t j = iStart; j < iEnd; ++j)
		{
			adjacencyCounts.at(static_cast<size_t>(j)) += 2; // prev + next
		}
	}

	rNavigationData.adjacencyOffsets.resize(static_cast<size_t>(iVertexCount) + 1);
	rNavigationData.adjacencyOffsets.at(0) = 0;
	for (int64_t i = 0; i < iVertexCount; ++i)
	{
		rNavigationData.adjacencyOffsets.at(static_cast<size_t>(i) + 1) = static_cast<int32_t>(rNavigationData.adjacencyOffsets.at(static_cast<size_t>(i)) + adjacencyCounts.at(static_cast<size_t>(i)));
	}
	rNavigationData.adjacencyNeighbors.resize(static_cast<size_t>(rNavigationData.adjacencyOffsets.at(static_cast<size_t>(iVertexCount))));
	std::vector<int64_t> adjacencyCursor(rNavigationData.adjacencyOffsets.begin(), rNavigationData.adjacencyOffsets.end() - 1);

	auto AddNeighbor = [&](int64_t iVertex, int64_t iNeighbor)
	{
		rNavigationData.adjacencyNeighbors.at(static_cast<size_t>(adjacencyCursor.at(static_cast<size_t>(iVertex))++)) = iNeighbor;
	};
	for (int64_t i = 0; i < iVisibilityEdgeCount; ++i)
	{
		int64_t iA = rNavigationData.visibilityEdgeA.at(i);
		int64_t iB = rNavigationData.visibilityEdgeB.at(i);
		AddNeighbor(iA, iB);
		AddNeighbor(iB, iA);
	}
	for (int64_t i = 0; i < iPolygonCount; ++i)
	{
		auto [iStart, iEnd] = std::pair<int64_t, int64_t>{PolygonRange(rNavigationData.polygonOffsets, i, iVertexCount)};
		int64_t iCount = iEnd - iStart;
		if (iCount < 2)
		{
			continue;
		}
		for (int64_t j = 0; j < iCount; ++j)
		{
			AddNeighbor(iStart + j, iStart + ((j + 1) % iCount));
			AddNeighbor(iStart + j, iStart + ((j + iCount - 1) % iCount));
		}
	}

	for (int64_t i = 0; i < iVertexCount; ++i)
	{
		int64_t iBegin = rNavigationData.adjacencyOffsets.at(static_cast<size_t>(i));
		int64_t iStop = rNavigationData.adjacencyOffsets.at(static_cast<size_t>(i) + 1);
		std::sort(rNavigationData.adjacencyNeighbors.begin() + iBegin, rNavigationData.adjacencyNeighbors.begin() + iStop);
	}
}

// Emit non-adjacent convex-vertex pairs only when SegmentBlockedByObstacle finds no crossing
// and the midpoint lies outside every polygon. Fixed pair order makes emission deterministic;
// the crossing predicate's boolean OR is order-independent. Requires nonempty vertices and
// the grid bounds and edge CSR built by BuildNavigationAcceleration.
static void BuildCellVisibilityGraph(NavData& rNavigationData)
{
	int64_t iVertexCount = std::ssize(rNavigationData.vertices);
	const XMFLOAT2* pVertices = rNavigationData.vertices.data();

	// Cache polygon membership and convexity in O(V) for O(1) lookup during pair enumeration.
	std::vector<int64_t> vertexPolygon(static_cast<size_t>(iVertexCount), 0);
	std::vector<int64_t> vertexLocal(static_cast<size_t>(iVertexCount), 0);
	std::vector<int64_t> vertexPolygonCount(static_cast<size_t>(iVertexCount), 0);
	std::vector<bool> vertexConvex(static_cast<size_t>(iVertexCount), false);
	for (int64_t i = 0; i < std::ssize(rNavigationData.polygonOffsets); ++i)
	{
		auto [iStart, iEnd] = std::pair<int64_t, int64_t>{PolygonRange(rNavigationData.polygonOffsets, i, iVertexCount)};
		int64_t iCount = iEnd - iStart;

		// BuildCellNavigationData mirrors Y; rotation preserves orientation, so the CCW template contours
		// asserted in NavBuild.cpp become clockwise in the cell frame. Use measured winding for the
		// convexity sign. Fewer than three vertices have no interior and produce zero cross products;
		// skip IsPolygonCcw for them.
		bool bCounterClockwise = (iCount >= 3) && common::IsPolygonCcw(std::span<const XMFLOAT2>(&pVertices[iStart], static_cast<size_t>(iCount)));
		ASSERT(!bCounterClockwise);
		float fConvexSign = bCounterClockwise ? 1.0f : -1.0f;

		for (int64_t j = 0; j < iCount; ++j)
		{
			int64_t iVertex = iStart + j;
			vertexPolygon.at(static_cast<size_t>(iVertex)) = i;
			vertexLocal.at(static_cast<size_t>(iVertex)) = j;
			vertexPolygonCount.at(static_cast<size_t>(iVertex)) = iCount;

			const XMFLOAT2& rPrevious = pVertices[iStart + (j + iCount - 1) % iCount];
			const XMFLOAT2& rCurrent = pVertices[iVertex];
			const XMFLOAT2& rNext = pVertices[iStart + (j + 1) % iCount];
			float fCross = (rCurrent.x - rPrevious.x) * (rNext.y - rCurrent.y) - (rCurrent.y - rPrevious.y) * (rNext.x - rCurrent.x);
			vertexConvex.at(static_cast<size_t>(iVertex)) = (fCross * fConvexSign) > 0.0f;
		}
	}

	for (int64_t i = 0; i < iVertexCount; ++i)
	{
		// Shortest obstacle-avoiding paths bend at convex obstacle vertices.
		// BuildNavigationAdjacency supplies the full polygon perimeter independently of visibility pruning.
		if (!vertexConvex.at(static_cast<size_t>(i)))
		{
			continue;
		}

		for (int64_t j = i + 1; j < iVertexCount; ++j)
		{
			if (!vertexConvex.at(static_cast<size_t>(j)))
			{
				continue;
			}

			// Adjacent pairs on one polygon are perimeter edges; BuildNavigationAdjacency already supplies them.
			if (vertexPolygon.at(static_cast<size_t>(i)) == vertexPolygon.at(static_cast<size_t>(j)))
			{
				int64_t iLocalI = vertexLocal.at(static_cast<size_t>(i));
				int64_t iLocalJ = vertexLocal.at(static_cast<size_t>(j));
				int64_t iCount = vertexPolygonCount.at(static_cast<size_t>(i));
				if (iLocalJ - iLocalI == 1 || (iLocalI == 0 && iLocalJ == iCount - 1))
				{
					continue;
				}
			}

			XMFLOAT2 f2A = pVertices[i];
			XMFLOAT2 f2B = pVertices[j];

			// Blocked test first: it early-exits on its first crossing edge, which is the common case.
			if (SegmentBlockedByObstacle(f2A, f2B, pVertices, rNavigationData))
			{
				continue;
			}

			// A segment lying wholly inside a polygon crosses no edge, so the blocked test alone misses it.
			XMFLOAT2 f2Midpoint {(f2A.x + f2B.x) * 0.5f, (f2A.y + f2B.y) * 0.5f};
			if (PointInAnyPolygon(f2Midpoint, pVertices, rNavigationData))
			{
				continue;
			}

			rNavigationData.visibilityEdgeA.push_back(static_cast<int32_t>(i));
			rNavigationData.visibilityEdgeB.push_back(static_cast<int32_t>(j));
		}
	}
}

void BuildCellNavigationData(NavData& rNavigationData, const std::vector<IslandPlacement>& rPlacements)
{
	std::chrono::steady_clock::time_point startTimePoint = std::chrono::steady_clock::now();

	rNavigationData.vertices.clear();
	rNavigationData.polygonOffsets.clear();
	rNavigationData.visibilityEdgeA.clear();
	rNavigationData.visibilityEdgeB.clear();

	// Walk per-cell placements; each placement's template contour (UV-space) is rotated and offset by the
	// placement's cell-local center, so the whole graph lands in centered cell-local meters and no grid
	// coordinate enters it. Topology offsets are rebased per island.
	for (const IslandPlacement& rPlacement : rPlacements)
	{
		auto it = gpIslandTerrain->mIslands.find(rPlacement.islandCrc);
		if (it == gpIslandTerrain->mIslands.end())
		{
			// ReadGridSave rejects a save or replay placement CRC with no loaded template and generation only
			// emits loaded ones, so reaching this is an internal invariant break.
			LOG(kNavData, kError, "BuildCellNavData: placement islandCrc={} has no loaded island template", rPlacement.islandCrc);
			throw std::out_of_range("placement islandCrc has no loaded island template");
		}

		const IslandTemplate& rTemplate = it->second;
		const NavContour& rContour = rTemplate.navContour;
		int64_t iVertexCount = std::ssize(rContour.vertices);
		if (iVertexCount == 0)
		{
			continue;
		}

		int64_t iVertexBase = std::ssize(rNavigationData.vertices);

		common::SinCos rotation = common::DeterministicSinCos(rPlacement.fRotation);
		float fCosine = rotation.fCos;
		float fSine = rotation.fSin;
		float fFootprintX = rTemplate.fQuadFootprintX;
		float fFootprintY = rTemplate.fQuadFootprintY;

		// Local axes: +U = +world.x, +V = -world.y (V is world-Y inverted).
		for (const XMFLOAT2& rVertex : rContour.vertices)
		{
			float fU = rVertex.x;
			float fV = rVertex.y;

			float fLocalX = (fU - 0.5f) * fFootprintX;
			float fLocalY = (0.5f - fV) * fFootprintY;

			float fRotatedX = fLocalX * fCosine - fLocalY * fSine;
			float fRotatedY = fLocalX * fSine + fLocalY * fCosine;

			rNavigationData.vertices.push_back({rPlacement.f2WorldPosition.x + fRotatedX, rPlacement.f2WorldPosition.y + fRotatedY});
		}

		for (int64_t iOffset : rContour.polygonOffsets)
		{
			rNavigationData.polygonOffsets.push_back(static_cast<int32_t>(iVertexBase + iOffset));
		}
	}

	DebugCheckCrossingEdges(rNavigationData);

	// Acceleration first: the visibility pass queries SegmentBlockedByObstacle, which reads the
	// f2GridMinimum/f2GridMaximum and edge CSR built here. Its edges then need a second adjacency build — only
	// BuildNavigationAdjacency, not another full BuildNavigationAcceleration, because it rewrites adjacencyOffsets and every
	// adjacencyNeighbors slot outright.
	BuildNavigationAcceleration(rNavigationData);

	// Empty cells must retain the cleared derived state produced by BuildNavigationAcceleration,
	// matching NavData::Read on the client.
	if (!rNavigationData.vertices.empty())
	{
		BuildCellVisibilityGraph(rNavigationData);
		BuildNavigationAdjacency(rNavigationData);
	}

	LOG(kNavData, kInfo, "BuildCellNavData: placements={} vertices={} polygons={} visEdges={} wireBytes={} elapsedUs={}", std::ssize(rPlacements), std::ssize(rNavigationData.vertices), std::ssize(rNavigationData.polygonOffsets), std::ssize(rNavigationData.visibilityEdgeA), std::ssize(rNavigationData.visibilityEdgeA) * (sizeof(int32_t) + sizeof(int32_t)), std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimePoint).count());
}

void BuildNavigationAcceleration(NavData& rNavigationData)
{
	rNavigationData.polygonMinimum.clear();
	rNavigationData.polygonMaximum.clear();
	rNavigationData.edgeA.clear();
	rNavigationData.edgeB.clear();
	rNavigationData.gridEdgeOffsets.clear();
	rNavigationData.gridEdges.clear();
	rNavigationData.adjacencyOffsets.clear();
	rNavigationData.adjacencyNeighbors.clear();
	rNavigationData.f2GridMinimum = {};
	rNavigationData.f2GridMaximum = {};

	int64_t iVertexCount = std::ssize(rNavigationData.vertices);
	if (iVertexCount == 0)
	{
		return;
	}

	int64_t iPolygonCount = std::ssize(rNavigationData.polygonOffsets);

	// The vertex bounds define the edge-grid domain.
	float fMinX = std::numeric_limits<float>::max();
	float fMinY = std::numeric_limits<float>::max();
	float fMaxX = std::numeric_limits<float>::lowest();
	float fMaxY = std::numeric_limits<float>::lowest();
	for (const XMFLOAT2& rVertex : rNavigationData.vertices)
	{
		fMinX = std::min(fMinX, rVertex.x);
		fMinY = std::min(fMinY, rVertex.y);
		fMaxX = std::max(fMaxX, rVertex.x);
		fMaxY = std::max(fMaxY, rVertex.y);
	}
	rNavigationData.f2GridMinimum = {fMinX, fMinY};
	rNavigationData.f2GridMaximum = {fMaxX, fMaxY};

	rNavigationData.polygonMinimum.resize(iPolygonCount);
	rNavigationData.polygonMaximum.resize(iPolygonCount);
	rNavigationData.edgeA.reserve(static_cast<size_t>(iVertexCount));
	rNavigationData.edgeB.reserve(static_cast<size_t>(iVertexCount));
	for (int64_t i = 0; i < iPolygonCount; ++i)
	{
		auto [iStart, iEnd] = std::pair<int64_t, int64_t>{PolygonRange(rNavigationData.polygonOffsets, i, iVertexCount)};
		int64_t iCount = iEnd - iStart;

		float fPolygonMinX = std::numeric_limits<float>::max();
		float fPolygonMinY = std::numeric_limits<float>::max();
		float fPolygonMaxX = std::numeric_limits<float>::lowest();
		float fPolygonMaxY = std::numeric_limits<float>::lowest();
		for (int64_t j = 0; j < iCount; ++j)
		{
			const XMFLOAT2& rVertex = rNavigationData.vertices.at(static_cast<size_t>(iStart + j));
			fPolygonMinX = std::min(fPolygonMinX, rVertex.x);
			fPolygonMinY = std::min(fPolygonMinY, rVertex.y);
			fPolygonMaxX = std::max(fPolygonMaxX, rVertex.x);
			fPolygonMaxY = std::max(fPolygonMaxY, rVertex.y);

			rNavigationData.edgeA.push_back(iStart + j);
			rNavigationData.edgeB.push_back(iStart + ((j + 1) % iCount));
		}
		rNavigationData.polygonMinimum.at(i) = {fPolygonMinX, fPolygonMinY};
		rNavigationData.polygonMaximum.at(i) = {fPolygonMaxX, fPolygonMaxY};
	}

	BuildNavigationEdgeGrid(rNavigationData);
	BuildNavigationAdjacency(rNavigationData);
}

void NavData::Write(std::ostream& rStream) const
{
	common::Write(rStream, static_cast<int32_t>(std::ssize(vertices)));
	for (const XMFLOAT2& rVertex : vertices)
	{
		common::Write(rStream, rVertex);
	}

	common::Write(rStream, static_cast<int32_t>(std::ssize(polygonOffsets)));
	for (int32_t iOffset : polygonOffsets)
	{
		common::Write(rStream, iOffset);
	}

	common::Write(rStream, static_cast<int32_t>(std::ssize(visibilityEdgeA)));
	for (int32_t iEdge : visibilityEdgeA)
	{
		common::Write(rStream, iEdge);
	}
	for (int32_t iEdge : visibilityEdgeB)
	{
		common::Write(rStream, iEdge);
	}
}

void NavData::Read(std::istream& rStream)
{
	int32_t iVertexCount = 0;
	common::Read(rStream, iVertexCount);
	vertices.resize(iVertexCount);
	for (XMFLOAT2& rVertex : vertices)
	{
		common::Read(rStream, rVertex);
	}

	int32_t iPolygonCount = 0;
	common::Read(rStream, iPolygonCount);
	polygonOffsets.resize(iPolygonCount);
	for (int32_t& riOffset : polygonOffsets)
	{
		common::Read(rStream, riOffset);
	}

	int32_t iEdgeCount = 0;
	common::Read(rStream, iEdgeCount);
	visibilityEdgeA.resize(iEdgeCount);
	visibilityEdgeB.resize(iEdgeCount);
	for (int32_t& riEdge : visibilityEdgeA)
	{
		common::Read(rStream, riEdge);
	}
	for (int32_t& riEdge : visibilityEdgeB)
	{
		common::Read(rStream, riEdge);
	}

	// Derived broad-phase data is not serialized; rebuild it from the vertices just read so the client
	// matches the server's BuildCellNavigationData result.
	BuildNavigationAcceleration(*this);
}

} // namespace engine
