#include "Pch.h"

#include "Missiles.h"

#include "Data/Audio.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Collection.h"
#include "Frame/CellStaticData.h"

#include "Frame/HealthDamage.h"
#include "Profile/ProfileRows.h"
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
static int64_t siFleetExhaustAreaLightTypeIndex = 0xFF;
static int64_t siRandomExhaustAreaLightTypeIndex = 0xFF;

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

void XM_CALLCONV SynchronizeMissile(FrameInterpolate& rFrameInterpolate, engine::area_lights_t uiAreaLight, engine::smoke_trails_t uiSmokeTrail, engine::sound_t uiSound, FXMVECTOR vecPosition, FXMVECTOR vecDirection, FXMVECTOR vecVelocity, GXMVECTOR vecPreviousPosition, MissileFlags_t flags, engine::LifecycleFlags_t lifecycleFlags, float fPitch, [[maybe_unused]] float fDeltaRotation, float fExhaustLength)
{
	if ((uiAreaLight.uuid.iValue != 0) && !(lifecycleFlags & engine::LifecycleFlags::kDying) && !(flags & kFalling))
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
			.iTypeIndex = (flags & kTargetFleet) ? siRandomExhaustAreaLightTypeIndex : siFleetExhaustAreaLightTypeIndex,
			.pVecVisiblePositions = {vecTopLeft, vecTopRight, vecBottomLeft, vecBottomRight},
			.fIntensityMultiplier = fIntensityMultiplier,
		});
	}

	if ((uiSmokeTrail.uuid.iValue != 0) && !(flags & kFalling))
	{
		float fTrailOffset = kfTrailOffset;
		XMVECTOR vecTrailOffset = XMVectorMultiply(XMVectorReplicate(fTrailOffset), XMVector3Normalize(vecDirection));
		XMVECTOR vecTrailPosition = XMVectorAdd(vecPosition, (lifecycleFlags & engine::LifecycleFlags::kDying) ? XMVectorZero() : vecTrailOffset);
		SynchronizeMissileTrail(rFrameInterpolate, uiSmokeTrail, vecTrailPosition);
	}

	if ((uiSound.uuid.iValue != 0) && !(lifecycleFlags & engine::LifecycleFlags::kDying) && !(flags & kFalling))
	{
		engine::SoundsInterpolate::Sync(rFrameInterpolate, uiSound,
		{
			.vecPosition = vecPosition,
			.vecVelocity = vecVelocity,
			.uiCrc = data::kAudioMissile182794__qubodup__rocketlaunch_loopwavCrc,
			.fVolume = gMissileLoopVolume.mfCurrent,
			.fPitch = fPitch,
			.fadeOutTime = kMissileSoundFadeOutTime,
		});
	}
}
#endif // BT_CLIENT

#if defined(BT_CLIENT)
void MissilesInterpolate::ClientInitialize(Frame& rFrame, int64_t iIndex)
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

	if (rPostRender.pLifecycleFlags[iIndex] & engine::LifecycleFlags::kDestroy)
	{
		return;
	}

	int64_t iAreaLightType = (rPostRender.pFlags[iIndex] & kTargetRandom)
		? siFleetExhaustAreaLightTypeIndex : siRandomExhaustAreaLightTypeIndex;
	engine::AreaLightsPostRender::Add(rFrame, rMissiles.puiAreaLights[iIndex], iAreaLightType);

	engine::SmokeTrailsPostRender::Add(rFrame, rMissiles.puiSmokeTrails[iIndex], siSmokeTrailTypeIndex);

	engine::SoundsPostRender::Add(rFrame, rPostRender.puiSounds[iIndex]);

	SynchronizeMissile(rFrame.interpolate, rMissiles.puiAreaLights[iIndex], rMissiles.puiSmokeTrails[iIndex], rPostRender.puiSounds[iIndex], rMissiles.pVecPositions[iIndex], rMissiles.pVecDirections[iIndex], rPostRender.pVecVelocities[iIndex], rMissiles.pVecPositions[iIndex], rPostRender.pFlags[iIndex], rPostRender.pLifecycleFlags[iIndex], rPostRender.pfPitches[iIndex], rPostRender.pfDeltaRotations[iIndex], rPostRender.pfExhaustLengths[iIndex]);
}

void MissilesInterpolate::ClientInitializeAll(Frame& rFrame)
{
	MissilesInterpolate& rMissiles = *rFrame.interpolate.pMissiles;
	for (int64_t i = 0; i < rMissiles.iCount; ++i)
	{
		if (rMissiles.pDestroyedTimes[i].count() >= 0.0f)
		{
			continue;
		}
		ClientInitialize(rFrame, i);
	}
}
#endif // BT_CLIENT

void MissilesInterpolate::Register()
{
#if defined(BT_CLIENT)
	engine::AreaLightsInterpolate::RegisterType(siFleetExhaustAreaLightTypeIndex,
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

	engine::AreaLightsInterpolate::RegisterType(siRandomExhaustAreaLightTypeIndex,
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
		.iBaseParticleCount = kiMissileExplosionBaseParticleCount,
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
	static constexpr float kpfSizeMultipliers[] = {1.0f, 0.5f, 0.25f,};
	for (int64_t j = 0; float fSizeMultiplier : kpfSizeMultipliers)
	{
		float fScaledPercent = fPercent * fSizeMultiplier;
		XMVECTOR vecExplosionPosition = vecPosition;
		if (j > 0)
		{
			vecExplosionPosition = common::RandomPositionJitter<0.2f>(vecPosition, rFrame.postRender.randomEngine);
		}

		engine::ExplosionsPostRender::Spawn(rFrame, emitterCoordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick),
			{
				.iTypeIndex = siMissileExplosionTypeIndex,
				.vecPosition = vecExplosionPosition,
				.vecDirection = vecDirection,
				.flags = {engine::ExplosionFlags::kYellow},
				.lifecycleFlags = {engine::LifecycleFlags::kDying},
				.iTrailCount = static_cast<int64_t>(2.0f * fScaledPercent * (flags & kDirectional ? 0.6f : 1.0f) * kfExplosionTrailCountMinimum + kfExplosionTrailCountRandom * common::Random(rFrame.postRender.randomEngine)),
				.fTrailAngle = flags & kDirectional ? XM_PI : XM_2PI,
				.iParticleCount = static_cast<int64_t>(2.0f * fScaledPercent * kfExplosionParticleCount),
				.fParticleAngle = flags & kDirectional ? XM_PI : XM_2PI,
				.fSizePercent = 0.5f + 2.0f * fScaledPercent,
				.fSmokePercent = flags & kDirectional ? fScaledPercent : 0.5f * fScaledPercent,
				.fTimePercent = fScaledPercent,
			});
		++j;
	}
}

void MissilesPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kTransfer;
	}, [&](int64_t i)
	{
		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];
		XMVECTOR vecDirection = rCurrentInterpolate.pVecDirections[i];
		engine::AlignmentIdentifier alignment = rCurrentPostRender.pAlignments[i];
		MissileFlags_t missileFlags;
		missileFlags.Set(alignment == rFrame.postRender.fleetAlignment ? kTargetRandom : kTargetFleet);
		TransferRequest request
		{
			.change = MakeTransfer(SpawnInfo
			{
				.vecPosition = vecPosition,
				.vecDirection = vecDirection,
				.vecVelocity = rCurrentPostRender.pVecVelocities[i],
				.vecStoredDirection = vecDirection,
				.fAcceleration = rCurrentPostRender.pfAccelerations[i],
				.flags = missileFlags,
				.alignment = alignment,
				.deltaRotationDelay = rCurrentPostRender.pDeltaRotationDelays[i],
				.fDeltaRotation = rCurrentPostRender.pfDeltaRotations[i],
				.fDeltaRotationMaximum = rCurrentPostRender.pfDeltaRotationMaximum[i],
				.fPitch = rCurrentPostRender.pfPitches[i],
				.time = rCurrentPostRender.pTimes[i],
				.nextJitter = rCurrentPostRender.pNextJitter[i],
				.bTransfer = true,
			}),
		};
		if (!rFrame.postRender.PrepareTransferRequest(request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Missile Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Direction: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iQ, rStaticData.coordinate.iR, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDirection), rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, rFrame.postRender.transferRequests.size(), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		rFrame.postRender.PushTransferRequest(request);
	});
}

void MissilesPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && rCurrentInterpolate.pDestroyedTimes[i].count() <= 0.0f;
	}, [&](int64_t i)
	{
		rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDestroy);
	});
}

bool MissilesPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rSpawnInformation)
{
	if (!engine::IsInsideCell(rSpawnInformation.vecPosition))
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
	rCurrentInterpolate.pDestroyedTimes[iIndex] = std::chrono::duration<float>(-1.0f); // Sentinel: -1.0f = not exploding

	rCurrentPostRender.pVecVelocities[iIndex] = rSpawnInformation.vecVelocity;
	rCurrentPostRender.pVecExplosionDirections[iIndex] = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	rCurrentPostRender.pVecStoredDirections[iIndex] = rSpawnInformation.vecStoredDirection;
	rCurrentPostRender.puiRegistryTargets[iIndex] = rSpawnInformation.uiTarget;
	rCurrentPostRender.pTimes[iIndex] = rSpawnInformation.time;
	// An arrival restores the ramp verbatim — including the negative value of a finished ramp — and draws no fresh ramp delay.
	rCurrentPostRender.pDeltaRotationDelays[iIndex] = rSpawnInformation.bTransfer
		? rSpawnInformation.deltaRotationDelay
		: 0.5f * kMissileDeltaRotationDelay + std::chrono::duration<float>(common::Random<kMissileDeltaRotationDelay.count()>(rFrame.postRender.randomEngine));
	rCurrentPostRender.pfDeltaRotations[iIndex] = rSpawnInformation.fDeltaRotation;
	float fExhaustLength = (flags & kFalling) ? 0.0f : kfMissileExhaustLength + common::Random<kfMissileExhaustLengthRandom>(rFrame.postRender.randomEngine);
	rCurrentPostRender.pfExhaustLengths[iIndex] = fExhaustLength;
	rCurrentPostRender.pNextJitter[iIndex] = rSpawnInformation.nextJitter;
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
	MissilesInterpolate::ClientInitialize(rFrame, iIndex);
#endif // BT_CLIENT

	return true;
}

void MissilesPostRender::Fall(Frame& __restrict rFrame, int64_t i, std::chrono::duration<float> deltaTime)
{
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	ASSERT(!(rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying));
	ASSERT(!(rCurrentPostRender.pFlags[i] & kFalling));

	// The registry subscription is released by the Update caller, which is the only site holding a live query
	// window; the falling missile itself keeps no handle past that release.
	rCurrentPostRender.pFlags[i].Set(kFalling);
	rCurrentPostRender.pfDeltaRotations[i] = 0.0f;
	rCurrentPostRender.pVecVelocities[i] = XMVectorSetZ(rCurrentPostRender.pVecVelocities[i], XMVectorGetZ(rCurrentPostRender.pVecVelocities[i]) - kfMissileGravity * deltaTime.count());

#if defined(BT_CLIENT)
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.puiAreaLights[i]);
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.puiSmokeTrails[i]);
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentPostRender.puiSounds[i]);
#endif
}

void MissilesPostRender::Explode([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData, [[maybe_unused]] int64_t i, [[maybe_unused]] bool bDirectional)
{
	MissilesInterpolate& rCurrentInterpolate = *rFrame.interpolate.pMissiles;
	MissilesPostRender& rCurrentPostRender = *rFrame.postRender.pMissiles;

	ASSERT(!(rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying));
	// PostCollision runs after the Update query window is gone, so the handle is only cleared.
	rCurrentPostRender.puiRegistryTargets[i] = {};

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioExplosions80401__steveygos93__explosion2wavCrc, rStaticData.coordinate, rCurrentInterpolate.pVecPositions[i], gExplosionVolume.mfCurrent);
#endif

	rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDying);
	if (bDirectional)
	{
		rCurrentPostRender.pFlags[i].Set(kDirectional);
	}
	rCurrentPostRender.pVecExplosionDirections[i] = bDirectional ? engine::gpIslandTerrain->CellNormal(rStaticData, rCurrentInterpolate.pVecPositions[i]) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	rCurrentInterpolate.pDestroyedTimes[i] = kMissileDestroyTime;

#if defined(BT_CLIENT)
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.puiAreaLights[i]);
#endif

	// Remove sound when exploding (missile engine sound stops, replaced by explosion sound)
#if defined(BT_CLIENT)
	engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentPostRender.puiSounds[i]);
#endif

	SpawnMissileExplosion(rFrame, rStaticData.coordinate, 1.0f, rCurrentInterpolate.pVecPositions[i], rCurrentPostRender.pVecExplosionDirections[i], rCurrentPostRender.pFlags[i]);

	// Register area damage for the AreaDamage phase, tagged with the side that fired the missile
	engine::AreaDamage::Add(
	{
		.vecPosition = rCurrentInterpolate.pVecPositions[i],
		.fRadius = kfMissileDamageRadius,
		.fDamage = kfMissileDamage,
		.uiCategory = FiredBlastCategory(rCurrentPostRender.pAlignments[i], rFrame.postRender.fleetAlignment),
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
		bEqual &= common::LogDifference<"pDestroyedTimes">(i, pDestroyedTimes[i], rOther.pDestroyedTimes[i]);
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
		bEqual &= common::LogDifference<"pLifecycleFlags">(i, pLifecycleFlags[i], rOther.pLifecycleFlags[i]);
		bEqual &= common::LogDifference<"pFlags">(i, pFlags[i], rOther.pFlags[i]);
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pVecExplosionDirections">(i, pVecExplosionDirections[i], rOther.pVecExplosionDirections[i]);
		bEqual &= common::LogDifference<"pVecStoredDirections">(i, pVecStoredDirections[i], rOther.pVecStoredDirections[i]);
		bEqual &= common::LogDifference<"puiRegistryTargets">(i, puiRegistryTargets[i], rOther.puiRegistryTargets[i]);
		bEqual &= common::LogDifference<"pTimes">(i, pTimes[i], rOther.pTimes[i]);
		bEqual &= common::LogDifference<"pDeltaRotationDelays">(i, pDeltaRotationDelays[i], rOther.pDeltaRotationDelays[i]);
		bEqual &= common::LogDifference<"pfDeltaRotations">(i, pfDeltaRotations[i], rOther.pfDeltaRotations[i]);
		bEqual &= common::LogDifference<"pNextJitter">(i, pNextJitter[i], rOther.pNextJitter[i]);
		bEqual &= common::LogDifference<"pfDeltaRotationMaximum">(i, pfDeltaRotationMaximum[i], rOther.pfDeltaRotationMaximum[i]);
		bEqual &= common::LogDifference<"pfAccelerations">(i, pfAccelerations[i], rOther.pfAccelerations[i]);
		bEqual &= common::LogDifference<"pfPitches">(i, pfPitches[i], rOther.pfPitches[i]);
		bEqual &= common::LogDifference<"pfExhaustLengths">(i, pfExhaustLengths[i], rOther.pfExhaustLengths[i]);
		bEqual &= common::LogDifference<"pAlignments">(i, pAlignments[i], rOther.pAlignments[i]);
	}

	return bEqual;
}

} // namespace game
