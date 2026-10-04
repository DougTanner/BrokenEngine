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
		int32_t iSlot0 = -1;
		int32_t iSlot1 = -1;
	};

	void Run();
	void SplitInBand(uint32_t uiTriangle, uint32_t uiA, uint32_t uiB, uint32_t uiC, uint32_t uiMidpointAB, uint32_t uiMidpointBC, uint32_t uiMidpointCA, uint8_t uiChildDepth);
	void AbsorbMidpoints(uint32_t uiTriangle, uint32_t uiA, uint32_t uiB, uint32_t uiC, uint32_t uiMidpointAB, uint32_t uiMidpointBC, uint32_t uiMidpointCA, int32_t iExistingMidpoints, uint8_t uiChildDepth);

	uint64_t EdgeKey(uint32_t uiA, uint32_t uiB) const
	{
		uint32_t uiMinimum = std::min(uiA, uiB);
		uint32_t uiMaximum = std::max(uiA, uiB);
		return (static_cast<uint64_t>(uiMinimum) << 32) | static_cast<uint64_t>(uiMaximum);
	}

	float VertexX(uint32_t uiV) const
	{
		return rMeshPositions.at(static_cast<int64_t>(uiV) * 3 + 0);
	}
	float VertexY(uint32_t uiV) const
	{
		return rMeshPositions.at(static_cast<int64_t>(uiV) * 3 + 1);
	}
	float VertexZ(uint32_t uiV) const
	{
		return rMeshPositions.at(static_cast<int64_t>(uiV) * 3 + 2);
	}

	void EdgeAdd(uint64_t uiKey, int32_t iTriangle)
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

	void EdgeRemove(uint64_t uiKey, int32_t iTriangle)
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

	int32_t EdgeOther(uint64_t uiKey, int32_t iTriangle) const
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

	bool IsBandEligible(uint32_t uiA, uint32_t uiB, uint32_t uiC) const
	{
		float fMinimumZ = std::min({VertexZ(uiA), VertexZ(uiB), VertexZ(uiC)});
		float fMaximumZ = std::max({VertexZ(uiA), VertexZ(uiB), VertexZ(uiC)});
		return fMinimumZ <= fBandMaximumZ && fMaximumZ >= fBandMinimumZ;
	}

	float LongestEdgeXY(uint32_t uiA, uint32_t uiB, uint32_t uiC) const
	{
		auto LengthSquared = [this](uint32_t uiU, uint32_t uiV) -> float
		{
			float fDeltaX = VertexX(uiV) - VertexX(uiU);
			float fDeltaY = VertexY(uiV) - VertexY(uiU);
			return fDeltaX * fDeltaX + fDeltaY * fDeltaY;
		};
		float fMaximumSquared = std::max({LengthSquared(uiA, uiB), LengthSquared(uiB, uiC), LengthSquared(uiC, uiA)});
		return std::sqrt(fMaximumSquared);
	}

	uint32_t AddVertex(float fX, float fY, float fZ)
	{
		uint32_t uiNewIndex = static_cast<uint32_t>(rMeshPositions.size() / 3);
		rMeshPositions.insert(rMeshPositions.end(), {fX, fY, fZ});
		return uiNewIndex;
	}

	uint32_t GetOrCreateMidpoint(uint32_t uiA, uint32_t uiB)
	{
		uint64_t uiKey = EdgeKey(uiA, uiB);
		auto it = edgeMidpoints.find(uiKey);
		if (it != edgeMidpoints.end())
		{
			return it->second;
		}
		float fMidpointX = 0.5f * (VertexX(uiA) + VertexX(uiB));
		float fMidpointY = 0.5f * (VertexY(uiA) + VertexY(uiB));
		float fMidpointZ = 0.5f * (VertexZ(uiA) + VertexZ(uiB));
		uint32_t uiMidpoint = AddVertex(fMidpointX, fMidpointY, fMidpointZ);
		edgeMidpoints.emplace(uiKey, uiMidpoint);
		return uiMidpoint;
	}

	uint32_t LookupMidpoint(uint32_t uiA, uint32_t uiB) const
	{
		auto it = edgeMidpoints.find(EdgeKey(uiA, uiB));
		return it != edgeMidpoints.end() ? it->second : UINT32_MAX;
	}

	uint32_t AppendTriangle(uint32_t uiA, uint32_t uiB, uint32_t uiC, uint8_t uiDepth)
	{
		uint32_t uiNewTriangle = static_cast<uint32_t>(rMeshIndices.size() / 3);
		rMeshIndices.insert(rMeshIndices.end(), {uiA, uiB, uiC});
		triangleAlive.push_back(1);
		triangleDepth.push_back(uiDepth);
		EdgeAdd(EdgeKey(uiA, uiB), static_cast<int32_t>(uiNewTriangle));
		EdgeAdd(EdgeKey(uiB, uiC), static_cast<int32_t>(uiNewTriangle));
		EdgeAdd(EdgeKey(uiC, uiA), static_cast<int32_t>(uiNewTriangle));
		return uiNewTriangle;
	}

	void KillTriangle(uint32_t uiTriangle)
	{
		uint32_t uiA = rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 0);
		uint32_t uiB = rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 1);
		uint32_t uiC = rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 2);
		EdgeRemove(EdgeKey(uiA, uiB), static_cast<int32_t>(uiTriangle));
		EdgeRemove(EdgeKey(uiB, uiC), static_cast<int32_t>(uiTriangle));
		EdgeRemove(EdgeKey(uiC, uiA), static_cast<int32_t>(uiTriangle));
		triangleAlive[uiTriangle] = 0;
		rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 0) = UINT32_MAX;
		rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 1) = UINT32_MAX;
		rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 2) = UINT32_MAX;
	}

	std::vector<float>& rMeshPositions;
	std::vector<uint32_t>& rMeshIndices;
	float fBandMinimumZ = 0.0f;
	float fBandMaximumZ = 0.0f;
	float fMaximumEdge = 0.0f;
	int32_t iMaximumDepth = 0;
	int64_t& riDepthCapHits;

	std::unordered_map<uint64_t, uint32_t> edgeMidpoints;
	std::unordered_map<uint64_t, EdgeSlots> edgeTriangles;
	std::deque<uint32_t> worklist;
	std::vector<uint8_t> triangleAlive;
	std::vector<uint8_t> triangleDepth;
};

void BeachSubdivider::Run()
{
	int64_t iInitialTriangleCount = static_cast<int64_t>(rMeshIndices.size() / 3);
	triangleAlive.assign(static_cast<size_t>(iInitialTriangleCount), 1);
	triangleDepth.assign(static_cast<size_t>(iInitialTriangleCount), 0);

	for (int64_t i = 0; i < iInitialTriangleCount; ++i)
	{
		uint32_t uiA = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 0);
		uint32_t uiB = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 1);
		uint32_t uiC = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 2);
		EdgeAdd(EdgeKey(uiA, uiB), static_cast<int32_t>(i));
		EdgeAdd(EdgeKey(uiB, uiC), static_cast<int32_t>(i));
		EdgeAdd(EdgeKey(uiC, uiA), static_cast<int32_t>(i));
	}

	for (int64_t i = 0; i < iInitialTriangleCount; ++i)
	{
		uint32_t uiA = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 0);
		uint32_t uiB = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 1);
		uint32_t uiC = rMeshIndices.at(static_cast<int64_t>(i) * 3 + 2);
		if (IsBandEligible(uiA, uiB, uiC) && LongestEdgeXY(uiA, uiB, uiC) > fMaximumEdge)
		{
			worklist.push_back(static_cast<uint32_t>(i));
		}
	}

	while (!worklist.empty())
	{
		uint32_t uiTriangle = worklist.front();
		worklist.pop_front();
		if (triangleAlive.at(uiTriangle) == 0)
		{
			continue;
		}
		if (triangleDepth.at(uiTriangle) >= iMaximumDepth)
		{
			++riDepthCapHits;
			continue;
		}

		uint32_t uiA = rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 0);
		uint32_t uiB = rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 1);
		uint32_t uiC = rMeshIndices.at(static_cast<int64_t>(uiTriangle) * 3 + 2);

		bool bBandTrigger = IsBandEligible(uiA, uiB, uiC) && LongestEdgeXY(uiA, uiB, uiC) > fMaximumEdge;
		uint32_t uiMidpointAB = LookupMidpoint(uiA, uiB);
		uint32_t uiMidpointBC = LookupMidpoint(uiB, uiC);
		uint32_t uiMidpointCA = LookupMidpoint(uiC, uiA);
		int32_t iExistingMidpoints = (uiMidpointAB != UINT32_MAX) + (uiMidpointBC != UINT32_MAX) + (uiMidpointCA != UINT32_MAX);

		if (!bBandTrigger && iExistingMidpoints == 0)
		{
			continue;
		}

		uint8_t uiChildDepth = static_cast<uint8_t>(triangleDepth.at(uiTriangle) + 1);

		if (bBandTrigger)
		{
			SplitInBand(uiTriangle, uiA, uiB, uiC, uiMidpointAB, uiMidpointBC, uiMidpointCA, uiChildDepth);
			continue;
		}

		AbsorbMidpoints(uiTriangle, uiA, uiB, uiC, uiMidpointAB, uiMidpointBC, uiMidpointCA, iExistingMidpoints, uiChildDepth);
	}

	// UINT32_MAX marks dead triangles.
	std::vector<uint32_t> compactedIndices;
	compactedIndices.reserve(rMeshIndices.size());
	for (int64_t i = 0; i < std::ssize(rMeshIndices); i += 3)
	{
		if (rMeshIndices.at(i) == UINT32_MAX)
		{
			continue;
		}
		compactedIndices.push_back(rMeshIndices.at(i + 0));
		compactedIndices.push_back(rMeshIndices.at(i + 1));
		compactedIndices.push_back(rMeshIndices.at(i + 2));
	}
	rMeshIndices = std::move(compactedIndices);
}

void BeachSubdivider::SplitInBand(uint32_t uiTriangle, uint32_t uiA, uint32_t uiB, uint32_t uiC, uint32_t uiMidpointAB, uint32_t uiMidpointBC, uint32_t uiMidpointCA, uint8_t uiChildDepth)
{
	// In-band 1->4 split. Capture neighbors BEFORE mutating edge tables so we know
	// who to notify on each parent edge. Create midpoints for any edges that don't
	// already have one; those new midpoints are what neighbors will absorb.
	int32_t iNeighborAB = EdgeOther(EdgeKey(uiA, uiB), static_cast<int32_t>(uiTriangle));
	int32_t iNeighborBC = EdgeOther(EdgeKey(uiB, uiC), static_cast<int32_t>(uiTriangle));
	int32_t iNeighborCA = EdgeOther(EdgeKey(uiC, uiA), static_cast<int32_t>(uiTriangle));

	if (uiMidpointAB == UINT32_MAX)
	{
		uiMidpointAB = GetOrCreateMidpoint(uiA, uiB);
	}
	if (uiMidpointBC == UINT32_MAX)
	{
		uiMidpointBC = GetOrCreateMidpoint(uiB, uiC);
	}
	if (uiMidpointCA == UINT32_MAX)
	{
		uiMidpointCA = GetOrCreateMidpoint(uiC, uiA);
	}

	KillTriangle(uiTriangle);

	uint32_t uiChild0 = AppendTriangle(uiA, uiMidpointAB, uiMidpointCA, uiChildDepth);
	uint32_t uiChild1 = AppendTriangle(uiMidpointAB, uiB, uiMidpointBC, uiChildDepth);
	uint32_t uiChild2 = AppendTriangle(uiMidpointCA, uiMidpointBC, uiC, uiChildDepth);
	uint32_t uiChild3 = AppendTriangle(uiMidpointAB, uiMidpointBC, uiMidpointCA, uiChildDepth);

	worklist.push_back(uiChild0);
	worklist.push_back(uiChild1);
	worklist.push_back(uiChild2);
	worklist.push_back(uiChild3);

	if (iNeighborAB >= 0 && triangleAlive.at(static_cast<int64_t>(iNeighborAB)) != 0)
	{
		worklist.push_back(static_cast<uint32_t>(iNeighborAB));
	}
	if (iNeighborBC >= 0 && triangleAlive.at(static_cast<int64_t>(iNeighborBC)) != 0)
	{
		worklist.push_back(static_cast<uint32_t>(iNeighborBC));
	}
	if (iNeighborCA >= 0 && triangleAlive.at(static_cast<int64_t>(iNeighborCA)) != 0)
	{
		worklist.push_back(static_cast<uint32_t>(iNeighborCA));
	}
}

void BeachSubdivider::AbsorbMidpoints(uint32_t uiTriangle, uint32_t uiA, uint32_t uiB, uint32_t uiC, uint32_t uiMidpointAB, uint32_t uiMidpointBC, uint32_t uiMidpointCA, int32_t iExistingMidpoints, uint8_t uiChildDepth)
{
	// Existing midpoints add no new T-junctions to adjacent triangles.
	KillTriangle(uiTriangle);

	if (iExistingMidpoints == 1)
	{
		if (uiMidpointAB != UINT32_MAX)
		{
			AppendTriangle(uiA, uiMidpointAB, uiC, uiChildDepth);
			AppendTriangle(uiMidpointAB, uiB, uiC, uiChildDepth);
		}
		else if (uiMidpointBC != UINT32_MAX)
		{
			AppendTriangle(uiA, uiB, uiMidpointBC, uiChildDepth);
			AppendTriangle(uiA, uiMidpointBC, uiC, uiChildDepth);
		}
		else
		{
			AppendTriangle(uiA, uiB, uiMidpointCA, uiChildDepth);
			AppendTriangle(uiB, uiC, uiMidpointCA, uiChildDepth);
		}
	}
	else if (iExistingMidpoints == 2)
	{
		if (uiMidpointAB != UINT32_MAX && uiMidpointBC != UINT32_MAX)
		{
			// Midpoints on A-B and B-C. Corner B is between them.
			AppendTriangle(uiMidpointAB, uiB, uiMidpointBC, uiChildDepth);
			AppendTriangle(uiA, uiMidpointAB, uiMidpointBC, uiChildDepth);
			AppendTriangle(uiA, uiMidpointBC, uiC, uiChildDepth);
		}
		else if (uiMidpointBC != UINT32_MAX && uiMidpointCA != UINT32_MAX)
		{
			// Midpoints on B-C and C-A. Corner C is between them.
			AppendTriangle(uiMidpointBC, uiC, uiMidpointCA, uiChildDepth);
			AppendTriangle(uiB, uiMidpointBC, uiMidpointCA, uiChildDepth);
			AppendTriangle(uiA, uiB, uiMidpointCA, uiChildDepth);
		}
		else
		{
			// Midpoints on A-B and C-A. Corner A is between them.
			AppendTriangle(uiA, uiMidpointAB, uiMidpointCA, uiChildDepth);
			AppendTriangle(uiMidpointAB, uiB, uiMidpointCA, uiChildDepth);
			AppendTriangle(uiMidpointCA, uiB, uiC, uiChildDepth);
		}
	}
	else
	{
		AppendTriangle(uiA, uiMidpointAB, uiMidpointCA, uiChildDepth);
		AppendTriangle(uiMidpointAB, uiB, uiMidpointBC, uiChildDepth);
		AppendTriangle(uiMidpointCA, uiMidpointBC, uiC, uiChildDepth);
		AppendTriangle(uiMidpointAB, uiMidpointBC, uiMidpointCA, uiChildDepth);
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
