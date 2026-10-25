#pragma once

#include "Random.h"

namespace common
{

// Clamps fValue to fMagnitudeLimit's magnitude while preserving fValue's sign; callers provide a nonnegative fMagnitudeLimit.
constexpr float ClampMagnitude(float fValue, float fMagnitudeLimit)
{
	return fValue >= 0.0f ? std::min(fValue, fMagnitudeLimit) : -std::min(-fValue, fMagnitudeLimit);
}

struct AreaVertices
{
	XMVECTOR vecTopLeft {};
	XMVECTOR vecTopRight {};
	XMVECTOR vecBottomLeft {};
	XMVECTOR vecBottomRight {};
};

// Validate an XMVECTOR at a spawn / transfer invariant boundary: all 4 lanes finite, and
// W == 1.0 for positions (IS_POSITION=true) or W == 0.0 for directions and direction-like
// vectors, e.g. velocities (IS_POSITION=false). `XMVectorMultiplyAdd` propagates all 4 lanes,
// so velocity.W != 0 would drift into position.W; `XMVector3Normalize` divides by 3D length
// only, so a W-lane NaN survives and silently poisons downstream 4D consumers.
template<bool IS_POSITION>
inline void XM_CALLCONV ValidateVector(FXMVECTOR vec)
{
	ASSERT(std::isfinite(XMVectorGetX(vec))
		&& std::isfinite(XMVectorGetY(vec))
		&& std::isfinite(XMVectorGetZ(vec))
		&& std::isfinite(XMVectorGetW(vec)));
	if constexpr (IS_POSITION)
	{
		ASSERT(XMVectorGetW(vec) == 1.0f);
	}
	else
	{
		ASSERT(XMVectorGetW(vec) == 0.0f);
	}
}

// Converts 0xRRGGBBAA to normalized RGBA components.
XMVECTOR XM_CALLCONV ColorToVector(uint32_t uiColor);

// Clamps normalized components to [0, 1] and packs them as 0xRRGGBBAA.
uint32_t XM_CALLCONV ColorToUint(FXMVECTOR vecColor);

// Interpolates packed RGBA colors with fPercent in [0, 1].
uint32_t InterpolatePackedColor(uint32_t uiFirstColor, uint32_t uiSecondColor, float fPercent);

XMVECTOR XM_CALLCONV ToBaseHeight(FXMVECTOR vecPosition, FXMVECTOR vecEyePosition, float fBaseHeight);
float RotationFromPosition(FXMVECTOR vecPosition);
XMVECTOR XM_CALLCONV QuaternionFromDirection(FXMVECTOR vecDirection, FXMVECTOR vecOriginNormal, FXMVECTOR vecUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
AreaVertices XM_CALLCONV CalculateArea(FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fForward, float fBack, float fWidth);
XMVECTOR XM_CALLCONV RotateTowardsPercent(FXMVECTOR vecDirection, FXMVECTOR vecTowards, float fPercent);
XMVECTOR XM_CALLCONV RandomAngleJitter(FXMVECTOR vecDirection, float fMaximumJitter, RandomEngine& rRandomEngine);

// Generate random XY offset in range [-JITTER, +JITTER] for each component
template<float JITTER>
inline XMVECTOR XM_CALLCONV RandomXYJitter(RandomEngine& rRandomEngine)
{
	return XMVectorSet(-JITTER + Random(2.0f * JITTER, rRandomEngine), -JITTER + Random(2.0f * JITTER, rRandomEngine), 0.0f, 0.0f);
}

// For dynamic values such as rType.fParticlePositionJitter.
// Operation order matches the compile-time RandomXYJitter<JITTER> form so identical magnitudes consume the RNG identically
inline XMVECTOR XM_CALLCONV RandomXYJitter(float fJitter, RandomEngine& rRandomEngine)
{
	return XMVectorSet(-fJitter + Random(2.0f * fJitter, rRandomEngine), -fJitter + Random(2.0f * fJitter, rRandomEngine), 0.0f, 0.0f);
}

// Add random XY jitter to a position (no normalization)
template<float JITTER>
inline XMVECTOR XM_CALLCONV RandomPositionJitter(FXMVECTOR vecPosition, RandomEngine& rRandomEngine)
{
	return XMVectorAdd(vecPosition, RandomXYJitter<JITTER>(rRandomEngine));
}

inline XMVECTOR XM_CALLCONV RandomPositionJitter(FXMVECTOR vecPosition, float fJitter, RandomEngine& rRandomEngine)
{
	return XMVectorAdd(vecPosition, RandomXYJitter(fJitter, rRandomEngine));
}

template<float JITTER>
inline XMVECTOR XM_CALLCONV RandomDirectionJitter(FXMVECTOR vecDirection, RandomEngine& rRandomEngine)
{
	return XMVector3Normalize(XMVectorAdd(vecDirection, RandomXYJitter<JITTER>(rRandomEngine)));
}

XMMATRIX XM_CALLCONV RotationMatrixFromDirection(FXMVECTOR vecDirection, FXMVECTOR vecOriginNormal, FXMVECTOR vecUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

float XM_CALLCONV Distance(FXMVECTOR vecOne, FXMVECTOR vecTwo);

XMVECTOR XM_CALLCONV DirectionTo(FXMVECTOR vecFrom, FXMVECTOR vecTo);

// Solves ||(T - S) + Vt*t|| = vp*t for the smallest positive t (the linear root when the quadratic coefficient is negligible) and returns T + Vt*t,
// for a projectile that does not inherit shooter velocity. Returns vecTargetPosition when no positive t exists.
XMVECTOR XM_CALLCONV ComputeLeadPosition(FXMVECTOR vecShooterPosition, FXMVECTOR vecTargetPosition, FXMVECTOR vecTargetVelocity, float fProjectileSpeed);

// Computes on make_unsigned_t to avoid signed-overflow UB on large valid inputs; result is identical to the signed form for non-negative inputs
template<std::integral T>
constexpr inline T RoundUp(T iToRound, T iMultiple)
{
	using UnsignedT = std::make_unsigned_t<T>;
	return static_cast<T>(((static_cast<UnsignedT>(iToRound) + static_cast<UnsignedT>(iMultiple) - 1) / static_cast<UnsignedT>(iMultiple)) * static_cast<UnsignedT>(iMultiple));
}

template<std::integral T, T MULTIPLE>
constexpr inline T RoundUp(T iToRound)
{
	static_assert(MULTIPLE > 0, "Multiple must be positive");

	using UnsignedT = std::make_unsigned_t<T>;
	if constexpr (std::has_single_bit(static_cast<UnsignedT>(MULTIPLE)))
	{
		return static_cast<T>((static_cast<UnsignedT>(iToRound) + static_cast<UnsignedT>(MULTIPLE) - 1) & ~(static_cast<UnsignedT>(MULTIPLE) - 1));
	}
	else
	{
		return static_cast<T>(((static_cast<UnsignedT>(iToRound) + static_cast<UnsignedT>(MULTIPLE) - 1) / static_cast<UnsignedT>(MULTIPLE)) * static_cast<UnsignedT>(MULTIPLE));
	}
}

template<std::integral T>
constexpr inline T RoundDown(T iToRound, T iMultiple)
{
	return (iToRound / iMultiple) * iMultiple;
}

template<std::floating_point T>
constexpr inline T RoundDown(T fToRound, T fMultiple)
{
	T fInverse = static_cast<T>(1.0) / fMultiple;
	return std::floor(fToRound * fInverse) / fInverse;
}

template <std::unsigned_integral T>
inline T FloatToUnsignedNormalizedInteger(float fValue)
{
	ASSERT(fValue >= 0.0f && fValue <= 1.0f);
	if constexpr (sizeof(T) <= 2)
	{
		// float represents max() exactly for 8/16-bit; preserves existing baked output bit-for-bit
		return static_cast<T>(static_cast<float>(std::numeric_limits<T>::max()) * fValue);
	}
	else
	{
		// 32/64-bit: static_cast<float>(max()) rounds up to 2^N and overflows the int cast at fValue == 1.0; scale in double
		return static_cast<T>(static_cast<double>(std::numeric_limits<T>::max()) * static_cast<double>(fValue));
	}
}

template <std::unsigned_integral T>
inline float UnsignedNormalizedIntegerToFloat(T uiValue)
{
	if constexpr (sizeof(T) <= 2)
	{
		return static_cast<float>(uiValue) / static_cast<float>(std::numeric_limits<T>::max());
	}
	else
	{
		return static_cast<float>(static_cast<double>(uiValue) / static_cast<double>(std::numeric_limits<T>::max()));
	}
}

// Frame-rate independent exponential decay factor using Padé (1,1) approximation
// Approximates exp(-fDecayRate * fDeltaTime) for finite positive scaled times
// Always returns [0, 1]: non-positive or NaN scaled time returns 1, positive infinity returns 0
// Fast (no transcendentals), accurate (~1% for x < 0.35, ~9% at x = 1.0)
// Usage: velocity *= ExponentialDecay(3.0f, fDeltaTime);
constexpr float ExponentialDecay(float fDecayRate, float fDeltaTime)
{
	float fScaledTime = fDecayRate * fDeltaTime;
	if (!(fScaledTime > 0.0f))
	{
		return 1.0f;
	}
	if (fScaledTime == std::numeric_limits<float>::infinity())
	{
		return 0.0f;
	}
	return std::max(0.0f, (2.0f - fScaledTime) / (2.0f + fScaledTime));
}

// Frame-rate independent interpolation factor using Padé (1,1) approximation
// Returns 1 - exp(-fRate * fDeltaTime) ≈ 2x / (2 + x) where x = fRate * fDeltaTime
// Use for "move toward target" operations (rotation, position lerp)
// Returns [0, 1] only for scaled time in [0, 2]; callers outside that domain must clamp the result
// Usage: direction = lerp(direction, target, ExponentialInterpolant(10.0f, fDeltaTime));
constexpr float ExponentialInterpolant(float fRate, float fDeltaTime)
{
	float fScaledTime = fRate * fDeltaTime;
	return (2.0f * fScaledTime) / (2.0f + fScaledTime);
}

struct SinCos
{
	float fSin = 0.0f;
	float fCos = 1.0f;
};

// Deterministic sin/cos for simulation / CRC-fed paths. libm std::sin/std::cos rounding differs
// across toolchains, so it must never feed CRC'd or client/server-shared state; XMScalarSinCos is
// a header-inline minimax polynomial (basic IEEE ops only), bit-identical under /fp:strict.
inline SinCos DeterministicSinCos(float fRadians)
{
	SinCos result;
	XMScalarSinCos(&result.fSin, &result.fCos, fRadians);
	return result;
}

template<typename... ARGS>
inline std::pair<XMVECTOR, XMVECTOR> XM_CALLCONV ComputeAxisAlignedBoundingBox(FXMVECTOR vecFirst, ARGS... vecRest)
{
	XMVECTOR vecMinimum = vecFirst;
	XMVECTOR vecMaximum = vecFirst;

	((vecMinimum = XMVectorMin(vecMinimum, vecRest), vecMaximum = XMVectorMax(vecMaximum, vecRest)), ...);

	return {vecMinimum, vecMaximum};
}

bool XM_CALLCONV AxisAlignedBoundingBoxIntersectsArea(XMFLOAT4 f4Area, FXMVECTOR vecMinimum, FXMVECTOR vecMaximum);

inline constexpr int64_t kiHexDirectionCount = 6;

// One edge neighbour of a flat-top hexagon on the axial lattice: the axial coordinate delta to the neighbour and the
// offset n from this hexagon's center to the neighbour's center. n is also the shared edge's normal: that edge is the
// line dot(p, n) = |n|^2 / 2.
struct HexDirection
{
	int32_t iDeltaQ = 0;
	int32_t iDeltaR = 0;
	XMFLOAT2 f2CenterOffset {};
};

// A flat-top regular hexagon tiling. The entries run clockwise from NE: NE (1,0), SE (1,-1), S (0,-1), SW (-1,0),
// NW (-1,1), N (0,1), so entry k+3 is entry k negated and entry k+1's offset minus entry k's is entry k+2's. The
// basis vectors are the center offsets one q step and one r step make; the half extents are the bounding
// rectangle's (circumradius, apothem).
struct HexDirections
{
	HexDirection pEntries[kiHexDirectionCount] {};
	XMFLOAT2 f2BasisQ {};
	XMFLOAT2 f2BasisR {};
	XMFLOAT2 f2HalfExtents {};
};

// fApothem is the center-to-edge distance. sqrt(3) is the only irrational factor, so the r basis vector, the N and S
// offsets, and every offset's y component are exact in float.
constexpr HexDirections XM_CALLCONV MakeHexDirections(float fApothem)
{
	XMFLOAT2 f2BasisQ(fApothem * std::numbers::sqrt3_v<float>, fApothem);
	XMFLOAT2 f2BasisR(0.0f, 2.0f * fApothem);
	auto Direction = [&](int32_t iDeltaQ, int32_t iDeltaR)
	{
		float fDeltaQ = static_cast<float>(iDeltaQ);
		float fDeltaR = static_cast<float>(iDeltaR);
		return HexDirection {.iDeltaQ = iDeltaQ, .iDeltaR = iDeltaR, .f2CenterOffset = XMFLOAT2(fDeltaQ * f2BasisQ.x + fDeltaR * f2BasisR.x, fDeltaQ * f2BasisQ.y + fDeltaR * f2BasisR.y)};
	};

	return HexDirections
	{
		.pEntries =
		{
			Direction(1, 0),
			Direction(1, -1),
			Direction(0, -1),
			Direction(-1, 0),
			Direction(-1, 1),
			Direction(0, 1),
		},
		.f2BasisQ = f2BasisQ,
		.f2BasisR = f2BasisR,
		.f2HalfExtents = XMFLOAT2(2.0f * fApothem / std::numbers::sqrt3_v<float>, fApothem),
	};
}

// Whether a position lies inside the hexagon centered at the origin, shrunk by fInset on every edge. Closed on the
// edges toward entries 1-3 (SE, S, SW) and open on 4, 5, and 0, so of two edge neighbours the one with the larger
// coordinate (larger r, or equal r and larger q) owns their shared edge, and exactly one of the three hexagons at a
// vertex owns it. A NaN or infinite position is outside.
bool XM_CALLCONV IsInsideHex(FXMVECTOR vecPosition, const HexDirections& rDirections, float fInset);

// The fraction of the segment from vecStart to vecEnd at which it leaves the hexagon centered at the origin: 0 when
// the start is outside IsInsideHex, otherwise a hit exactly when the end is outside, at the smallest edge crossing.
// A start on a closed edge moving outward leaves at 0.
std::optional<float> XM_CALLCONV TraceHexExit(FXMVECTOR vecStart, FXMVECTOR vecEnd, const HexDirections& rDirections);

// For a position outside IsInsideHex, the entry whose edge it crosses with the largest dot(position, offset), which is
// the nearest neighbour center among the crossed edges; an exact tie goes to the first such entry in table order.
int64_t XM_CALLCONV HexExitDirection(FXMVECTOR vecPosition, const HexDirections& rDirections);

// The offset from a hexagon's center to the center of the one at axial delta (iDeltaQ, iDeltaR), with W 0.
XMVECTOR XM_CALLCONV HexCenterOffset(int64_t iDeltaQ, int64_t iDeltaR, const HexDirections& rDirections);

// The same offset in double, from the float basis promoted to double, so a double absolute position moves by exactly
// the float offset a one-step delta gives.
std::pair<double, double> XM_CALLCONV HexCenterOffsetDouble(int64_t iDeltaQ, int64_t iDeltaR, const HexDirections& rDirections);

// The number of edge-neighbour steps an axial delta spans.
int64_t XM_CALLCONV HexGridDistance(int64_t iDeltaQ, int64_t iDeltaR);

// Whether an axis-aligned rectangle, packed (minX, maxY, maxX, minY) as AxisAlignedBoundingBoxIntersectsArea reads
// it, shares interior area with the hexagon centered at vecHexCenter; edge touching returns false.
bool XM_CALLCONV RectangleOverlapsHex(XMFLOAT4 f4Rectangle, FXMVECTOR vecHexCenter, const HexDirections& rDirections);

} // namespace common
