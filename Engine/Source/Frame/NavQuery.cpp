#include "NavQuery.h"

#include "Ui/WrapperBase.h"
#include "NavBuild.h"
#include "NavBuildInternal.h"

namespace engine
{

// Conservative edge-CSR AABB buckets and DDA traversal of every clipped-segment cell test all real
// crossings; the boolean OR is order-independent. Callers require f2GridMinimum/f2GridMaximum and edge CSR from
// BuildNavigationAcceleration on nonempty vertices. NavQueryDirection checks empty vertices; NavCellData runs
// visibility only for a nonempty cell after building acceleration. Both share this definition through
// NavBuildInternal.h to agree on blocked paths.
bool SegmentBlockedByObstacle(XMFLOAT2 f2A, XMFLOAT2 f2B, const XMFLOAT2* pVertices, const NavData& rNavData)
{
	float fMinX = rNavData.f2GridMinimum.x;
	float fMinY = rNavData.f2GridMinimum.y;
	float fMaxX = rNavData.f2GridMaximum.x;
	float fMaxY = rNavData.f2GridMaximum.y;

	float fDeltaX = f2B.x - f2A.x;
	float fDeltaY = f2B.y - f2A.y;

	// Obstacle edges live strictly inside the grid domain, so an intersection can only occur there.
	// Slab-clip the query segment to [min,max] (parametric range [fT0,fT1] over A->B).
	float fT0 = 0.0f;
	float fT1 = 1.0f;
	if (std::abs(fDeltaX) < 1e-20f)
	{
		if (f2A.x < fMinX || f2A.x > fMaxX)
		{
			return false;
		}
	}
	else
	{
		float fInverseDelta = 1.0f / fDeltaX;
		float fTa = (fMinX - f2A.x) * fInverseDelta;
		float fTb = (fMaxX - f2A.x) * fInverseDelta;
		if (fTa > fTb)
		{
			float fTemporary = fTa;
			fTa = fTb;
			fTb = fTemporary;
		}
		fT0 = std::max(fT0, fTa);
		fT1 = std::min(fT1, fTb);
	}
	if (std::abs(fDeltaY) < 1e-20f)
	{
		if (f2A.y < fMinY || f2A.y > fMaxY)
		{
			return false;
		}
	}
	else
	{
		float fInverseDelta = 1.0f / fDeltaY;
		float fTa = (fMinY - f2A.y) * fInverseDelta;
		float fTb = (fMaxY - f2A.y) * fInverseDelta;
		if (fTa > fTb)
		{
			float fTemporary = fTa;
			fTa = fTb;
			fTb = fTemporary;
		}
		fT0 = std::max(fT0, fTa);
		fT1 = std::min(fT1, fTb);
	}
	if (fT0 > fT1)
	{
		return false;
	}

	// Clipped endpoints (both inside the grid domain).
	float fP0x = f2A.x + fT0 * fDeltaX;
	float fP0y = f2A.y + fT0 * fDeltaY;
	float fP1x = f2A.x + fT1 * fDeltaX;
	float fP1y = f2A.y + fT1 * fDeltaY;

	float fCellSizeX = (fMaxX - fMinX) / static_cast<float>(kiNavZonesX);
	float fCellSizeY = (fMaxY - fMinY) / static_cast<float>(kiNavZonesY);

	int64_t iX = NavGridCell(fP0x, fMinX, fMaxX, kiNavZonesX);
	int64_t iY = NavGridCell(fP0y, fMinY, fMaxY, kiNavZonesY);
	int64_t iEndX = NavGridCell(fP1x, fMinX, fMaxX, kiNavZonesX);
	int64_t iEndY = NavGridCell(fP1y, fMinY, fMaxY, kiNavZonesY);

	float fSegmentDeltaX = fP1x - fP0x;
	float fSegmentDeltaY = fP1y - fP0y;
	int64_t iStepX = (fSegmentDeltaX > 0.0f) ? 1 : ((fSegmentDeltaX < 0.0f) ? -1 : 0);
	int64_t iStepY = (fSegmentDeltaY > 0.0f) ? 1 : ((fSegmentDeltaY < 0.0f) ? -1 : 0);

	// Amanatides-Woo parametric crossing distances (in segment-t units).
	float fTMaxX = std::numeric_limits<float>::max();
	float fTDeltaX = std::numeric_limits<float>::max();
	if (iStepX != 0 && fCellSizeX > 1e-6f)
	{
		float fBoundaryX = fMinX + static_cast<float>(iX + (iStepX > 0 ? 1 : 0)) * fCellSizeX;
		fTMaxX = (fBoundaryX - fP0x) / fSegmentDeltaX;
		fTDeltaX = fCellSizeX / std::abs(fSegmentDeltaX);
	}
	float fTMaxY = std::numeric_limits<float>::max();
	float fTDeltaY = std::numeric_limits<float>::max();
	if (iStepY != 0 && fCellSizeY > 1e-6f)
	{
		float fBoundaryY = fMinY + static_cast<float>(iY + (iStepY > 0 ? 1 : 0)) * fCellSizeY;
		fTMaxY = (fBoundaryY - fP0y) / fSegmentDeltaY;
		fTDeltaY = fCellSizeY / std::abs(fSegmentDeltaY);
	}

	// Walk cells along the clipped segment. Bounded by the grid extent; the step guard prevents any
	// runaway from float drift.
	int64_t iMaxSteps = 2 * (kiNavZonesX + kiNavZonesY);
	for (int64_t i = 0; i <= iMaxSteps; ++i)
	{
		int64_t iCell = iY * kiNavZonesX + iX;
		int64_t iBegin = rNavData.gridEdgeOffsets.at(static_cast<size_t>(iCell));
		int64_t iStop = rNavData.gridEdgeOffsets.at(static_cast<size_t>(iCell) + 1);
		for (int64_t k = iBegin; k < iStop; ++k)
		{
			int64_t iEdge = rNavData.gridEdges.at(static_cast<size_t>(k));
			if (SegmentsIntersect(f2A, f2B, pVertices[rNavData.edgeA.at(static_cast<size_t>(iEdge))], pVertices[rNavData.edgeB.at(static_cast<size_t>(iEdge))]))
			{
				return true;
			}
		}

		if (iX == iEndX && iY == iEndY)
		{
			break;
		}

		if (fTMaxX < fTMaxY)
		{
			iX += iStepX;
			fTMaxX += fTDeltaX;
		}
		else
		{
			iY += iStepY;
			fTMaxY += fTDeltaY;
		}

		if (iX < 0 || iX >= kiNavZonesX || iY < 0 || iY >= kiNavZonesY)
		{
			break;
		}
	}
	return false;
}

// PointInAnyPolygon uses a per-polygon AABB broad phase and shares PointInPolygon's winding core with
// the builder through NavBuildInternal.h.
bool PointInAnyPolygon(XMFLOAT2 f2Point, const XMFLOAT2* pVertices, const NavData& rNavData)
{
	for (int64_t i = 0; i < std::ssize(rNavData.polygonOffsets); ++i)
	{
		// Broad phase: a point outside the polygon's AABB cannot be inside the polygon.
		const XMFLOAT2& rMin = rNavData.polygonMinimum.at(i);
		const XMFLOAT2& rMax = rNavData.polygonMaximum.at(i);
		if (f2Point.x < rMin.x || f2Point.x > rMax.x || f2Point.y < rMin.y || f2Point.y > rMax.y)
		{
			continue;
		}

		auto [iStart, iEnd] = std::pair<int64_t, int64_t>(PolygonRange(rNavData.polygonOffsets, i, std::ssize(rNavData.vertices)));
		int64_t iCount = iEnd - iStart;

		if (iCount > 0 && PointInPolygon(f2Point, std::span<const XMFLOAT2>(&pVertices[iStart], static_cast<size_t>(iCount))))
		{
			return true;
		}
	}
	return false;
}

static float Distance(XMFLOAT2 f2A, XMFLOAT2 f2B)
{
	float fDeltaX = f2B.x - f2A.x;
	float fDeltaY = f2B.y - f2A.y;
	return std::sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);
}

static XMFLOAT2 NearestPolygonEdgePoint(XMFLOAT2 f2Position, const XMFLOAT2* pVertices, const NavData& rNavData)
{
	float fBestDistanceSquared = std::numeric_limits<float>::max();
	XMFLOAT2 f2BestPoint = f2Position;

	for (int64_t j = 0; j < std::ssize(rNavData.polygonOffsets); ++j)
	{
		// Broad phase: skip a polygon whose AABB is already farther than the best edge found so far.
		// Equal distances retain the first edge in index order.
		const XMFLOAT2& rMin = rNavData.polygonMinimum.at(j);
		const XMFLOAT2& rMax = rNavData.polygonMaximum.at(j);
		float fBoundingBoxDeltaX = (f2Position.x < rMin.x) ? (rMin.x - f2Position.x) : ((f2Position.x > rMax.x) ? (f2Position.x - rMax.x) : 0.0f);
		float fBoundingBoxDeltaY = (f2Position.y < rMin.y) ? (rMin.y - f2Position.y) : ((f2Position.y > rMax.y) ? (f2Position.y - rMax.y) : 0.0f);
		if (fBoundingBoxDeltaX * fBoundingBoxDeltaX + fBoundingBoxDeltaY * fBoundingBoxDeltaY > fBestDistanceSquared)
		{
			continue;
		}

		auto [iStart, iEnd] = std::pair<int64_t, int64_t>(PolygonRange(rNavData.polygonOffsets, j, std::ssize(rNavData.vertices)));
		int64_t iCount = iEnd - iStart;

		for (int64_t i = 0; i < iCount; ++i)
		{
			int64_t iNext = (i + 1) % iCount;
			XMFLOAT2 f2A = pVertices[iStart + i];
			XMFLOAT2 f2B = pVertices[iStart + iNext];

			float fEdgeDeltaX = f2B.x - f2A.x;
			float fEdgeDeltaY = f2B.y - f2A.y;
			float fEdgeLengthSquared = fEdgeDeltaX * fEdgeDeltaX + fEdgeDeltaY * fEdgeDeltaY;

			float fT = 0.0f;
			if (fEdgeLengthSquared > 1e-10f)
			{
				fT = std::clamp(((f2Position.x - f2A.x) * fEdgeDeltaX + (f2Position.y - f2A.y) * fEdgeDeltaY) / fEdgeLengthSquared, 0.0f, 1.0f);
			}

			XMFLOAT2 f2Closest {f2A.x + fT * fEdgeDeltaX, f2A.y + fT * fEdgeDeltaY};
			float fDeltaX = f2Closest.x - f2Position.x;
			float fDeltaY = f2Closest.y - f2Position.y;
			float fDistanceSquared = fDeltaX * fDeltaX + fDeltaY * fDeltaY;

			if (fDistanceSquared < fBestDistanceSquared)
			{
				fBestDistanceSquared = fDistanceSquared;
				f2BestPoint = f2Closest;
			}
		}
	}

	return f2BestPoint;
}

static XMFLOAT2 SnapOutsidePolygon(XMFLOAT2 f2Position, const XMFLOAT2* pVertices, const NavData& rNavData)
{
	static constexpr float kfSnapOffset = 0.5f;
	XMFLOAT2 f2EdgePoint = NearestPolygonEdgePoint(f2Position, pVertices, rNavData);
	float fDeltaX = f2EdgePoint.x - f2Position.x;
	float fDeltaY = f2EdgePoint.y - f2Position.y;
	float fLength = std::sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);
	if (fLength > 1e-6f)
	{
		f2EdgePoint.x += (fDeltaX / fLength) * kfSnapOffset;
		f2EdgePoint.y += (fDeltaY / fLength) * kfSnapOffset;
	}
	return f2EdgePoint;
}

// A* scratch memory layout, allocated as a single contiguous block
struct AStarMemory
{
	float* pGCost = nullptr;
	float* pFCost = nullptr;
	int32_t* pParent = nullptr;
	bool* pClosed = nullptr;
	bool* pStartVisible = nullptr;
	int32_t* pOpenSet = nullptr;  // binary min-heap of node indices
	int32_t* pHeapPosition = nullptr;  // per-node position in pOpenSet (-1 = not in heap)
};

struct AStarMemoryLayout
{
	int64_t iGCostOffset = 0;
	int64_t iFCostOffset = 0;
	int64_t iParentOffset = 0;
	int64_t iOpenSetOffset = 0;
	int64_t iHeapPositionOffset = 0;
	int64_t iClosedOffset = 0;
	int64_t iStartVisibleOffset = 0;
	int64_t iByteCount = 0;
};

static constexpr AStarMemoryLayout ComputeAStarMemoryLayout(int64_t iTotalNodes, int64_t iVertexCount)
{
	// Layout: all 4-byte types first (float, int32_t), then bool arrays last to avoid alignment issues
	AStarMemoryLayout layout {};
	layout.iFCostOffset = layout.iGCostOffset + static_cast<int64_t>(iTotalNodes) * static_cast<int64_t>(sizeof(float));
	layout.iParentOffset = layout.iFCostOffset + static_cast<int64_t>(iTotalNodes) * static_cast<int64_t>(sizeof(float));
	layout.iOpenSetOffset = layout.iParentOffset + static_cast<int64_t>(iTotalNodes) * static_cast<int64_t>(sizeof(int32_t));
	layout.iHeapPositionOffset = layout.iOpenSetOffset + static_cast<int64_t>(iTotalNodes) * static_cast<int64_t>(sizeof(int32_t));
	layout.iClosedOffset = layout.iHeapPositionOffset + static_cast<int64_t>(iTotalNodes) * static_cast<int64_t>(sizeof(int32_t));
	layout.iStartVisibleOffset = layout.iClosedOffset + static_cast<int64_t>(iTotalNodes) * static_cast<int64_t>(sizeof(bool));
	layout.iByteCount = layout.iStartVisibleOffset + static_cast<int64_t>(iVertexCount) * static_cast<int64_t>(sizeof(bool));
	return layout;
}

static AStarMemory BindAStarMemory(std::byte* pMemory, const AStarMemoryLayout& rLayout)
{
	AStarMemory memory
	{
		.pGCost = reinterpret_cast<float*>(pMemory + rLayout.iGCostOffset),
		.pFCost = reinterpret_cast<float*>(pMemory + rLayout.iFCostOffset),
		.pParent = reinterpret_cast<int32_t*>(pMemory + rLayout.iParentOffset),
		.pClosed = reinterpret_cast<bool*>(pMemory + rLayout.iClosedOffset),
		.pStartVisible = reinterpret_cast<bool*>(pMemory + rLayout.iStartVisibleOffset),
		.pOpenSet = reinterpret_cast<int32_t*>(pMemory + rLayout.iOpenSetOffset),
		.pHeapPosition = reinterpret_cast<int32_t*>(pMemory + rLayout.iHeapPositionOffset),
	};
	return memory;
}

// Indexed binary min-heap over A* open-set node indices, keyed by (fCost, node index). The index
// tie-break makes pop order a total order, so the search is deterministic across builds. pHeapPosition
// enables O(log n) decrease-key. Operates in place on the AStarMemory scratch arrays.
struct AStarHeap
{
	int32_t* pOpenSet = nullptr;
	int32_t* pHeapPosition = nullptr;
	const float* pFCost = nullptr;
	int64_t iHeapCount = 0;

	bool Less(int64_t iNodeA, int64_t iNodeB) const
	{
		float fA = pFCost[iNodeA];
		float fB = pFCost[iNodeB];
		if (fA != fB)
		{
			return fA < fB;
		}
		return iNodeA < iNodeB;
	}

	void Swap(int64_t iIndexA, int64_t iIndexB)
	{
		int64_t iNodeA = pOpenSet[iIndexA];
		int64_t iNodeB = pOpenSet[iIndexB];
		pOpenSet[iIndexA] = static_cast<int32_t>(iNodeB);
		pOpenSet[iIndexB] = static_cast<int32_t>(iNodeA);
		pHeapPosition[iNodeA] = static_cast<int32_t>(iIndexB);
		pHeapPosition[iNodeB] = static_cast<int32_t>(iIndexA);
	}

	void SiftUp(int64_t iIndex)
	{
		while (iIndex > 0)
		{
			int64_t iParent = (iIndex - 1) / 2;
			if (!Less(pOpenSet[iIndex], pOpenSet[iParent]))
			{
				break;
			}
			Swap(iIndex, iParent);
			iIndex = iParent;
		}
	}

	void SiftDown(int64_t iIndex)
	{
		while (true)
		{
			int64_t iSmallest = iIndex;
			int64_t iLeft = 2 * iIndex + 1;
			int64_t iRight = 2 * iIndex + 2;
			if (iLeft < iHeapCount && Less(pOpenSet[iLeft], pOpenSet[iSmallest]))
			{
				iSmallest = iLeft;
			}
			if (iRight < iHeapCount && Less(pOpenSet[iRight], pOpenSet[iSmallest]))
			{
				iSmallest = iRight;
			}
			if (iSmallest == iIndex)
			{
				break;
			}
			Swap(iIndex, iSmallest);
			iIndex = iSmallest;
		}
	}

	void Push(int64_t iNode)
	{
		pOpenSet[iHeapCount] = static_cast<int32_t>(iNode);
		pHeapPosition[iNode] = static_cast<int32_t>(iHeapCount);
		++iHeapCount;
		SiftUp(iHeapCount - 1);
	}

	int64_t Pop()
	{
		int64_t iTop = pOpenSet[0];
		pHeapPosition[iTop] = -1;
		--iHeapCount;
		if (iHeapCount > 0)
		{
			pOpenSet[0] = pOpenSet[iHeapCount];
			pHeapPosition[pOpenSet[0]] = 0;
			SiftDown(0);
		}
		return iTop;
	}
};

// A* pathfinding on the visibility graph with temporary start/end nodes
// Returns the direction toward the first waypoint, or zero vector if no path found
static XMVECTOR AStarPath(XMFLOAT2 f2Start, XMFLOAT2 f2End, const XMFLOAT2* pVertices, const NavData& rNavData, const AStarMemory& rMemory, float fBaseHeight, XMVECTOR* pOutNextWaypoint)
{
	int64_t iVertexCount = std::ssize(rNavData.vertices);
	int64_t iStartNode = iVertexCount;
	int64_t iEndNode = iVertexCount + 1;
	int64_t iTotalNodes = iVertexCount + 2;

	for (int64_t i = 0; i < iTotalNodes; ++i)
	{
		rMemory.pGCost[i] = std::numeric_limits<float>::max();
		rMemory.pFCost[i] = std::numeric_limits<float>::max();
		rMemory.pParent[i] = -1;
		rMemory.pClosed[i] = false;
		rMemory.pHeapPosition[i] = -1;
	}

	// Eagerly compute start visibility to all obstacle vertices (fully consumed when the start node pops).
	// End visibility is computed lazily per expanded vertex at the consumption site below: A* usually
	// terminates after a small frontier, and the closed-set guarantees each vertex expands at most once.
	for (int64_t i = 0; i < iVertexCount; ++i)
	{
		rMemory.pStartVisible[i] = !SegmentBlockedByObstacle(f2Start, pVertices[i], pVertices, rNavData);
	}

	auto GetPosition = [&](int64_t iNode) -> XMFLOAT2
	{
		if (iNode == iStartNode)
		{
			return f2Start;
		}
		if (iNode == iEndNode)
		{
			return f2End;
		}
		return pVertices[iNode];
	};

	AStarHeap heap {.pOpenSet = rMemory.pOpenSet, .pHeapPosition = rMemory.pHeapPosition, .pFCost = rMemory.pFCost, .iHeapCount = 0};

	rMemory.pGCost[iStartNode] = 0.0f;
	rMemory.pFCost[iStartNode] = Distance(f2Start, f2End);
	heap.Push(iStartNode);

	while (heap.iHeapCount > 0)
	{
		int64_t iCurrent = heap.Pop();

		if (iCurrent == iEndNode)
		{
			int64_t iNode = iEndNode;
			while (rMemory.pParent[iNode] != iStartNode && rMemory.pParent[iNode] != -1)
			{
				iNode = rMemory.pParent[iNode];
			}

			XMFLOAT2 f2Waypoint = GetPosition(iNode);
			if (pOutNextWaypoint != nullptr)
			{
				*pOutNextWaypoint = XMVectorSet(f2Waypoint.x, f2Waypoint.y, fBaseHeight, 1.0f);
			}
			XMVECTOR vecDirection = XMVectorSet(f2Waypoint.x - f2Start.x, f2Waypoint.y - f2Start.y, 0.0f, 0.0f);
			return XMVector3Normalize(vecDirection);
		}

		rMemory.pClosed[iCurrent] = true;

		auto TryNeighbor = [&](int64_t iNeighbor)
		{
			if (rMemory.pClosed[iNeighbor])
			{
				return;
			}

			XMFLOAT2 f2Current = GetPosition(iCurrent);
			XMFLOAT2 f2Neighbor = GetPosition(iNeighbor);
			float fTentativeG = rMemory.pGCost[iCurrent] + Distance(f2Current, f2Neighbor);

			if (fTentativeG < rMemory.pGCost[iNeighbor])
			{
				rMemory.pGCost[iNeighbor] = fTentativeG;
				rMemory.pFCost[iNeighbor] = fTentativeG + Distance(f2Neighbor, f2End);
				rMemory.pParent[iNeighbor] = static_cast<int32_t>(iCurrent);

				if (rMemory.pHeapPosition[iNeighbor] >= 0)
				{
					heap.SiftUp(rMemory.pHeapPosition[iNeighbor]); // fCost decreased -> may move up
				}
				else
				{
					heap.Push(iNeighbor);
				}
			}
		};

		if (iCurrent == iStartNode)
		{
			// The sole caller enters A* only after this post-snap start-to-end segment tested blocked.
			for (int64_t i = 0; i < iVertexCount; ++i)
			{
				if (rMemory.pStartVisible[i])
				{
					TryNeighbor(i);
				}
			}
		}
		else if (iCurrent < iVertexCount)
		{
			// Visibility-graph + polygon-perimeter neighbors, precomputed into one adjacency span.
			int64_t iBegin = rNavData.adjacencyOffsets.at(static_cast<size_t>(iCurrent));
			int64_t iStop = rNavData.adjacencyOffsets.at(static_cast<size_t>(iCurrent) + 1);
			for (int64_t k = iBegin; k < iStop; ++k)
			{
				TryNeighbor(rNavData.adjacencyNeighbors.at(static_cast<size_t>(k)));
			}

			// Lazy end-visibility: computed only for the vertices A* actually expands (each expands at most once).
			if (!SegmentBlockedByObstacle(f2End, pVertices[iCurrent], pVertices, rNavData))
			{
				TryNeighbor(iEndNode);
			}

			if (rMemory.pStartVisible[iCurrent])
			{
				TryNeighbor(iStartNode);
			}
		}
	}

	return XMVectorZero();
}

// The nearest-boundary tangent has a positive outward component to avoid the vertex-attractor escape cycle.
// A near-zero outward normal returns zero, letting callers use their straight-line fallback.
static XMVECTOR NavMissFallbackDirection(XMFLOAT2 f2Position, XMFLOAT2 f2Destination, const XMFLOAT2* pVertices, const NavData& rNavData)
{
	static constexpr float kfNavFallbackOutwardBias = 0.25f;

	XMFLOAT2 f2Edge = NearestPolygonEdgePoint(f2Position, pVertices, rNavData);
	float fOutwardX = f2Position.x - f2Edge.x;
	float fOutwardY = f2Position.y - f2Edge.y;
	float fLength = std::sqrt(fOutwardX * fOutwardX + fOutwardY * fOutwardY);
	if (fLength < 1e-6f)
	{
		return XMVectorZero();
	}
	fOutwardX /= fLength;
	fOutwardY /= fLength;

	// Boundary tangent, signed to whichever way makes progress toward the destination. The comparison is
	// a total order, so the choice is deterministic.
	float fTangentX = -fOutwardY;
	float fTangentY = fOutwardX;
	float fSign = (fTangentX * (f2Destination.x - f2Position.x) + fTangentY * (f2Destination.y - f2Position.y)) < 0.0f ? -1.0f : 1.0f;

	return XMVector3Normalize(XMVectorSet(fTangentX * fSign + fOutwardX * kfNavFallbackOutwardBias, fTangentY * fSign + fOutwardY * kfNavFallbackOutwardBias, 0.0f, 0.0f));
}

bool XM_CALLCONV NavQueryPointBlocked(FXMVECTOR vecPosition, const NavData& rNavData)
{
	if (rNavData.vertices.empty())
	{
		return false;
	}

	XMFLOAT4A f4Position {};
	XMStoreFloat4A(&f4Position, vecPosition);
	XMFLOAT2 f2Position {f4Position.x, f4Position.y};

	return PointInAnyPolygon(f2Position, rNavData.vertices.data(), rNavData);
}

XMVECTOR XM_CALLCONV NavQuerySnapToNavigable(FXMVECTOR vecPosition, const NavData& rNavData)
{
	float fBaseHeight = gBaseHeight.mfCurrent;
	ASSERT(XMVectorGetZ(vecPosition) == fBaseHeight);

	const XMFLOAT2* pVertices = rNavData.vertices.data();

	XMFLOAT4A f4Position {};
	XMStoreFloat4A(&f4Position, vecPosition);
	XMFLOAT2 f2Position {f4Position.x, f4Position.y};

	if (!PointInAnyPolygon(f2Position, pVertices, rNavData))
	{
		return vecPosition;
	}

	XMFLOAT2 f2BestPoint = SnapOutsidePolygon(f2Position, pVertices, rNavData);
	return XMVectorSet(f2BestPoint.x, f2BestPoint.y, fBaseHeight, 1.0f);
}

#if defined(BT_SERVER)
XMVECTOR XM_CALLCONV NavQueryDirection(FXMVECTOR vecPosition, FXMVECTOR vecDestination, const NavData& rNavData, XMVECTOR* pOutNextWaypoint, bool* pOutEnteredAStar)
#else
XMVECTOR XM_CALLCONV NavQueryDirection(FXMVECTOR vecPosition, FXMVECTOR vecDestination, const NavData& rNavData, XMVECTOR* pOutNextWaypoint)
#endif // BT_SERVER
{
#if defined(BT_SERVER)
	if constexpr (kbProfiling)
	{
		if (pOutEnteredAStar != nullptr)
		{
			*pOutEnteredAStar = false;
		}
	}
#endif // BT_SERVER

	float fBaseHeight = gBaseHeight.mfCurrent;
	ASSERT(XMVectorGetZ(vecPosition) == fBaseHeight);
	ASSERT(XMVectorGetZ(vecDestination) == fBaseHeight);

	// Seed the waypoint with the destination and W=1 before early exits; successful paths replace it.
	if (pOutNextWaypoint != nullptr)
	{
		*pOutNextWaypoint = XMVectorSetW(vecDestination, 1.0f);
	}

	XMVECTOR vecDelta = XMVectorSubtract(vecDestination, vecPosition);
	if (XMVectorGetX(XMVector3LengthSq(vecDelta)) < 1e-8f)
	{
		return XMVectorZero();
	}

	if (rNavData.vertices.empty())
	{
		return XMVector3Normalize(vecDelta);
	}

	int64_t iVertexCount = std::ssize(rNavData.vertices);
	int64_t iTotalNodes = iVertexCount + 2;

	// Workbuffer allocation for A* scratch memory only (vertices already in NavData's cell-local frame)
	AStarMemoryLayout aStarMemoryLayout = ComputeAStarMemoryLayout(iTotalNodes, iVertexCount);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	auto pMemory = rWorkbuffer.PushBuffer<std::byte*>(aStarMemoryLayout.iByteCount);
	AStarMemory aStarMemory = BindAStarMemory(pMemory.mpData, aStarMemoryLayout);

	const XMFLOAT2* pVertices = rNavData.vertices.data();

	XMFLOAT4A f4Position {};
	XMStoreFloat4A(&f4Position, vecPosition);
	XMFLOAT4A f4Destination {};
	XMStoreFloat4A(&f4Destination, vecDestination);

	XMFLOAT2 f2Position {f4Position.x, f4Position.y};
	XMFLOAT2 f2Destination {f4Destination.x, f4Destination.y};

	XMVECTOR vecResult = XMVectorZero();

	// If start is inside an obstacle, direct toward nearest polygon edge to escape first
	if (PointInAnyPolygon(f2Position, pVertices, rNavData))
	{
		XMFLOAT2 f2SnapPoint = SnapOutsidePolygon(f2Position, pVertices, rNavData);
		XMVECTOR vecEscape = XMVectorSet(f2SnapPoint.x - f4Position.x, f2SnapPoint.y - f4Position.y, 0.0f, 0.0f);
		if (XMVectorGetX(XMVector3LengthSq(vecEscape)) > 1e-8f)
		{
			if (pOutNextWaypoint != nullptr)
			{
				*pOutNextWaypoint = XMVectorSet(f2SnapPoint.x, f2SnapPoint.y, fBaseHeight, 1.0f);
			}
			return XMVector3Normalize(vecEscape);
		}
		f2Position = f2SnapPoint;
	}

	if (PointInAnyPolygon(f2Destination, pVertices, rNavData))
	{
		f2Destination = SnapOutsidePolygon(f2Destination, pVertices, rNavData);
	}

	if (!SegmentBlockedByObstacle(f2Position, f2Destination, pVertices, rNavData))
	{
		if (pOutNextWaypoint != nullptr)
		{
			*pOutNextWaypoint = XMVectorSet(f2Destination.x, f2Destination.y, fBaseHeight, 1.0f);
		}
		vecResult = XMVector3Normalize(XMVectorSet(f2Destination.x - f2Position.x, f2Destination.y - f2Position.y, 0.0f, 0.0f));
	}
	else
	{
#if defined(BT_SERVER)
		if constexpr (kbProfiling)
		{
			if (pOutEnteredAStar != nullptr)
			{
				*pOutEnteredAStar = true;
			}
		}
#endif // BT_SERVER
		vecResult = AStarPath(f2Position, f2Destination, pVertices, rNavData, aStarMemory, fBaseHeight, pOutNextWaypoint);

		// Fallback: if A* found no path, slide along the nearest obstacle boundary. The visibility graph
		// spans the whole cell, so a miss means it is disconnected — an invariant violation worth a warning.
		if (XMVectorGetX(XMVector3LengthSq(vecResult)) < 1e-8f)
		{
			LOG(kNavData, kWarning, "NavQuery A* found no path: position={} destination={} vertices={}", common::WbV2(XMLoadFloat2(&f2Position), 2), common::WbV2(XMLoadFloat2(&f2Destination), 2), iVertexCount);
			vecResult = NavMissFallbackDirection(f2Position, f2Destination, pVertices, rNavData);
		}
	}

	return vecResult;
}

} // namespace engine
