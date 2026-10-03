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

bool XM_CALLCONV InsideArea(FXMVECTOR vecPosition, const XMFLOAT4& rf4Area)
{
	float fPositionX = XMVectorGetX(vecPosition);
	float fPositionY = XMVectorGetY(vecPosition);
	return fPositionX > rf4Area.x && fPositionX < rf4Area.z && fPositionY < rf4Area.y && fPositionY > rf4Area.w;
}

bool XM_CALLCONV InsideArea(FXMVECTOR vecPosition, FXMVECTOR vecArea)
{
	// vecArea: x=minX, y=maxY, z=maxX, w=minY
	// Inside if: minX < posX < maxX AND minY < posY < maxY
	// Rearranged: posX > minX AND posY > minY AND maxX > posX AND maxY > posY
	XMVECTOR vecPositionAndMaximum = XMVectorPermute<0, 1, 6, 5>(vecPosition, vecArea);  // (posX, posY, maxX, maxY)
	XMVECTOR vecMinimumAndPosition = XMVectorPermute<4, 7, 0, 1>(vecPosition, vecArea);  // (minX, minY, posX, posY)
	uint32_t uiControlResult = XMVector4GreaterR(vecPositionAndMaximum, vecMinimumAndPosition);
	return XMComparisonAllTrue(uiControlResult);
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
