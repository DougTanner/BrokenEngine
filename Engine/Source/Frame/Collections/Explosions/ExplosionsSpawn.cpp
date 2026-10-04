#include "Explosions.h"

#include "Ui/WrapperBase.h"

#if defined(BT_CLIENT)
#include "Data/Texture.h"
#include "Graphics/Managers/ParticleManager.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Frame/Collections/SmokeTrails/SmokeTrails.h"
#include "Frame/Collections/WindRadials/WindRadials.h"
#endif // BT_CLIENT

namespace engine
{

using enum ExplosionFlags;

#if defined(BT_CLIENT)

// Forward declaration for shared helper (defined in Explosions.cpp)
void XM_CALLCONV SyncExplosionTrail(game::FrameInterpolate& rFrameInterpolate, smoke_trails_t trailId, FXMVECTOR vecPosition, float fIntensity);

#endif // BT_CLIENT

bool ExplosionsPostRender::Spawn(game::Frame& __restrict rFrame, std::chrono::duration<float> currentTime, const SpawnInfo& rSpawnInformation)
{
	if (!common::InsideArea(rSpawnInformation.vecPosition, engine::LocalFrameArea()))
	{
		return false;
	}

	ExplosionsInterpolate& rInterpolate = rFrame.interpolate.explosions;
	ExplosionsPostRender& rPostRender = rFrame.postRender.explosions;

	common::ValidateVector<true >(rSpawnInformation.vecPosition);
	common::ValidateVector<false>(rSpawnInformation.vecDirection);

	const ExplosionType& rType = ExplosionsInterpolate::sTypes.at(rSpawnInformation.uiTypeIndex);

	GrowPairedCollections(rInterpolate, rPostRender, rInterpolate.Members(), rPostRender.Members());
	int64_t iSpawnIndex = AddElement(rInterpolate, rPostRender);

	rInterpolate.puiTypeIndices[iSpawnIndex] = rSpawnInformation.uiTypeIndex;
	rInterpolate.pFlags[iSpawnIndex] = rSpawnInformation.flags;
	rInterpolate.pfStartTimes[iSpawnIndex] = currentTime.count();
	rInterpolate.pVecPositions[iSpawnIndex] = rSpawnInformation.vecPosition;
	rInterpolate.pVecDirections[iSpawnIndex] = rSpawnInformation.vecDirection;

	rInterpolate.pfTimePercents[iSpawnIndex] = rSpawnInformation.fTimePercent;

	rInterpolate.piTrailCounts[iSpawnIndex] = static_cast<int32_t>(std::min(rSpawnInformation.uiTrailCount, static_cast<uint32_t>(kiMaxExplosionTrails)));

	// Initialize trail arrays to invalid
	for (int64_t j = 0; j < kiMaxExplosionTrails; ++j)
	{
		rInterpolate.pfTrailTimes[j][iSpawnIndex] = 0.0f;
#if defined(BT_CLIENT)
		rInterpolate.pTrails[j][iSpawnIndex] = smoke_trails_t {};
		rInterpolate.pfTrailIntensities[j][iSpawnIndex] = 0.0f;
		rInterpolate.pVecTrailStartPositions[j][iSpawnIndex] = XMVectorZero();
		rInterpolate.pVecTrailEndPositions[j][iSpawnIndex] = XMVectorZero();
#endif
	}

	// Consume random unconditionally to keep random engine in sync across client/server
	[[maybe_unused]] float fPrimaryRotation = common::Random<XM_2PI>(rFrame.postRender.randomEngine);

	// Fire-and-forget effects: Primary light
#if defined(BT_CLIENT)
	if (rType.uiPrimaryLightControllerTypeIndex != kuiInvalidControllerType)
	{
		PointLightsPostRender::AddControlled(rFrame, currentTime, rType.uiPrimaryLightControllerTypeIndex, rSpawnInformation.vecPosition, fPrimaryRotation);
	}
#endif

	// Fire-and-forget effects: Primary puff
#if defined(BT_CLIENT)
	if (rType.uiPrimaryPuffControllerTypeIndex != kuiInvalidControllerType)
	{
		PuffsPostRender::AddControlled(rFrame, currentTime, rType.uiPrimaryPuffControllerTypeIndex, rSpawnInformation.vecPosition);
	}
#endif

	// Fire-and-forget effects: Secondary explosions (staggered)
	int64_t iSecondaryExplosions = static_cast<int64_t>(rType.uiSecondaryExplosionCount);
	std::chrono::duration<float> durationDelayDelta(iSecondaryExplosions > 0 ? (0.75f * rSpawnInformation.fTimePercent * rType.fPrimaryTime) / static_cast<float>(iSecondaryExplosions) : 0.0f);
	std::chrono::duration<float> durationDelay = durationDelayDelta;

	for (int64_t k = 0; k < iSecondaryExplosions; ++k, durationDelay += durationDelayDelta)
	{
		XMVECTOR vecSecondaryOffset = XMVector3RotateSafe(XMVectorSet(rType.fSecondaryPositionMinimum + std::pow(rSpawnInformation.fSizePercent, 1.5f) * common::Random<1.0f>(rFrame.postRender.randomEngine) * rType.fSecondaryPositionJitter, 0.0f, 0.0f, 0.0f), XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, common::Random<XM_2PI>(rFrame.postRender.randomEngine)));
		[[maybe_unused]] XMVECTOR vecSecondaryPosition = XMVectorAdd(vecSecondaryOffset, rSpawnInformation.vecPosition);

		// Consume random unconditionally to keep random engine in sync across client/server
		[[maybe_unused]] float fSecondaryRotation = common::Random<XM_2PI>(rFrame.postRender.randomEngine);

#if defined(BT_CLIENT)
		if (rType.uiSecondaryLightControllerTypeIndex != kuiInvalidControllerType)
		{
			PointLightsPostRender::AddControlled(rFrame, std::chrono::duration<float>(currentTime.count() + durationDelay.count()), rType.uiSecondaryLightControllerTypeIndex, vecSecondaryPosition, fSecondaryRotation);
		}
#endif

#if defined(BT_CLIENT)
		if (rType.uiSecondaryPuffControllerTypeIndex != kuiInvalidControllerType)
		{
			PuffsPostRender::AddControlled(rFrame, std::chrono::duration<float>(currentTime.count() + durationDelay.count()), rType.uiSecondaryPuffControllerTypeIndex, vecSecondaryPosition);
		}
#endif
	}

	// Fire-and-forget wind deposit (radial, auto-expires)
#if defined(BT_CLIENT)
	if (rType.uiWindRadialControllerTypeIndex != kuiInvalidControllerType)
	{
		float fWindSizePercent = std::sqrt(rSpawnInformation.fSizePercent);
		WindRadialsPostRender::AddControlled(rFrame, currentTime.count(), rType.uiWindRadialControllerTypeIndex, rSpawnInformation.vecPosition, ExplosionsInterpolate::sTuning.pWindIntensity->mfCurrent * fWindSizePercent, ExplosionsInterpolate::sTuning.pWindWidth->mfCurrent * fWindSizePercent);
	}
#endif

	XMVECTOR vecDirection2dNormal = XMVector3Normalize(XMVectorMultiply(XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f), rSpawnInformation.vecDirection));
	int64_t iTrailCount = rInterpolate.piTrailCounts[iSpawnIndex];

	for (int64_t j = 0; j < iTrailCount; ++j)
	{
		std::chrono::duration<float> durationTrailTime(rSpawnInformation.fTimePercent * (rType.fTrailTimeMinimum + common::Random<1.0f>(rFrame.postRender.randomEngine) * rType.fTrailTimeRandom));
		[[maybe_unused]] float fTrailIntensity = rSpawnInformation.fSmokePercent * (rType.fTrailIntensityMinimum + common::Random<1.0f>(rFrame.postRender.randomEngine) * rType.fTrailIntensityRandom);

		XMVECTOR vecTrailDirection = vecDirection2dNormal;
		if (j != 0)
		{
			vecTrailDirection = XMVector3RotateSafe(vecTrailDirection, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, rSpawnInformation.fTrailAngle * (common::Random(rFrame.postRender.randomEngine) - 0.5f)));
		}

		[[maybe_unused]] XMVECTOR vecTrailStart = XMVectorMultiplyAdd(vecTrailDirection, XMVectorReplicate(rType.fTrailStart), rSpawnInformation.vecPosition);
		[[maybe_unused]] float fTrailLength = rType.fTrailLengthMinimum + common::Random<1.0f>(rFrame.postRender.randomEngine) * rType.fTrailLengthRandom;

		rInterpolate.pfTrailTimes[j][iSpawnIndex] = durationTrailTime.count();

#if defined(BT_CLIENT)
		// j == 0 is the central trail along the explosion direction; j > 0 are angle-jittered side trails
		bool bPrimary = (j == 0);
		float fLengthMultiplier = bPrimary ? ExplosionsInterpolate::sTuning.pPrimaryTrailLength->mfCurrent : ExplosionsInterpolate::sTuning.pSecondaryTrailLength->mfCurrent;
		float fDurationMultiplier = bPrimary ? ExplosionsInterpolate::sTuning.pPrimaryTrailDuration->mfCurrent : ExplosionsInterpolate::sTuning.pSecondaryTrailDuration->mfCurrent;
		fTrailIntensity *= bPrimary ? ExplosionsInterpolate::sTuning.pPrimaryTrailIntensity->mfCurrent : ExplosionsInterpolate::sTuning.pSecondaryTrailIntensity->mfCurrent;
		// Scaling the head's travel distance by Duration keeps head speed constant when Update later scales
		// pfTrailTimes by the same Duration multiplier — so increasing Duration extends both space and time
		// in lockstep rather than slowing the head into the engine's smoke-decay window.
		fTrailLength *= fLengthMultiplier * fDurationMultiplier;

		XMVECTOR vecTrailEnd = XMVectorMultiplyAdd(vecTrailDirection, XMVectorReplicate(fTrailLength), rSpawnInformation.vecPosition);

		// Update fades the trail from its configured starting intensity.
		smoke_trails_t trailId;
		SmokeTrailsPostRender::Add(rFrame, trailId, ExplosionsInterpolate::suiExplosionTrailTypeIndex);

		rInterpolate.pTrails[j][iSpawnIndex] = trailId;
		rInterpolate.pfTrailIntensities[j][iSpawnIndex] = fTrailIntensity;
		rInterpolate.pVecTrailStartPositions[j][iSpawnIndex] = vecTrailStart;
		rInterpolate.pVecTrailEndPositions[j][iSpawnIndex] = vecTrailEnd;

		// Sync trail after Add()
		SyncExplosionTrail(rFrame.interpolate, trailId, vecTrailStart, fTrailIntensity);
#endif // BT_CLIENT
	}

	int64_t iTotalParticles = rSpawnInformation.uiParticleCount + rType.uiBaseParticleCount;

	// Per-type tweak multipliers (Particles tab). Null on server, optional on client.
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
	[[maybe_unused]] float fVisibleIntensityScale = Scale(rType.pParticleVisibleIntensityScale);
	[[maybe_unused]] float fWidthScale          = Scale(rType.pParticleWidthScale);
	[[maybe_unused]] float fLengthScale         = Scale(rType.pParticleLengthScale);
	[[maybe_unused]] float fLengthSpreadScale   = Scale(rType.pParticleLengthSpreadScale);
	[[maybe_unused]] float fVelocityDecayScale  = Scale(rType.pParticleVelocityDecayScale);
	[[maybe_unused]] float fGravityScale        = Scale(rType.pParticleGravityScale);
	[[maybe_unused]] float fIntensityDecayScale = Scale(rType.pParticleIntensityDecayScale);
	[[maybe_unused]] float fIntensityPowerScale = Scale(rType.pParticleIntensityPowerScale);

	for (int64_t i = 0; i < iTotalParticles; ++i)
	{
		XMFLOAT4A f4Position {};
		XMVECTOR vecParticlePosition = common::RandomPositionJitter(rSpawnInformation.vecPosition, rType.fParticlePositionJitter * fPositionJitterScale, rFrame.postRender.randomEngine);
		XMStoreFloat4A(&f4Position, vecParticlePosition);

		float fVelocityMagnitude = rType.fParticleVelocityMinimum * fVelocityBaseScale + common::Random<1.0f>(rFrame.postRender.randomEngine) * rType.fParticleVelocityRandom * fVelocitySpreadScale;
		XMVECTOR vecVelocity = XMVectorMultiply(XMVectorReplicate(fVelocityMagnitude), vecDirection2dNormal);
		vecVelocity = XMVector3RotateSafe(vecVelocity, XMQuaternionRotationRollPitchYaw(0.0f, 0.0f, -0.5f * rSpawnInformation.fParticleAngle + rSpawnInformation.fParticleAngle * common::Random(rFrame.postRender.randomEngine)));
		vecVelocity = XMVectorSetZ(vecVelocity, rType.fParticleVerticalVelocityMinimum * fVerticalVelocityBaseScale + common::Random<1.0f>(rFrame.postRender.randomEngine) * rType.fParticleVerticalVelocityRandom * fVerticalVelocitySpreadScale);
		XMFLOAT4A f4Velocity {};
		XMStoreFloat4A(&f4Velocity, vecVelocity);

		[[maybe_unused]] uint32_t uiParticleColor = rType.uiParticleColor;
		if (rSpawnInformation.flags & kYellow)
		{
			uiParticleColor |= ((100 + common::Random(25u, rFrame.postRender.randomEngine)) << 16) | ((common::Random(25u, rFrame.postRender.randomEngine)) << 8);
		}
		else if (rSpawnInformation.flags & kRed)
		{
			uiParticleColor |= ((50 + common::Random(25u, rFrame.postRender.randomEngine)) << 16) | ((common::Random(25u, rFrame.postRender.randomEngine)) << 8);
		}

		[[maybe_unused]] float fParticleIntensity = (rType.fParticleIntensityMinimum + common::Random<1.0f>(rFrame.postRender.randomEngine) * rType.fParticleIntensityRandom * fIntensitySpreadScale) * fVisibleIntensityScale;

		// Random consumed unconditionally to keep stream in sync across builds.
		[[maybe_unused]] float fLengthJitter = common::Random<1.0f>(rFrame.postRender.randomEngine);

#if defined(BT_CLIENT)
		if (!(rFrame.interpolate.frameFlags & FrameFlags::kRecalculated))
		{
			ParticleManager::Spawn(gpParticleManager->mLongParticlesSpawnLayout,
			{
				.iColor = static_cast<int32_t>(uiParticleColor),
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
#endif // BT_CLIENT
	}

	return true;
}

} // namespace engine
