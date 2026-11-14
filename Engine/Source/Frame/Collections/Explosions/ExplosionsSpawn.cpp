#include "Explosions.h"

#if defined(BT_CLIENT)

#include "Data/Texture.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Frame/Collections/SmokeTrails/SmokeTrails.h"
#include "Frame/Collections/WindRadials/WindRadials.h"
#include "Graphics/Managers/ParticleManager.h"
#include "Ui/WrapperBase.h"

#include "Ui/SmokeWrappers.h"
#include "Ui/WindDepositsWrappers.h"

namespace engine
{

using enum ExplosionFlags;

// Forward declaration for shared helper (defined in Explosions.cpp)
void XM_CALLCONV SyncExplosionTrail(game::FrameInterpolate& rFrameInterpolate, smoke_trails_t trailId, FXMVECTOR vecPosition, float fIntensity);

bool ExplosionsPostRender::Spawn(game::Frame& __restrict rFrame, GridCoord coordinate, SimulationTime currentTime, const SpawnInfo& rSpawnInformation, common::RandomEngine& rRandomEngine)
{
	if (!engine::IsInsideCell(rSpawnInformation.vecPosition))
	{
		return false;
	}

	if (!gpCamera->InSpawnVisibleArea(coordinate, rSpawnInformation.vecPosition))
	{
		return false;
	}

	ExplosionsInterpolate& rInterpolate = rFrame.interpolate.explosions;
	ExplosionsPostRender& rPostRender = rFrame.postRender.explosions;

	common::ValidateVector<true >(rSpawnInformation.vecPosition);
	common::ValidateVector<false>(rSpawnInformation.vecDirection);

	const ExplosionType& rType = ExplosionsInterpolate::sTypes.at(static_cast<size_t>(rSpawnInformation.iTypeIndex));

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	int64_t iSpawnIndex = AddElement(rInterpolate, rPostRender);
	rPostRender.pLifecycleFlags[iSpawnIndex] = rSpawnInformation.lifecycleFlags;

	rInterpolate.puiTypeIndices[iSpawnIndex] = static_cast<uint8_t>(rSpawnInformation.iTypeIndex);
	rInterpolate.pStartTimes[iSpawnIndex] = currentTime;
	rInterpolate.pVecPositions[iSpawnIndex] = rSpawnInformation.vecPosition;
	rInterpolate.pVecDirections[iSpawnIndex] = rSpawnInformation.vecDirection;

	rInterpolate.pfTimePercents[iSpawnIndex] = rSpawnInformation.fTimePercent;

	rInterpolate.piTrailCounts[iSpawnIndex] = static_cast<int32_t>(std::min(rSpawnInformation.iTrailCount, kiMaxExplosionTrails));

	// Initialize trail arrays to invalid
	for (int64_t j = 0; j < kiMaxExplosionTrails; ++j)
	{
		rInterpolate.pTrailTimes[j][iSpawnIndex] = std::chrono::duration<float>::zero();
		rInterpolate.pTrails[j][iSpawnIndex] = smoke_trails_t {};
		rInterpolate.pfTrailIntensities[j][iSpawnIndex] = 0.0f;
		rInterpolate.pVecTrailStartPositions[j][iSpawnIndex] = XMVectorZero();
		rInterpolate.pVecTrailEndPositions[j][iSpawnIndex] = XMVectorZero();
	}

	float fPrimaryRotation = common::Random<XM_2PI>(rRandomEngine);

	// Fire-and-forget effects: Primary light
	if (rType.iPrimaryLightControllerTypeIndex != kiInvalidControllerType)
	{
		PointLightsPostRender::AddControlled(rFrame, coordinate, currentTime, rType.iPrimaryLightControllerTypeIndex, rSpawnInformation.vecPosition, fPrimaryRotation);
	}

	// Fire-and-forget effects: Primary puff
	if (rType.iPrimaryPuffControllerTypeIndex != kiInvalidControllerType)
	{
		PuffsPostRender::AddControlled(rFrame, coordinate, currentTime, rType.iPrimaryPuffControllerTypeIndex, rSpawnInformation.vecPosition);
	}

	// Fire-and-forget effects: Secondary explosions (staggered)
	int64_t iSecondaryExplosions = rType.iSecondaryExplosionCount;
	std::chrono::duration<float> durationDelayDelta = iSecondaryExplosions > 0 ? (0.75f * rSpawnInformation.fTimePercent * rType.primaryTime) / static_cast<float>(iSecondaryExplosions) : std::chrono::duration<float>::zero();
	std::chrono::duration<float> durationDelay = durationDelayDelta;

	for (int64_t k = 0; k < iSecondaryExplosions; ++k, durationDelay += durationDelayDelta)
	{
		XMVECTOR vecSecondaryOffset = XMVector3RotateSafe(XMVectorSet(rType.fSecondaryPositionMinimum + std::pow(rSpawnInformation.fSizePercent, 1.5f) * common::Random<1.0f>(rRandomEngine) * rType.fSecondaryPositionJitter, 0.0f, 0.0f, 0.0f), XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, common::Random<XM_2PI>(rRandomEngine)));
		XMVECTOR vecSecondaryPosition = XMVectorAdd(vecSecondaryOffset, rSpawnInformation.vecPosition);

		float fSecondaryRotation = common::Random<XM_2PI>(rRandomEngine);

		if (rType.iSecondaryLightControllerTypeIndex != kiInvalidControllerType)
		{
			PointLightsPostRender::AddControlled(rFrame, coordinate, currentTime + common::FloatSecondsToNanoseconds(durationDelay), rType.iSecondaryLightControllerTypeIndex, vecSecondaryPosition, fSecondaryRotation);
		}

		if (rType.iSecondaryPuffControllerTypeIndex != kiInvalidControllerType)
		{
			PuffsPostRender::AddControlled(rFrame, coordinate, currentTime + common::FloatSecondsToNanoseconds(durationDelay), rType.iSecondaryPuffControllerTypeIndex, vecSecondaryPosition);
		}
	}

	// Fire-and-forget wind deposit (radial, auto-expires)
	if (rType.iWindRadialControllerTypeIndex != kiInvalidControllerType)
	{
		float fWindSizePercent = std::sqrt(rSpawnInformation.fSizePercent);
		WindRadialsPostRender::AddControlled(rFrame, coordinate, currentTime, rType.iWindRadialControllerTypeIndex, rSpawnInformation.vecPosition, game::gWindDepositExplosionsIntensity.mfCurrent * fWindSizePercent, game::gWindDepositExplosionsWidth.mfCurrent * fWindSizePercent);
	}

	XMVECTOR vecDirection2dNormal = XMVector3Normalize(XMVectorMultiply(XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f), rSpawnInformation.vecDirection));
	int64_t iTrailCount = rInterpolate.piTrailCounts[iSpawnIndex];

	for (int64_t j = 0; j < iTrailCount; ++j)
	{
		std::chrono::duration<float> durationTrailTime = rSpawnInformation.fTimePercent * (rType.trailTimeMinimum + common::Random<1.0f>(rRandomEngine) * rType.trailTimeRandom);
		float fTrailIntensity = rSpawnInformation.fSmokePercent * (rType.fTrailIntensityMinimum + common::Random<1.0f>(rRandomEngine) * rType.fTrailIntensityRandom);

		XMVECTOR vecTrailDirection = vecDirection2dNormal;
		if (j != 0)
		{
			vecTrailDirection = common::RotateDirectionAboutZ(vecTrailDirection, rSpawnInformation.fTrailAngle * (common::Random(rRandomEngine) - 0.5f));
		}

		XMVECTOR vecTrailStart = XMVectorMultiplyAdd(vecTrailDirection, XMVectorReplicate(rType.fTrailStart), rSpawnInformation.vecPosition);
		float fTrailLength = rType.fTrailLengthMinimum + common::Random<1.0f>(rRandomEngine) * rType.fTrailLengthRandom;

		rInterpolate.pTrailTimes[j][iSpawnIndex] = durationTrailTime;

		// j == 0 is the central trail along the explosion direction; j > 0 are angle-jittered side trails
		bool bPrimary = (j == 0);
		float fLengthMultiplier = bPrimary ? game::gExplosionPrimaryTrailLength.mfCurrent : game::gExplosionSecondaryTrailLength.mfCurrent;
		float fDurationMultiplier = bPrimary ? game::gExplosionPrimaryTrailDuration.mfCurrent : game::gExplosionSecondaryTrailDuration.mfCurrent;
		fTrailIntensity *= bPrimary ? game::gExplosionPrimaryTrailIntensity.mfCurrent : game::gExplosionSecondaryTrailIntensity.mfCurrent;
		// Scaling the head's travel distance by Duration keeps head speed constant when Update later scales
		// pTrailTimes by the same Duration multiplier — so increasing Duration extends both space and time
		// in lockstep rather than slowing the head into the engine's smoke-decay window.
		fTrailLength *= fLengthMultiplier * fDurationMultiplier;

		XMVECTOR vecTrailEnd = XMVectorMultiplyAdd(vecTrailDirection, XMVectorReplicate(fTrailLength), rSpawnInformation.vecPosition);

		// Update fades the trail from its configured starting intensity.
		smoke_trails_t trailId;
		SmokeTrailsPostRender::Add(rFrame, trailId, ExplosionsInterpolate::siExplosionTrailTypeIndex);

		rInterpolate.pTrails[j][iSpawnIndex] = trailId;
		rInterpolate.pfTrailIntensities[j][iSpawnIndex] = fTrailIntensity;
		rInterpolate.pVecTrailStartPositions[j][iSpawnIndex] = vecTrailStart;
		rInterpolate.pVecTrailEndPositions[j][iSpawnIndex] = vecTrailEnd;

		// Sync trail after Add()
		SyncExplosionTrail(rFrame.interpolate, trailId, vecTrailStart, fTrailIntensity);
	}

	int64_t iTotalParticles = rSpawnInformation.iParticleCount + rType.iBaseParticleCount;

	// Per-type tweak multipliers (Particles tab).
	auto Scale = [](const Wrapper* pWrapper)
	{
		return pWrapper != nullptr ? pWrapper->mfCurrent : 1.0f;
	};
	float fPositionJitterScale         = Scale(rType.pParticlePositionJitterScale);
	float fVelocityBaseScale           = Scale(rType.pParticleVelocityBaseScale);
	float fVelocitySpreadScale         = Scale(rType.pParticleVelocitySpreadScale);
	float fVerticalVelocityBaseScale   = Scale(rType.pParticleVerticalVelocityBaseScale);
	float fVerticalVelocitySpreadScale = Scale(rType.pParticleVerticalVelocitySpreadScale);
	float fIntensitySpreadScale        = Scale(rType.pParticleIntensitySpreadScale);
	float fVisibleIntensityScale       = Scale(rType.pParticleVisibleIntensityScale);
	float fWidthScale                  = Scale(rType.pParticleWidthScale);
	float fLengthScale                 = Scale(rType.pParticleLengthScale);
	float fLengthSpreadScale           = Scale(rType.pParticleLengthSpreadScale);
	float fVelocityDecayScale          = Scale(rType.pParticleVelocityDecayScale);
	float fGravityScale                = Scale(rType.pParticleGravityScale);
	float fIntensityDecayScale         = Scale(rType.pParticleIntensityDecayScale);
	float fIntensityPowerScale         = Scale(rType.pParticleIntensityPowerScale);

	RenderBasis basis = MakeRenderBasis(coordinate, gpCamera->mBasisCoordinate);

	for (int64_t i = 0; i < iTotalParticles; ++i)
	{
		XMVECTOR vecParticlePosition = common::RandomPositionJitter(rSpawnInformation.vecPosition, rType.fParticlePositionJitter * fPositionJitterScale, rRandomEngine);

		float fVelocityMagnitude = rType.fParticleVelocityMinimum * fVelocityBaseScale + common::Random<1.0f>(rRandomEngine) * rType.fParticleVelocityRandom * fVelocitySpreadScale;
		XMVECTOR vecVelocity = XMVectorMultiply(XMVectorReplicate(fVelocityMagnitude), vecDirection2dNormal);
		vecVelocity = XMVector3RotateSafe(vecVelocity, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, -0.5f * rSpawnInformation.fParticleAngle + rSpawnInformation.fParticleAngle * common::Random(rRandomEngine)));
		vecVelocity = XMVectorSetZ(vecVelocity, rType.fParticleVerticalVelocityMinimum * fVerticalVelocityBaseScale + common::Random<1.0f>(rRandomEngine) * rType.fParticleVerticalVelocityRandom * fVerticalVelocitySpreadScale);
		XMFLOAT4A f4Velocity {};
		XMStoreFloat4A(&f4Velocity, vecVelocity);

		uint32_t uiParticleColor = rType.uiParticleColor;
		if (rSpawnInformation.flags & kYellow)
		{
			uiParticleColor |= ((100 + common::Random(25i64, rRandomEngine)) << 16) | ((common::Random(25i64, rRandomEngine)) << 8);
		}
		else if (rSpawnInformation.flags & kRed)
		{
			uiParticleColor |= ((50 + common::Random(25i64, rRandomEngine)) << 16) | ((common::Random(25i64, rRandomEngine)) << 8);
		}

		float fParticleIntensity = (rType.fParticleIntensityMinimum + common::Random<1.0f>(rRandomEngine) * rType.fParticleIntensityRandom * fIntensitySpreadScale) * fVisibleIntensityScale;

		float fLengthJitter = common::Random<1.0f>(rRandomEngine);

		if (!(rFrame.interpolate.frameFlags & FrameFlags::kRecalculated))
		{
			// ParticleManager::Spawn culls against the camera cell's visible area, so convert before the call.
			XMFLOAT4A f4Position {};
			XMStoreFloat4A(&f4Position, Rebase(basis, vecParticlePosition));
			ParticleManager::Spawn(gpParticleManager->mLongParticlesSpawnLayout,
			{
				.iColor = static_cast<int32_t>(uiParticleColor),
				.iColorEnd = static_cast<int32_t>(rType.uiParticleColorEnd),
				.fVelocityDecay = rType.fParticleVelocityDecay * fVelocityDecayScale,
				.fGravity = rType.fParticleGravity * fGravityScale,
				.fIntensityDecay = rType.fParticleIntensityDecay * fIntensityDecayScale,
				.fSize = rType.fParticleWidth * fWidthScale,
				.fLength = rType.fParticleLength * fLengthScale * (1.0f + fLengthJitter * fLengthSpreadScale),
				.fVisibleIntensity = fParticleIntensity,
				.fIntensityPower = rType.fParticleIntensityPower * fIntensityPowerScale,
				.f4Position = f4Position,
				.f4Velocity = f4Velocity,
			}, rType.particleCrc);
		}
	}

	return true;
}

} // namespace engine

#endif // BT_CLIENT
