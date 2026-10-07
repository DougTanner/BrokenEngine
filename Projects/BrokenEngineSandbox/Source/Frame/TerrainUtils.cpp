#include "TerrainUtils.h"

#include "Frame/CellStaticData.h"

namespace game
{

constexpr float kfPreferredElevation = 0.2f;
constexpr float kfElevationCorrectionStrength = 2.0f;
constexpr float kfSteerRate = 3.0f;
constexpr float kfLookAheadDistance = 20.0f;
constexpr float kfHighElevationThreshold = 0.5f;
constexpr float kfUrgentSteerMultiplier = 3.0f;
constexpr float kfMinimumGradientSquared = 0.0001f;
constexpr float kfReturnToIslandDistance = 150.0f;

AiSteeringResult XM_CALLCONV ComputeArtificialIntelligenceSteering(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecCurrentDirection, FXMVECTOR vecCellCenter, float fDeltaTime, bool bAlternateContour)
{
	XMVECTOR vecDirection = XMVector3Normalize(vecCurrentDirection);

	XMVECTOR vecNormal = engine::gpIslandTerrain->CellNormal(rStaticData, vecPosition);
	float fNormalX = XMVectorGetX(vecNormal);
	float fNormalY = XMVectorGetY(vecNormal);
	float fGradientSquared = fNormalX * fNormalX + fNormalY * fNormalY;

	float fLocalSteerRate = kfSteerRate;
	XMVECTOR vecDesiredDirection = XMVectorZero();

	if (fGradientSquared > kfMinimumGradientSquared)
	{
		// Contour direction: perpendicular to downhill gradient
		XMVECTOR vecContour = bAlternateContour
			? XMVectorSet(fNormalY, -fNormalX, 0.0f, 0.0f)
			: XMVectorSet(-fNormalY, fNormalX, 0.0f, 0.0f);

		// Elevation correction: push toward preferred elevation
		float fElevationArtificialIntelligence = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(vecPosition);
		float fElevationError = fElevationArtificialIntelligence - kfPreferredElevation;
		XMVECTOR vecCorrection = XMVectorScale(XMVectorSet(fNormalX, fNormalY, 0.0f, 0.0f), fElevationError * kfElevationCorrectionStrength);

		vecDesiredDirection = XMVector3Normalize(XMVectorAdd(vecContour, vecCorrection));

		XMVECTOR vecAhead = XMVectorAdd(vecPosition, XMVectorScale(vecDirection, kfLookAheadDistance));
		float fElevationAhead = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(vecAhead);
		if (fElevationAhead > kfHighElevationThreshold)
		{
			fLocalSteerRate *= kfUrgentSteerMultiplier;
		}
	}
	else
	{
		vecDesiredDirection = XMVector3Normalize(XMVectorSubtract(vecCellCenter, vecPosition));
	}

	if (common::Distance(vecPosition, vecCellCenter) > kfReturnToIslandDistance)
	{
		vecDesiredDirection = XMVector3Normalize(XMVectorSubtract(vecCellCenter, vecPosition));
		fLocalSteerRate = kfSteerRate * kfUrgentSteerMultiplier;
	}

	XMVECTOR vecArtificialIntelligenceDirection = XMVector3Normalize(XMVectorLerp(vecDirection, vecDesiredDirection, common::ExponentialInterpolant(fLocalSteerRate, fDeltaTime)));

	return {.vecArtificialIntelligenceDirection = vecArtificialIntelligenceDirection,};
}

constexpr int64_t kiFrontSamples = 4;
constexpr float kfFrontSamplesStep = 4.0f;
constexpr int64_t kiSideSamples = 2;
constexpr float kfSideSamplesStep = 2.0f;
constexpr float kfStepReduceWeight = 0.1f;
constexpr float kfAvoidTerrainMinimum = 0.5f;
constexpr float kfAvoidTerrainMaximum = 2.5f;
constexpr float kfAvoidTerrainDeltaAngleMinimum = 16.0f;
constexpr float kfAvoidTerrainDeltaAngleMaximum = 32.0f;
constexpr float kfDeltaAngleChangeAvoidTerrain = 0.995f;

float XM_CALLCONV ComputeTerrainAvoidance(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, FXMVECTOR vecDirection, float fCurrentDeltaRotation)
{
	// All samples use this cell's grid, so reuse one sampler across the nested loop.
	engine::CellElevationSampler sampler = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData);

	XMVECTOR vecLeftDirection = XMVector3Cross(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), vecDirection);
	float fLeftElevation = 0.0f;
	float fRightElevation = 0.0f;
	float fTotalWeight = 0.0f;

	for (int64_t j = 0; j < kiFrontSamples; ++j)
	{
		float fWeightFront = 1.0f - static_cast<float>(j) * kfStepReduceWeight;
		XMVECTOR vecSamplePosition = XMVectorMultiplyAdd(XMVectorReplicate(static_cast<float>(j + 1) * kfFrontSamplesStep), vecDirection, vecPosition);

		for (int64_t k = 0; k < kiSideSamples; ++k)
		{
			float fWeight = fWeightFront - static_cast<float>(k) * kfStepReduceWeight;
			fTotalWeight += fWeight;

			XMVECTOR vecSamplePositionLeft = XMVectorMultiplyAdd(XMVectorReplicate(static_cast<float>(k + 1) * kfSideSamplesStep), vecLeftDirection, vecSamplePosition);
			fLeftElevation += fWeight * sampler.Sample(vecSamplePositionLeft);

			XMVECTOR vecSamplePositionRight = XMVectorMultiplyAdd(XMVectorReplicate(static_cast<float>(k + 1) * -kfSideSamplesStep), vecLeftDirection, vecSamplePosition);
			fRightElevation += fWeight * sampler.Sample(vecSamplePositionRight);
		}
	}

	float fTotalWeightInverse = 1.0f / fTotalWeight;
	fLeftElevation *= fTotalWeightInverse;
	fRightElevation *= fTotalWeightInverse;

	if (fLeftElevation > kfAvoidTerrainMinimum || fRightElevation > kfAvoidTerrainMinimum)
	{
		float fPercent = fLeftElevation > fRightElevation ? (fLeftElevation - kfAvoidTerrainMinimum) / kfAvoidTerrainMaximum : (fRightElevation - kfAvoidTerrainMinimum) / kfAvoidTerrainMaximum;
		fPercent = std::clamp(fPercent, 0.0f, 1.0f);

		float fAvoidDeltaAngle = (1.0f - fPercent) * kfAvoidTerrainDeltaAngleMinimum + fPercent * kfAvoidTerrainDeltaAngleMaximum;
		float fWantedDeltaRotation = fLeftElevation > fRightElevation ? -fAvoidDeltaAngle : fAvoidDeltaAngle;
		fCurrentDeltaRotation = kfDeltaAngleChangeAvoidTerrain * fCurrentDeltaRotation + (1.0f - kfDeltaAngleChangeAvoidTerrain) * fWantedDeltaRotation;
	}

	return fCurrentDeltaRotation;
}

} // namespace game
