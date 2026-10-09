#include "Pch.h"

#include "Missiles.h"

#include "Data/Audio.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Collection.h"
#include "Frame/CellStaticData.h"

#include "Frame/HealthDamage.h"
#include "Profile/ProfileManager.h"
#include "Ui/ParticleWrappers.h"
#if defined(BT_CLIENT)
#include "Data/Scene.h"
#include "Data/Texture.h"
#include "Ui/LightingWrappers.h"
#include "Ui/SmokeWrappers.h"
#include "Ui/SoundWrappers.h"
#include "Ui/WrapperBase.h"
#endif

namespace engine
{
template struct Collection<game::MissilesInterpolate>;
template struct Collection<game::MissilesPostRender>;
} // namespace engine

namespace game
{

using enum MissileFlags;


#if defined(BT_CLIENT)
constexpr float kfExhaustWidth = 0.25f;
constexpr float kfExhaustOffset = -0.45f;
#endif // BT_CLIENT


#if defined(BT_CLIENT)
constexpr float kfTrailOffset = -0.9f;
constexpr float kfTrailWidth = 0.15f;
#endif

constexpr int64_t kiMissileExplosionBaseParticleCount = 15;
constexpr uint32_t kuiMissileExplosionParticleColor = 0xFF0000FF;
constexpr float kfMissileExplosionParticleVelocityMinimum = 1.0f;
constexpr float kfMissileExplosionParticleVelocityRandom = 4.0f;
constexpr float kfMissileExplosionParticleVerticalVelocityMinimum = -2.0f;
constexpr float kfMissileExplosionParticleVerticalVelocityRandom = 4.0f;
constexpr float kfMissileExplosionParticleIntensityDecay = 2.4f;
constexpr float kfExplosionParticleCount = 15.0f;
constexpr float kfExplosionTrailCountMinimum = 2.0f;
constexpr float kfExplosionTrailCountRandom = 2.0f;

// Long enough for a smooth per-frame SetVolume ramp (XAudio2 applies volume in discrete steps, so
// shorter fades get only a handful of audible stair-steps); the explosion one-shot masks the tail
constexpr std::chrono::duration<float> kMissileSoundFadeOutTime(0.15f);

constexpr float kfDeltaRotationLimitMinimum = 2.0f;
constexpr float kfDeltaRotationLimitRandom = 2.0f;

#if defined(BT_CLIENT)
static int64_t siPlayerExhaustAreaLightTypeIndex = 0xFF;
static int64_t siEnemyExhaustAreaLightTypeIndex = 0xFF;

static int64_t siSmokeTrailTypeIndex = 0xFF;
#endif // BT_CLIENT

static int64_t siMissileExplosionTypeIndex = 0xFF;

#if defined(BT_CLIENT)
void XM_CALLCONV SynchronizeMissileTrail(FrameInterpolate& rFrameInterpolate, engine::smoke_trails_t uiSmokeTrail, FXMVECTOR vecPosition)
{
	if (!(uiSmokeTrail.uuid.iValue != 0))
	{
		return;
	}

	engine::SmokeTrailsInterpolate::Sync(rFrameInterpolate, uiSmokeTrail,
	{
		.vecPosition = vecPosition,
		.fIntensity = gMissileTrailIntensity.mfCurrent,
	});
}

void XM_CALLCONV SynchronizeMissile(FrameInterpolate& rFrameInterpolate, engine::area_lights_t uiAreaLight, engine::smoke_trails_t uiSmokeTrail, engine::sound_t uiSound, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecVelocity, GXMVECTOR vecPreviousPosition, MissileFlags_t flags, float fPitch, [[maybe_unused]] float fDeltaRotation, float fExhaustLength)
{
	if ((uiAreaLight.uuid.iValue != 0) && !(flags & kExploding) && !(flags & kFalling))
	{
		float fLength = fExhaustLength;
		float fWidth = kfExhaustWidth;
		if ((rFrameInterpolate.iTick) % 2 == 0)
		{
			fWidth = -fWidth;
		}

		XMVECTOR vecExhaustOffset = XMVectorMultiply(XMVectorReplicate(kfExhaustOffset), XMVector3Normalize(vecDirection));
		XMVECTOR vecExhaustDirection = XMVector3Normalize(XMVectorAdd(vecDirection, XMVector3Normalize(XMVectorSubtract(vecPosition, vecPreviousPosition))));
		auto [vecTopLeft, vecTopRight, vecBottomLeft, vecBottomRight] = common::CalculateArea(XMVectorAdd(vecPosition, vecExhaustOffset), vecExhaustDirection, 0.0f, fLength, fWidth);

		// Intensity varies from 50% at min length to 100% at max length
		float fIntensityMultiplier = 0.5f + 0.5f * (fLength - kfMissileExhaustLength) / kfMissileExhaustLengthRandom;

		engine::AreaLightsInterpolate::Sync(rFrameInterpolate, uiAreaLight,
		{
			.iTypeIndex = (flags & kTargetPlayer) ? siEnemyExhaustAreaLightTypeIndex : siPlayerExhaustAreaLightTypeIndex,
			.vecVisiblePositions = {vecTopLeft, vecTopRight, vecBottomLeft, vecBottomRight},
			.fIntensityMultiplier = fIntensityMultiplier,
		});
	}

	if ((uiSmokeTrail.uuid.iValue != 0) && !(flags & kFalling))
	{
		float fTrailOffset = kfTrailOffset;
		XMVECTOR vecTrailOffset = XMVectorMultiply(XMVectorReplicate(fTrailOffset), XMVector3Normalize(vecDirection));
		XMVECTOR vecTrailPosition = XMVectorAdd(vecPosition, (flags & kExploding) ? XMVectorZero() : vecTrailOffset);
		SynchronizeMissileTrail(rFrameInterpolate, uiSmokeTrail, vecTrailPosition);
	}

	if ((uiSound.uuid.iValue != 0) && !(flags & kExploding) && !(flags & kFalling))
	{
		engine::SoundsInterpolate::Sync(rFrameInterpolate, uiSound,
		{
			.vecPosition = vecPosition,
			.vecVelocity = vecVelocity,
			.uiCrc = data::kAudioMissile182794__qubodup__rocketlaunch_loopwavCrc,
			.fVolume = gMissileLoopVolume.mfCurrent,
			.fPitch = fPitch,
			.fFadeOutTime = kMissileSoundFadeOutTime.count(),
		});
	}
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void MissilesInterpolate::ClientInit(Frame& rFrame, int64_t iIndex)
{
	MissilesInterpolate& rMissiles = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rPostRender = *rFrame.postRender.pMissiles;

	rMissiles.puiAreaLights[iIndex] = {};
	rMissiles.puiSmokeTrails[iIndex] = {};
	rPostRender.puiSounds[iIndex] = {};
	if (rPostRender.pFlags[iIndex] & kFalling)
	{
		return;
	}

	if (rPostRender.pFlags[iIndex] & kSilentDespawn)
	{
		return;
	}

	int64_t iAreaLightType = (rPostRender.pFlags[iIndex] & kTargetEnemy)
		? siPlayerExhaustAreaLightTypeIndex : siEnemyExhaustAreaLightTypeIndex;
	engine::AreaLightsPostRender::Add(rFrame, rMissiles.puiAreaLights[iIndex], iAreaLightType);

	engine::SmokeTrailsPostRender::Add(rFrame, rMissiles.puiSmokeTrails[iIndex], siSmokeTrailTypeIndex);

	engine::SoundsPostRender::Add(rFrame, rPostRender.puiSounds[iIndex]);

	SynchronizeMissile(rFrame.interpolate, rMissiles.puiAreaLights[iIndex], rMissiles.puiSmokeTrails[iIndex], rPostRender.puiSounds[iIndex], rMissiles.pVecPositions[iIndex], rMissiles.pVecDirections[iIndex], rPostRender.pVecVelocities[iIndex], rMissiles.pVecPositions[iIndex], rPostRender.pFlags[iIndex], rPostRender.pfPitches[iIndex], rPostRender.pfDeltaRotations[iIndex], rPostRender.pfExhaustLengths[iIndex]);
}

void MissilesInterpolate::ClientInitAll(Frame& rFrame)
{
	MissilesInterpolate& rMissiles = *rFrame.interpolate.pMissiles;
	for (int64_t i = 0; i < rMissiles.iCount; ++i)
	{
		if (rMissiles.pfDestroyedTimes[i] >= 0.0f)
		{
			continue;
		}
		ClientInit(rFrame, i);
	}
}
#endif // BT_CLIENT

void MissilesInterpolate::Register()
{
#if defined(BT_CLIENT)
	engine::AreaLightsInterpolate::RegisterType(siPlayerExhaustAreaLightTypeIndex,
	{
		.uiCrc = data::kTexturesMissilesBC73pngCrc,
		.puiColors = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		.pf2TextureCoordinates = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}},
		.fVisibleIntensity = gMissileExhaustVisibleIntensity.mfCurrent,
		.fLightingSize = gMissileExhaustLightingArea.mfCurrent,
		.fLightingIntensity = gMissileExhaustLightingIntensity.mfCurrent,
		.pVisibleIntensityWrapper = &gMissileExhaustVisibleIntensity,
		.pLightingSizeWrapper = &gMissileExhaustLightingArea,
		.pLightingIntensityWrapper = &gMissileExhaustLightingIntensity,
	});

	engine::AreaLightsInterpolate::RegisterType(siEnemyExhaustAreaLightTypeIndex,
	{
		.uiCrc = data::kTexturesMissilesBC71pngCrc,
		.puiColors = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		.pf2TextureCoordinates = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}},
		.fVisibleIntensity = gMissileExhaustVisibleIntensity.mfCurrent,
		.fLightingSize = gMissileExhaustLightingArea.mfCurrent,
		.fLightingIntensity = gMissileExhaustLightingIntensity.mfCurrent,
		.pVisibleIntensityWrapper = &gMissileExhaustVisibleIntensity,
		.pLightingSizeWrapper = &gMissileExhaustLightingArea,
		.pLightingIntensityWrapper = &gMissileExhaustLightingIntensity,
	});

	engine::SmokeTrailsInterpolate::RegisterType(siSmokeTrailTypeIndex,
	{
		.uiCrc = 0,
		.iColor = 0xFFFFFFFF,
		.fWidth = kfTrailWidth,
	});
#endif // BT_CLIENT

	engine::ExplosionsInterpolate::RegisterType(siMissileExplosionTypeIndex,
	{
#if defined(BT_CLIENT)
		.iPrimaryLightControllerTypeIndex = engine::ExplosionsInterpolate::suiPrimaryLightControllerTypeIndex,
		.iSecondaryLightControllerTypeIndex = engine::ExplosionsInterpolate::suiSecondaryLightControllerTypeIndex,
		.iPrimaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::suiPrimaryPuffControllerTypeIndex,
		.iSecondaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::suiSecondaryPuffControllerTypeIndex,
		.iTrailTypeIndex = engine::ExplosionsInterpolate::siExplosionTrailTypeIndex,
		.iWindRadialControllerTypeIndex = engine::ExplosionsInterpolate::suiWindRadialControllerTypeIndex,
#endif // BT_CLIENT
		.uiBaseParticleCount = static_cast<uint32_t>(kiMissileExplosionBaseParticleCount),
		.uiParticleColor = kuiMissileExplosionParticleColor,
		.fParticleVelocityMinimum = kfMissileExplosionParticleVelocityMinimum,
		.fParticleVelocityRandom = kfMissileExplosionParticleVelocityRandom,
		.fParticleVerticalVelocityMinimum = kfMissileExplosionParticleVerticalVelocityMinimum,
		.fParticleVerticalVelocityRandom = kfMissileExplosionParticleVerticalVelocityRandom,
		.fParticleIntensityDecay = kfMissileExplosionParticleIntensityDecay,
		.fSecondaryPositionJitter = 0.8f,
		.pParticleWidthScale = &gMissileExplosionParticleWidth,
		.pParticleLengthScale = &gMissileExplosionParticleLength,
		.pParticleLengthSpreadScale = &gMissileExplosionParticleLengthSpread,
		.pParticlePositionJitterScale = &gMissileExplosionParticlePositionJitter,
		.pParticleVelocityBaseScale = &gMissileExplosionParticleVelocityBase,
		.pParticleVelocitySpreadScale = &gMissileExplosionParticleVelocitySpread,
		.pParticleVerticalVelocityBaseScale = &gMissileExplosionParticleVerticalVelocityBase,
		.pParticleVerticalVelocitySpreadScale = &gMissileExplosionParticleVerticalVelocitySpread,
		.pParticleVelocityDecayScale = &gMissileExplosionParticleVelocityDecay,
		.pParticleGravityScale = &gMissileExplosionParticleGravity,
		.pParticleVisibleIntensityScale = &gMissileExplosionParticleVisibleIntensity,
		.pParticleIntensitySpreadScale = &gMissileExplosionParticleIntensitySpread,
		.pParticleIntensityDecayScale = &gMissileExplosionParticleIntensityDecay,
		.pParticleIntensityPowerScale = &gMissileExplosionParticleIntensityPower,
	});
}

static void SpawnMissileExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, float fPercent, XMVECTOR vecPosition, XMVECTOR vecDirection, MissileFlags_t flags)
{
	static constexpr float kfSizeMultipliers[] = {1.0f, 0.5f, 0.25f,};
	for (int64_t j = 0; float fSizeMultiplier : kfSizeMultipliers)
	{
		float fScaledPercent = fPercent * fSizeMultiplier;
		XMVECTOR vecExplosionPosition = vecPosition;
		if (j > 0)
		{
			vecExplosionPosition = common::RandomPositionJitter<0.2f>(vecPosition, rFrame.postRender.randomEngine);
		}

		engine::ExplosionsPostRender::Spawn(rFrame, emitterCoordinate, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime),
			{
				.iTypeIndex = siMissileExplosionTypeIndex,
				.vecPosition = vecExplosionPosition,
				.vecDirection = vecDirection,
				.flags = {engine::ExplosionFlags::kDestroysSelf, engine::ExplosionFlags::kYellow},
				.iTrailCount = static_cast<int64_t>(2.0f * fScaledPercent * (flags & kDirectional ? 0.6f : 1.0f) * kfExplosionTrailCountMinimum + kfExplosionTrailCountRandom * common::Random(rFrame.postRender.randomEngine)),
				.fTrailAngle = flags & kDirectional ? XM_PI : XM_2PI,
				.uiParticleCount = static_cast<uint32_t>(2.0f * fScaledPercent * kfExplosionParticleCount),
				.fParticleAngle = flags & kDirectional ? XM_PI : XM_2PI,
				.fSizePercent = 0.5f + 2.0f * fScaledPercent,
				.fSmokePercent = flags & kDirectional ? fScaledPercent : 0.5f * fScaledPercent,
				.fTimePercent = fScaledPercent,
			});
		++j;
	}
}

static void RemoveOwnedObjects([[maybe_unused]] Frame& rFrame, [[maybe_unused]] MissilesInterpolate& rCurrentInterpolate, MissilesPostRender& rCurrentPostRender, int64_t i)
{
#if defined(BT_CLIENT)
	if ((rCurrentInterpolate.puiAreaLights[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.areaLights, rFrame.postRender.areaLights, rCurrentInterpolate.puiAreaLights[i], rFrame.interpolate.areaLights.Members(), rFrame.postRender.areaLights.Members());
	}

	if ((rCurrentInterpolate.puiSmokeTrails[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.smokeTrails, rFrame.postRender.smokeTrails, rCurrentInterpolate.puiSmokeTrails[i], rFrame.interpolate.smokeTrails.Members(), rFrame.postRender.smokeTrails.Members());
	}
	if ((rCurrentPostRender.puiSounds[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.sounds, rFrame.postRender.sounds, rCurrentPostRender.puiSounds[i], rFrame.interpolate.sounds.Members(), rFrame.postRender.sounds.Members());
	}
#endif // BT_CLIENT

	// Removal runs outside any query window, so the handle is only cleared. Subscriber counts are derived afresh
	// from the surviving handles each window, so a cleared handle needs no release.
	rCurrentPostRender.puiRegistryTargets[i] = {};
}

void MissilesPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	engine::CellBounds bounds = engine::ComputeCellBounds(engine::LocalCellArea());

	for (int64_t i = rCurrentInterpolate.iCount - 1; i >= 0; --i)
	{
		if (!(rCurrentPostRender.pFlags[i] & kTransfer)) [[likely]]
		{
			continue;
		}

		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];

		TransferRequest request
		{
			.eType = StatusChangeType::kTransferMissile,
			.data =
			{
				.vecPosition = vecPosition,
				.vecDirection = rCurrentInterpolate.pVecDirections[i],
				.vecVelocity = rCurrentPostRender.pVecVelocities[i],
				.alignment = rCurrentPostRender.pAlignments[i],
				.fAcceleration = rCurrentPostRender.pfAccelerations[i],
				.deltaRotationDelaySeconds = std::chrono::duration<float>(rCurrentPostRender.pfDeltaRotationDelays[i]),
				.timeSeconds = std::chrono::duration<float>(rCurrentPostRender.pfTimes[i]),
				.nextJitterSeconds = std::chrono::duration<float>(rCurrentPostRender.pfNextJitter[i]),
				.fDeltaRotation = rCurrentPostRender.pfDeltaRotations[i],
				.fDeltaRotationMaximum = rCurrentPostRender.pfDeltaRotationMaximum[i],
				.fPitch = rCurrentPostRender.pfPitches[i],
			},
		};
		if (PrepareTransferRequest(rFrame.postRender, bounds, request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Missile Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Delta: ({},{}) Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iX, rStaticData.coordinate.iY, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDeltaX), static_cast<int32_t>(request.iDeltaY), rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, rFrame.postRender.transferRequests.size(), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		PushTransferRequest(rFrame.postRender, request);

		RemoveOwnedObjects(rFrame, rCurrentInterpolate, rCurrentPostRender, i);

		engine::DestroyElement(rCurrentInterpolate, rCurrentPostRender, i, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	}
}

void MissilesPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		bool bSilentDespawn = rCurrentPostRender.pFlags[i] & kSilentDespawn;
		bool bExplosionFinished = (rCurrentPostRender.pFlags[i] & kExploding) && rCurrentInterpolate.pfDestroyedTimes[i] <= 0.0f;
		return bSilentDespawn || bExplosionFinished;
	}, [&](int64_t i)
	{
		// Owned effects and the registry handle may already be released by Fall() or Explode()
		RemoveOwnedObjects(rFrame, rCurrentInterpolate, rCurrentPostRender, i);

		engine::DestroyElement(rCurrentInterpolate, rCurrentPostRender, i, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	});
}

bool MissilesPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rSpawnInformation)
{
	if (!common::InsideArea(rSpawnInformation.vecPosition, engine::LocalCellArea()))
	{
		return false;
	}

	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	common::ValidateVector<true >(rSpawnInformation.vecPosition);
	common::ValidateVector<false>(rSpawnInformation.vecDirection);
	common::ValidateVector<false>(rSpawnInformation.vecVelocity);
	common::ValidateVector<false>(rSpawnInformation.vecStoredDirection);

	engine::GrowPairedCollections(rCurrentInterpolate, rCurrentPostRender, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	int64_t iIndex = engine::AddElement(rCurrentInterpolate, rCurrentPostRender);

	rCurrentInterpolate.pVecPositions[iIndex] = rSpawnInformation.vecPosition;
	rCurrentInterpolate.pVecDirections[iIndex] = rSpawnInformation.vecDirection;
	MissileFlags_t flags = rSpawnInformation.flags;
	if (rSpawnInformation.time.count() >= kMissileLifetime.count())
	{
		flags.Set(kFalling);
	}
	rCurrentPostRender.pFlags[iIndex] = flags;
	rCurrentInterpolate.pfDestroyedTimes[iIndex] = -1.0f; // Sentinel: -1.0f = not exploding

	rCurrentPostRender.pVecVelocities[iIndex] = rSpawnInformation.vecVelocity;
	rCurrentPostRender.pVecExplosionDirections[iIndex] = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	rCurrentPostRender.pVecStoredDirections[iIndex] = rSpawnInformation.vecStoredDirection;
	rCurrentPostRender.puiRegistryTargets[iIndex] = rSpawnInformation.uiTarget;
	rCurrentPostRender.pfTimes[iIndex] = rSpawnInformation.time.count();
	// An arrival restores the ramp verbatim — including the negative value of a finished ramp — and draws no fresh ramp delay.
	rCurrentPostRender.pfDeltaRotationDelays[iIndex] = rSpawnInformation.bTransfer
		? rSpawnInformation.deltaRotationDelay.count()
		: 0.5f * kMissileDeltaRotationDelay.count() + common::Random<kMissileDeltaRotationDelay.count()>(rFrame.postRender.randomEngine);
	rCurrentPostRender.pfDeltaRotations[iIndex] = rSpawnInformation.fDeltaRotation;
	float fExhaustLength = (flags & kFalling) ? 0.0f : kfMissileExhaustLength + common::Random<kfMissileExhaustLengthRandom>(rFrame.postRender.randomEngine);
	rCurrentPostRender.pfExhaustLengths[iIndex] = fExhaustLength;
	rCurrentPostRender.pfNextJitter[iIndex] = rSpawnInformation.nextJitter.count();
	rCurrentPostRender.pfDeltaRotationMaximum[iIndex] = rSpawnInformation.bTransfer
		? rSpawnInformation.fDeltaRotationMaximum
		: (flags & kFalling) ? 0.0f : kfDeltaRotationLimitMinimum + common::Random<kfDeltaRotationLimitRandom>(rFrame.postRender.randomEngine);
	rCurrentPostRender.pfAccelerations[iIndex] = rSpawnInformation.fAcceleration;

	float fPitch = rSpawnInformation.bTransfer
		? rSpawnInformation.fPitch
		: (flags & kFalling) ? 0.0f : kfMissilePitchMinimum + common::Random<kfMissilePitchRandom>(rFrame.postRender.randomEngine);
	rCurrentPostRender.pfPitches[iIndex] = fPitch;
	rCurrentPostRender.pAlignments[iIndex] = rSpawnInformation.alignment;

	// Sync owned objects after Add()
#if defined(BT_CLIENT)
	MissilesInterpolate::ClientInit(rFrame, iIndex);
#endif // BT_CLIENT

	return true;
}

void MissilesPostRender::Fall(Frame& __restrict rFrame, int64_t i, std::chrono::duration<float> deltaTime)
{
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	ASSERT(!(rCurrentPostRender.pFlags[i] & kExploding));
	ASSERT(!(rCurrentPostRender.pFlags[i] & kFalling));

	// The registry subscription is released by the Update caller, which is the only site holding a live query
	// window; the falling missile itself keeps no handle past that release.
	rCurrentPostRender.pFlags[i].Set(kFalling);
	rCurrentPostRender.pfDeltaRotations[i] = 0.0f;
	rCurrentPostRender.pVecVelocities[i] = XMVectorSetZ(rCurrentPostRender.pVecVelocities[i], XMVectorGetZ(rCurrentPostRender.pVecVelocities[i]) - kfMissileGravity * deltaTime.count());

#if defined(BT_CLIENT)
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	if ((rCurrentInterpolate.puiAreaLights[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.areaLights, rFrame.postRender.areaLights, rCurrentInterpolate.puiAreaLights[i], rFrame.interpolate.areaLights.Members(), rFrame.postRender.areaLights.Members());
		rCurrentInterpolate.puiAreaLights[i] = {};
	}
	if ((rCurrentInterpolate.puiSmokeTrails[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.smokeTrails, rFrame.postRender.smokeTrails, rCurrentInterpolate.puiSmokeTrails[i], rFrame.interpolate.smokeTrails.Members(), rFrame.postRender.smokeTrails.Members());
		rCurrentInterpolate.puiSmokeTrails[i] = {};
	}
	if ((rCurrentPostRender.puiSounds[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.sounds, rFrame.postRender.sounds, rCurrentPostRender.puiSounds[i], rFrame.interpolate.sounds.Members(), rFrame.postRender.sounds.Members());
		rCurrentPostRender.puiSounds[i] = {};
	}
#endif
}

void MissilesPostRender::Explode([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData, [[maybe_unused]] int64_t i, [[maybe_unused]] bool bDirectional)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	ASSERT(!(rCurrentPostRender.pFlags[i] & kExploding));
	// PostCollision runs after the Update query window is gone, so the handle is only cleared.
	rCurrentPostRender.puiRegistryTargets[i] = {};

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioExplosions80401__steveygos93__explosion2wavCrc, rStaticData.coordinate, rCurrentInterpolate.pVecPositions[i], gExplosionVolume.mfCurrent);
#endif

	rCurrentPostRender.pFlags[i].Set(kExploding);
	if (bDirectional)
	{
		rCurrentPostRender.pFlags[i].Set(kDirectional);
	}
	rCurrentPostRender.pVecExplosionDirections[i] = bDirectional ? engine::gpIslandTerrain->CellNormal(rStaticData, rCurrentInterpolate.pVecPositions[i]) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	rCurrentInterpolate.pfDestroyedTimes[i] = kMissileDestroyTime.count();

#if defined(BT_CLIENT)
	if ((rCurrentInterpolate.puiAreaLights[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.areaLights, rFrame.postRender.areaLights, rCurrentInterpolate.puiAreaLights[i], rFrame.interpolate.areaLights.Members(), rFrame.postRender.areaLights.Members());
		rCurrentInterpolate.puiAreaLights[i] = {};
	}
#endif

	// Remove sound when exploding (missile engine sound stops, replaced by explosion sound)
#if defined(BT_CLIENT)
	if ((rCurrentPostRender.puiSounds[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.sounds, rFrame.postRender.sounds, rCurrentPostRender.puiSounds[i], rFrame.interpolate.sounds.Members(), rFrame.postRender.sounds.Members());
		rCurrentPostRender.puiSounds[i] = {};
	}
#endif

	SpawnMissileExplosion(rFrame, rStaticData.coordinate, 1.0f, rCurrentInterpolate.pVecPositions[i], rCurrentPostRender.pVecExplosionDirections[i], rCurrentPostRender.pFlags[i]);

	// Register area damage for the AreaDamage phase
	engine::AreaDamage::Add(
	{
		.vecPosition = rCurrentInterpolate.pVecPositions[i],
		.fRadius = kfMissileDamageRadius,
		.fDamage = kfMissileDamage,
		.uiCategory = CollisionCategory::kuiMissile,
	});
}

bool MissilesInterpolate::LogDifferences(const MissilesInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("MissilesInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
		bEqual &= common::LogDifference<"pfDestroyedTimes">(i, pfDestroyedTimes[i], rOther.pfDestroyedTimes[i]);
	}

	return bEqual;
}

bool MissilesPostRender::LogDifferences(const MissilesPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("MissilesPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pFlags">(i, pFlags[i], rOther.pFlags[i]);
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pVecExplosionDirections">(i, pVecExplosionDirections[i], rOther.pVecExplosionDirections[i]);
		bEqual &= common::LogDifference<"pVecStoredDirections">(i, pVecStoredDirections[i], rOther.pVecStoredDirections[i]);
		bEqual &= common::LogDifference<"puiRegistryTargets">(i, puiRegistryTargets[i], rOther.puiRegistryTargets[i]);
		bEqual &= common::LogDifference<"pfTimes">(i, pfTimes[i], rOther.pfTimes[i]);
		bEqual &= common::LogDifference<"pfDeltaRotationDelays">(i, pfDeltaRotationDelays[i], rOther.pfDeltaRotationDelays[i]);
		bEqual &= common::LogDifference<"pfDeltaRotations">(i, pfDeltaRotations[i], rOther.pfDeltaRotations[i]);
		bEqual &= common::LogDifference<"pfNextJitter">(i, pfNextJitter[i], rOther.pfNextJitter[i]);
		bEqual &= common::LogDifference<"pfDeltaRotationMax">(i, pfDeltaRotationMaximum[i], rOther.pfDeltaRotationMaximum[i]);
		bEqual &= common::LogDifference<"pfAccelerations">(i, pfAccelerations[i], rOther.pfAccelerations[i]);
		bEqual &= common::LogDifference<"pfPitches">(i, pfPitches[i], rOther.pfPitches[i]);
		bEqual &= common::LogDifference<"pfExhaustLengths">(i, pfExhaustLengths[i], rOther.pfExhaustLengths[i]);
		bEqual &= common::LogDifference<"pAlignments">(i, pAlignments[i], rOther.pAlignments[i]);
	}

	return bEqual;
}

} // namespace game
