#include "MathUtils.h"

namespace common
{

XMVECTOR XM_CALLCONV ToBaseHeight(FXMVECTOR vecPosition, FXMVECTOR vecEyePosition, float fBaseHeight)
{
	// The SDK returns all-lane QNaN when the position-to-eye line is parallel to the base-height plane.
	XMVECTOR vecIntersect = XMPlaneIntersectLine(XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, fBaseHeight, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)), vecPosition, vecEyePosition);
	return XMVector3IsNaN(vecIntersect) ? XMVectorSet(0.0f, 0.0f, fBaseHeight, 1.0f) : vecIntersect;
}

float RotationFromPosition(FXMVECTOR vecPosition)
{
	// atan2 is single-valued over [-pi, pi], finite at the origin, and distinguishes north (+y) from south (-y)
	return std::atan2(XMVectorGetY(vecPosition), XMVectorGetX(vecPosition));
}

XMVECTOR XM_CALLCONV QuaternionFromDirection(FXMVECTOR vecDirection, FXMVECTOR vecOriginNormal, FXMVECTOR vecUp)
{
	XMVECTOR vecCross = XMVector3Cross(vecOriginNormal, vecDirection);
	float fDot = XMVectorGetX(XMVector3Dot(vecDirection, vecOriginNormal));

	// Tolerance test on the pre-normalized cross length: a nearly (anti-)parallel input yields a tiny cross whose
	// normalization is a garbage axis. fDot disambiguates parallel (identity) from anti-parallel (180 deg about up).
	static constexpr float kfParallelEpsilonSquared = 1.0e-6f;
	if (XMVectorGetX(XMVector3LengthSq(vecCross)) < kfParallelEpsilonSquared)
	{
		return fDot < 0.0f ? XMQuaternionRotationNormal(vecUp, XM_PI) : XMQuaternionIdentity();
	}

	float fAngle = std::acos(std::clamp(fDot, -1.0f, 1.0f));
	return XMQuaternionRotationNormal(XMVector3Normalize(vecCross), fAngle);
}

AreaVertices XM_CALLCONV CalculateArea(FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fForward, float fBack, float fWidth)
{
	auto vecForward = XMVectorMultiply(XMVectorReplicate(fForward), vecDirection);
	auto vecBack = XMVectorMultiply(XMVectorReplicate(-fBack), vecDirection);

	auto vecWidthNormal = XMVector3Normalize(XMVector3Cross(vecDirection, XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)));
	auto vecLeft = XMVectorMultiply(XMVectorReplicate(fWidth), vecWidthNormal);
	auto vecRight = XMVectorMultiply(XMVectorReplicate(-fWidth), vecWidthNormal);

	auto vecTopLeft = XMVectorAdd(vecPosition, XMVectorAdd(vecForward, vecLeft));
	auto vecTopRight = XMVectorAdd(vecPosition, XMVectorAdd(vecForward, vecRight));
	auto vecBottomLeft = XMVectorAdd(vecPosition, XMVectorAdd(vecBack, vecLeft));
	auto vecBottomRight = XMVectorAdd(vecPosition, XMVectorAdd(vecBack, vecRight));

	return AreaVertices {.vecTopLeft = vecTopLeft, .vecTopRight = vecTopRight, .vecBottomLeft = vecBottomLeft, .vecBottomRight = vecBottomRight};
}

XMVECTOR XM_CALLCONV RotateTowardsPercent(FXMVECTOR vecDirection, FXMVECTOR vecTowards, float fPercent)
{
	float fCrossZ = XMVectorGetZ(XMVector3Cross(vecTowards, vecDirection));
	float fAngle = XMVectorGetX(XMVector2AngleBetweenNormals(vecTowards, vecDirection));
	return XMVector4Transform(vecDirection, XMMatrixRotationZ(fPercent * (fCrossZ > 0.0f ? -fAngle : fAngle)));
}

XMVECTOR XM_CALLCONV RandomAngleJitter(FXMVECTOR vecDirection, float fMaximumJitter, RandomEngine& rRandomEngine)
{
	float fJitter = -fMaximumJitter + Random<2.0f>(rRandomEngine) * fMaximumJitter;
	return XMVector4Transform(vecDirection, XMMatrixRotationZ(fJitter));
}

XMMATRIX XM_CALLCONV RotationMatrixFromDirection(FXMVECTOR vecDirection, FXMVECTOR vecOriginNormal, FXMVECTOR vecUp)
{
	return XMMatrixRotationQuaternion(QuaternionFromDirection(vecDirection, vecOriginNormal, vecUp));
}

float XM_CALLCONV Distance(FXMVECTOR vecOne, FXMVECTOR vecTwo)
{
	return XMVectorGetX(XMVector3Length(XMVectorSubtract(vecTwo, vecOne)));
}

XMVECTOR XM_CALLCONV DirectionTo(FXMVECTOR vecFrom, FXMVECTOR vecTo)
{
	// Compare all three spatial lanes so points equal only in X are not treated as coincident.
	static constexpr float kfCoincidentEpsilon = 1.0e-6f;
	if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecTo, vecFrom))) < kfCoincidentEpsilon * kfCoincidentEpsilon) [[unlikely]]
	{
		return XMVectorZero();
	}

	return XMVector3Normalize(XMVectorSubtract(vecTo, vecFrom));
}

XMVECTOR XM_CALLCONV ComputeLeadPosition(FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition, FXMVECTOR vecTargetVelocity, float fProjectileSpeed)
{
	XMVECTOR vecOffset = XMVectorSubtract(vecTargetPosition, vecShooterPosition);
	float fQuadraticCoefficient = XMVectorGetX(XMVector3Dot(vecTargetVelocity, vecTargetVelocity)) - fProjectileSpeed * fProjectileSpeed;
	float fLinearCoefficient = 2.0f * XMVectorGetX(XMVector3Dot(vecOffset, vecTargetVelocity));
	float fConstantCoefficient = XMVectorGetX(XMVector3Dot(vecOffset, vecOffset));

	float fInterceptTime = -1.0f;
	// Scale the degeneracy threshold by the largest coefficient for kilometer-scale coordinates. All-zero
	// coefficients produce NaN roots, leaving fInterceptTime negative and returning vecTargetPosition.
	float fLargestCoefficientMagnitude = std::max(std::abs(fQuadraticCoefficient), std::max(std::abs(fLinearCoefficient), std::abs(fConstantCoefficient)));
	static constexpr float kfRelativeEpsilon = 1.0e-6f;
	if (std::abs(fQuadraticCoefficient) < kfRelativeEpsilon * fLargestCoefficientMagnitude)
	{
		// Quadratic coefficient negligible against the largest coefficient, as when target speed nearly equals projectile speed: solve the linear equation
		if (std::abs(fLinearCoefficient) > kfRelativeEpsilon * fLargestCoefficientMagnitude)
		{
			fInterceptTime = -fConstantCoefficient / fLinearCoefficient;
		}
	}
	else
	{
		float fDiscriminant = fLinearCoefficient * fLinearCoefficient - 4.0f * fQuadraticCoefficient * fConstantCoefficient;
		if (fDiscriminant >= 0.0f)
		{
			float fSquareRoot = std::sqrt(fDiscriminant);
			float fInverseTwiceQuadraticCoefficient = 0.5f / fQuadraticCoefficient;
			float fFirstInterceptTime = (-fLinearCoefficient - fSquareRoot) * fInverseTwiceQuadraticCoefficient;
			float fSecondInterceptTime = (-fLinearCoefficient + fSquareRoot) * fInverseTwiceQuadraticCoefficient;
			if (fFirstInterceptTime > 0.0f && fSecondInterceptTime > 0.0f)
			{
				fInterceptTime = std::min(fFirstInterceptTime, fSecondInterceptTime);
			}
			else if (fFirstInterceptTime > 0.0f)
			{
				fInterceptTime = fFirstInterceptTime;
			}
			else if (fSecondInterceptTime > 0.0f)
			{
				fInterceptTime = fSecondInterceptTime;
			}
		}
	}

	if (fInterceptTime <= 0.0f)
	{
		return vecTargetPosition;
	}
	return XMVectorMultiplyAdd(vecTargetVelocity, XMVectorReplicate(fInterceptTime), vecTargetPosition);
}

bool XM_CALLCONV AxisAlignedBoundingBoxIntersectsArea(XMFLOAT4 f4Area, FXMVECTOR vecMinimum, FXMVECTOR vecMaximum)
{
	float fMinimumX = XMVectorGetX(vecMinimum);
	float fMaximumX = XMVectorGetX(vecMaximum);
	float fMinimumY = XMVectorGetY(vecMinimum);
	float fMaximumY = XMVectorGetY(vecMaximum);

	return !(fMaximumX < f4Area.x || fMinimumX > f4Area.z || fMaximumY < f4Area.w || fMinimumY > f4Area.y);
}

// |n|^2 / 2 for edge normal n, the edge's dot(p, n) threshold. Every hex dot product is an XMVector2Dot, so the inside
// test, the exit trace, and the exit direction compare identically rounded values.
static XMVECTOR XM_CALLCONV HexEdgeThreshold(FXMVECTOR vecNormal)
{
	return XMVectorScale(XMVector2Dot(vecNormal, vecNormal), 0.5f);
}

// Closed on the edges toward entries 1-3 and open on 4, 5, and 0: edge k of a hexagon is edge k+3 of its neighbour
// across it, and {1, 2, 3} and {4, 5, 0} partition the entries, so every edge has one owner. Both compares are false
// for a NaN dot, and every axis has edge normals of both signs, so a NaN or infinite position fails some edge.
static bool XM_CALLCONV PassesHexEdge(int64_t iDirection, FXMVECTOR vecDot, FXMVECTOR vecThreshold)
{
	if (iDirection >= 1 && iDirection <= 3)
	{
		return XMVector2LessOrEqual(vecDot, vecThreshold);
	}
	return XMVector2Less(vecDot, vecThreshold);
}

bool XM_CALLCONV IsInsideHex(FXMVECTOR vecPosition, const HexDirections& rDirections, float fInset)
{
	for (int64_t k = 0; k < kiHexDirectionCount; ++k)
	{
		XMVECTOR vecNormal = XMLoadFloat2(&rDirections.pEntries[k].f2CenterOffset);
		XMVECTOR vecThreshold = XMVectorSubtract(HexEdgeThreshold(vecNormal), XMVectorScale(XMVector2Length(vecNormal), fInset));
		if (!PassesHexEdge(k, XMVector2Dot(vecPosition, vecNormal), vecThreshold))
		{
			return false;
		}
	}
	return true;
}

std::optional<float> XM_CALLCONV TraceHexExit(FXMVECTOR vecStart, FXMVECTOR vecEnd, const HexDirections& rDirections)
{
	std::optional<float> optionalTime;
	for (int64_t k = 0; k < kiHexDirectionCount; ++k)
	{
		XMVECTOR vecNormal = XMLoadFloat2(&rDirections.pEntries[k].f2CenterOffset);
		XMVECTOR vecThreshold = HexEdgeThreshold(vecNormal);
		XMVECTOR vecStartDot = XMVector2Dot(vecStart, vecNormal);
		if (!PassesHexEdge(k, vecStartDot, vecThreshold))
		{
			return 0.0f;
		}

		XMVECTOR vecEndDot = XMVector2Dot(vecEnd, vecNormal);
		if (PassesHexEdge(k, vecEndDot, vecThreshold))
		{
			continue;
		}

		// The start passes this edge's compare and the end fails it, so the divisor is positive and the numerator lies
		// in [0, divisor]; rounding keeps the time in [0, 1].
		float fStartDot = XMVectorGetX(vecStartDot);
		float fTime = (XMVectorGetX(vecThreshold) - fStartDot) / (XMVectorGetX(vecEndDot) - fStartDot);
		optionalTime = std::min(optionalTime.value_or(fTime), fTime);
	}
	return optionalTime;
}

int64_t XM_CALLCONV HexExitDirection(FXMVECTOR vecPosition, const HexDirections& rDirections)
{
	int64_t iExitDirection = -1;
	float fLargestDot = 0.0f;
	for (int64_t k = 0; k < kiHexDirectionCount; ++k)
	{
		XMVECTOR vecNormal = XMLoadFloat2(&rDirections.pEntries[k].f2CenterOffset);
		XMVECTOR vecDot = XMVector2Dot(vecPosition, vecNormal);
		if (PassesHexEdge(k, vecDot, HexEdgeThreshold(vecNormal)))
		{
			continue;
		}

		float fDot = XMVectorGetX(vecDot);
		if (iExitDirection < 0 || fDot > fLargestDot)
		{
			iExitDirection = k;
			fLargestDot = fDot;
		}
	}
	return iExitDirection;
}

XMVECTOR XM_CALLCONV HexCenterOffset(int64_t iDeltaQ, int64_t iDeltaR, const HexDirections& rDirections)
{
	return XMVectorAdd(XMVectorScale(XMLoadFloat2(&rDirections.f2BasisQ), static_cast<float>(iDeltaQ)), XMVectorScale(XMLoadFloat2(&rDirections.f2BasisR), static_cast<float>(iDeltaR)));
}

std::pair<double, double> XM_CALLCONV HexCenterOffsetDouble(int64_t iDeltaQ, int64_t iDeltaR, const HexDirections& rDirections)
{
	double fDeltaQ = static_cast<double>(iDeltaQ);
	double fDeltaR = static_cast<double>(iDeltaR);
	return {fDeltaQ * static_cast<double>(rDirections.f2BasisQ.x) + fDeltaR * static_cast<double>(rDirections.f2BasisR.x), fDeltaQ * static_cast<double>(rDirections.f2BasisQ.y) + fDeltaR * static_cast<double>(rDirections.f2BasisR.y)};
}

int64_t XM_CALLCONV HexGridDistance(int64_t iDeltaQ, int64_t iDeltaR)
{
	return (std::abs(iDeltaQ) + std::abs(iDeltaR) + std::abs(iDeltaQ + iDeltaR)) / 2;
}

bool XM_CALLCONV RectangleOverlapsHex(XMFLOAT4 f4Rectangle, FXMVECTOR vecHexCenter, const HexDirections& rDirections)
{
	XMFLOAT2A f2HexCenter {};
	XMStoreFloat2A(&f2HexCenter, vecHexCenter);
	float fHexHalfWidth = rDirections.f2HalfExtents.x;
	float fHexHalfHeight = rDirections.f2HalfExtents.y;
	bool bBoundingRectanglesOverlap = f4Rectangle.x < f2HexCenter.x + fHexHalfWidth && f4Rectangle.z > f2HexCenter.x - fHexHalfWidth
	                               && f4Rectangle.w < f2HexCenter.y + fHexHalfHeight && f4Rectangle.y > f2HexCenter.y - fHexHalfHeight;
	if (!bBoundingRectanglesOverlap)
	{
		return false;
	}

	// The bounding rectangle test covers the x axis and the N and S edges. Entries 0 and 1 are the two slanted edge
	// normals; entries 3 and 4 are their negations and give the same absolute-value test.
	XMVECTOR vecRectangleCenter = XMVectorSet(0.5f * (f4Rectangle.x + f4Rectangle.z), 0.5f * (f4Rectangle.y + f4Rectangle.w), 0.0f, 0.0f);
	XMVECTOR vecRectangleHalfExtents = XMVectorSet(0.5f * (f4Rectangle.z - f4Rectangle.x), 0.5f * (f4Rectangle.y - f4Rectangle.w), 0.0f, 0.0f);
	XMVECTOR vecOffset = XMVectorSubtract(vecRectangleCenter, vecHexCenter);
	for (int64_t k = 0; k < 2; ++k)
	{
		XMVECTOR vecNormal = XMLoadFloat2(&rDirections.pEntries[k].f2CenterOffset);
		XMVECTOR vecReach = XMVectorAdd(HexEdgeThreshold(vecNormal), XMVector2Dot(vecRectangleHalfExtents, XMVectorAbs(vecNormal)));
		if (XMVector2GreaterOrEqual(XMVectorAbs(XMVector2Dot(vecOffset, vecNormal)), vecReach))
		{
			return false;
		}
	}
	return true;
}

XMVECTOR XM_CALLCONV ColorToVector(uint32_t uiColor)
{
	static constexpr float kfMultiplier = 1.0f / 255.0f;
	return XMVectorSet(kfMultiplier * static_cast<float>(uiColor >> 24), kfMultiplier * static_cast<float>((uiColor & 0x00FF0000) >> 16), kfMultiplier * static_cast<float>((uiColor & 0x0000FF00) >> 8), kfMultiplier * static_cast<float>(uiColor & 0x000000FF));
}

uint32_t XM_CALLCONV ColorToUint(FXMVECTOR vecColor)
{
	// Saturate to [0,1] so out-of-range lanes can't overflow an 8-bit field and bleed into the adjacent channel,
	// and round-to-nearest (+0.5f, lanes non-negative after saturate) so ColorToUint(ColorToVector(c)) == c
	XMFLOAT4A f4Color {};
	XMStoreFloat4A(&f4Color, XMVectorSaturate(vecColor));

	static constexpr float kfMultiplier = 255.0f;
	return static_cast<uint32_t>(kfMultiplier * f4Color.x + 0.5f) << 24 | static_cast<uint32_t>(kfMultiplier * f4Color.y + 0.5f) << 16 | static_cast<uint32_t>(kfMultiplier * f4Color.z + 0.5f) << 8 | static_cast<uint32_t>(kfMultiplier * f4Color.w + 0.5f);
}

uint32_t InterpolatePackedColor(uint32_t uiFirstColor, uint32_t uiSecondColor, float fPercent)
{
	return ColorToUint(XMVectorLerp(ColorToVector(uiFirstColor), ColorToVector(uiSecondColor), fPercent));
}

} // namespace common
