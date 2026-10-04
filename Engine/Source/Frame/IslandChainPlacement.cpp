#include "IslandChainPlacement.h"

#include "Frame/IslandTerrain.h"

namespace engine
{

constexpr uint64_t kuiCrcPickSeedMultiplier = 0xD1B5'4A32'D192'ED03ui64;
constexpr uint64_t kuiPositionSeedMultiplier = 0xCBF2'9CE4'8422'2325ui64;
constexpr uint64_t kuiRotationSeedMultiplier = 0x9E37'79B9'7F4A'7C15ui64;
constexpr uint64_t kuiAnchorSeedMultiplier = 0xFF51'AFD7'ED55'8CCDui64;
constexpr uint64_t kuiCurveSeedMultiplier = 0xC4CE'B9FE'1A85'EC53ui64;

// Anchor: large island centered at 1/3 of the cell from the SW (min) corner.
constexpr float kfAnchorFraction = 1.0f / 3.0f;
constexpr float kfAnchorJitterMeters = 25.0f;
constexpr float kfAnchorRotationJitter = 0.2f;
constexpr float kfCellEdgeMarginMeters = 10.0f;

constexpr float kfTouchGapMeters = 5.0f;
constexpr float kfChainHeadingStartJitter = 0.4f;
constexpr float kfChainHeadingTurn = 0.7f;
constexpr float kfChainHeadingTurnJitter = 0.25f;
constexpr float kfTouchDirectionJitter = 0.7f;
constexpr int64_t kiTouchAttempts = 32;

constexpr float kfSurroundSpacingMeters = 110.0f;
constexpr int64_t kiMinimumSurroundSlots = 6;
constexpr float kfSurroundDirectionJitter = 0.20f;

constexpr float kfTailDirectionJitter = 0.6f;

// A long island (footprint aspect >= this) aligns its long axis to the attach direction with small
// jitter; squarer islands get a full random rotation. The 2x1 / 3x1 Large strips align to the curve.
constexpr float kfLongAspectThreshold = 2.0f;
constexpr float kfTangentRotationJitter = 0.15f;

// Reserve size for the per-candidate world-vertex scratch.
constexpr int64_t kiHullScratchReserve = 256;

enum class Role
{
	kHuge,
	kLarge,
	kMedium,
	kSmall,
};

// Per-cell generation state threaded through the placement helpers. Every position below is cell-local
// meters, so the cell center is the origin and no coordinate-derived term appears in the geometry.
struct CellContext
{
	float fHalfWidth = 0.0f;
	float fHalfHeight = 0.0f;

	common::RandomEngine crcRandom;
	common::RandomEngine positionRandom;
	common::RandomEngine rotationRandom;
	common::RandomEngine anchorRandom;
	common::RandomEngine curveRandom;

	std::vector<IslandPlacement>* pOut = nullptr;

	// Accepted world hulls: storage holds the rotated world verts (pointer-stable — reserved up front,
	// never reallocated), views are the ConvexHull2D the SAT test reads. scratch is reused per candidate.
	std::vector<std::vector<XMFLOAT2>> placedHullStorage;
	std::vector<common::ConvexHull2D> placedHullViews;
	std::vector<XMFLOAT2> scratch;
};

// Signed jitter in [-fRadius, +fRadius).
static float SignedJitter(float fRadius, common::RandomEngine& rRandom)
{
	return common::Random(2.0f * fRadius, rRandom) - fRadius;
}

// The CRC bucket for a role, falling back through related buckets to mIslandCrcsSorted (asserted
// non-empty), so small asset sets still place something.
static const std::vector<common::crc_t>& PickBucket(Role eRole)
{
	const IslandTerrain& rTerrain = *gpIslandTerrain;
	switch (eRole)
	{
		case Role::kHuge:
			if (!rTerrain.mHugeCrcs.empty())
			{
				return rTerrain.mHugeCrcs;
			}
			if (!rTerrain.mLargeCrcs.empty())
			{
				return rTerrain.mLargeCrcs;
			}
			if (!rTerrain.mMediumCrcs.empty())
			{
				return rTerrain.mMediumCrcs;
			}
			return rTerrain.mIslandCrcsSorted;
		case Role::kLarge:
			if (!rTerrain.mLargeCrcs.empty())
			{
				return rTerrain.mLargeCrcs;
			}
			if (!rTerrain.mMediumCrcs.empty())
			{
				return rTerrain.mMediumCrcs;
			}
			if (!rTerrain.mHugeCrcs.empty())
			{
				return rTerrain.mHugeCrcs;
			}
			return rTerrain.mIslandCrcsSorted;
		case Role::kMedium:
			if (!rTerrain.mMediumCrcs.empty())
			{
				return rTerrain.mMediumCrcs;
			}
			if (!rTerrain.mLargeCrcs.empty())
			{
				return rTerrain.mLargeCrcs;
			}
			if (!rTerrain.mSmallCrcs.empty())
			{
				return rTerrain.mSmallCrcs;
			}
			return rTerrain.mIslandCrcsSorted;
		case Role::kSmall:
			if (!rTerrain.mSmallCrcs.empty())
			{
				return rTerrain.mSmallCrcs;
			}
			if (!rTerrain.mMediumCrcs.empty())
			{
				return rTerrain.mMediumCrcs;
			}
			return rTerrain.mIslandCrcsSorted;
	}
	return rTerrain.mIslandCrcsSorted;
}

// Random template CRC for a role. common::Random(N) is inclusive on N — pass size - 1.
static common::crc_t PickCrc(Role eRole, common::RandomEngine& rCrcRandom)
{
	const std::vector<common::crc_t>& rBucket = PickBucket(eRole);
	return rBucket.at(common::Random(static_cast<uint32_t>(rBucket.size() - 1ui32), rCrcRandom));
}

// Long islands follow the attachment direction with jitter; squarer islands rotate freely.
// The rendered long edge is offset 90 degrees from the major footprint axis, requiring a quarter-turn.
static float OrientForTangent(common::crc_t crc, float fAttachWorld, common::RandomEngine& rRotationRandom)
{
	const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(crc);
	float fX = rTemplate.fQuadFootprintX;
	float fY = rTemplate.fQuadFootprintY;
	float fLong = std::max(fX, fY);
	float fShort = std::min(fX, fY);
	if (fShort <= 0.0f || fLong / fShort < kfLongAspectThreshold)
	{
		return common::Random(2.0f * DirectX::XM_PI, rRotationRandom);
	}

	float fBase = (fX >= fY) ? (fAttachWorld + 0.5f * DirectX::XM_PI) : fAttachWorld;
	return fBase + SignedJitter(kfTangentRotationJitter, rRotationRandom);
}

// Degenerate valid-area hulls use a four-corner footprint rectangle in rRectangleStorage.
static const XMFLOAT2* LocalHull(const IslandTemplate& rTemplate, XMFLOAT2 (&rRectangleStorage)[4], int32_t& rCount)
{
	const XMFLOAT2* pLocalHull = rTemplate.pf2ValidAreaVertices;
	int32_t iCount = rTemplate.iValidAreaVertexCount;
	if (pLocalHull == nullptr || iCount < 3)
	{
		float fHalfWidth = 0.5f * rTemplate.fQuadFootprintX;
		float fHalfHeight = 0.5f * rTemplate.fQuadFootprintY;
		rRectangleStorage[0] = {-fHalfWidth, -fHalfHeight};
		rRectangleStorage[1] = { fHalfWidth, -fHalfHeight};
		rRectangleStorage[2] = { fHalfWidth,  fHalfHeight};
		rRectangleStorage[3] = {-fHalfWidth,  fHalfHeight};
		pLocalHull = rRectangleStorage;
		iCount = 4;
	}
	rCount = iCount;
	return pLocalHull;
}

static int64_t CommitPlacement(CellContext& rContext, common::crc_t crc, XMFLOAT2 f2Local, float fRotation, const common::ConvexHull2D& rCandidate)
{
	// placedHullViews hold raw vertex pointers into placedHullStorage, reserved to kiMaximumIslandsPerCell.
	// Placement must not exceed that reserve; the stable-view contract protects SAT overlap tests,
	// deterministic packing, and CRC agreement.
	ASSERT(rContext.placedHullStorage.size() < rContext.placedHullStorage.capacity());
	rContext.placedHullStorage.emplace_back(rContext.scratch.begin(), rContext.scratch.end());
	common::ConvexHull2D view = rCandidate;
	view.pVertices = rContext.placedHullStorage.back().data();
	rContext.placedHullViews.push_back(view);
	rContext.pOut->push_back({.islandCrc = crc, .f2WorldPosition = f2Local, .fRotation = fRotation});
	return std::ssize(rContext.placedHullViews) - 1;
}

// Place the large anchor at the clamped target (output index 0). The center is clamped so the rotation-
// expanded footprint rectangle stays inside the cell; returns false only if the island cannot fit at all.
// Returns the accepted cell-local position via rAcceptedLocalOut.
static bool PlaceAnchor(CellContext& rContext, common::crc_t crc, float fTargetLocalX, float fTargetLocalY, float fRotation, XMFLOAT2& rAcceptedLocalOut)
{
	const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(crc);
	XMFLOAT2 rectangleHull[4] {};
	int32_t iLocalCount = 0;
	const XMFLOAT2* pLocalHull = LocalHull(rTemplate, rectangleHull, iLocalCount);

	common::SinCos rotation = common::DeterministicSinCos(fRotation);
	float fAbsoluteCosine = std::abs(rotation.fCos);
	float fAbsoluteSine = std::abs(rotation.fSin);
	float fRotatedHalfWidth = 0.5f * (rTemplate.fQuadFootprintX * fAbsoluteCosine + rTemplate.fQuadFootprintY * fAbsoluteSine);
	float fRotatedHalfHeight = 0.5f * (rTemplate.fQuadFootprintX * fAbsoluteSine + rTemplate.fQuadFootprintY * fAbsoluteCosine);
	float fLimitX = rContext.fHalfWidth - fRotatedHalfWidth - kfCellEdgeMarginMeters;
	float fLimitY = rContext.fHalfHeight - fRotatedHalfHeight - kfCellEdgeMarginMeters;
	if (fLimitX < 0.0f || fLimitY < 0.0f)
	{
		return false;
	}

	float fLocalX = std::clamp(fTargetLocalX, -fLimitX, fLimitX);
	float fLocalY = std::clamp(fTargetLocalY, -fLimitY, fLimitY);
	XMFLOAT2 f2Local {fLocalX, fLocalY};

	rContext.scratch.resize(static_cast<size_t>(iLocalCount));
	common::ConvexHull2D candidate = common::BuildWorldHull(std::span<const XMFLOAT2>(pLocalHull, static_cast<size_t>(iLocalCount)), f2Local, rotation.fCos, rotation.fSin, std::span<XMFLOAT2>(rContext.scratch));
	CommitPlacement(rContext, crc, f2Local, fRotation, candidate);
	rAcceptedLocalOut = {fLocalX, fLocalY};
	return true;
}

// One contact-placement attempt: position `crc` just-touching the host island (placed index iHost) along
// fDirection, leaving kfTouchGapMeters between hulls, oriented fRotation. Rejects if the resulting pose leaves
// the cell or its hull overlaps any already-placed hull. On success commits the placement and returns the
// new placed index via rPlacedIndexOut.
static bool TryTouchPlace(CellContext& rContext, common::crc_t crc, int64_t iHost, float fDirection, float fRotation, int64_t& rPlacedIndexOut)
{
	const IslandTemplate& rTemplate = gpIslandTerrain->mIslands.at(crc);
	XMFLOAT2 rectangleHull[4] {};
	int32_t iLocalCount = 0;
	const XMFLOAT2* pLocalHull = LocalHull(rTemplate, rectangleHull, iLocalCount);

	common::SinCos direction = common::DeterministicSinCos(fDirection);
	float fDirectionX = direction.fCos;
	float fDirectionY = direction.fSin;

	// Candidate's near-edge projection onto fDirection, relative to its own center (rotation applied).
	common::SinCos rotation = common::DeterministicSinCos(fRotation);
	float fCosine = rotation.fCos;
	float fSine = rotation.fSin;
	float fCandidateMinimum = std::numeric_limits<float>::max();
	for (int64_t i = 0; i < iLocalCount; ++i)
	{
		float fRotatedX = pLocalHull[i].x * fCosine - pLocalHull[i].y * fSine;
		float fRotatedY = pLocalHull[i].x * fSine + pLocalHull[i].y * fCosine;
		fCandidateMinimum = std::min(fCandidateMinimum, fRotatedX * fDirectionX + fRotatedY * fDirectionY);
	}

	// Host's far-edge projection onto fDirection.
	const common::ConvexHull2D& rHost = rContext.placedHullViews.at(static_cast<size_t>(iHost));
	float fHostMaximum = std::numeric_limits<float>::lowest();
	for (int64_t i = 0; i < rHost.iVertexCount; ++i)
	{
		fHostMaximum = std::max(fHostMaximum, rHost.pVertices[i].x * fDirectionX + rHost.pVertices[i].y * fDirectionY);
	}

	// Slide the candidate center along fDirection so its near edge sits kfTouchGapMeters beyond the host's far
	// edge — the two hulls just clear each other along fDirection (separating axis), leaving the small gap.
	XMFLOAT2 f2Host = rContext.pOut->at(static_cast<size_t>(iHost)).f2WorldPosition;
	float fPlacementDistance = fHostMaximum + kfTouchGapMeters - fCandidateMinimum - (f2Host.x * fDirectionX + f2Host.y * fDirectionY);
	XMFLOAT2 f2Local {f2Host.x + fPlacementDistance * fDirectionX, f2Host.y + fPlacementDistance * fDirectionY};

	rContext.scratch.resize(static_cast<size_t>(iLocalCount));
	common::ConvexHull2D candidate = common::BuildWorldHull(std::span<const XMFLOAT2>(pLocalHull, static_cast<size_t>(iLocalCount)), f2Local, fCosine, fSine, std::span<XMFLOAT2>(rContext.scratch));

	// Keep the whole cluster inside the cell (the touch pose is not clamped — that would break contact).
	float fLimitX = rContext.fHalfWidth - kfCellEdgeMarginMeters;
	float fLimitY = rContext.fHalfHeight - kfCellEdgeMarginMeters;
	if (candidate.f2AabbMin.x < -fLimitX || candidate.f2AabbMax.x > fLimitX || candidate.f2AabbMin.y < -fLimitY || candidate.f2AabbMax.y > fLimitY)
	{
		return false;
	}

	for (const common::ConvexHull2D& rPlaced : rContext.placedHullViews)
	{
		if (common::ConvexHullsOverlap(candidate, rPlaced))
		{
			return false;
		}
	}

	rPlacedIndexOut = CommitPlacement(rContext, crc, f2Local, fRotation, candidate);
	return true;
}

void GenerateIslandChain(GridCoord coordinate, std::vector<IslandPlacement>& rOut)
{
	rOut.clear();

	float fCellWidth = kfCellWidth;
	float fCellHeight = kfCellHeight;

	// Coordinates seed the random streams; all placement geometry stays centered in cell-local meters.
	CellContext context;
	context.fHalfWidth = 0.5f * fCellWidth;
	context.fHalfHeight = 0.5f * fCellHeight;
	context.crcRandom = common::RandomEngine(SeedFromGridCoordinate(coordinate, kuiCrcPickSeedMultiplier));
	context.positionRandom = common::RandomEngine(SeedFromGridCoordinate(coordinate, kuiPositionSeedMultiplier));
	context.rotationRandom = common::RandomEngine(SeedFromGridCoordinate(coordinate, kuiRotationSeedMultiplier));
	context.anchorRandom = common::RandomEngine(SeedFromGridCoordinate(coordinate, kuiAnchorSeedMultiplier));
	context.curveRandom = common::RandomEngine(SeedFromGridCoordinate(coordinate, kuiCurveSeedMultiplier));
	context.pOut = &rOut;
	context.placedHullStorage.reserve(static_cast<size_t>(kiMaximumIslandsPerCell));
	context.placedHullViews.reserve(static_cast<size_t>(kiMaximumIslandsPerCell));
	context.scratch.reserve(static_cast<size_t>(kiHullScratchReserve));
	rOut.reserve(static_cast<size_t>(kiMaximumIslandsPerCell));

	// 1. ANCHOR (output index 0) — Huge island in the SW third, always placed.
	common::crc_t anchorCrc = PickCrc(Role::kHuge, context.crcRandom);
	float fAnchorTargetX = -context.fHalfWidth + kfAnchorFraction * fCellWidth + SignedJitter(kfAnchorJitterMeters, context.anchorRandom);
	float fAnchorTargetY = -context.fHalfHeight + kfAnchorFraction * fCellHeight + SignedJitter(kfAnchorJitterMeters, context.anchorRandom);
	float fAnchorRotation = SignedJitter(kfAnchorRotationJitter, context.anchorRandom);
	XMFLOAT2 f2AnchorLocal {};
	bool bAnchorPlaced = PlaceAnchor(context, anchorCrc, fAnchorTargetX, fAnchorTargetY, fAnchorRotation, f2AnchorLocal);
	// Output index 0 must contain the anchor because subsequent chain growth starts there.
	ASSERT(bAnchorPlaced);

	// 2. BIG-ISLAND CHAIN — 2 Large then 3 Medium, contact-linked along a hard-turning curve from the Huge
	// anchor (each link touches the chain tip — the anchor or the previous link). Stop as soon as a link
	// cannot fit inside the frame, so the chain extends as far down the curve as the cell allows.
	static constexpr Role kChainRoles[] = {Role::kLarge, Role::kLarge, Role::kMedium, Role::kMedium, Role::kMedium};
	float fHeading = 0.25f * DirectX::XM_PI + SignedJitter(kfChainHeadingStartJitter, context.anchorRandom);
	int64_t iTipIndex = 0;
	for (Role eRole : kChainRoles)
	{
		common::crc_t crc = PickCrc(eRole, context.crcRandom);
		bool bPlaced = false;
		for (int64_t i = 0; i < kiTouchAttempts; ++i)
		{
			float fDirection = fHeading + (i == 0 ? 0.0f : SignedJitter(kfTouchDirectionJitter, context.positionRandom));
			float fRotation = OrientForTangent(crc, fDirection, context.rotationRandom);
			int64_t iPlaced = 0;
			if (TryTouchPlace(context, crc, iTipIndex, fDirection, fRotation, iPlaced))
			{
				iTipIndex = iPlaced;
				bPlaced = true;
				break;
			}
		}
		if (!bPlaced)
		{
			break;
		}
		fHeading += kfChainHeadingTurn + SignedJitter(kfChainHeadingTurnJitter, context.curveRandom);
	}

	// 3. SURROUND — ring every big island (the anchor + chain links, indices [0, iBigIslandCount)) with
	// smalls that touch ONLY that big island, never another small. Evenly-spaced angular slots (count scales
	// with the island's perimeter, + jitter); a slot whose pose overlaps a neighbour is simply left empty.
	int64_t iBigIslandCount = std::ssize(context.placedHullViews);
	for (int64_t i = 0; i < iBigIslandCount; ++i)
	{
		const IslandTemplate& rBig = gpIslandTerrain->mIslands.at(context.pOut->at(static_cast<size_t>(i)).islandCrc);
		float fPerimeter = 2.0f * (rBig.fQuadFootprintX + rBig.fQuadFootprintY);
		int64_t iSlots = std::clamp(static_cast<int64_t>(fPerimeter / kfSurroundSpacingMeters), kiMinimumSurroundSlots, kiMaximumSurroundSlots);
		for (int64_t j = 0; j < iSlots; ++j)
		{
			float fDirection = (2.0f * DirectX::XM_PI * static_cast<float>(j)) / static_cast<float>(iSlots) + SignedJitter(kfSurroundDirectionJitter, context.positionRandom);
			common::crc_t crc = PickCrc(Role::kSmall, context.crcRandom);
			float fRotation = common::Random(2.0f * DirectX::XM_PI, context.rotationRandom);
			int64_t iPlaced = 0;
			TryTouchPlace(context, crc, i, fDirection, fRotation, iPlaced);
		}
	}

	// 4. TAIL — a few extra smalls trailing off the end of the curve, attached to the chain tip (a big
	// island), biased toward the forward heading so the chain peters out into islets.
	for (int64_t i = 0; i < kiTailSmallCount; ++i)
	{
		float fDirection = fHeading + SignedJitter(kfTailDirectionJitter, context.positionRandom);
		common::crc_t crc = PickCrc(Role::kSmall, context.crcRandom);
		float fRotation = common::Random(2.0f * DirectX::XM_PI, context.rotationRandom);
		int64_t iPlaced = 0;
		TryTouchPlace(context, crc, iTipIndex, fDirection, fRotation, iPlaced);
	}
}

} // namespace engine
