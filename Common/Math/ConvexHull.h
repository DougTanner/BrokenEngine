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

// Input vertices are centered at the origin and the output keeps their winding, so a hull that must
// be CCW starts CCW. (fCos, fSin) follows GlobalElevation's CCW convention and must be deterministic:
// island placement derives it with common::DeterministicSinCos, ship hitboxes take it from the unit
// heading. The output storage holds at least the input vertex count.
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

// Bake-time producers verify this before serializing a hull for ConvexHullsOverlap or a prism test: CCW, with every
// vertex turning strictly left. Callers exclude polygons with fewer than three vertices.
inline bool IsPolygonStrictlyConvexCcw(std::span<const XMFLOAT2> vertices)
{
	if (!IsPolygonCcw(vertices))
	{
		return false;
	}
	int64_t iVertexCount = std::ssize(vertices);
	for (int64_t i = 0; i < iVertexCount; ++i)
	{
		const XMFLOAT2& rPreviousVertex = vertices[static_cast<size_t>((i + iVertexCount - 1) % iVertexCount)];
		const XMFLOAT2& rCurrentVertex = vertices[static_cast<size_t>(i)];
		const XMFLOAT2& rNextVertex = vertices[static_cast<size_t>((i + 1) % iVertexCount)];
		float fCross = (rCurrentVertex.x - rPreviousVertex.x) * (rNextVertex.y - rPreviousVertex.y) - (rCurrentVertex.y - rPreviousVertex.y) * (rNextVertex.x - rPreviousVertex.x);
		if (fCross <= 0.0f)
		{
			return false;
		}
	}
	return true;
}

// Andrew's monotone chain, for offline bakes (it allocates). Sorts points in place by x, then y, and returns their CCW
// convex hull with collinear points dropped. Fewer than three points, or all points collinear, give fewer than three
// vertices.
inline std::vector<XMFLOAT2> BuildConvexHull(std::span<XMFLOAT2> points)
{
	if (std::ssize(points) < 3)
	{
		return {};
	}

	std::sort(points.begin(), points.end(), [](const XMFLOAT2& rA, const XMFLOAT2& rB)
	{
		return rA.x < rB.x || (rA.x == rB.x && rA.y < rB.y);
	});

	// Cross product of (rA-rO) x (rB-rO); <= 0 drops collinear points, yielding a CCW hull.
	auto Cross = [](const XMFLOAT2& rO, const XMFLOAT2& rA, const XMFLOAT2& rB)
	{
		return (rA.x - rO.x) * (rB.y - rO.y) - (rA.y - rO.y) * (rB.x - rO.x);
	};

	int64_t iCount = std::ssize(points);
	std::vector<XMFLOAT2> hull(static_cast<size_t>(iCount) * 2);
	int64_t iHullVertexCount = 0;
	for (const XMFLOAT2& rPoint : points)
	{
		while (iHullVertexCount >= 2 && Cross(hull.at(static_cast<size_t>(iHullVertexCount - 2)), hull.at(static_cast<size_t>(iHullVertexCount - 1)), rPoint) <= 0.0f)
		{
			--iHullVertexCount;
		}
		hull.at(static_cast<size_t>(iHullVertexCount++)) = rPoint;
	}
	int64_t iLower = iHullVertexCount + 1;
	for (int64_t i = iCount - 2; i >= 0; --i)
	{
		const XMFLOAT2& rPoint = points[static_cast<size_t>(i)];
		while (iHullVertexCount >= iLower && Cross(hull.at(static_cast<size_t>(iHullVertexCount - 2)), hull.at(static_cast<size_t>(iHullVertexCount - 1)), rPoint) <= 0.0f)
		{
			--iHullVertexCount;
		}
		hull.at(static_cast<size_t>(iHullVertexCount++)) = rPoint;
	}
	// Last point repeats the first; drop it.
	hull.resize(static_cast<size_t>(iHullVertexCount) - 1);
	return hull;
}

// Winding-number test for a simple polygon of either direction. The shared core of navigation's obstacle tests and the
// outline hit tests, so boundary rules cannot drift between callers.
inline bool PointInPolygon(XMFLOAT2 f2Point, std::span<const XMFLOAT2> vertices)
{
	int64_t iVertexCount = std::ssize(vertices);
	int64_t iWinding = 0;
	for (int64_t i = 0; i < iVertexCount; ++i)
	{
		int64_t iNext = (i + 1) % iVertexCount;
		XMFLOAT2 f2A = vertices[i];
		XMFLOAT2 f2B = vertices[iNext];

		if (f2A.y <= f2Point.y)
		{
			if (f2B.y > f2Point.y)
			{
				float fCross = (f2B.x - f2A.x) * (f2Point.y - f2A.y) - (f2Point.x - f2A.x) * (f2B.y - f2A.y);
				if (fCross > 0.0f)
				{
					++iWinding;
				}
			}
		}
		else
		{
			if (f2B.y <= f2Point.y)
			{
				float fCross = (f2B.x - f2A.x) * (f2Point.y - f2A.y) - (f2Point.x - f2A.x) * (f2B.y - f2A.y);
				if (fCross < 0.0f)
				{
					--iWinding;
				}
			}
		}
	}
	return iWinding != 0;
}

// Cyrus-Beck clip: narrows [rfEnter, rfExit] of the point path rf3Start + t * rf3Delta to the part inside the CCW
// convex polygon extruded over [fMinimumZ, fMaximumZ], boundary included. Returns false once the interval is empty.
// It lives here because ModelViewer compiles the navigation query but not Collision.cpp.
inline bool ClipSegmentToPrism(const XMFLOAT3A& rf3Start, const XMFLOAT3A& rf3Delta, std::span<const XMFLOAT2> vertices, float fMinimumZ, float fMaximumZ, float& rfEnter, float& rfExit)
{
	// The path is inside the half-space where fOutside + t * fRate <= 0.
	auto ClipToHalfSpace = [&rfEnter, &rfExit](float fOutside, float fRate)
	{
		if (fRate == 0.0f)
		{
			return fOutside <= 0.0f;
		}
		float fTime = -fOutside / fRate;
		if (fRate < 0.0f)
		{
			rfEnter = std::max(rfEnter, fTime);
		}
		else
		{
			rfExit = std::min(rfExit, fTime);
		}
		return rfEnter <= rfExit;
	};

	if (!ClipToHalfSpace(fMinimumZ - rf3Start.z, -rf3Delta.z) || !ClipToHalfSpace(rf3Start.z - fMaximumZ, rf3Delta.z))
	{
		return false;
	}
	for (int64_t i = 0; i < std::ssize(vertices); ++i)
	{
		const XMFLOAT2& rStartVertex = vertices[i];
		const XMFLOAT2& rEndVertex = vertices[(i + 1) % std::ssize(vertices)];
		// Outward normal of the CCW edge (v1 - v0) is (edge.y, -edge.x).
		float fNormalX = rEndVertex.y - rStartVertex.y;
		float fNormalY = rStartVertex.x - rEndVertex.x;
		float fOutside = fNormalX * (rf3Start.x - rStartVertex.x) + fNormalY * (rf3Start.y - rStartVertex.y);
		float fRate = fNormalX * rf3Delta.x + fNormalY * rf3Delta.y;
		if (!ClipToHalfSpace(fOutside, fRate))
		{
			return false;
		}
	}
	return true;
}

// Moves a polygon's edges outward by fOffsetMeters, inward when negative, then simplifies the result within
// fSimplifyEpsilonMeters. Returns the largest resulting path, or nothing when none survives. For offline bakes and
// explicit UI edits: it allocates.
inline std::vector<XMFLOAT2> OffsetPolygon(std::span<const XMFLOAT2> vertices, float fOffsetMeters, float fSimplifyEpsilonMeters)
{
	static constexpr double kfMiterLimit = 2.0;
	static constexpr int64_t kiClipperPrecision = 4;

	Clipper2Lib::PathsD polygon(1);
	polygon.at(0).reserve(vertices.size());
	for (const XMFLOAT2& rVertex : vertices)
	{
		polygon.at(0).emplace_back(rVertex.x, rVertex.y);
	}

	Clipper2Lib::PathsD offset = Clipper2Lib::InflatePaths(polygon, static_cast<double>(fOffsetMeters), Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon, kfMiterLimit, static_cast<int>(kiClipperPrecision));
	// The offset keeps the input's direction, so the largest path is the one of greatest absolute area.
	auto it = std::ranges::max_element(offset, {}, [](const Clipper2Lib::PathD& rPath)
	{
		return std::abs(Clipper2Lib::Area(rPath));
	});
	if (it == offset.end())
	{
		return {};
	}

	Clipper2Lib::PathD simplified = Clipper2Lib::SimplifyPath(*it, static_cast<double>(fSimplifyEpsilonMeters));
	std::vector<XMFLOAT2> result;
	result.reserve(simplified.size());
	for (const Clipper2Lib::PointD& rPoint : simplified)
	{
		result.emplace_back(static_cast<float>(rPoint.x), static_cast<float>(rPoint.y));
	}
	return result;
}

} // namespace common
