#include "BeachPass.h"


struct BeachLeaf
{
	int64_t iWidth = 0;
	int64_t iHeight = 0;
	float fMetersPerTexel = 0.0f;
};

// One half-float step above the threshold: a texel at it could sink, since Terrain.vert compares an inexact LINEAR sample.
constexpr float kfHeightFloorMeters = common::kfUnderwaterMaskThresholdMeters + 1.0f / 512.0f;

constexpr float kfLandMeters = 0.1f;
constexpr float kfShelfBandMeters = 0.25f;
constexpr float kfGateStartMeters = 12.0f;
constexpr float kfGateEndMeters = 22.0f;
constexpr float kfPatchSoftness = 0.15f;

// fEdgeZero may exceed fEdgeOne, which reverses the ramp.
static float Smoothstep(float fEdgeZero, float fEdgeOne, float fValue)
{
	float fT = std::clamp((fValue - fEdgeZero) / (fEdgeOne - fEdgeZero), 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

// Odd remap with slope 1 + kfGain at Z=0 and identity from kfBandMeters out; it stays monotone while kfGain is below 4.63.
static void ApplyHeightCurve(std::span<float> elevation)
{
	static constexpr float kfBandMeters = 1.0f;
	static constexpr float kfGain = 3.0f;

	for (float& rfHeight : elevation)
	{
		float fT = std::abs(rfHeight) / kfBandMeters;
		if (fT >= 1.0f)
		{
			continue;
		}

		float fRemaining = 1.0f - fT;
		float fRemainingSquared = fRemaining * fRemaining;
		rfHeight = std::copysign(kfBandMeters * (fT + kfGain * fT * fRemainingSquared * fRemainingSquared), rfHeight);
	}
}

// Exact Euclidean distance in meters from each texel to the nearest texel at or above kfLandMeters (Felzenszwalb-Huttenlocher).
static std::vector<float> DistancesToLand(std::span<const float> elevation, const BeachLeaf& rLeaf)
{
	// Finite, because the parabola intersection subtracts two of these.
	static constexpr double kfNoLand = 1.0e20;

	std::vector<double> squaredDistances(elevation.size());
	for (int64_t i = 0; i < std::ssize(squaredDistances); ++i)
	{
		squaredDistances.at(i) = elevation[i] >= kfLandMeters ? 0.0 : kfNoLand;
	}

	int64_t iLongestLine = std::max(rLeaf.iWidth, rLeaf.iHeight);
	std::vector<double> line(static_cast<size_t>(iLongestLine));
	std::vector<int64_t> roots(static_cast<size_t>(iLongestLine));
	std::vector<double> boundaries(static_cast<size_t>(iLongestLine) + 1);
	auto TransformLine = [&](int64_t iStart, int64_t iStride, int64_t iCount)
	{
		for (int64_t i = 0; i < iCount; ++i)
		{
			line.at(i) = squaredDistances.at(iStart + i * iStride);
		}

		auto Intersection = [&line](int64_t iRight, int64_t iLeft) -> double
		{
			double fRight = static_cast<double>(iRight);
			double fLeft = static_cast<double>(iLeft);
			return (line.at(iRight) + fRight * fRight - line.at(iLeft) - fLeft * fLeft) / (2.0 * (fRight - fLeft));
		};

		// Lower envelope of the parabolas rooted at each position; roots[k] wins from boundaries[k] to boundaries[k + 1].
		int64_t iEnvelope = 0;
		roots.at(0) = 0;
		boundaries.at(0) = -std::numeric_limits<double>::infinity();
		boundaries.at(1) = std::numeric_limits<double>::infinity();
		for (int64_t i = 1; i < iCount; ++i)
		{
			double fBoundary = Intersection(i, roots.at(iEnvelope));
			while (fBoundary <= boundaries.at(iEnvelope))
			{
				--iEnvelope;
				fBoundary = Intersection(i, roots.at(iEnvelope));
			}
			++iEnvelope;
			roots.at(iEnvelope) = i;
			boundaries.at(iEnvelope) = fBoundary;
			boundaries.at(iEnvelope + 1) = std::numeric_limits<double>::infinity();
		}

		iEnvelope = 0;
		for (int64_t i = 0; i < iCount; ++i)
		{
			while (boundaries.at(iEnvelope + 1) < static_cast<double>(i))
			{
				++iEnvelope;
			}
			int64_t iRoot = roots.at(iEnvelope);
			double fOffset = static_cast<double>(i - iRoot);
			squaredDistances.at(iStart + i * iStride) = fOffset * fOffset + line.at(iRoot);
		}
	};
	for (int64_t i = 0; i < rLeaf.iHeight; ++i)
	{
		TransformLine(i * rLeaf.iWidth, 1, rLeaf.iWidth);
	}
	for (int64_t i = 0; i < rLeaf.iWidth; ++i)
	{
		TransformLine(i, rLeaf.iWidth, rLeaf.iHeight);
	}

	std::vector<float> distances(static_cast<size_t>(std::ssize(squaredDistances)));
	for (int64_t i = 0; i < std::ssize(distances); ++i)
	{
		distances.at(i) = static_cast<float>(std::sqrt(squaredDistances.at(i))) * rLeaf.fMetersPerTexel;
	}
	return distances;
}

// Lowers the coast toward a linear foreshore that joins a Dean profile with equal slope, measured from the land line so
// the flat shelf's noise does not define the shore. It only lowers, so bays keep their deep water and steep coast barely changes.
static void ApplyDistanceProfile(std::span<float> elevation, const BeachLeaf& rLeaf, std::span<const float> distances)
{
	static constexpr float kfForeshoreSlope = 0.15f;
	static constexpr float kfForeshoreSlopeVariation = 0.4f;
	static constexpr float kfSlopePeriodXMeters = 73.0f;
	static constexpr float kfSlopePeriodYMeters = 127.0f;
	static constexpr float kfForeshoreDropMeters = 0.6f;
	static constexpr float kfDeanScale = 0.5f;
	static constexpr float kfSmoothRadiusMeters = 0.15f;
	static constexpr float kfSmoothRampMeters = 2.0f;
	static constexpr float kfBlendStartMeters = 25.0f;
	static constexpr float kfBlendEndMeters = 45.0f;

	for (int64_t i = 0; i < rLeaf.iHeight; ++i)
	{
		float fY = (0.5f * static_cast<float>(rLeaf.iHeight) - static_cast<float>(i) - 0.5f) * rLeaf.fMetersPerTexel;
		for (int64_t j = 0; j < rLeaf.iWidth; ++j)
		{
			float& rfHeight = elevation[i * rLeaf.iWidth + j];
			if (rfHeight < kfHeightFloorMeters)
			{
				continue;
			}

			// Land has a distance of zero, where the profile at radius zero would flatten it to kfLandMeters.
			if (rfHeight >= kfLandMeters)
			{
				continue;
			}

			float fDistance = distances[i * rLeaf.iWidth + j];
			if (fDistance >= kfBlendEndMeters)
			{
				continue;
			}

			float fX = (static_cast<float>(j) + 0.5f - 0.5f * static_cast<float>(rLeaf.iWidth)) * rLeaf.fMetersPerTexel;
			float fSlope = kfForeshoreSlope * (1.0f + kfForeshoreSlopeVariation * std::sin(XM_2PI * fX / kfSlopePeriodXMeters + 1.0f) * std::sin(XM_2PI * fY / kfSlopePeriodYMeters + 2.0f));
			float fJoinMeters = kfForeshoreDropMeters / fSlope;
			float fProfile = kfLandMeters - fSlope * fDistance;
			if (fDistance > fJoinMeters)
			{
				// With offset = xs^(1/3), the Dean curve's slope at distance xs equals fSlope, which makes the join tangent.
				float fDeanOffset = 2.0f * kfDeanScale / (3.0f * fSlope);
				float fDeanOrigin = fDeanOffset * fDeanOffset * fDeanOffset;
				fProfile = kfLandMeters - kfForeshoreDropMeters - kfDeanScale * (std::pow(fDistance - fJoinMeters + fDeanOrigin, 2.0f / 3.0f) - fDeanOffset * fDeanOffset);
			}
			fProfile = std::max(fProfile, kfHeightFloorMeters);

			// Non-land texels sit at least one texel from land, so the radius is never zero.
			float fRadius = kfSmoothRadiusMeters * std::min(fDistance / kfSmoothRampMeters, 1.0f);
			float fOverlap = std::max(fRadius - std::abs(rfHeight - fProfile), 0.0f) / fRadius;
			float fSmoothMinimum = std::min(rfHeight, fProfile) - 0.25f * fRadius * fOverlap * fOverlap;
			float fBlended = fSmoothMinimum + Smoothstep(kfBlendStartMeters, kfBlendEndMeters, fDistance) * (rfHeight - fSmoothMinimum);
			rfHeight = std::max(fBlended, kfHeightFloorMeters);
		}
	}
}

// A low-frequency field in -1..1, independent of the coast.
static std::vector<float> PatchField(const BeachLeaf& rLeaf)
{
	struct Wave
	{
		float fDirectionX = 0.0f;
		float fDirectionY = 0.0f;
		float fWavelengthMeters = 0.0f;
		float fPhase = 0.0f;
	};

	static constexpr Wave kpWaves[] =
	{
		{.fDirectionX = 0.6f, .fDirectionY = 0.8f, .fWavelengthMeters = 83.0f, .fPhase = 1.0f},
		{.fDirectionX = -0.8f, .fDirectionY = 0.6f, .fWavelengthMeters = 109.0f, .fPhase = 2.5f},
		{.fDirectionX = 0.96f, .fDirectionY = 0.28f, .fWavelengthMeters = 143.0f, .fPhase = 4.0f},
	};

	std::vector<float> field(static_cast<size_t>(rLeaf.iWidth * rLeaf.iHeight));
	for (int64_t i = 0; i < rLeaf.iHeight; ++i)
	{
		float fY = (0.5f * static_cast<float>(rLeaf.iHeight) - static_cast<float>(i) - 0.5f) * rLeaf.fMetersPerTexel;
		for (int64_t j = 0; j < rLeaf.iWidth; ++j)
		{
			float fX = (static_cast<float>(j) + 0.5f - 0.5f * static_cast<float>(rLeaf.iWidth)) * rLeaf.fMetersPerTexel;
			float fField = 0.0f;
			for (const Wave& rWave : kpWaves)
			{
				fField += std::sin(XM_2PI * (rWave.fDirectionX * fX + rWave.fDirectionY * fY) / rWave.fWavelengthMeters + rWave.fPhase);
			}
			field.at(i * rLeaf.iWidth + j) = fField / static_cast<float>(std::ssize(kpWaves));
		}
	}
	return field;
}

// The mean weight over the shelf rises monotonically with the edge, so a fixed bisection is deterministic.
static float SolveBarFieldEdge(std::span<const float> shelfField)
{
	static constexpr int64_t kiBisectionCount = 24;

	auto MeanWeight = [&shelfField](float fEdge) -> double
	{
		double fSum = 0.0;
		for (float fField : shelfField)
		{
			fSum += Smoothstep(fEdge + kfPatchSoftness, fEdge - kfPatchSoftness, fField);
		}
		return fSum / static_cast<double>(std::ssize(shelfField));
	};

	double fTargetWeight = static_cast<double>(kfBeachBarCoveragePercent) / 100.0;
	float fLow = -1.0f - kfPatchSoftness;
	float fHigh = 1.0f + kfPatchSoftness;
	for (int64_t i = 0; i < kiBisectionCount; ++i)
	{
		float fMiddle = 0.5f * (fLow + fHigh);
		if (MeanWeight(fMiddle) < fTargetWeight)
		{
			fLow = fMiddle;
		}
		else
		{
			fHigh = fMiddle;
		}
	}
	return 0.5f * (fLow + fHigh);
}

void ApplyBeachPass(std::span<float> elevation, int64_t iWidth, int64_t iHeight, float fMetersPerTexel)
{
	BeachLeaf leaf {.iWidth = iWidth, .iHeight = iHeight, .fMetersPerTexel = fMetersPerTexel};

	std::vector<float> source(elevation.begin(), elevation.end());
	std::vector<float> curve = source;
	ApplyHeightCurve(curve);
	std::vector<float> distances = DistancesToLand(source, leaf);
	std::vector<float> field = PatchField(leaf);

	// The profile removes the shelf, and the curve then steepens what remains around Z=0.
	ApplyDistanceProfile(elevation, leaf, distances);
	ApplyHeightCurve(elevation);

	std::vector<float> shelfField;
	for (int64_t i = 0; i < std::ssize(source); ++i)
	{
		float fHeight = source.at(i);
		if (std::abs(fHeight) <= kfShelfBandMeters && distances.at(i) >= kfGateEndMeters)
		{
			shelfField.push_back(field.at(i));
		}
	}

	// Bars only lower source heights, so a leaf with no shallows has none to keep.
	if (shelfField.empty())
	{
		return;
	}

	float fFieldEdge = SolveBarFieldEdge(shelfField);
	for (int64_t i = 0; i < std::ssize(source); ++i)
	{
		float fBar = std::min(source.at(i), std::max(curve.at(i) - kfBeachBarDepthMeters, kfHeightFloorMeters));
		float fWeight = Smoothstep(kfGateStartMeters, kfGateEndMeters, distances.at(i)) * Smoothstep(fFieldEdge + kfPatchSoftness, fFieldEdge - kfPatchSoftness, field.at(i));
		elevation[i] = std::lerp(elevation[i], fBar, fWeight);
	}
}
