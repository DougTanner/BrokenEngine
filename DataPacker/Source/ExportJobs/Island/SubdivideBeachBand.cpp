#include "SubdivideBeachBand.h"


// BeachSubdivider keeps caller-owned mesh/config references that outlive Run; worklist, per-triangle
// state, and edge maps are internal. Run drives the worklist.
struct BeachSubdivider
{
	BeachSubdivider(std::vector<float>& rMeshPositions, std::vector<uint32_t>& rMeshIndices, const SubdivisionConfig& rConfig, int64_t& riDepthCapHits)
		: rMeshPositions(rMeshPositions)
		, rMeshIndices(rMeshIndices)
		, fBandMinimumZ(rConfig.fBandMinimumMeters)
		, fBandMaximumZ(rConfig.fBandMaximumMeters)
		, fMaximumEdge(rConfig.fMaximumEdgeMeters)
		, iMaximumDepth(rConfig.iMaximumDepth)
		, riDepthCapHits(riDepthCapHits)
	{
	}

	// Edge -> the (up to) two triangles sharing it. -1 marks an empty slot.
	struct EdgeSlots
	{
		int64_t iSlot0 = -1;
		int64_t iSlot1 = -1;
	};

	void Run();
	void SplitInBand(int64_t iTriangle, int64_t iA, int64_t iB, int64_t iC, int64_t iMidpointAB, int64_t iMidpointBC, int64_t iMidpointCA, int64_t iChildDepth);
	void AbsorbMidpoints(int64_t iTriangle, int64_t iA, int64_t iB, int64_t iC, int64_t iMidpointAB, int64_t iMidpointBC, int64_t iMidpointCA, int64_t iExistingMidpoints, int64_t iChildDepth);

	uint64_t EdgeKey(int64_t iA, int64_t iB) const
	{
		uint32_t uiMinimum = static_cast<uint32_t>(std::min(iA, iB));
		uint32_t uiMaximum = static_cast<uint32_t>(std::max(iA, iB));
		return (static_cast<uint64_t>(uiMinimum) << 32) | static_cast<uint64_t>(uiMaximum);
	}

	float VertexX(int64_t iV) const
	{
		return rMeshPositions.at(static_cast<size_t>(iV * 3 + 0));
	}
	float VertexY(int64_t iV) const
	{
		return rMeshPositions.at(static_cast<size_t>(iV * 3 + 1));
	}
	float VertexZ(int64_t iV) const
	{
		return rMeshPositions.at(static_cast<size_t>(iV * 3 + 2));
	}

	void EdgeAdd(uint64_t uiKey, int64_t iTriangle)
	{
		EdgeSlots& rSlots = edgeTriangles.try_emplace(uiKey, EdgeSlots {}).first->second;
		if (rSlots.iSlot0 < 0)
		{
			rSlots.iSlot0 = iTriangle;
			return;
		}
		if (rSlots.iSlot1 < 0)
		{
			rSlots.iSlot1 = iTriangle;
			return;
		}
		ASSERT(false);  // 3+ triangles share a single edge -- malformed mesh input.
	}

	void EdgeRemove(uint64_t uiKey, int64_t iTriangle)
	{
		auto it = edgeTriangles.find(uiKey);
		if (it == edgeTriangles.end())
		{
			return;
		}
		EdgeSlots& rSlots = it->second;
		if (rSlots.iSlot0 == iTriangle)
		{
			rSlots.iSlot0 = -1;
		}
		if (rSlots.iSlot1 == iTriangle)
		{
			rSlots.iSlot1 = -1;
		}
		if (rSlots.iSlot0 < 0 && rSlots.iSlot1 < 0)
		{
			edgeTriangles.erase(it);
		}
	}

	int64_t EdgeOther(uint64_t uiKey, int64_t iTriangle) const
	{
		auto it = edgeTriangles.find(uiKey);
		if (it == edgeTriangles.end())
		{
			return -1;
		}
		const EdgeSlots& rSlots = it->second;
		if (rSlots.iSlot0 == iTriangle)
		{
			return rSlots.iSlot1;
		}
		if (rSlots.iSlot1 == iTriangle)
		{
			return rSlots.iSlot0;
		}
		return -1;
	}

	bool IsBandEligible(int64_t iA, int64_t iB, int64_t iC) const
	{
		float fMinimumZ = std::min({VertexZ(iA), VertexZ(iB), VertexZ(iC)});
		float fMaximumZ = std::max({VertexZ(iA), VertexZ(iB), VertexZ(iC)});
		return fMinimumZ <= fBandMaximumZ && fMaximumZ >= fBandMinimumZ;
	}

	float LongestEdgeXY(int64_t iA, int64_t iB, int64_t iC) const
	{
		auto LengthSquared = [this](int64_t iU, int64_t iV) -> float
		{
			float fDeltaX = VertexX(iV) - VertexX(iU);
			float fDeltaY = VertexY(iV) - VertexY(iU);
			return fDeltaX * fDeltaX + fDeltaY * fDeltaY;
		};
		float fMaximumSquared = std::max({LengthSquared(iA, iB), LengthSquared(iB, iC), LengthSquared(iC, iA)});
		return std::sqrt(fMaximumSquared);
	}

	int64_t AddVertex(float fX, float fY, float fZ)
	{
		int64_t iNewIndex = std::ssize(rMeshPositions) / 3;
		rMeshPositions.insert(rMeshPositions.end(), {fX, fY, fZ});
		return iNewIndex;
	}

	int64_t GetOrCreateMidpoint(int64_t iA, int64_t iB)
	{
		uint64_t uiKey = EdgeKey(iA, iB);
		auto it = edgeMidpoints.find(uiKey);
		if (it != edgeMidpoints.end())
		{
			return it->second;
		}
		float fMidpointX = 0.5f * (VertexX(iA) + VertexX(iB));
		float fMidpointY = 0.5f * (VertexY(iA) + VertexY(iB));
		float fMidpointZ = 0.5f * (VertexZ(iA) + VertexZ(iB));
		int64_t iMidpoint = AddVertex(fMidpointX, fMidpointY, fMidpointZ);
		edgeMidpoints.emplace(uiKey, iMidpoint);
		return iMidpoint;
	}

	int64_t LookupMidpoint(int64_t iA, int64_t iB) const
	{
		auto it = edgeMidpoints.find(EdgeKey(iA, iB));
		return it != edgeMidpoints.end() ? it->second : UINT32_MAX;
	}

	int64_t AppendTriangle(int64_t iA, int64_t iB, int64_t iC, int64_t iDepth)
	{
		int64_t iNewTriangle = std::ssize(rMeshIndices) / 3;
		rMeshIndices.insert(rMeshIndices.end(), {static_cast<uint32_t>(iA), static_cast<uint32_t>(iB), static_cast<uint32_t>(iC)});
		triangleAlive.push_back(1);
		triangleDepth.push_back(iDepth);
		EdgeAdd(EdgeKey(iA, iB), iNewTriangle);
		EdgeAdd(EdgeKey(iB, iC), iNewTriangle);
		EdgeAdd(EdgeKey(iC, iA), iNewTriangle);
		return iNewTriangle;
	}

	void KillTriangle(int64_t iTriangle)
	{
		int64_t iA = rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 0));
		int64_t iB = rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 1));
		int64_t iC = rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 2));
		EdgeRemove(EdgeKey(iA, iB), iTriangle);
		EdgeRemove(EdgeKey(iB, iC), iTriangle);
		EdgeRemove(EdgeKey(iC, iA), iTriangle);
		triangleAlive[static_cast<size_t>(iTriangle)] = 0;
		rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 0)) = UINT32_MAX;
		rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 1)) = UINT32_MAX;
		rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 2)) = UINT32_MAX;
	}

	std::vector<float>& rMeshPositions;
	std::vector<uint32_t>& rMeshIndices;
	float fBandMinimumZ = 0.0f;
	float fBandMaximumZ = 0.0f;
	float fMaximumEdge = 0.0f;
	int64_t iMaximumDepth = 0;
	int64_t& riDepthCapHits;

	std::unordered_map<uint64_t, int64_t> edgeMidpoints;
	std::unordered_map<uint64_t, EdgeSlots> edgeTriangles;
	std::deque<int64_t> worklist;
	std::vector<int64_t> triangleAlive;
	std::vector<int64_t> triangleDepth;
};

void BeachSubdivider::Run()
{
	int64_t iInitialTriangleCount = std::ssize(rMeshIndices) / 3;
	triangleAlive.assign(static_cast<size_t>(iInitialTriangleCount), 1);
	triangleDepth.assign(static_cast<size_t>(iInitialTriangleCount), 0);

	for (int64_t i = 0; i < iInitialTriangleCount; ++i)
	{
		int64_t iA = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 0);
		int64_t iB = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 1);
		int64_t iC = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 2);
		EdgeAdd(EdgeKey(iA, iB), i);
		EdgeAdd(EdgeKey(iB, iC), i);
		EdgeAdd(EdgeKey(iC, iA), i);
	}

	for (int64_t i = 0; i < iInitialTriangleCount; ++i)
	{
		int64_t iA = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 0);
		int64_t iB = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 1);
		int64_t iC = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 2);
		if (IsBandEligible(iA, iB, iC) && LongestEdgeXY(iA, iB, iC) > fMaximumEdge)
		{
			worklist.push_back(i);
		}
	}

	while (!worklist.empty())
	{
		int64_t iTriangle = worklist.front();
		worklist.pop_front();
		if (triangleAlive.at(static_cast<size_t>(iTriangle)) == 0)
		{
			continue;
		}
		if (triangleDepth.at(static_cast<size_t>(iTriangle)) >= iMaximumDepth)
		{
			++riDepthCapHits;
			continue;
		}

		int64_t iA = rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 0));
		int64_t iB = rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 1));
		int64_t iC = rMeshIndices.at(static_cast<size_t>(iTriangle * 3 + 2));

		bool bBandTrigger = IsBandEligible(iA, iB, iC) && LongestEdgeXY(iA, iB, iC) > fMaximumEdge;
		int64_t iMidpointAB = LookupMidpoint(iA, iB);
		int64_t iMidpointBC = LookupMidpoint(iB, iC);
		int64_t iMidpointCA = LookupMidpoint(iC, iA);
		int64_t iExistingMidpoints = (iMidpointAB != UINT32_MAX) + (iMidpointBC != UINT32_MAX) + (iMidpointCA != UINT32_MAX);

		if (!bBandTrigger && iExistingMidpoints == 0)
		{
			continue;
		}

		int64_t iChildDepth = triangleDepth.at(static_cast<size_t>(iTriangle)) + 1;

		if (bBandTrigger)
		{
			SplitInBand(iTriangle, iA, iB, iC, iMidpointAB, iMidpointBC, iMidpointCA, iChildDepth);
			continue;
		}

		AbsorbMidpoints(iTriangle, iA, iB, iC, iMidpointAB, iMidpointBC, iMidpointCA, iExistingMidpoints, iChildDepth);
	}

	// UINT32_MAX marks dead triangles.
	std::vector<uint32_t> compactedIndices;
	compactedIndices.reserve(static_cast<size_t>(std::ssize(rMeshIndices)));
	for (int64_t i = 0; i < std::ssize(rMeshIndices); i += 3)
	{
		if (rMeshIndices.at(i) == UINT32_MAX)
		{
			continue;
		}
		compactedIndices.insert(compactedIndices.end(), {rMeshIndices.at(i + 0), rMeshIndices.at(i + 1), rMeshIndices.at(i + 2)});
	}
	rMeshIndices = std::move(compactedIndices);
}

void BeachSubdivider::SplitInBand(int64_t iTriangle, int64_t iA, int64_t iB, int64_t iC, int64_t iMidpointAB, int64_t iMidpointBC, int64_t iMidpointCA, int64_t iChildDepth)
{
	// In-band 1->4 split. Capture neighbors BEFORE mutating edge tables so we know
	// who to notify on each parent edge. Create midpoints for any edges that don't
	// already have one; those new midpoints are what neighbors will absorb.
	int64_t iNeighborAB = EdgeOther(EdgeKey(iA, iB), iTriangle);
	int64_t iNeighborBC = EdgeOther(EdgeKey(iB, iC), iTriangle);
	int64_t iNeighborCA = EdgeOther(EdgeKey(iC, iA), iTriangle);

	if (iMidpointAB == UINT32_MAX)
	{
		iMidpointAB = GetOrCreateMidpoint(iA, iB);
	}
	if (iMidpointBC == UINT32_MAX)
	{
		iMidpointBC = GetOrCreateMidpoint(iB, iC);
	}
	if (iMidpointCA == UINT32_MAX)
	{
		iMidpointCA = GetOrCreateMidpoint(iC, iA);
	}

	KillTriangle(iTriangle);

	int64_t iChild0 = AppendTriangle(iA, iMidpointAB, iMidpointCA, iChildDepth);
	int64_t iChild1 = AppendTriangle(iMidpointAB, iB, iMidpointBC, iChildDepth);
	int64_t iChild2 = AppendTriangle(iMidpointCA, iMidpointBC, iC, iChildDepth);
	int64_t iChild3 = AppendTriangle(iMidpointAB, iMidpointBC, iMidpointCA, iChildDepth);

	worklist.push_back(iChild0);
	worklist.push_back(iChild1);
	worklist.push_back(iChild2);
	worklist.push_back(iChild3);

	if (iNeighborAB >= 0 && triangleAlive.at(static_cast<size_t>(iNeighborAB)) != 0)
	{
		worklist.push_back(iNeighborAB);
	}
	if (iNeighborBC >= 0 && triangleAlive.at(static_cast<size_t>(iNeighborBC)) != 0)
	{
		worklist.push_back(iNeighborBC);
	}
	if (iNeighborCA >= 0 && triangleAlive.at(static_cast<size_t>(iNeighborCA)) != 0)
	{
		worklist.push_back(iNeighborCA);
	}
}

void BeachSubdivider::AbsorbMidpoints(int64_t iTriangle, int64_t iA, int64_t iB, int64_t iC, int64_t iMidpointAB, int64_t iMidpointBC, int64_t iMidpointCA, int64_t iExistingMidpoints, int64_t iChildDepth)
{
	// Existing midpoints add no new T-junctions to adjacent triangles.
	KillTriangle(iTriangle);

	if (iExistingMidpoints == 1)
	{
		if (iMidpointAB != UINT32_MAX)
		{
			AppendTriangle(iA, iMidpointAB, iC, iChildDepth);
			AppendTriangle(iMidpointAB, iB, iC, iChildDepth);
		}
		else if (iMidpointBC != UINT32_MAX)
		{
			AppendTriangle(iA, iB, iMidpointBC, iChildDepth);
			AppendTriangle(iA, iMidpointBC, iC, iChildDepth);
		}
		else
		{
			AppendTriangle(iA, iB, iMidpointCA, iChildDepth);
			AppendTriangle(iB, iC, iMidpointCA, iChildDepth);
		}
	}
	else if (iExistingMidpoints == 2)
	{
		if (iMidpointAB != UINT32_MAX && iMidpointBC != UINT32_MAX)
		{
			// Midpoints on A-B and B-C. Corner B is between them.
			AppendTriangle(iMidpointAB, iB, iMidpointBC, iChildDepth);
			AppendTriangle(iA, iMidpointAB, iMidpointBC, iChildDepth);
			AppendTriangle(iA, iMidpointBC, iC, iChildDepth);
		}
		else if (iMidpointBC != UINT32_MAX && iMidpointCA != UINT32_MAX)
		{
			// Midpoints on B-C and C-A. Corner C is between them.
			AppendTriangle(iMidpointBC, iC, iMidpointCA, iChildDepth);
			AppendTriangle(iB, iMidpointBC, iMidpointCA, iChildDepth);
			AppendTriangle(iA, iB, iMidpointCA, iChildDepth);
		}
		else
		{
			// Midpoints on A-B and C-A. Corner A is between them.
			AppendTriangle(iA, iMidpointAB, iMidpointCA, iChildDepth);
			AppendTriangle(iMidpointAB, iB, iMidpointCA, iChildDepth);
			AppendTriangle(iMidpointCA, iB, iC, iChildDepth);
		}
	}
	else
	{
		AppendTriangle(iA, iMidpointAB, iMidpointCA, iChildDepth);
		AppendTriangle(iMidpointAB, iB, iMidpointBC, iChildDepth);
		AppendTriangle(iMidpointCA, iMidpointBC, iC, iChildDepth);
		AppendTriangle(iMidpointAB, iMidpointBC, iMidpointCA, iChildDepth);
	}
	// Absorption children inherit the parent's out-of-band Z-range (Z-range of children
	// is a subset of the parent's), so they cannot trigger a band split themselves --
	// no need to push them. They will be re-pushed automatically if a future in-band
	// split creates a new midpoint on one of their edges (via the iNeighbor* lookups).
}


void SubdivideBeachBand(std::vector<float>& rMeshPositions, std::vector<uint32_t>& rMeshIndices, const SubdivisionConfig& rConfig, int64_t& riDepthCapHits)
{
	BeachSubdivider subdivider(rMeshPositions, rMeshIndices, rConfig, riDepthCapHits);
	subdivider.Run();
}
