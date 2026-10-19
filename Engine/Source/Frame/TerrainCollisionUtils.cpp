#include "TerrainCollisionUtils.h"

#include "Frame/CellStaticData.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavQuery.h"

namespace engine
{

SegmentHit XM_CALLCONV TracePointAgainstTerrain(const CellStaticData& rStaticData, FXMVECTOR vecStartPosition, FXMVECTOR vecEndPosition)
{
	static constexpr float kfGridPitchX = kfCellWidth / static_cast<float>(kiElevationGridDimension);
	static constexpr float kfGridPitchY = kfCellHeight / static_cast<float>(kiElevationGridDimension);
	static constexpr float kfCellOriginX = kfBaseAreaMinimumX;
	static constexpr float kfCellOriginY = kfBaseAreaMinimumY;

	CellElevationSampler sampler = gpIslandTerrain->MakeCellElevationSampler(rStaticData);
	XMFLOAT4A f4Start {};
	XMFLOAT4A f4End {};
	XMStoreFloat4A(&f4Start, vecStartPosition);
	XMStoreFloat4A(&f4End, vecEndPosition);

	float fDeltaX = f4End.x - f4Start.x;
	float fDeltaY = f4End.y - f4Start.y;
	float fDeltaZ = f4End.z - f4Start.z;
	int64_t iGridX = static_cast<int64_t>(std::floor((f4Start.x - kfCellOriginX) / kfGridPitchX));
	int64_t iGridY = static_cast<int64_t>(std::floor((f4Start.y - kfCellOriginY) / kfGridPitchY));
	int64_t iStepX = fDeltaX > 0.0f ? 1 : fDeltaX < 0.0f ? -1 : 0;
	int64_t iStepY = fDeltaY > 0.0f ? 1 : fDeltaY < 0.0f ? -1 : 0;
	int64_t iNextBoundaryX = iGridX + (iStepX > 0 ? 1 : 0);
	int64_t iNextBoundaryY = iGridY + (iStepY > 0 ? 1 : 0);
	float fAbsoluteDeltaX = std::abs(fDeltaX);
	float fAbsoluteDeltaY = std::abs(fDeltaY);

	float fCurrentPercent = 0.0f;
	for (;;)
	{
		XMVECTOR vecCurrentPosition = XMVectorLerp(vecStartPosition, vecEndPosition, fCurrentPercent);
		float fPointElevation = sampler.Sample(vecCurrentPosition);
		float fCurrentZ = f4Start.z + fDeltaZ * fCurrentPercent;
		if (fCurrentZ <= fPointElevation)
		{
			return
			{
				.bHit = true,
				.fTime = fCurrentPercent,
				.vecPosition = XMVectorSetZ(vecCurrentPosition, fPointElevation),
			};
		}

		float fDistanceX = std::numeric_limits<float>::max();
		float fDistanceY = std::numeric_limits<float>::max();
		if (iStepX != 0)
		{
			float fBoundaryX = kfCellOriginX + static_cast<float>(iNextBoundaryX) * kfGridPitchX;
			fDistanceX = std::abs(fBoundaryX - f4Start.x);
		}
		if (iStepY != 0)
		{
			float fBoundaryY = kfCellOriginY + static_cast<float>(iNextBoundaryY) * kfGridPitchY;
			fDistanceY = std::abs(fBoundaryY - f4Start.y);
		}

		bool bAdvanceX = false;
		bool bAdvanceY = false;
		if (iStepX == 0 && iStepY != 0)
		{
			bAdvanceY = true;
		}
		else if (iStepY == 0 && iStepX != 0)
		{
			bAdvanceX = true;
		}
		else if (iStepX != 0 && iStepY != 0)
		{
			// Compare the two rational crossing times by cross multiplication. A double exactly holds
			// the product of two floats, so mathematical corner ties advance both axes without epsilon.
			double fScaledDistanceX = static_cast<double>(fDistanceX) * static_cast<double>(fAbsoluteDeltaY);
			double fScaledDistanceY = static_cast<double>(fDistanceY) * static_cast<double>(fAbsoluteDeltaX);
			bAdvanceX = fScaledDistanceX <= fScaledDistanceY;
			bAdvanceY = fScaledDistanceY <= fScaledDistanceX;
		}

		float fNextPercent = 1.0f;
		if (bAdvanceX || bAdvanceY)
		{
			fNextPercent = bAdvanceX ? fDistanceX / fAbsoluteDeltaX : fDistanceY / fAbsoluteDeltaY;
			fNextPercent = std::min(1.0f, fNextPercent);
		}
		float fElevation = sampler.fSeaFloor;
		if (sampler.pGrid != nullptr && iGridX >= 0 && iGridX < kiElevationGridDimension && iGridY >= 0 && iGridY < kiElevationGridDimension)
		{
			fElevation = sampler.pGrid->at(static_cast<size_t>(iGridY * kiElevationGridDimension + iGridX));
		}
		// The traversed cell owns the open interval after fCurrentPercent. If a height step rises immediately
		// across a boundary, collision begins at the boundary even when the exact point belongs to the
		// outgoing cell under floor ownership.
		if (fCurrentZ <= fElevation)
		{
			XMVECTOR vecPosition = XMVectorSetZ(vecCurrentPosition, fElevation);
			return
			{
				.bHit = true,
				.fTime = fCurrentPercent,
				.vecPosition = vecPosition,
			};
		}

		float fNextZ = f4Start.z + fDeltaZ * fNextPercent;
		// The next boundary is excluded from this cell. Equality is resolved below by Sample(), whose
		// floor lookup defines ownership for positive crossings and final endpoints.
		if (fNextZ < fElevation)
		{
			float fHitPercent = (fElevation - f4Start.z) / fDeltaZ;
			XMVECTOR vecPosition = XMVectorSetZ(XMVectorLerp(vecStartPosition, vecEndPosition, fHitPercent), fElevation);
			return
			{
				.bHit = true,
				.fTime = fHitPercent,
				.vecPosition = vecPosition,
			};
		}

		if (fNextPercent >= 1.0f)
		{
			float fEndElevation = sampler.Sample(vecEndPosition);
			if (f4End.z <= fEndElevation)
			{
				return
				{
					.bHit = true,
					.fTime = 1.0f,
					.vecPosition = XMVectorSetZ(vecEndPosition, fEndElevation),
				};
			}
			return {};
		}

		if (bAdvanceX)
		{
			iGridX += iStepX;
			iNextBoundaryX += iStepX;
		}
		if (bAdvanceY)
		{
			iGridY += iStepY;
			iNextBoundaryY += iStepY;
		}
		fCurrentPercent = fNextPercent;
	}
}

// A blocking grid cell nearer the disc center than the radius minus this overlaps the disc. About 30 times the float
// spacing at the cell's largest local coordinates, so the float-rounded position of an earlier contact never reads as
// an overlap.
constexpr float kfDiscOverlapTolerance = 0.001f;

constexpr double kfGridPitchX = kfCellWidth / static_cast<float>(kiElevationGridDimension);
constexpr double kfGridPitchY = kfCellHeight / static_cast<float>(kiElevationGridDimension);
constexpr double kfCellOriginX = kfBaseAreaMinimumX;
constexpr double kfCellOriginY = kfBaseAreaMinimumY;

// One elevation grid cell's square in cell-local meters. The disc query computes in double, where float positions,
// the grid pitch, and every cell edge are exact.
struct DiscGridSquare
{
	double fMinimumX = 0.0;
	double fMinimumY = 0.0;
	double fMaximumX = 0.0;
	double fMaximumY = 0.0;
};

struct DiscGridRange
{
	int64_t iFirstX = 0;
	int64_t iLastX = -1;
	int64_t iFirstY = 0;
	int64_t iLastY = -1;
};

struct DiscSquareOffset
{
	double fX = 0.0;
	double fY = 0.0;
	double fLengthSquared = 0.0;
};

struct DiscSweepHit
{
	double fTime = 0.0;
	int64_t iGridCell = -1;
	DiscGridSquare square {};
};

static DiscGridSquare GridCellSquare(int64_t iGridX, int64_t iGridY)
{
	double fMinimumX = kfCellOriginX + static_cast<double>(iGridX) * kfGridPitchX;
	double fMinimumY = kfCellOriginY + static_cast<double>(iGridY) * kfGridPitchY;
	return
	{
		.fMinimumX = fMinimumX,
		.fMinimumY = fMinimumY,
		.fMaximumX = fMinimumX + kfGridPitchX,
		.fMaximumY = fMinimumY + kfGridPitchY,
	};
}

// The in-grid cells overlapping the box, floor-indexed like CellElevationSampler::Sample. Off-grid cells are never
// visited, so they never block.
static DiscGridRange OverlappedGridCells(double fMinimumX, double fMinimumY, double fMaximumX, double fMaximumY)
{
	return
	{
		.iFirstX = std::max(static_cast<int64_t>(std::floor((fMinimumX - kfCellOriginX) / kfGridPitchX)), 0i64),
		.iLastX = std::min(static_cast<int64_t>(std::floor((fMaximumX - kfCellOriginX) / kfGridPitchX)), kiElevationGridDimension - 1),
		.iFirstY = std::max(static_cast<int64_t>(std::floor((fMinimumY - kfCellOriginY) / kfGridPitchY)), 0i64),
		.iLastY = std::min(static_cast<int64_t>(std::floor((fMaximumY - kfCellOriginY) / kfGridPitchY)), kiElevationGridDimension - 1),
	};
}

// Offset to (fX, fY) from the square's closest point; zero inside the square.
static DiscSquareOffset OffsetFromSquare(const DiscGridSquare& rSquare, double fX, double fY)
{
	double fOffsetX = fX - std::clamp(fX, rSquare.fMinimumX, rSquare.fMaximumX);
	double fOffsetY = fY - std::clamp(fY, rSquare.fMinimumY, rSquare.fMaximumY);
	return
	{
		.fX = fOffsetX,
		.fY = fOffsetY,
		.fLengthSquared = fOffsetX * fOffsetX + fOffsetY * fOffsetY,
	};
}

// The clearance test: (fX, fY) is clear when no in-grid blocking cell lies nearer than fRadius - kfDiscOverlapTolerance.
static bool IsDiscClear(const std::vector<float>& rGrid, float fThresholdHeight, double fX, double fY, double fRadius, double fOverlapRadiusSquared)
{
	DiscGridRange range = OverlappedGridCells(fX - fRadius, fY - fRadius, fX + fRadius, fY + fRadius);
	for (int64_t j = range.iFirstY; j <= range.iLastY; ++j)
	{
		for (int64_t i = range.iFirstX; i <= range.iLastX; ++i)
		{
			if (rGrid.at(static_cast<size_t>(j * kiElevationGridDimension + i)) < fThresholdHeight)
			{
				continue;
			}

			if (OffsetFromSquare(GridCellSquare(i, j), fX, fY).fLengthSquared < fOverlapRadiusSquared)
			{
				return false;
			}
		}
	}
	return true;
}

// Narrows [rfEnterTime, rfExitTime] to the times at which start + t * motion lies in [fMinimum, fMaximum] on one axis;
// false once the interval is empty. Inclusive bounds make touching a contact, except a start on a bound of an axis
// without motion: that path runs along the bound and only grazes.
static bool ClipSlab(double fStart, double fMotion, double fMinimum, double fMaximum, double& rfEnterTime, double& rfExitTime)
{
	if (fMotion == 0.0)
	{
		return fStart > fMinimum && fStart < fMaximum;
	}

	double fTimeA = (fMinimum - fStart) / fMotion;
	double fTimeB = (fMaximum - fStart) / fMotion;
	rfEnterTime = std::max(rfEnterTime, std::min(fTimeA, fTimeB));
	rfExitTime = std::min(rfExitTime, std::max(fTimeA, fTimeB));
	return rfEnterTime <= rfExitTime;
}

// Earliest t in [0, 1] at which the disc centered at start + t * motion touches the square, or -1 when it does not or
// only grazes it; the start lies farther than fRadius from the square. The touch region is the square's fRadius-grown
// box with its four corners rounded to circles of radius fRadius about the square's corners, so a path entering the
// box in a corner region reaches the rest of the touch region only through that circle.
static double DiscContactTime(const DiscGridSquare& rSquare, double fStartX, double fStartY, double fMotionX, double fMotionY, double fRadius)
{
	double fEnterTime = 0.0;
	double fExitTime = 1.0;
	if (!ClipSlab(fStartX, fMotionX, rSquare.fMinimumX - fRadius, rSquare.fMaximumX + fRadius, fEnterTime, fExitTime) || !ClipSlab(fStartY, fMotionY, rSquare.fMinimumY - fRadius, rSquare.fMaximumY + fRadius, fEnterTime, fExitTime))
	{
		return -1.0;
	}

	double fEnterX = fStartX + fEnterTime * fMotionX;
	double fEnterY = fStartY + fEnterTime * fMotionY;
	bool bOutsideX = fEnterX < rSquare.fMinimumX || fEnterX > rSquare.fMaximumX;
	bool bOutsideY = fEnterY < rSquare.fMinimumY || fEnterY > rSquare.fMaximumY;
	if (!bOutsideX || !bOutsideY)
	{
		return fEnterTime;
	}

	// The start lies outside the corner circle, so a hit needs motion toward the corner. The smaller root of
	// |offset + t * motion|^2 = fRadius^2 is taken in the form whose denominator never cancels, which keeps it positive.
	double fOffsetX = fStartX - std::clamp(fEnterX, rSquare.fMinimumX, rSquare.fMaximumX);
	double fOffsetY = fStartY - std::clamp(fEnterY, rSquare.fMinimumY, rSquare.fMaximumY);
	double fMotionLengthSquared = fMotionX * fMotionX + fMotionY * fMotionY;
	double fOffsetAlongMotion = fOffsetX * fMotionX + fOffsetY * fMotionY;
	double fClearanceSquared = fOffsetX * fOffsetX + fOffsetY * fOffsetY - fRadius * fRadius;
	if (fOffsetAlongMotion >= 0.0)
	{
		return -1.0;
	}

	double fDiscriminant = fOffsetAlongMotion * fOffsetAlongMotion - fMotionLengthSquared * fClearanceSquared;
	if (fDiscriminant <= 0.0)
	{
		return -1.0;
	}

	double fTime = fClearanceSquared / (std::sqrt(fDiscriminant) - fOffsetAlongMotion);
	return fTime <= 1.0 ? fTime : -1.0;
}

// Rounds one contact coordinate to float in the direction of the hit normal's component, so the rounding never moves
// the contact nearer the hit square.
static float RoundAlongNormal(double fCoordinate, double fNormal)
{
	float fRounded = static_cast<float>(fCoordinate);
	if (fNormal > 0.0 && static_cast<double>(fRounded) < fCoordinate)
	{
		return std::nextafter(fRounded, std::numeric_limits<float>::infinity());
	}

	if (fNormal < 0.0 && static_cast<double>(fRounded) > fCoordinate)
	{
		return std::nextafter(fRounded, -std::numeric_limits<float>::infinity());
	}
	return fRounded;
}

DiscTerrainResult XM_CALLCONV ResolveDiscAgainstTerrain(const CellStaticData& rStaticData, const NavData& rNavData, FXMVECTOR vecStart, FXMVECTOR vecEnd, float fRadius, float fThresholdHeight, bool bSlide)
{
	DiscTerrainResult result
	{
		.vecPosition = XMVectorSetW(vecEnd, 1.0f),
	};

	// An island-free cell has no elevation grid, and nothing blocks there.
	CellElevationSampler sampler = gpIslandTerrain->MakeCellElevationSampler(rStaticData);
	if (sampler.pGrid == nullptr)
	{
		return result;
	}
	const std::vector<float>& rGrid = *sampler.pGrid;

	XMFLOAT4A f4Start {};
	XMFLOAT4A f4End {};
	XMStoreFloat4A(&f4Start, vecStart);
	XMStoreFloat4A(&f4End, vecEnd);

	double fDiscRadius = fRadius;
	double fRadiusSquared = fDiscRadius * fDiscRadius;
	double fOverlapRadius = fDiscRadius - static_cast<double>(kfDiscOverlapTolerance);
	double fOverlapRadiusSquared = fOverlapRadius * fOverlapRadius;
	double fMotionX = static_cast<double>(f4End.x) - static_cast<double>(f4Start.x);
	double fMotionY = static_cast<double>(f4End.y) - static_cast<double>(f4Start.y);
	double fResolvedStartX = f4Start.x;
	double fResolvedStartY = f4Start.y;

	// When the resolved start is still not clear, the cells overlapping it are left out of both sweeps, so the body
	// can leave them by its own motion.
	bool bExcludeOverlaps = false;
	result.flags.Set(DiscTerrainFlags::kStartOverlap, !IsDiscClear(rGrid, fThresholdHeight, fResolvedStartX, fResolvedStartY, fDiscRadius, fOverlapRadiusSquared));
	if (result.flags & DiscTerrainFlags::kStartOverlap) [[unlikely]]
	{
		XMVECTOR vecSnapped = NavQuerySnapToNavigable(vecStart, rNavData);
		XMFLOAT4A f4Snapped {};
		XMStoreFloat4A(&f4Snapped, vecSnapped);
		bool bMoved = f4Snapped.x != f4Start.x || f4Snapped.y != f4Start.y;
		if (bMoved)
		{
			fResolvedStartX = f4Snapped.x;
			fResolvedStartY = f4Snapped.y;
			double fMoveX = fResolvedStartX - static_cast<double>(f4Start.x);
			double fMoveY = fResolvedStartY - static_cast<double>(f4Start.y);
			double fMoveDistance = std::sqrt(fMoveX * fMoveX + fMoveY * fMoveY);
			result.vecNormal = XMVectorSet(static_cast<float>(fMoveX / fMoveDistance), static_cast<float>(fMoveY / fMoveDistance), 0.0f, 0.0f);
			result.flags.Set(DiscTerrainFlags::kContact);
			result.fStartOverlapDistance = static_cast<float>(fMoveDistance);
		}
		bExcludeOverlaps = !bMoved || !IsDiscClear(rGrid, fThresholdHeight, fResolvedStartX, fResolvedStartY, fDiscRadius, fOverlapRadiusSquared);
	}

	// Earliest contact along the motion, kept by strict < so the first blocking cell in row-major order wins a tie. A
	// cell at most fRadius from the sweep start is touching and is hit at t = 0 only when the motion has a component
	// into it.
	auto Sweep = [&](double fFromX, double fFromY, double fAlongX, double fAlongY, int64_t iExcludedGridCell) -> DiscSweepHit
	{
		DiscSweepHit hit {};
		DiscGridRange range = OverlappedGridCells(std::min(fFromX, fFromX + fAlongX) - fDiscRadius, std::min(fFromY, fFromY + fAlongY) - fDiscRadius, std::max(fFromX, fFromX + fAlongX) + fDiscRadius, std::max(fFromY, fFromY + fAlongY) + fDiscRadius);
		for (int64_t j = range.iFirstY; j <= range.iLastY; ++j)
		{
			for (int64_t i = range.iFirstX; i <= range.iLastX; ++i)
			{
				int64_t iGridCell = j * kiElevationGridDimension + i;
				if (rGrid.at(static_cast<size_t>(iGridCell)) < fThresholdHeight)
				{
					continue;
				}

				if (iGridCell == iExcludedGridCell)
				{
					continue;
				}

				DiscGridSquare square = GridCellSquare(i, j);
				if (bExcludeOverlaps && OffsetFromSquare(square, fResolvedStartX, fResolvedStartY).fLengthSquared < fOverlapRadiusSquared)
				{
					continue;
				}

				double fTime = -1.0;
				DiscSquareOffset offset = OffsetFromSquare(square, fFromX, fFromY);
				if (offset.fLengthSquared <= fRadiusSquared)
				{
					if (fAlongX * offset.fX + fAlongY * offset.fY < 0.0)
					{
						fTime = 0.0;
					}
				}
				else
				{
					fTime = DiscContactTime(square, fFromX, fFromY, fAlongX, fAlongY, fDiscRadius);
				}

				if (fTime >= 0.0 && (hit.iGridCell < 0 || fTime < hit.fTime))
				{
					hit =
					{
						.fTime = fTime,
						.iGridCell = iGridCell,
						.square = square,
					};
				}
			}
		}
		return hit;
	};

	// Zero motion takes only the start-overlap resolution.
	double fPositionX = fResolvedStartX + fMotionX;
	double fPositionY = fResolvedStartY + fMotionY;
	if (fMotionX != 0.0 || fMotionY != 0.0)
	{
		DiscSweepHit hit = Sweep(fResolvedStartX, fResolvedStartY, fMotionX, fMotionY, -1);
		if (hit.iGridCell >= 0)
		{
			double fContactX = fResolvedStartX + hit.fTime * fMotionX;
			double fContactY = fResolvedStartY + hit.fTime * fMotionY;
			DiscSquareOffset normalOffset = OffsetFromSquare(hit.square, fContactX, fContactY);
			double fNormalLength = std::sqrt(normalOffset.fLengthSquared);
			double fNormalX = normalOffset.fX / fNormalLength;
			double fNormalY = normalOffset.fY / fNormalLength;
			if (!(result.flags & DiscTerrainFlags::kContact))
			{
				result.vecNormal = XMVectorSet(static_cast<float>(fNormalX), static_cast<float>(fNormalY), 0.0f, 0.0f);
				result.flags.Set(DiscTerrainFlags::kContact);
			}

			// A contact rounded nearer than fRadius to a face would put the next collinear blocking cell's corner circle
			// across the slide path and stop the slide there.
			fPositionX = RoundAlongNormal(fContactX, fNormalX);
			fPositionY = RoundAlongNormal(fContactY, fNormalY);
			if (bSlide)
			{
				double fRemainingTime = 1.0 - hit.fTime;
				double fSlideX = fRemainingTime * fMotionX;
				double fSlideY = fRemainingTime * fMotionY;
				double fSlideIntoNormal = std::min(0.0, fSlideX * fNormalX + fSlideY * fNormalY);
				fSlideX -= fSlideIntoNormal * fNormalX;
				fSlideY -= fSlideIntoNormal * fNormalY;
				if (fSlideX != 0.0 || fSlideY != 0.0)
				{
					// The slide starts from the float-rounded contact and leaves the hit cell out: the slide has no component
					// into the hit normal, so only round-off in that projection could make the hit cell stop it.
					DiscSweepHit slideHit = Sweep(fPositionX, fPositionY, fSlideX, fSlideY, hit.iGridCell);
					if (slideHit.iGridCell >= 0)
					{
						double fSlideContactX = fPositionX + slideHit.fTime * fSlideX;
						double fSlideContactY = fPositionY + slideHit.fTime * fSlideY;
						DiscSquareOffset slideNormalOffset = OffsetFromSquare(slideHit.square, fSlideContactX, fSlideContactY);
						fPositionX = RoundAlongNormal(fSlideContactX, slideNormalOffset.fX);
						fPositionY = RoundAlongNormal(fSlideContactY, slideNormalOffset.fY);
					}
					else
					{
						fPositionX += fSlideX;
						fPositionY += fSlideY;
					}
				}
			}
		}
	}

	// Without contact the position is the end, unchanged.
	if (result.flags & DiscTerrainFlags::kContact)
	{
		result.vecPosition = XMVectorSet(static_cast<float>(fPositionX), static_cast<float>(fPositionY), f4End.z, 1.0f);
	}
	return result;
}

} // namespace engine
