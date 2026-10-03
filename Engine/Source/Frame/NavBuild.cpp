#include "NavBuild.h"

#include "NavBuildInternal.h"

namespace engine
{

struct ContourEdge
{
	XMFLOAT2 f2A {};
	XMFLOAT2 f2B {};
	// Cell-edge identifiers key each marching-squares endpoint; shared edges use one key on both sides,
	// so ChainEdgesIntoPolygons keeps a midpoint together without float drift.
	uint64_t uiKeyA = 0;
	uint64_t uiKeyB = 0;
};

// Encode (orientation, row, col) as a unique 64-bit cell-edge identifier. Horizontal edges have
// orient=0 with row ∈ [0, iHeight] / col ∈ [0, iWidth-1]; vertical edges have orient=1 with
// row ∈ [0, iHeight-1] / col ∈ [0, iWidth]. The 32+16+16 packing leaves comfortable headroom for
// the heightmap sizes the engine builds (kiElevationDivisor = 4 caps heightmap dims at a few
// thousand pixels).
static constexpr uint64_t EncodeEdgeKey(uint32_t uiOrientation, int32_t iRow, int32_t iColumn)
{
	return (static_cast<uint64_t>(uiOrientation) << 32) | (static_cast<uint64_t>(static_cast<uint32_t>(iRow)) << 16) | static_cast<uint64_t>(static_cast<uint32_t>(iColumn));
}

// Marching squares: extract isocontour edges at the given world-space elevation threshold.
// Heightmap pixels are engine-meters relative to beach (0 == sea level) — sampled directly.
// Heightmap is anisotropic (DataPacker auto-crop produces non-square dims); UV scale is per-axis
// so the contour lives in [0, 1]² regardless of aspect ratio. Row stride is iWidth.
static void ExtractContourEdges(std::vector<ContourEdge>& rEdges, std::span<const float> heightmapData, int32_t iWidth, int32_t iHeight, float fWorldThreshold)
{
	float fScaleU = 1.0f / static_cast<float>(iWidth - 1);
	float fScaleV = 1.0f / static_cast<float>(iHeight - 1);

	for (int32_t i = 0; i < iHeight - 1; ++i)
	{
		for (int32_t j = 0; j < iWidth - 1; ++j)
		{
			float fTopLeft = heightmapData[i * iWidth + j];
			float fTopRight = heightmapData[i * iWidth + j + 1];
			float fBottomRight = heightmapData[(i + 1) * iWidth + j + 1];
			float fBottomLeft = heightmapData[(i + 1) * iWidth + j];

			// Classification: 1 = above threshold (obstacle), 0 = below (navigable)
			uint32_t uiCase = 0;
			if (fTopLeft >= fWorldThreshold)
			{
				uiCase |= 8;
			}
			if (fTopRight >= fWorldThreshold)
			{
				uiCase |= 4;
			}
			if (fBottomRight >= fWorldThreshold)
			{
				uiCase |= 2;
			}
			if (fBottomLeft >= fWorldThreshold)
			{
				uiCase |= 1;
			}

			if (uiCase == 0 || uiCase == 15)
			{
				continue;
			}

			float fCellU = static_cast<float>(j) * fScaleU;
			float fCellV = static_cast<float>(i) * fScaleV;
			float fStepU = fScaleU;
			float fStepV = fScaleV;

			// Integer cell-edge identifiers keep shared contour crossings together despite float drift.
			auto Lerp = [](float fA, float fB, float fThresholdValue) -> float
			{
				float fDenominator = fA - fB;
				if (std::abs(fDenominator) < 1.0e-8f)
				{
					return 0.5f;
				}
				return std::clamp((fA - fThresholdValue) / fDenominator, 0.0f, 1.0f);
			};

			uint64_t uiKeyTop = EncodeEdgeKey(0, i, j);
			uint64_t uiKeyRight = EncodeEdgeKey(1, i, j + 1);
			uint64_t uiKeyBottom = EncodeEdgeKey(0, i + 1, j);
			uint64_t uiKeyLeft = EncodeEdgeKey(1, i, j);

			float fTopT = Lerp(fTopLeft, fTopRight, fWorldThreshold);
			XMFLOAT2 f2Top {fCellU + fTopT * fStepU, fCellV};

			float fRightT = Lerp(fTopRight, fBottomRight, fWorldThreshold);
			XMFLOAT2 f2Right {fCellU + fStepU, fCellV + fRightT * fStepV};

			float fBottomT = Lerp(fBottomLeft, fBottomRight, fWorldThreshold);
			XMFLOAT2 f2Bottom {fCellU + fBottomT * fStepU, fCellV + fStepV};

			float fLeftT = Lerp(fTopLeft, fBottomLeft, fWorldThreshold);
			XMFLOAT2 f2Left {fCellU, fCellV + fLeftT * fStepV};

			// Cases 5 and 10 are saddle points: disambiguate by averaging corners
			switch (uiCase)
			{
				case 1:
					rEdges.push_back({.f2A = f2Bottom, .f2B = f2Left, .uiKeyA = uiKeyBottom, .uiKeyB = uiKeyLeft});
					break;
				case 2:
					rEdges.push_back({.f2A = f2Right, .f2B = f2Bottom, .uiKeyA = uiKeyRight, .uiKeyB = uiKeyBottom});
					break;
				case 3:
					rEdges.push_back({.f2A = f2Right, .f2B = f2Left, .uiKeyA = uiKeyRight, .uiKeyB = uiKeyLeft});
					break;
				case 4:
					rEdges.push_back({.f2A = f2Top, .f2B = f2Right, .uiKeyA = uiKeyTop, .uiKeyB = uiKeyRight});
					break;
				case 5:
				{
					float fCenter = (fTopLeft + fTopRight + fBottomRight + fBottomLeft) * 0.25f;
					if (fCenter >= fWorldThreshold)
					{
						rEdges.push_back({.f2A = f2Top, .f2B = f2Left, .uiKeyA = uiKeyTop, .uiKeyB = uiKeyLeft});
						rEdges.push_back({.f2A = f2Bottom, .f2B = f2Right, .uiKeyA = uiKeyBottom, .uiKeyB = uiKeyRight});
					}
					else
					{
						rEdges.push_back({.f2A = f2Top, .f2B = f2Right, .uiKeyA = uiKeyTop, .uiKeyB = uiKeyRight});
						rEdges.push_back({.f2A = f2Bottom, .f2B = f2Left, .uiKeyA = uiKeyBottom, .uiKeyB = uiKeyLeft});
					}
					break;
				}
				case 6:
					rEdges.push_back({.f2A = f2Top, .f2B = f2Bottom, .uiKeyA = uiKeyTop, .uiKeyB = uiKeyBottom});
					break;
				case 7:
					rEdges.push_back({.f2A = f2Top, .f2B = f2Left, .uiKeyA = uiKeyTop, .uiKeyB = uiKeyLeft});
					break;
				case 8:
					rEdges.push_back({.f2A = f2Left, .f2B = f2Top, .uiKeyA = uiKeyLeft, .uiKeyB = uiKeyTop});
					break;
				case 9:
					rEdges.push_back({.f2A = f2Bottom, .f2B = f2Top, .uiKeyA = uiKeyBottom, .uiKeyB = uiKeyTop});
					break;
				case 10:
				{
					float fCenter = (fTopLeft + fTopRight + fBottomRight + fBottomLeft) * 0.25f;
					if (fCenter >= fWorldThreshold)
					{
						rEdges.push_back({.f2A = f2Left, .f2B = f2Bottom, .uiKeyA = uiKeyLeft, .uiKeyB = uiKeyBottom});
						rEdges.push_back({.f2A = f2Right, .f2B = f2Top, .uiKeyA = uiKeyRight, .uiKeyB = uiKeyTop});
					}
					else
					{
						rEdges.push_back({.f2A = f2Left, .f2B = f2Top, .uiKeyA = uiKeyLeft, .uiKeyB = uiKeyTop});
						rEdges.push_back({.f2A = f2Right, .f2B = f2Bottom, .uiKeyA = uiKeyRight, .uiKeyB = uiKeyBottom});
					}
					break;
				}
				case 11:
					rEdges.push_back({.f2A = f2Right, .f2B = f2Top, .uiKeyA = uiKeyRight, .uiKeyB = uiKeyTop});
					break;
				case 12:
					rEdges.push_back({.f2A = f2Left, .f2B = f2Right, .uiKeyA = uiKeyLeft, .uiKeyB = uiKeyRight});
					break;
				case 13:
					rEdges.push_back({.f2A = f2Bottom, .f2B = f2Right, .uiKeyA = uiKeyBottom, .uiKeyB = uiKeyRight});
					break;
				case 14:
					rEdges.push_back({.f2A = f2Left, .f2B = f2Bottom, .uiKeyA = uiKeyLeft, .uiKeyB = uiKeyBottom});
					break;
				default:
					break;
			}
		}
	}
}

static void ChainEdgesIntoPolygons(std::vector<std::vector<XMFLOAT2>>& rPolygons, const std::vector<ContourEdge>& rEdges)
{
	// Cell-edge identifiers join shared crossings without position quantization.
	struct EdgeReference
	{
		int64_t iEdgeIndex = 0;
		bool bIsEndpointB = false; // false = matched on f2A / uiKeyA, true = matched on f2B / uiKeyB
	};
	std::unordered_multimap<uint64_t, EdgeReference> vertexToEdge;
	vertexToEdge.reserve(rEdges.size() * 2);
	for (int64_t i = 0; i < std::ssize(rEdges); ++i)
	{
		vertexToEdge.insert({rEdges.at(i).uiKeyA, {.iEdgeIndex = i, .bIsEndpointB = false}});
		vertexToEdge.insert({rEdges.at(i).uiKeyB, {.iEdgeIndex = i, .bIsEndpointB = true}});

		// Each cell-edge key has at most two entries, one per side; boundary edges have one. This makes
		// the unused chain candidate unique regardless of bucket order. Duplicate ExtractContourEdges
		// entries make NavData depend on hash-bucket order and diverge across builds.
		ASSERT(vertexToEdge.count(rEdges.at(i).uiKeyA) <= 2);
		ASSERT(vertexToEdge.count(rEdges.at(i).uiKeyB) <= 2);
	}

	std::vector<bool> used(rEdges.size(), false);

	for (int64_t i = 0; i < std::ssize(rEdges); ++i)
	{
		if (used.at(i))
		{
			continue;
		}

		std::vector<XMFLOAT2> polygon = { rEdges.at(i).f2A, rEdges.at(i).f2B };
		used.at(i) = true;
		uint64_t uiHeadKey = rEdges.at(i).uiKeyA;
		uint64_t uiTailKey = rEdges.at(i).uiKeyB;

		bool bGrowing = true;
		while (bGrowing)
		{
			bGrowing = false;
			auto [itBegin, itEnd] = vertexToEdge.equal_range(uiTailKey);
			for (auto it = itBegin; it != itEnd; ++it)
			{
				int64_t j = it->second.iEdgeIndex;
				if (used.at(j))
				{
					continue;
				}

				if (it->second.bIsEndpointB)
				{
					polygon.push_back(rEdges.at(j).f2A);
					uiTailKey = rEdges.at(j).uiKeyA;
				}
				else
				{
					polygon.push_back(rEdges.at(j).f2B);
					uiTailKey = rEdges.at(j).uiKeyB;
				}
				used.at(j) = true;
				bGrowing = true;
				break;
			}
		}

		// Closed iff the chain returned to the head's cell-edge. Integer-exact compare — no
		// epsilon needed, since cell-edge keys are derived from cell indices, not float positions.
		if (std::ssize(polygon) >= 3 && uiTailKey == uiHeadKey)
		{
			polygon.pop_back();
			rPolygons.push_back(std::move(polygon));
		}
	}
}

// Shared segment intersection test for NavCellData's crossing diagnostic and NavQuery's edge test; it
// accepts proper interior crossings only, not endpoint touching.
bool SegmentsIntersect(XMFLOAT2 f2A1, XMFLOAT2 f2A2, XMFLOAT2 f2B1, XMFLOAT2 f2B2)
{
	float fDeltaAX = f2A2.x - f2A1.x;
	float fDeltaAY = f2A2.y - f2A1.y;
	float fDeltaBX = f2B2.x - f2B1.x;
	float fDeltaBY = f2B2.y - f2B1.y;

	float fDenominator = fDeltaAX * fDeltaBY - fDeltaAY * fDeltaBX;
	if (std::abs(fDenominator) < 1.0e-10f)
	{
		return false;
	}

	float fDifferenceX = f2B1.x - f2A1.x;
	float fDifferenceY = f2B1.y - f2A1.y;

	float fT = (fDifferenceX * fDeltaBY - fDifferenceY * fDeltaBX) / fDenominator;
	float fU = (fDifferenceX * fDeltaAY - fDifferenceY * fDeltaAX) / fDenominator;

	static constexpr float kfSegmentEpsilon = 1.0e-6f;
	return fT > kfSegmentEpsilon && fT < (1.0f - kfSegmentEpsilon) && fU > kfSegmentEpsilon && fU < (1.0f - kfSegmentEpsilon);
}

// The builder and NavQuery's PointInAnyPolygon share this winding-number core so boundary rules cannot
// drift.
bool PointInPolygon(XMFLOAT2 f2Point, std::span<const XMFLOAT2> vertices)
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

void BuildNavContour(NavContour& rContour, std::span<const float> heightmapData, int32_t iHeightmapWidth, int32_t iHeightmapHeight, float fWorldThreshold, float fClearanceMeters, float fFootprintXMeters, float fFootprintYMeters)
{
	LOG(kNavData, kDebug, "NavBuild: heightmap {}x{} worldThreshold={}", iHeightmapWidth, iHeightmapHeight, common::Wb(fWorldThreshold, 4));

	std::vector<ContourEdge> contourEdges;
	contourEdges.reserve(static_cast<size_t>(iHeightmapWidth) * static_cast<size_t>(iHeightmapHeight));
	ExtractContourEdges(contourEdges, heightmapData, iHeightmapWidth, iHeightmapHeight, fWorldThreshold);

	LOG(kNavData, kDebug, "NavBuild: extracted {} contour edges", std::ssize(contourEdges));

	if (contourEdges.empty())
	{
		return;
	}

	std::vector<std::vector<XMFLOAT2>> polygons;
	ChainEdgesIntoPolygons(polygons, contourEdges);

	LOG(kNavData, kDebug, "NavBuild: chained into {} polygons", std::ssize(polygons));

	// Union raw chained polygons in UV, then transform to centered local meters for isotropic inflation
	// and simplification before mapping back to UV. Clipper2 uses integer-coordinate robustness and
	// squares miter joins that exceed the limit; its finishing union removes offset self-intersections.
	static constexpr double kfMiterLimit = 2.0;
	static constexpr double kfSimplifyEpsilonMeters = 1.00;
	// Clipper2 PathsD quantizes doubles to int64 at 10^precision per unit. Keep precision 6 for both
	// the UV union and metric offset so marching-squares detail and meter-scale clearance remain stable.
	static constexpr int kiClipperPrecision = 6;

	Clipper2Lib::PathsD obstacles;
	obstacles.reserve(polygons.size());
	for (const std::vector<XMFLOAT2>& rPolygon : polygons)
	{
		if (std::ssize(rPolygon) < 3)
		{
			continue;
		}
		Clipper2Lib::PathD path;
		path.reserve(rPolygon.size());
		for (const XMFLOAT2& rVertex : rPolygon)
		{
			path.emplace_back(rVertex.x, rVertex.y);
		}
		obstacles.push_back(std::move(path));
	}

	// Union resolves overlaps; positive winding = outer obstacle, negative = enclosed hole.
	Clipper2Lib::PathsD unioned = Clipper2Lib::Union(obstacles, Clipper2Lib::FillRule::NonZero, kiClipperPrecision);
	for (Clipper2Lib::PathD& rPath : unioned)
	{
		for (Clipper2Lib::PointD& rPoint : rPath)
		{
			rPoint.x = (rPoint.x - 0.5) * static_cast<double>(fFootprintXMeters);
			rPoint.y = (rPoint.y - 0.5) * static_cast<double>(fFootprintYMeters);
		}
	}

	// Clearance is added to the terrain-push contour; distances and tolerances are in meters.
	Clipper2Lib::PathsD inflated = Clipper2Lib::InflatePaths(unioned, static_cast<double>(fClearanceMeters), Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon, kfMiterLimit, kiClipperPrecision);
	// Topology-preserving simplification (does not introduce crossings).
	Clipper2Lib::PathsD simplified = Clipper2Lib::SimplifyPaths(inflated, kfSimplifyEpsilonMeters);

	// Holes lie inside obstacles, which the visibility graph cannot route through.
	int64_t iDroppedHoles = 0;
	for (const Clipper2Lib::PathD& rPath : simplified)
	{
		if (std::ssize(rPath) < 3)
		{
			continue;
		}
		if (Clipper2Lib::Area(rPath) <= 0.0)
		{
			++iDroppedHoles;
			continue;
		}
		rContour.polygonOffsets.push_back(static_cast<int32_t>(rContour.vertices.size()));
		for (const Clipper2Lib::PointD& rPoint : rPath)
		{
			float fU = static_cast<float>(rPoint.x / static_cast<double>(fFootprintXMeters) + 0.5);
			float fV = static_cast<float>(rPoint.y / static_cast<double>(fFootprintYMeters) + 0.5);
			rContour.vertices.push_back(
			{
				fU,
				fV,
			});
		}
	}

	if (iDroppedHoles > 0)
	{
		LOG(kNavData, kDebug, "NavBuild: dropped {} interior holes", iDroppedHoles);
	}

	LOG(kNavData, kDebug, "NavBuild: total vertices={} polygons={}", std::ssize(rContour.vertices), std::ssize(rContour.polygonOffsets));

	if (rContour.vertices.empty())
	{
		return;
	}

	// Clipper2's Area > 0 filter above kept only outer obstacle loops; this verifies the float-cast
	// vertices still wind the same way, so a near-degenerate truncation can't silently flip one into a
	// hole. Not a PointInPolygon precondition — that test counts nonzero winding and is orientation-
	// agnostic. The invariant is UV-space only: BuildCellNavigationData mirrors Y when it places a template, so
	// the merged world-space polygons are wound clockwise.
	int64_t iPolygonCount = std::ssize(rContour.polygonOffsets);
	int32_t iVertexTotal = static_cast<int32_t>(rContour.vertices.size());
	for (int64_t i = 0; i < iPolygonCount; ++i)
	{
		auto [iStart, iEnd] = PolygonRange(rContour.polygonOffsets, i, iVertexTotal);
		int32_t iCount = iEnd - iStart;
		if (iCount < 3)
		{
			continue;
		}
		ASSERT(common::IsPolygonCcw(std::span<const XMFLOAT2>(&rContour.vertices.at(iStart), static_cast<size_t>(iCount))));
	}
}

} // namespace engine
