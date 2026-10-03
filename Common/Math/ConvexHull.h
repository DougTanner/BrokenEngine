#pragma once

namespace common
{

// A 2D convex hull in world space: CCW vertices plus a precomputed AABB for broadphase rejection.
// pVertices points into caller-owned storage (e.g. a scratch vector) that must outlive the hull;
// BuildWorldHull fills it from a template's island-local polygon. Used by IslandChainPlacement to
// pack islands by their true valid-area hull (rectangles may overlap underwater, hulls may not).
struct ConvexHull2D
{
	const XMFLOAT2* pVertices = nullptr;
	int64_t iVertexCount = 0;
	XMFLOAT2 f2AabbMin {};
	XMFLOAT2 f2AabbMax {};
};

// Input vertices are CCW and centered at the origin; (fCos, fSin) follows GlobalElevation's CCW
// convention. The output storage holds at least the input vertex count. Derive (fCos, fSin) with
// common::DeterministicSinCos because this math feeds CRC-verified placement.
inline ConvexHull2D BuildWorldHull(std::span<const XMFLOAT2> localVertices, XMFLOAT2 f2WorldPosition, float fCos, float fSin, std::span<XMFLOAT2> outputVertices)
{
	XMFLOAT2 f2Min {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
	XMFLOAT2 f2Max {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
	for (int64_t i = 0; i < static_cast<int64_t>(localVertices.size()); ++i)
	{
		float fLocalX = localVertices[i].x;
		float fLocalY = localVertices[i].y;
		float fWorldX = f2WorldPosition.x + fLocalX * fCos - fLocalY * fSin;
		float fWorldY = f2WorldPosition.y + fLocalX * fSin + fLocalY * fCos;
		outputVertices[i] = {fWorldX, fWorldY};
		f2Min.x = std::min(f2Min.x, fWorldX);
		f2Min.y = std::min(f2Min.y, fWorldY);
		f2Max.x = std::max(f2Max.x, fWorldX);
		f2Max.y = std::max(f2Max.y, fWorldY);
	}

	return {.pVertices = outputVertices.data(), .iVertexCount = static_cast<int64_t>(localVertices.size()), .f2AabbMin = f2Min, .f2AabbMax = f2Max};
}

// Strict inequalities let edge-touching AABBs pack flush.
inline bool AabbsOverlap2D(const ConvexHull2D& rFirstHull, const ConvexHull2D& rSecondHull)
{
	return rFirstHull.f2AabbMin.x < rSecondHull.f2AabbMax.x && rSecondHull.f2AabbMin.x < rFirstHull.f2AabbMax.x && rFirstHull.f2AabbMin.y < rSecondHull.f2AabbMax.y
	    && rSecondHull.f2AabbMin.y < rFirstHull.f2AabbMax.y;
}

// Separating Axis Theorem for two CCW convex polygons. True iff they share interior area; edge-
// touching returns false so placement can pack hulls flush without registering overlap. Scale-
// invariant (axes are un-normalized edge normals). Deterministic: fixed axis order (A's edges then
// B's, ascending index) and pure scalar float math.
inline bool ConvexHullsOverlap(const ConvexHull2D& rFirstHull, const ConvexHull2D& rSecondHull)
{
	if (!AabbsOverlap2D(rFirstHull, rSecondHull))
	{
		return false;
	}

	for (int64_t k = 0; k < 2; ++k)
	{
		const ConvexHull2D& rEdgeHull = (k == 0) ? rFirstHull : rSecondHull;
		for (int64_t i = 0; i < rEdgeHull.iVertexCount; ++i)
		{
			const XMFLOAT2& rStartVertex = rEdgeHull.pVertices[i];
			const XMFLOAT2& rEndVertex = rEdgeHull.pVertices[(i + 1) % rEdgeHull.iVertexCount];
			// Outward normal of the CCW edge (v1 - v0) is (edge.y, -edge.x).
			float fAxisX = rEndVertex.y - rStartVertex.y;
			float fAxisY = rStartVertex.x - rEndVertex.x;

			float fMinA = std::numeric_limits<float>::max();
			float fMaxA = std::numeric_limits<float>::lowest();
			for (int64_t j = 0; j < rFirstHull.iVertexCount; ++j)
			{
				float fProjection = rFirstHull.pVertices[j].x * fAxisX + rFirstHull.pVertices[j].y * fAxisY;
				fMinA = std::min(fMinA, fProjection);
				fMaxA = std::max(fMaxA, fProjection);
			}

			float fMinB = std::numeric_limits<float>::max();
			float fMaxB = std::numeric_limits<float>::lowest();
			for (int64_t j = 0; j < rSecondHull.iVertexCount; ++j)
			{
				float fProjection = rSecondHull.pVertices[j].x * fAxisX + rSecondHull.pVertices[j].y * fAxisY;
				fMinB = std::min(fMinB, fProjection);
				fMaxB = std::max(fMaxB, fProjection);
			}

			if (fMaxA <= fMinB || fMaxB <= fMinA)
			{
				return false;
			}
		}
	}

	return true;
}

// The shoelace sum is positive for CCW polygons in the y-up frame. ConvexHullsOverlap requires CCW
// input, and its producers assert this. Nav's nonzero-winding test is orientation-agnostic; its
// assertion is a truncation tripwire because BuildCellNavigationData mirrors Y, making those polygons
// clockwise. Runtime and bake-time checks use this shared predicate; callers exclude polygons with
// fewer than three vertices.
inline bool IsPolygonCcw(std::span<const XMFLOAT2> vertices)
{
	const XMFLOAT2& rOrigin = vertices[0];
	float fSignedArea = 0.0f;
	for (int64_t i = 0; i < static_cast<int64_t>(vertices.size()); ++i)
	{
		const XMFLOAT2& rCurrentVertex = vertices[i];
		const XMFLOAT2& rNextVertex = vertices[(i + 1) % vertices.size()];
		float fCurrentVertexX = rCurrentVertex.x - rOrigin.x;
		float fCurrentVertexY = rCurrentVertex.y - rOrigin.y;
		float fNextVertexX = rNextVertex.x - rOrigin.x;
		float fNextVertexY = rNextVertex.y - rOrigin.y;
		fSignedArea += fCurrentVertexX * fNextVertexY - fNextVertexX * fCurrentVertexY;
	}
	return fSignedArea > 0.0f;
}

} // namespace common
