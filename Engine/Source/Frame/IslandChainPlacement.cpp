#include "IslandChainPlacement.h"

#include "Frame/FrameUtils.h"
#include "Frame/IslandTerrain.h"

namespace engine
{

constexpr uint64_t kuiCrcPickSeedMultiplier = 0xD1B5'4A32'D192'ED03ui64;
constexpr uint64_t kuiPositionSeedMultiplier = 0xCBF2'9CE4'8422'2325ui64;
constexpr uint64_t kuiRotationSeedMultiplier = 0x9E37'79B9'7F4A'7C15ui64;
constexpr uint64_t kuiAnchorSeedMultiplier = 0xFF51'AFD7'ED55'8CCDui64;
constexpr uint64_t kuiCurveSeedMultiplier = 0xC4CE'B9FE'1A85'EC53ui64;
constexpr uint64_t kuiSizeSeedMultiplier = 0xA076'1D64'78BD'642Fui64;
constexpr uint64_t kuiFillerSeedMultiplier = 0xE703'7ED1'A0B4'28DBui64;

// A region's cell at anchor-relative axial (q, r) has both in [-kiRegionAnchorOffset, kiRegionSideCells - kiRegionAnchorOffset),
// which keeps cell (0,0) inside region (0,0).
constexpr int64_t kiRegionAnchorOffset = kiRegionSideCells / 2;
constexpr int64_t kiRegionCellCount = kiRegionSideCells * kiRegionSideCells;

// Region confinement, in axial terms about the region center in the anchor cell's frame. The row spacing is the distance
// between neighbouring lines of constant q or r.
constexpr float kfRegionPaddingMeters = 60.0f;
constexpr XMFLOAT2 kf2RegionCenter {-0.5f * (kCellDirections.f2BasisQ.x + kCellDirections.f2BasisR.x), -0.5f * (kCellDirections.f2BasisQ.y + kCellDirections.f2BasisR.y)};
constexpr float kfRegionRowSpacing = kCellDirections.f2BasisQ.x;
constexpr float kfRegionAxialLimit = 0.5f * static_cast<float>(kiRegionSideCells) - kfRegionPaddingMeters / kfRegionRowSpacing;

// Route walks: the share of a region's cells its walks may claim, and the weight of each turn away from the heading in
// hexagon-direction steps.
constexpr float kfMinimumRouteBudget = 0.55f;
constexpr float kfRouteBudgetRange = 0.45f;
constexpr int64_t kiMinimumWalkCells = 3;
constexpr float kpfTurnWeights[] = {3.0f, 2.0f, 0.4f, 0.1f};
constexpr float kfMinimumWander = 0.15f;
constexpr float kfWanderRange = 0.35f;
constexpr int64_t kiCornerCuttingPasses = 2;

constexpr int64_t kiCurveSamplesPerSpan = 24;
constexpr float kfRouteStepMeters = 10.0f;
constexpr float kfTangentReachMeters = 15.0f;

// Footprints and hulls carry common::kfIslandScale, so the distances measured against them carry it too.
constexpr float kfAnchorJitterMeters = 25.0f * common::kfIslandScale;
constexpr float kfSurroundSpacingMeters = 110.0f * common::kfIslandScale;

constexpr float kfTouchGapMeters = 5.0f;
constexpr float kfGroupGapMeters = 60.0f;
constexpr float kfTouchDirectionJitter = 1.5f;
constexpr int64_t kiTouchAttempts = 12;

constexpr int64_t kiFreeIslandSteps = 30;
constexpr float kfFreeIslandStepMeters = 25.0f;

constexpr float kfMinimumWaterMeters = 130.0f;
constexpr float kfMinimumWaterRangeMeters = 120.0f;
constexpr float kfWaterSpanMeters = 80.0f;
constexpr float kfWaterSpanRangeMeters = 200.0f;
constexpr float kfMinimumSurroundDensity = 0.3f;
constexpr float kfSurroundDensityRange = 0.5f;
constexpr float kfMinimumHugeChance = 0.15f;
constexpr float kfHugeChanceRange = 0.45f;
constexpr int64_t kiMinimumGroupMembers = 2;
constexpr int64_t kiGroupMemberRange = 2;
constexpr float kfMemberLargeChance = 0.65f;
constexpr float kfMemberHugeChance = 0.5f;
constexpr float kfProjectWindowMeters = 700.0f;

constexpr int64_t kiMinimumSurroundSlots = 6;
constexpr int64_t kiMaximumSurroundSlots = 16;
constexpr float kfSurroundDirectionJitter = 0.20f;

// The inset exceeds kfCellLandMarginMeters, so filler never reaches a neighbour's list and that list stays pure.
constexpr float kfFillerInsetMeters = 30.0f;
constexpr float kpfFillerGapsMeters[] = {30.0f, 0.0f};
constexpr int64_t kiFillerAttempts = 96;
constexpr float kfFillerMediumChance = 0.55f;
constexpr int64_t kiFillerExtraRange = 2;
static_assert(kfFillerInsetMeters > kfCellLandMarginMeters);

// Filler treats the cell's listed islands as one group and itself as another.
constexpr int64_t kiListedIslandGroup = 0;
constexpr int64_t kiFillerGroup = 1;

// A long island (footprint aspect >= this) aligns its long axis to the route tangent with small jitter; squarer islands
// get a full random rotation. The 2x1 / 3x1 Large strips align to the curve.
constexpr float kfLongAspectThreshold = 1.5f;
constexpr float kfTangentRotationJitter = 0.15f;

// Reserve size for the per-candidate world-vertex scratch.
constexpr int64_t kiHullScratchReserve = 256;

// The cell's hexagon grown by kfCellLandMarginMeters, CCW about its center.
constexpr XMFLOAT2 kf2MarginHalfExtents = common::MakeHexDirections(kfCellApothem + kfCellLandMarginMeters).f2HalfExtents;
constexpr XMFLOAT2 kpf2MarginHexagon[] =
{
	{kf2MarginHalfExtents.x, 0.0f},
	{0.5f * kf2MarginHalfExtents.x, kf2MarginHalfExtents.y},
	{-0.5f * kf2MarginHalfExtents.x, kf2MarginHalfExtents.y},
	{-kf2MarginHalfExtents.x, 0.0f},
	{-0.5f * kf2MarginHalfExtents.x, -kf2MarginHalfExtents.y},
	{0.5f * kf2MarginHalfExtents.x, -kf2MarginHalfExtents.y},
};

enum class Role
{
	kHuge,
	kLarge,
	kMedium,
	kSmall,
};

// Generation data of one accepted island, index-parallel with IslandHulls::pOut.
struct PlacedIsland
{
	float fRadius = 0.0f;
	float fTangent = 0.0f;
	int64_t iGroup = 0;
};

// Accepted islands in one frame: storage holds the rotated world vertices and views are the ConvexHull2D the SAT test
// reads. Growing storage moves each inner vector without moving its heap block, so the views' vertex pointers stay
// valid. scratch is reused per candidate.
struct IslandHulls
{
	std::vector<IslandPlacement>* pOut = nullptr;

	std::vector<std::vector<XMFLOAT2>> placedHullStorage;
	std::vector<common::ConvexHull2D> placedHullViews;
	std::vector<PlacedIsland> placedIslands;
	std::vector<XMFLOAT2> scratch;
};

// A posed island awaiting acceptance; its hull reads IslandHulls::scratch until the next pose.
struct IslandCandidate
{
	common::crc_t crc = 0;
	XMFLOAT2 f2Position {};
	float fRotation = 0.0f;
	float fRadius = 0.0f;
	common::ConvexHull2D hull {};
};

// Per-region generation state. Every position is in the anchor cell's frame, so the anchor cell's center is the origin
// and no coordinate-derived term appears in the geometry.
struct RegionContext
{
	common::RandomEngine crcRandom;
	common::RandomEngine positionRandom;
	common::RandomEngine rotationRandom;
	common::RandomEngine sizeRandom;

	IslandHulls hulls;
	int64_t iNextGroup = 0;
};

// Signed jitter in [-fRadius, +fRadius).
static float SignedJitter(float fRadius, common::RandomEngine& rRandom)
{
	return common::Random(2.0f * fRadius, rRandom) - fRadius;
}

// The CRC bucket for a role. Each role uses its own bucket, which the IslandTerrain constructor asserts
// is non-empty.
static const std::vector<common::crc_t>& PickBucket(Role eRole)
{
	const IslandTerrain& rTerrain = *gpIslandTerrain;
	switch (eRole)
	{
		case Role::kHuge:
			return rTerrain.mHugeCrcs;
		case Role::kLarge:
			return rTerrain.mLargeCrcs;
		case Role::kMedium:
			return rTerrain.mMediumCrcs;
		case Role::kSmall:
			return rTerrain.mSmallCrcs;
	}
	ASSERT(false);
	std::unreachable();
}

// Random template CRC for a role. common::Random(N) is inclusive on N — pass size - 1.
static common::crc_t PickCrc(Role eRole, common::RandomEngine& rCrcRandom)
{
	const std::vector<common::crc_t>& rBucket = PickBucket(eRole);
	return rBucket.at(common::Random(std::ssize(rBucket) - 1, rCrcRandom));
}

// The hull's long axis lies along the footprint's longer side at rotation 0, under the rotation BuildWorldHull applies,
// so a long island's rotation is the tangent for an X-long footprint and a quarter turn more for a Y-long one.
static float OrientForTangent(common::crc_t crc, float fTangent, common::RandomEngine& rRotationRandom)
{
	const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(crc);
	float fX = rTemplate.fQuadFootprintX;
	float fY = rTemplate.fQuadFootprintY;
	float fLong = std::max(fX, fY);
	float fShort = std::min(fX, fY);
	if (fLong / fShort < kfLongAspectThreshold)
	{
		return common::Random(2.0f * DirectX::XM_PI, rRotationRandom);
	}

	float fBase = (fX >= fY) ? fTangent : (fTangent + 0.5f * DirectX::XM_PI);
	float fFlip = common::RandomUnitFloat(rRotationRandom) < 0.5f ? 0.0f : DirectX::XM_PI;
	return fBase + fFlip + SignedJitter(kfTangentRotationJitter, rRotationRandom);
}

// atan2 of a nonzero vector from header-inline polynomials, bit-identical under /fp:strict.
static float DeterministicAngle(float fX, float fY)
{
	float fAngle = XMScalarACos(fX / std::sqrt(fX * fX + fY * fY));
	return fY < 0.0f ? -fAngle : fAngle;
}

static XMFLOAT2 CellOffset(int64_t iDeltaQ, int64_t iDeltaR)
{
	XMFLOAT2A f2Offset {};
	XMStoreFloat2A(&f2Offset, common::HexCenterOffset(iDeltaQ, iDeltaR, kCellDirections));
	return f2Offset;
}

static int64_t RegionCellIndex(GridCoord cell)
{
	return (cell.iQ + kiRegionAnchorOffset) * kiRegionSideCells + cell.iR + kiRegionAnchorOffset;
}

static bool IsRegionCell(GridCoord cell)
{
	int64_t iFirst = -kiRegionAnchorOffset;
	int64_t iEnd = kiRegionSideCells - kiRegionAnchorOffset;
	return cell.iQ >= iFirst && cell.iQ < iEnd && cell.iR >= iFirst && cell.iR < iEnd;
}

static float RouteLength(const std::vector<XMFLOAT2>& rRoute)
{
	return static_cast<float>(std::ssize(rRoute) - 1) * kfRouteStepMeters;
}

static XMFLOAT2 RoutePoint(const std::vector<XMFLOAT2>& rRoute, float fDistance)
{
	int64_t iLastPoint = std::ssize(rRoute) - 1;
	float fIndex = fDistance / kfRouteStepMeters;
	int64_t iPoint = std::min(static_cast<int64_t>(fIndex), iLastPoint - 1);
	float fBlend = fIndex - static_cast<float>(iPoint);
	const XMFLOAT2& rStart = rRoute.at(iPoint);
	const XMFLOAT2& rEnd = rRoute.at(iPoint + 1);
	return {rStart.x + (rEnd.x - rStart.x) * fBlend, rStart.y + (rEnd.y - rStart.y) * fBlend};
}

static float RouteTangent(const std::vector<XMFLOAT2>& rRoute, float fDistance)
{
	float fLength = RouteLength(rRoute);
	float fStart = std::max(fDistance - kfTangentReachMeters, 0.0f);
	float fEnd = std::min(fDistance + kfTangentReachMeters, fLength);
	XMFLOAT2 f2Start = RoutePoint(rRoute, fStart);
	XMFLOAT2 f2End = RoutePoint(rRoute, fEnd);
	return DeterministicAngle(f2End.x - f2Start.x, f2End.y - f2Start.y);
}

// The distance along [fFrom, fTo] of the route point nearest f2Point.
static float ProjectOntoRoute(const std::vector<XMFLOAT2>& rRoute, XMFLOAT2 f2Point, float fFrom, float fTo)
{
	int64_t iLastPoint = std::ssize(rRoute) - 1;
	int64_t iFirst = static_cast<int64_t>(fFrom / kfRouteStepMeters);
	int64_t iLast = std::min(static_cast<int64_t>(fTo / kfRouteStepMeters), iLastPoint);
	float fBestSquared = std::numeric_limits<float>::max();
	int64_t iBest = iFirst;
	for (int64_t i = iFirst; i <= iLast; ++i)
	{
		float fDeltaX = rRoute.at(i).x - f2Point.x;
		float fDeltaY = rRoute.at(i).y - f2Point.y;
		float fSquared = fDeltaX * fDeltaX + fDeltaY * fDeltaY;
		if (fSquared < fBestSquared)
		{
			fBestSquared = fSquared;
			iBest = i;
		}
	}
	return static_cast<float>(iBest) * kfRouteStepMeters;
}

static float CatmullRom(float fPoint0, float fPoint1, float fPoint2, float fPoint3, float fBlend)
{
	float fBlendSquared = fBlend * fBlend;
	float fBlendCubed = fBlendSquared * fBlend;
	return 0.5f * (2.0f * fPoint1 + (fPoint2 - fPoint0) * fBlend + (2.0f * fPoint0 - 5.0f * fPoint1 + 4.0f * fPoint2 - fPoint3) * fBlendSquared + (3.0f * fPoint1 - fPoint0 - 3.0f * fPoint2 + fPoint3) * fBlendCubed);
}

// A Catmull-Rom curve through the waypoints, resampled every kfRouteStepMeters of its length.
static std::vector<XMFLOAT2> SplineRoute(const std::vector<XMFLOAT2>& rWaypoints)
{
	int64_t iWaypointCount = std::ssize(rWaypoints);
	std::vector<XMFLOAT2> samples;
	samples.reserve(static_cast<size_t>((iWaypointCount - 1) * kiCurveSamplesPerSpan + 1));
	for (int64_t i = 0; i + 1 < iWaypointCount; ++i)
	{
		const XMFLOAT2& rPoint0 = rWaypoints.at(std::max(i - 1, 0i64));
		const XMFLOAT2& rPoint1 = rWaypoints.at(i);
		const XMFLOAT2& rPoint2 = rWaypoints.at(i + 1);
		const XMFLOAT2& rPoint3 = rWaypoints.at(std::min(i + 2, iWaypointCount - 1));
		for (int64_t k = 0; k < kiCurveSamplesPerSpan; ++k)
		{
			float fBlend = static_cast<float>(k) / static_cast<float>(kiCurveSamplesPerSpan);
			samples.push_back({CatmullRom(rPoint0.x, rPoint1.x, rPoint2.x, rPoint3.x, fBlend), CatmullRom(rPoint0.y, rPoint1.y, rPoint2.y, rPoint3.y, fBlend)});
		}
	}
	samples.push_back(rWaypoints.back());

	std::vector<XMFLOAT2> route;
	route.push_back(samples.front());
	float fTravelled = 0.0f;
	float fNextSample = kfRouteStepMeters;
	for (int64_t i = 1; i < std::ssize(samples); ++i)
	{
		const XMFLOAT2& rStart = samples.at(i - 1);
		const XMFLOAT2& rEnd = samples.at(i);
		float fDeltaX = rEnd.x - rStart.x;
		float fDeltaY = rEnd.y - rStart.y;
		float fLength = std::sqrt(fDeltaX * fDeltaX + fDeltaY * fDeltaY);
		while (fTravelled + fLength >= fNextSample)
		{
			float fBlend = (fNextSample - fTravelled) / fLength;
			route.push_back({rStart.x + fDeltaX * fBlend, rStart.y + fDeltaY * fBlend});
			fNextSample += kfRouteStepMeters;
		}
		fTravelled += fLength;
	}
	return route;
}

// The region's routes, drawn only from the anchor and curve streams. Each walks unvisited region cells; region (0,0)'s
// first route starts at cell (0,0).
static std::vector<std::vector<XMFLOAT2>> BuildRoutes(GridCoord region)
{
	common::RandomEngine anchorRandom(SeedFromGridCoordinate(region, kuiAnchorSeedMultiplier));
	common::RandomEngine curveRandom(SeedFromGridCoordinate(region, kuiCurveSeedMultiplier));
	bool bOriginRegion = region == kOriginCoordinate;
	std::vector<std::vector<XMFLOAT2>> routes;
	std::array<bool, kiRegionCellCount> visited {};
	std::vector<GridCoord> freeCells;
	std::vector<GridCoord> walk;
	std::vector<XMFLOAT2> waypoints;
	std::vector<XMFLOAT2> cutWaypoints;

	int64_t iCellsLeft = static_cast<int64_t>(std::round((kfMinimumRouteBudget + kfRouteBudgetRange * common::RandomUnitFloat(anchorRandom)) * static_cast<float>(kiRegionCellCount)));
	while (iCellsLeft >= kiMinimumWalkCells)
	{
		freeCells.clear();
		freeCells.reserve(static_cast<size_t>(kiRegionCellCount));
		for (int64_t i = -kiRegionAnchorOffset; i < kiRegionSideCells - kiRegionAnchorOffset; ++i)
		{
			for (int64_t j = -kiRegionAnchorOffset; j < kiRegionSideCells - kiRegionAnchorOffset; ++j)
			{
				GridCoord cell {.iQ = static_cast<int32_t>(i), .iR = static_cast<int32_t>(j)};
				if (!visited.at(RegionCellIndex(cell)))
				{
					freeCells.push_back(cell);
				}
			}
		}

		bool bOriginRoute = bOriginRegion && routes.empty();
		GridCoord startCell = bOriginRoute ? kOriginCoordinate : freeCells.at(common::Random(std::ssize(freeCells) - 1, anchorRandom));
		int64_t iWantedCells = kiMinimumWalkCells + common::Random(iCellsLeft - kiMinimumWalkCells, anchorRandom);
		walk.clear();
		walk.push_back(startCell);
		visited.at(RegionCellIndex(startCell)) = true;
		int64_t iHeading = common::Random(common::kiHexDirectionCount - 1, anchorRandom);
		while (std::ssize(walk) < iWantedCells)
		{
			GridCoord cell = walk.back();
			std::array<float, common::kiHexDirectionCount> weights {};
			float fTotalWeight = 0.0f;
			for (const auto& [k, rDirection] : std::views::enumerate(kCellDirections.pEntries))
			{
				GridCoord next {.iQ = cell.iQ + rDirection.iDeltaQ, .iR = cell.iR + rDirection.iDeltaR};
				if (!IsRegionCell(next))
				{
					continue;
				}
				if (visited.at(RegionCellIndex(next)))
				{
					continue;
				}
				int64_t iTurn = std::abs(k - iHeading);
				iTurn = std::min(iTurn, common::kiHexDirectionCount - iTurn);
				weights.at(k) = kpfTurnWeights[iTurn];
				fTotalWeight += weights.at(k);
			}
			if (fTotalWeight == 0.0f)
			{
				break;
			}

			float fPick = common::RandomUnitFloat(anchorRandom) * fTotalWeight;
			for (const auto& [k, rfWeight] : std::views::enumerate(std::as_const(weights)))
			{
				if (rfWeight == 0.0f)
				{
					continue;
				}
				iHeading = k;
				fPick -= rfWeight;
				if (fPick < 0.0f)
				{
					break;
				}
			}

			const common::HexDirection& rStep = kCellDirections.pEntries[iHeading];
			GridCoord next {.iQ = cell.iQ + rStep.iDeltaQ, .iR = cell.iR + rStep.iDeltaR};
			visited.at(RegionCellIndex(next)) = true;
			walk.push_back(next);
		}
		iCellsLeft -= std::ssize(walk);
		if (std::ssize(walk) < 2)
		{
			continue;
		}

		float fWander = (kfMinimumWander + kfWanderRange * common::RandomUnitFloat(curveRandom)) * kfCellApothem;
		waypoints.clear();
		waypoints.reserve(static_cast<size_t>(std::ssize(walk)));
		for (const auto& [i, rCell] : std::views::enumerate(std::as_const(walk)))
		{
			float fJitter = bOriginRoute && i == 0 ? kfAnchorJitterMeters : fWander;
			XMFLOAT2 f2Center = CellOffset(rCell.iQ, rCell.iR);
			float fX = f2Center.x + SignedJitter(fJitter, curveRandom);
			float fY = f2Center.y + SignedJitter(fJitter, curveRandom);
			waypoints.push_back({fX, fY});
		}

		// Corner cutting, so a sharp walk turn reads as a bend. The ends stay put.
		for (int64_t i = 0; i < kiCornerCuttingPasses; ++i)
		{
			cutWaypoints.clear();
			cutWaypoints.reserve(static_cast<size_t>(2 * std::ssize(waypoints)));
			cutWaypoints.push_back(waypoints.front());
			for (int64_t k = 0; k + 1 < std::ssize(waypoints); ++k)
			{
				const XMFLOAT2& rStart = waypoints.at(k);
				const XMFLOAT2& rEnd = waypoints.at(k + 1);
				cutWaypoints.insert(cutWaypoints.end(), {{0.75f * rStart.x + 0.25f * rEnd.x, 0.75f * rStart.y + 0.25f * rEnd.y}, {0.25f * rStart.x + 0.75f * rEnd.x, 0.25f * rStart.y + 0.75f * rEnd.y}});
			}
			cutWaypoints.push_back(waypoints.back());
			waypoints.swap(cutWaypoints);
		}
		routes.push_back(SplineRoute(waypoints));
	}
	return routes;
}

static float HullRadius(const IslandTemplate& rTemplate)
{
	float fRadiusSquared = 0.0f;
	for (const XMFLOAT2& rVertex : std::span(rTemplate.pf2ValidAreaVertices, static_cast<size_t>(rTemplate.iValidAreaVertexCount)))
	{
		fRadiusSquared = std::max(fRadiusSquared, rVertex.x * rVertex.x + rVertex.y * rVertex.y);
	}
	return std::sqrt(fRadiusSquared);
}

static common::ConvexHull2D PoseHull(const IslandTemplate& rTemplate, XMFLOAT2 f2Position, float fRotation, std::vector<XMFLOAT2>& rScratch)
{
	common::SinCos rotation = common::DeterministicSinCos(fRotation);
	rScratch.resize(static_cast<size_t>(rTemplate.iValidAreaVertexCount));
	return common::BuildWorldHull(std::span<const XMFLOAT2>(rTemplate.pf2ValidAreaVertices, static_cast<size_t>(rTemplate.iValidAreaVertexCount)), f2Position, rotation.fCos, rotation.fSin, std::span<XMFLOAT2>(rScratch));
}

static IslandCandidate PoseCandidate(IslandHulls& rHulls, common::crc_t crc, XMFLOAT2 f2Position, float fRotation)
{
	const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(crc);
	return {.crc = crc, .f2Position = f2Position, .fRotation = fRotation, .fRadius = HullRadius(rTemplate), .hull = PoseHull(rTemplate, f2Position, fRotation, rHulls.scratch)};
}

// Stores the candidate's hull, which reads scratch, with no list entry.
static void StoreHull(IslandHulls& rHulls, const IslandCandidate& rCandidate, int64_t iGroup, float fTangent)
{
	std::vector<XMFLOAT2>& rPlacedHull = rHulls.placedHullStorage.emplace_back(rHulls.scratch.begin(), rHulls.scratch.end());
	common::ConvexHull2D view = rCandidate.hull;
	view.pVertices = rPlacedHull.data();
	rHulls.placedHullViews.push_back(view);
	rHulls.placedIslands.push_back({.fRadius = rCandidate.fRadius, .fTangent = fTangent, .iGroup = iGroup});
}

static int64_t CommitPlacement(IslandHulls& rHulls, const IslandCandidate& rCandidate, int64_t iGroup, float fTangent)
{
	StoreHull(rHulls, rCandidate, iGroup, fTangent);
	rHulls.pOut->push_back({.islandCrc = rCandidate.crc, .f2WorldPosition = rCandidate.f2Position, .fRotation = rCandidate.fRotation});
	return std::ssize(rHulls.placedHullViews) - 1;
}

// Distance from a point to a CCW convex hull; 0 inside.
static float DistanceToHull(XMFLOAT2 f2Point, const common::ConvexHull2D& rHull)
{
	float fBestSquared = std::numeric_limits<float>::max();
	bool bInside = true;
	for (int64_t i = 0; i < rHull.iVertexCount; ++i)
	{
		const XMFLOAT2& rStart = rHull.pVertices[i];
		const XMFLOAT2& rEnd = rHull.pVertices[(i + 1) % rHull.iVertexCount];
		float fEdgeX = rEnd.x - rStart.x;
		float fEdgeY = rEnd.y - rStart.y;
		float fToPointX = f2Point.x - rStart.x;
		float fToPointY = f2Point.y - rStart.y;
		if (fEdgeX * fToPointY - fEdgeY * fToPointX < 0.0f)
		{
			bInside = false;
		}
		float fBlend = std::clamp((fToPointX * fEdgeX + fToPointY * fEdgeY) / (fEdgeX * fEdgeX + fEdgeY * fEdgeY), 0.0f, 1.0f);
		float fDeltaX = fToPointX - fBlend * fEdgeX;
		float fDeltaY = fToPointY - fBlend * fEdgeY;
		fBestSquared = std::min(fBestSquared, fDeltaX * fDeltaX + fDeltaY * fDeltaY);
	}
	return bInside ? 0.0f : std::sqrt(fBestSquared);
}

// Distance from rFirst's vertices to rSecond; the measure is one-way.
static float GapBetween(const common::ConvexHull2D& rFirst, const common::ConvexHull2D& rSecond)
{
	float fBest = std::numeric_limits<float>::max();
	for (int64_t i = 0; i < rFirst.iVertexCount; ++i)
	{
		fBest = std::min(fBest, DistanceToHull(rFirst.pVertices[i], rSecond));
	}
	return fBest;
}

// A candidate may not overlap an accepted hull, and stays fGap from one of another group.
static bool Collides(const IslandHulls& rHulls, const IslandCandidate& rCandidate, int64_t iGroup, float fGap)
{
	for (const auto& [i, rPlaced] : std::views::enumerate(rHulls.placedIslands))
	{
		const common::ConvexHull2D& rHull = rHulls.placedHullViews.at(i);
		const XMFLOAT2& rPosition = rHulls.pOut->at(i).f2WorldPosition;
		bool bOtherGroup = rPlaced.iGroup != iGroup;
		float fDeltaX = rCandidate.f2Position.x - rPosition.x;
		float fDeltaY = rCandidate.f2Position.y - rPosition.y;
		float fReach = rCandidate.fRadius + rPlaced.fRadius + (bOtherGroup ? fGap : 0.0f);
		if (fDeltaX * fDeltaX + fDeltaY * fDeltaY > fReach * fReach)
		{
			continue;
		}
		if (common::ConvexHullsOverlap(rCandidate.hull, rHull))
		{
			return true;
		}
		if (bOtherGroup && GapBetween(rCandidate.hull, rHull) < fGap)
		{
			return true;
		}
	}
	return false;
}

// Every hull vertex in axial terms about the region center must stay within the padded rhombus. Hulls of two regions
// therefore stay twice the padding apart.
static bool IsInsideRegion(const common::ConvexHull2D& rHull)
{
	return std::ranges::all_of(std::span(rHull.pVertices, static_cast<size_t>(rHull.iVertexCount)), [](const XMFLOAT2& rVertex)
	{
		float fAxialQ = (rVertex.x - kf2RegionCenter.x) / kCellDirections.f2BasisQ.x;
		float fAxialR = (rVertex.y - kf2RegionCenter.y - fAxialQ * kCellDirections.f2BasisQ.y) / kCellDirections.f2BasisR.y;
		return std::abs(fAxialQ) <= kfRegionAxialLimit && std::abs(fAxialR) <= kfRegionAxialLimit;
	});
}

static std::optional<int64_t> TryPlaceInRegion(IslandHulls& rHulls, const IslandCandidate& rCandidate, int64_t iGroup, float fTangent)
{
	if (!IsInsideRegion(rCandidate.hull))
	{
		return std::nullopt;
	}
	if (Collides(rHulls, rCandidate, iGroup, kfGroupGapMeters))
	{
		return std::nullopt;
	}
	return CommitPlacement(rHulls, rCandidate, iGroup, fTangent);
}

// The center position that puts crc, oriented fRotation, just-touching the host island along fDirection: its near edge
// sits kfTouchGapMeters beyond the host's far edge, so the two hulls clear each other along fDirection (separating axis).
static XMFLOAT2 TouchPosition(const IslandHulls& rHulls, common::crc_t crc, int64_t iHost, float fDirection, float fRotation)
{
	const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(crc);

	common::SinCos direction = common::DeterministicSinCos(fDirection);
	float fDirectionX = direction.fCos;
	float fDirectionY = direction.fSin;

	// Candidate's near-edge projection onto fDirection, relative to its own center (rotation applied).
	common::SinCos rotation = common::DeterministicSinCos(fRotation);
	float fCosine = rotation.fCos;
	float fSine = rotation.fSin;
	float fCandidateMinimum = std::numeric_limits<float>::max();
	for (int64_t i = 0; i < rTemplate.iValidAreaVertexCount; ++i)
	{
		float fRotatedX = rTemplate.pf2ValidAreaVertices[i].x * fCosine - rTemplate.pf2ValidAreaVertices[i].y * fSine;
		float fRotatedY = rTemplate.pf2ValidAreaVertices[i].x * fSine + rTemplate.pf2ValidAreaVertices[i].y * fCosine;
		fCandidateMinimum = std::min(fCandidateMinimum, fRotatedX * fDirectionX + fRotatedY * fDirectionY);
	}

	// Host's far-edge projection onto fDirection.
	const common::ConvexHull2D& rHost = rHulls.placedHullViews.at(iHost);
	float fHostMaximum = std::numeric_limits<float>::lowest();
	for (int64_t i = 0; i < rHost.iVertexCount; ++i)
	{
		fHostMaximum = std::max(fHostMaximum, rHost.pVertices[i].x * fDirectionX + rHost.pVertices[i].y * fDirectionY);
	}

	XMFLOAT2 f2Host = rHulls.pOut->at(iHost).f2WorldPosition;
	float fPlacementDistance = fHostMaximum + kfTouchGapMeters - fCandidateMinimum - (f2Host.x * fDirectionX + f2Host.y * fDirectionY);
	return {f2Host.x + fPlacementDistance * fDirectionX, f2Host.y + fPlacementDistance * fDirectionY};
}

static std::optional<int64_t> TryTouchPlace(IslandHulls& rHulls, common::crc_t crc, int64_t iHost, float fDirection, float fRotation, int64_t iGroup, float fTangent)
{
	XMFLOAT2 f2Position = TouchPosition(rHulls, crc, iHost, fDirection, fRotation);
	return TryPlaceInRegion(rHulls, PoseCandidate(rHulls, crc, f2Position, fRotation), iGroup, fTangent);
}

// A free island on the route at the first distance that fits, shrinking its role class before moving on. rfDistance ends
// at the accepted distance.
static std::optional<int64_t> PlaceFreeIsland(RegionContext& rContext, const std::vector<XMFLOAT2>& rRoute, float& rfDistance, Role eRole, int64_t iGroup)
{
	float fLength = RouteLength(rRoute);
	for (int64_t i = 0; i < kiFreeIslandSteps && rfDistance <= fLength; ++i, rfDistance += kfFreeIslandStepMeters)
	{
		XMFLOAT2 f2Point = RoutePoint(rRoute, rfDistance);
		float fTangent = RouteTangent(rRoute, rfDistance);
		for (int64_t j = std::to_underlying(eRole); j <= std::to_underlying(Role::kSmall); ++j)
		{
			common::crc_t crc = PickCrc(static_cast<Role>(j), rContext.crcRandom);
			float fRotation = OrientForTangent(crc, fTangent, rContext.rotationRandom);
			std::optional<int64_t> optionalPlaced = TryPlaceInRegion(rContext.hulls, PoseCandidate(rContext.hulls, crc, f2Point, fRotation), iGroup, fTangent);
			if (optionalPlaced.has_value())
			{
				return optionalPlaced;
			}
		}
	}
	return std::nullopt;
}

// Rings a group member with Small islands that touch it. Evenly spaced angular slots, their count scaling with the
// perimeter and the group's density; a slot whose pose collides stays empty.
static void SurroundIsland(RegionContext& rContext, int64_t iHost, float fDensity)
{
	IslandHulls& rHulls = rContext.hulls;
	const IslandTemplate& rHost = gpIslandTerrain->mIslands.at(rHulls.pOut->at(iHost).islandCrc);
	PlacedIsland host = rHulls.placedIslands.at(iHost);
	float fPerimeter = 2.0f * (rHost.fQuadFootprintX + rHost.fQuadFootprintY);
	int64_t iRingSlots = std::clamp(static_cast<int64_t>(fPerimeter / kfSurroundSpacingMeters), kiMinimumSurroundSlots, kiMaximumSurroundSlots);
	int64_t iSlots = static_cast<int64_t>(static_cast<float>(iRingSlots) * fDensity);
	for (int64_t j = 0; j < iSlots; ++j)
	{
		float fDirection = (2.0f * DirectX::XM_PI * static_cast<float>(j)) / static_cast<float>(iSlots) + SignedJitter(kfSurroundDirectionJitter, rContext.positionRandom);
		common::crc_t crc = PickCrc(Role::kSmall, rContext.crcRandom);
		float fRotation = OrientForTangent(crc, host.fTangent, rContext.rotationRandom);
		TryTouchPlace(rHulls, crc, iHost, fDirection, fRotation, host.iGroup, host.fTangent);
	}
}

// Tight clusters of big islands around the route, with open water between clusters.
static void LayGroups(RegionContext& rContext, const std::vector<XMFLOAT2>& rRoute, bool bHugeHead)
{
	IslandHulls& rHulls = rContext.hulls;
	common::RandomEngine& rSizeRandom = rContext.sizeRandom;
	float fMinimumWater = kfMinimumWaterMeters + kfMinimumWaterRangeMeters * common::RandomUnitFloat(rSizeRandom);
	float fMaximumWater = fMinimumWater + kfWaterSpanMeters + kfWaterSpanRangeMeters * common::RandomUnitFloat(rSizeRandom);
	float fDensity = kfMinimumSurroundDensity + kfSurroundDensityRange * common::RandomUnitFloat(rSizeRandom);
	float fHugeChance = kfMinimumHugeChance + kfHugeChanceRange * common::RandomUnitFloat(rSizeRandom);
	float fLength = RouteLength(rRoute);
	float fDistance = 0.0f;
	bool bFirstGroup = true;
	std::vector<int64_t> members;
	while (fDistance < fLength)
	{
		int64_t iGroup = rContext.iNextGroup++;
		int64_t iMemberCount = kiMinimumGroupMembers + common::Random(kiGroupMemberRange, rSizeRandom);
		Role eHeadRole = (bFirstGroup && bHugeHead) || common::RandomUnitFloat(rSizeRandom) < fHugeChance ? Role::kHuge : Role::kLarge;
		bFirstGroup = false;
		std::optional<int64_t> optionalHead = PlaceFreeIsland(rContext, rRoute, fDistance, eHeadRole, iGroup);
		if (!optionalHead.has_value())
		{
			break;
		}

		members.clear();
		members.push_back(*optionalHead);
		float fFarthest = fDistance;
		for (int64_t i = 1; i < iMemberCount; ++i)
		{
			Role eRole = Role::kLarge;
			if (common::RandomUnitFloat(rSizeRandom) >= kfMemberLargeChance)
			{
				eRole = common::RandomUnitFloat(rSizeRandom) < kfMemberHugeChance ? Role::kHuge : Role::kMedium;
			}
			common::crc_t crc = PickCrc(eRole, rContext.crcRandom);
			int64_t iHost = members.at(common::Random(std::ssize(members) - 1, rSizeRandom));
			float fTangent = RouteTangent(rRoute, fDistance);
			std::optional<int64_t> optionalPlaced;
			for (int64_t j = 0; j < kiTouchAttempts && !optionalPlaced.has_value(); ++j)
			{
				float fDirection = fTangent + SignedJitter(kfTouchDirectionJitter, rContext.positionRandom);
				float fRotation = OrientForTangent(crc, fTangent, rContext.rotationRandom);
				optionalPlaced = TryTouchPlace(rHulls, crc, iHost, fDirection, fRotation, iGroup, fTangent);
			}
			if (!optionalPlaced.has_value())
			{
				continue;
			}
			members.push_back(*optionalPlaced);
			fFarthest = std::max(fFarthest, ProjectOntoRoute(rRoute, rHulls.pOut->at(*optionalPlaced).f2WorldPosition, fDistance, fDistance + kfProjectWindowMeters));
		}

		float fReach = 0.0f;
		for (int64_t iMember : members)
		{
			SurroundIsland(rContext, iMember, fDensity);
			fReach = std::max(fReach, rHulls.placedIslands.at(iMember).fRadius);
		}
		fDistance = fFarthest + fReach + fMinimumWater + (fMaximumWater - fMinimumWater) * common::RandomUnitFloat(rSizeRandom);
	}
}

static void GrowRegion(GridCoord region, std::vector<IslandPlacement>& rOut)
{
	RegionContext context;
	context.crcRandom = common::RandomEngine(SeedFromGridCoordinate(region, kuiCrcPickSeedMultiplier));
	context.positionRandom = common::RandomEngine(SeedFromGridCoordinate(region, kuiPositionSeedMultiplier));
	context.rotationRandom = common::RandomEngine(SeedFromGridCoordinate(region, kuiRotationSeedMultiplier));
	context.sizeRandom = common::RandomEngine(SeedFromGridCoordinate(region, kuiSizeSeedMultiplier));
	context.hulls.pOut = &rOut;
	context.hulls.scratch.reserve(static_cast<size_t>(kiHullScratchReserve));

	bool bOriginRegion = region == kOriginCoordinate;
	std::vector<std::vector<XMFLOAT2>> routes = BuildRoutes(region);
	for (const auto& [i, rRoute] : std::views::enumerate(std::as_const(routes)))
	{
		LayGroups(context, rRoute, bOriginRegion && i == 0);
	}
}

// The region holding axial coordinate iAxial, through true floor division.
static int64_t RegionIndex(int64_t iAxial)
{
	int64_t iOffset = iAxial + kiRegionAnchorOffset;
	int64_t iQuotient = iOffset / kiRegionSideCells;
	return iOffset % kiRegionSideCells < 0 ? iQuotient - 1 : iQuotient;
}

// Appends each of the region's placements whose hull overlaps the cell's margin hexagon, rebased from the anchor cell's
// frame into the cell's.
static void ListRegionPlacements(int64_t iRegionQ, int64_t iRegionR, GridCoord coordinate, std::vector<XMFLOAT2>& rScratch, std::vector<IslandPlacement>& rOut)
{
	GridCoord region {.iQ = static_cast<int32_t>(iRegionQ), .iR = static_cast<int32_t>(iRegionR)};
	auto [it, bInserted] = gpIslandTerrain->mRegionPlacements.try_emplace(region);
	if (bInserted)
	{
		GrowRegion(region, it->second);
	}

	XMFLOAT2 f2Cell = CellOffset(coordinate.iQ - iRegionQ * kiRegionSideCells, coordinate.iR - iRegionR * kiRegionSideCells);
	XMFLOAT2 pf2Hexagon[std::size(kpf2MarginHexagon)] {};
	common::ConvexHull2D hexagon = common::BuildWorldHull(kpf2MarginHexagon, f2Cell, 1.0f, 0.0f, pf2Hexagon);
	for (const IslandPlacement& rPlacement : it->second)
	{
		const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
		float fDeltaX = rPlacement.f2WorldPosition.x - f2Cell.x;
		float fDeltaY = rPlacement.f2WorldPosition.y - f2Cell.y;
		float fReach = HullRadius(rTemplate) + kf2MarginHalfExtents.x;
		if (fDeltaX * fDeltaX + fDeltaY * fDeltaY > fReach * fReach)
		{
			continue;
		}
		if (common::ConvexHullsOverlap(hexagon, PoseHull(rTemplate, rPlacement.f2WorldPosition, rPlacement.fRotation, rScratch)))
		{
			rOut.push_back({.islandCrc = rPlacement.islandCrc, .f2WorldPosition = {fDeltaX, fDeltaY}, .fRotation = rPlacement.fRotation});
		}
	}
}

static std::optional<int64_t> TryPlaceFiller(IslandHulls& rHulls, const IslandCandidate& rCandidate, float fGap)
{
	// The hull is convex, so it lies inside the convex hexagon exactly when every vertex does.
	if (!std::ranges::all_of(std::span(rCandidate.hull.pVertices, static_cast<size_t>(rCandidate.hull.iVertexCount)), [](const XMFLOAT2& rVertex)
	{
		return common::IsInsideHex(XMLoadFloat2(&rVertex), kCellDirections, kfFillerInsetMeters);
	}))
	{
		return std::nullopt;
	}
	if (Collides(rHulls, rCandidate, kiFillerGroup, fGap))
	{
		return std::nullopt;
	}
	return CommitPlacement(rHulls, rCandidate, kiFillerGroup, 0.0f);
}

// A head island and a few Small extras inside this cell, clear of every island in rOut, the only ones near enough to
// matter: an unlisted hull lies past the margin and filler stays inset from the hexagon. The head may relax its gap,
// and places nothing when no attempt fits.
static void AddFiller(GridCoord coordinate, std::vector<IslandPlacement>& rOut)
{
	common::RandomEngine fillerRandom(SeedFromGridCoordinate(coordinate, kuiFillerSeedMultiplier));
	IslandHulls hulls;
	hulls.pOut = &rOut;
	hulls.scratch.reserve(static_cast<size_t>(kiHullScratchReserve));
	for (const IslandPlacement& rPlacement : rOut)
	{
		StoreHull(hulls, PoseCandidate(hulls, rPlacement.islandCrc, rPlacement.f2WorldPosition, rPlacement.fRotation), kiListedIslandGroup, 0.0f);
	}

	std::optional<int64_t> optionalHead;
	for (float fGap : kpfFillerGapsMeters)
	{
		for (int64_t i = 0; i < kiFillerAttempts && !optionalHead.has_value(); ++i)
		{
			Role eRole = common::RandomUnitFloat(fillerRandom) < kfFillerMediumChance ? Role::kMedium : Role::kSmall;
			common::crc_t crc = PickCrc(eRole, fillerRandom);
			common::SinCos angle = common::DeterministicSinCos(common::Random(2.0f * DirectX::XM_PI, fillerRandom));
			float fDistance = kfCellApothem * std::sqrt(common::RandomUnitFloat(fillerRandom));
			float fRotation = common::Random(2.0f * DirectX::XM_PI, fillerRandom);
			XMFLOAT2 f2Position {fDistance * angle.fCos, fDistance * angle.fSin};
			optionalHead = TryPlaceFiller(hulls, PoseCandidate(hulls, crc, f2Position, fRotation), fGap);
		}
	}
	if (!optionalHead.has_value())
	{
		return;
	}

	int64_t iExtraCount = 1 + common::Random(kiFillerExtraRange, fillerRandom);
	for (int64_t i = 0; i < iExtraCount; ++i)
	{
		common::crc_t crc = PickCrc(Role::kSmall, fillerRandom);
		float fRotation = common::Random(2.0f * DirectX::XM_PI, fillerRandom);
		float fDirection = common::Random(2.0f * DirectX::XM_PI, fillerRandom);
		XMFLOAT2 f2Position = TouchPosition(hulls, crc, *optionalHead, fDirection, fRotation);
		TryPlaceFiller(hulls, PoseCandidate(hulls, crc, f2Position, fRotation), kpfFillerGapsMeters[0]);
	}
}

static bool IsCentred(const IslandPlacement& rPlacement)
{
	return IsInsideCell(XMLoadFloat2(&rPlacement.f2WorldPosition));
}

void GenerateIslandChain(GridCoord coordinate, std::vector<IslandPlacement>& rOut)
{
	rOut.clear();
	rOut.reserve(static_cast<size_t>(kiMaximumIslandsPerCell));

	std::vector<XMFLOAT2> scratch;
	scratch.reserve(static_cast<size_t>(kiHullScratchReserve));
	for (int64_t i = RegionIndex(coordinate.iQ - 1i64); i <= RegionIndex(coordinate.iQ + 1i64); ++i)
	{
		for (int64_t j = RegionIndex(coordinate.iR - 1i64); j <= RegionIndex(coordinate.iR + 1i64); ++j)
		{
			ListRegionPlacements(i, j, coordinate, scratch, rOut);
		}
	}

	if (std::ranges::none_of(rOut, IsCentred))
	{
		AddFiller(coordinate, rOut);
	}

	std::stable_partition(rOut.begin(), rOut.end(), IsCentred);
	ASSERT(std::ssize(rOut) <= kiMaximumIslandsPerCell);
}

std::span<const IslandPlacement> CentredIslands(std::span<const IslandPlacement> islands)
{
	return std::span<const IslandPlacement>(islands.begin(), std::ranges::partition_point(islands, IsCentred));
}

} // namespace engine
