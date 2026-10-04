#include "Spaceships.h"

#include "Data/Audio.h"
#include "Data/Texture.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/Collection.h"
#include "Frame/FrameStaticData.h"
#include "Ui/WrapperBase.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/HealthDamage.h"
#include "Profile/ProfileManager.h"
#include "Ui/ParticleWrappers.h"
#include "Ui/WindDepositsWrappers.h"
#if defined(BT_CLIENT)
#include "Data/Scene.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Ui/LightingWrappers.h"
#include "Ui/SoundWrappers.h"
#endif

namespace engine
{
template struct Collection<game::SpaceshipsInterpolate>;
template struct Collection<game::SpaceshipsPostRender>;
} // namespace engine

namespace game
{

constexpr float kfDeathKnockbackSpeed = 20.0f;

using enum SpaceshipFlags;

constexpr float kfSpaceshipExplosionParticleCount = 8.0f;
constexpr float kfSpaceshipExplosionSizeStart = kfSpaceshipRadius * 0.625f;
constexpr float kfSpaceshipExplosionSizeEnd = kfSpaceshipRadius * 0.25f;
constexpr float kfSpaceshipExplosionSmoke = 0.5f;
constexpr float kfSpaceshipExplosionPositionJitter = kfSpaceshipRadius * 0.375f;
constexpr float kfSpaceshipExplosionDirectionJitter = 0.5f;
constexpr uint32_t kuiSpaceshipExplosionTrailCount = 5;

static void RegisterEnemyBlasterType();

// Shared type indices (accessible from SpaceshipsCombat.cpp via extern)
uint8_t guiSpaceshipExplosionTypeIndex = 0xFF;
#if defined(BT_CLIENT)
static uint8_t suiSpaceshipHitFlashTypeIndex = 0xFF;
uint8_t guiSpaceshipHitFlashControllerTypeIndex = 0xFF;

static void RegisterSpaceshipHitFlashEffect();
#endif

constexpr uint32_t kuiSpaceshipExplosionBaseParticleCount = 16;
constexpr uint32_t kuiSpaceshipExplosionParticleColor = 0xFF0000FF;
constexpr float kfSpaceshipExplosionParticleVelocityMinimum = 1.0f;
constexpr float kfSpaceshipExplosionParticleVelocityRandom = 9.0f;
constexpr float kfSpaceshipExplosionParticleVerticalVelocityMinimum = -5.0f;
constexpr float kfSpaceshipExplosionParticleVerticalVelocityRandom = 10.0f;
constexpr float kfSpaceshipExplosionParticleIntensityDecay = 2.4f;
constexpr float kfSpaceshipExplosionTrailLengthRandom = kfSpaceshipRadius * 1.25f;
constexpr uint32_t kuiSpaceshipExplosionSecondaryCount = 1;

constexpr float kfSpawnBlasterPlayerAngle = 0.1f;
constexpr float kfBlastersSpeed = 50.0f;
constexpr float kfBlastersSpawnCooldown = 1.0f;

constexpr float kfEnemyBlasterSize = kfSpaceshipRadius * 0.15f;
// Enemy blaster lighting tuning is supplied by LightingWrappers (gEnemyBlaster*).

#if defined(BT_CLIENT)
constexpr std::chrono::duration<float> kHitFlashDuration(0.3f);
#endif // BT_CLIENT

// Returns false when no player is both non-exploding and outside arrival grace.
[[nodiscard]] bool XM_CALLCONV NearestAlivePlayerPosition(const PlayersInterpolate& rPlayers, const PlayersPostRender& rPlayersPostRender, FXMVECTOR vecFrom, XMVECTOR& rVecResult)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	bool bFound = false;

	for (int64_t i = 0; i < rPlayers.iCount; ++i)
	{
		if (rPlayersPostRender.pFlags[i] & PlayerFlags::kExploding)
		{
			continue;
		}

		if (rPlayersPostRender.pfArrivalGracePeriods[i] > 0.0f)
		{
			continue;
		}

		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(rPlayers.pVecPositions[i], vecFrom)));
		if (fDistanceSquared < fClosestDistanceSquared)
		{
			fClosestDistanceSquared = fDistanceSquared;
			rVecResult = rPlayers.pVecPositions[i];
			bFound = true;
		}
	}

	return bFound;
}

void SpaceshipsInterpolate::Register()
{
	engine::ExplosionsInterpolate::RegisterType(guiSpaceshipExplosionTypeIndex,
	{
#if defined(BT_CLIENT)
		.uiPrimaryLightControllerTypeIndex = engine::ExplosionsInterpolate::suiPrimaryLightControllerTypeIndex,
		.uiSecondaryLightControllerTypeIndex = engine::ExplosionsInterpolate::suiSecondaryLightControllerTypeIndex,
		.uiPrimaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::suiPrimaryPuffControllerTypeIndex,
		.uiSecondaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::suiSecondaryPuffControllerTypeIndex,
		.uiTrailTypeIndex = engine::ExplosionsInterpolate::suiExplosionTrailTypeIndex,
		.uiWindRadialControllerTypeIndex = engine::ExplosionsInterpolate::suiWindRadialControllerTypeIndex,
#endif // BT_CLIENT
		.uiBaseParticleCount = kuiSpaceshipExplosionBaseParticleCount,
		.uiParticleColor = kuiSpaceshipExplosionParticleColor,
		.fParticleVelocityMinimum = kfSpaceshipExplosionParticleVelocityMinimum,
		.fParticleVelocityRandom = kfSpaceshipExplosionParticleVelocityRandom,
		.fParticleVerticalVelocityMinimum = kfSpaceshipExplosionParticleVerticalVelocityMinimum,
		.fParticleVerticalVelocityRandom = kfSpaceshipExplosionParticleVerticalVelocityRandom,
		.fParticleIntensityDecay = kfSpaceshipExplosionParticleIntensityDecay,
		.fTrailLengthRandom = kfSpaceshipExplosionTrailLengthRandom,
		.uiSecondaryExplosionCount = kuiSpaceshipExplosionSecondaryCount,
		.pParticleWidthScale = &gSpaceshipExplosionParticleWidth,
		.pParticleLengthScale = &gSpaceshipExplosionParticleLength,
		.pParticleLengthSpreadScale = &gSpaceshipExplosionParticleLengthSpread,
		.pParticlePositionJitterScale = &gSpaceshipExplosionParticlePositionJitter,
		.pParticleVelocityBaseScale = &gSpaceshipExplosionParticleVelocityBase,
		.pParticleVelocitySpreadScale = &gSpaceshipExplosionParticleVelocitySpread,
		.pParticleVerticalVelocityBaseScale = &gSpaceshipExplosionParticleVerticalVelocityBase,
		.pParticleVerticalVelocitySpreadScale = &gSpaceshipExplosionParticleVerticalVelocitySpread,
		.pParticleVelocityDecayScale = &gSpaceshipExplosionParticleVelocityDecay,
		.pParticleGravityScale = &gSpaceshipExplosionParticleGravity,
		.pParticleVisibleIntensityScale = &gSpaceshipExplosionParticleVisibleIntensity,
		.pParticleIntensitySpreadScale = &gSpaceshipExplosionParticleIntensitySpread,
		.pParticleIntensityDecayScale = &gSpaceshipExplosionParticleIntensityDecay,
		.pParticleIntensityPowerScale = &gSpaceshipExplosionParticleIntensityPower,
	});

	RegisterEnemyBlasterType();
#if defined(BT_CLIENT)
	RegisterSpaceshipHitFlashEffect();
#endif
}

static uint8_t suiEnemyBlasterPointLightTypeIndex = 0xFF;
static uint8_t suiEnemyBlasterTypeIndex = 0xFF;

static void RegisterEnemyBlasterType()
{
	if (suiEnemyBlasterTypeIndex != 0xFF)
	{
		return;
	}

#if defined(BT_CLIENT)
	engine::PointLightsInterpolate::RegisterType(suiEnemyBlasterPointLightTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC72pngCrc,
		.uiColor = 0xFFFFFFFF,
		.fVisibleArea = kfEnemyBlasterSize,
		.bCameraAligned = true,
		.pVisibleIntensityWrapper = &gEnemyBlasterVisibleIntensity,
		.pLightingAreaWrapper = &gEnemyBlasterLightingArea,
		.pLightingIntensityWrapper = &gEnemyBlasterLightingIntensity,
	});
#endif // BT_CLIENT

	BlastersInterpolate::RegisterType(suiEnemyBlasterTypeIndex,
	{
		.f2Size = {kfEnemyBlasterSize, kfEnemyBlasterSize},
		.uiPointLightTypeIndex = suiEnemyBlasterPointLightTypeIndex,
	});
}

// The registry binds this collection's position column directly, so its ID needs no per-tick sync.
void XM_CALLCONV SyncSpaceship(FrameInterpolate& rFrameInterpolate, engine::pusher_t uiPusher, FXMVECTOR vecPosition)
{
	engine::PushersInterpolate::Sync(rFrameInterpolate, uiPusher,
	{
		.vecPosition = vecPosition,
		.fRadius = kfSpaceshipPusherRadius,
		.fIntensity = kfSpaceshipPusherIntensity,
		.fPower = kfSpaceshipPusherPower,
		.flags = {engine::PusherFlags::kTypeDefault},
	});
}

#if defined(BT_CLIENT)
static void RegisterSpaceshipHitFlashEffect()
{
	if (suiSpaceshipHitFlashTypeIndex == 0xFF)
	{
		engine::PointLightsInterpolate::RegisterType(suiSpaceshipHitFlashTypeIndex,
		{
			.uiCrc = data::kTexturesBlasterBC74pngCrc,
			.uiColor = 0xFFFFFFFF,
		});

		engine::PointLightsInterpolate::RegisterControllerType(guiSpaceshipHitFlashControllerTypeIndex,
		{
			.uiBaseTypeIndex = suiSpaceshipHitFlashTypeIndex,
			.uiKeyframeCount = 2,
			.bDestroysSelf = true,
			.times = {std::chrono::duration<float>(0.0f), kHitFlashDuration, std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
			.keyframes =
			{
				{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
				{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
				{},
				{},
			},
			.ppVisibleAreaScales = {&gHitFlashVisibleAreaOne, &gHitFlashVisibleAreaTwo, nullptr, nullptr},
			.ppVisibleIntensityScales = {&gHitFlashVisibleIntensityOne, &gHitFlashVisibleIntensityTwo, nullptr, nullptr},
			.ppLightingAreaScales = {&gHitFlashLightingAreaOne, &gHitFlashLightingAreaTwo, nullptr, nullptr},
			.ppLightingIntensityScales = {&gHitFlashLightingIntensityOne, &gHitFlashLightingIntensityTwo, nullptr, nullptr},
		});
	}
}
#endif // BT_CLIENT

void SpawnSpaceshipExplosion(Frame& __restrict rFrame, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent)
{
	XMVECTOR vecJitteredPosition = common::RandomPositionJitter<kfSpaceshipExplosionPositionJitter>(vecPosition, rFrame.postRender.randomEngine);
	XMVECTOR vecJitteredDirection = common::RandomDirectionJitter<kfSpaceshipExplosionDirectionJitter>(vecDirection, rFrame.postRender.randomEngine);

	engine::ExplosionsPostRender::Spawn(rFrame, std::chrono::duration<float>(rFrame.interpolate.fCurrentTime),
	{
		.uiTypeIndex = guiSpaceshipExplosionTypeIndex,
		.vecPosition = vecJitteredPosition,
		.vecDirection = vecJitteredDirection,
		.flags = {engine::ExplosionFlags::kDestroysSelf, engine::ExplosionFlags::kRed},
		.uiTrailCount = kuiSpaceshipExplosionTrailCount,
		.fTrailAngle = fPercent * XM_PI,
		.uiParticleCount = static_cast<uint32_t>(fPercent * kfSpaceshipExplosionParticleCount),
		.fParticleAngle = fPercent * XM_PIDIV2,
		.fSizePercent = fPercent * kfSpaceshipExplosionSizeStart + (1.0f - fPercent) * kfSpaceshipExplosionSizeEnd,
		.fSmokePercent = fPercent * kfSpaceshipExplosionSmoke,
		.fTimePercent = fPercent,
	});
}

#if defined(BT_CLIENT)
// Spaceship model (defined in SpaceshipsRender.cpp)
extern const common::crc_t kuiSpaceshipModel;
#endif

void SpaceshipsInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rCurrentFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateUpdateSpaceships);

	SpaceshipsInterpolate& rCurrent = *rCurrentFrameInterpolate.pSpaceships;
	const SpaceshipsInterpolate& rPrevious = *rPreviousFrame.interpolate.pSpaceships;
	const SpaceshipsPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pSpaceships;
	float fDeltaTime = rCurrentFrameInterpolate.fDeltaTime;

#if defined(BT_CLIENT)
	float fAnimationDuration = 0.0f;
	if (engine::gAnimationDataMap.contains(kuiSpaceshipModel))
	{
		fAnimationDuration = engine::gAnimationDataMap.at(kuiSpaceshipModel).mpAnimations[0].fDuration;
	}
#endif

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		float fDestroyedTime = rPrevious.pfDestroyedTimes[i];
		float fDeltaRotation = rPrevious.pfDeltaRotations[i];
#if defined(BT_CLIENT)
		float fAnimationTime = rPrevious.pfAnimationTimes[i];
#endif

		vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime), rPreviousPostRender.pVecVelocities[i], vecPosition);
		// Enforce W=1.0 — prevents drift via MultiplyAdd's 4-lane propagation.
		vecPosition = XMVectorSetW(vecPosition, 1.0f);

		vecDirection = XMVector3Normalize(XMVector4Transform(vecDirection, XMMatrixRotationZ(fDeltaTime * fDeltaRotation)));

		// The -1.0f sentinel means not exploding and must remain unchanged.
		if (fDestroyedTime > 0.0f)
		{
			fDestroyedTime = std::max(fDestroyedTime - fDeltaTime, 0.0f);
		}

#if defined(BT_CLIENT)
		if (fAnimationDuration > 0.0f)
		{
			fAnimationTime += fDeltaTime;
			if (fAnimationTime >= fAnimationDuration)
			{
				fAnimationTime = std::fmod(fAnimationTime, fAnimationDuration);
			}
		}
#endif // BT_CLIENT

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pfDestroyedTimes[i] = fDestroyedTime;
		rCurrent.pfDeltaRotations[i] = fDeltaRotation;
#if defined(BT_CLIENT)
		rCurrent.pfAnimationTimes[i] = fAnimationTime;
#endif

		// Sync owned objects (IDs copied in AllocateAndCopy)
		SyncSpaceship(rCurrentFrameInterpolate, rCurrent.puiPushers[i], vecPosition);

#if defined(BT_CLIENT)
		if ((rCurrent.puiWindTrails[i].uuid.iValue != 0))
		{
			engine::WindTrailsInterpolate::Sync(rCurrentFrameInterpolate, rCurrent.puiWindTrails[i],
			{
				.vecPosition = vecPosition,
				.fIntensity = game::gWindDepositSpaceshipsIntensity.mfCurrent,
				.fWidth = game::gWindDepositSpaceshipsWidth.mfCurrent,
				.fLengthMultiplier = game::gWindDepositSpaceshipsLengthMultiplier.mfCurrent,
			});
		}
#endif // BT_CLIENT
	}
}

void SpaceshipsInterpolate::AllocateAndCopy(SpaceshipsInterpolate& rCurrent, const SpaceshipsInterpolate& rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateAllocateAndCopySpaceships);
	engine::AllocateAndCopyMembers(rCurrent, rPrevious);
}

void SpaceshipsPostRender::AllocateAndCopy(SpaceshipsPostRender& rCurrent, const SpaceshipsPostRender& rPrevious)
{
	engine::AllocateAndCopyMembers(rCurrent, rPrevious);
}

static void RemoveOwnedObjects(Frame& rFrame, SpaceshipsInterpolate& rCurrentInterpolate, int64_t i, bool bClearRegistryId)
{
	if (bClearRegistryId)
	{
		rCurrentInterpolate.puiRegistryIds[i] = {};
	}
	engine::PushersPostRender::Remove(rFrame, rCurrentInterpolate.puiPushers[i]);
#if defined(BT_CLIENT)
	if ((rCurrentInterpolate.puiWindTrails[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.windTrails, rFrame.postRender.windTrails, rCurrentInterpolate.puiWindTrails[i], rFrame.interpolate.windTrails.Members(), rFrame.postRender.windTrails.Members());
	}
#endif
}

void SpaceshipsPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	engine::FrameBounds bounds = engine::ComputeFrameBounds(engine::LocalFrameArea());

	for (int64_t i = rCurrentInterpolate.iCount - 1; i >= 0; --i)
	{
		if (!(rCurrentPostRender.pFlags[i] & kTransfer)) [[likely]]
		{
			continue;
		}

		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];

		TransferRequest request
		{
			.eType = StatusChangeType::kTransferSpaceship,
			.data =
			{
				.vecPosition = vecPosition,
				.vecDirection = rCurrentInterpolate.pVecDirections[i],
				.vecVelocity = rCurrentPostRender.pVecVelocities[i],
				.alignment = rCurrentPostRender.pAlignments[i],
				.fHealth = rCurrentPostRender.pfHealths[i],
				.nextBlasterSpawnTimeSeconds = std::chrono::duration<float>(rCurrentPostRender.pfNextBlasterSpawnTimes[i]),
				.fDeltaRotation = rCurrentInterpolate.pfDeltaRotations[i],
			},
		};
		if (PrepareTransferRequest(rFrame.postRender, bounds, request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Spaceship Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Delta: ({},{}) Health: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iX, rStaticData.coordinate.iY, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDeltaX), static_cast<int32_t>(request.iDeltaY), common::Wb(rCurrentPostRender.pfHealths[i], 1), rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, rFrame.postRender.transferRequests.size(), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		PushTransferRequest(rFrame.postRender, request);

		RemoveOwnedObjects(rFrame, rCurrentInterpolate, i, true);

		engine::DestroyElement(rCurrentInterpolate, rCurrentPostRender, i, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	}
}

void SpaceshipsPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return (rCurrentPostRender.pFlags[i] & kExploding) && !(rCurrentInterpolate.pfDestroyedTimes[i] > 0.0f);
	}, [&](int64_t i)
	{
		RemoveOwnedObjects(rFrame, rCurrentInterpolate, i, false);

		engine::DestroyElement(rCurrentInterpolate, rCurrentPostRender, i, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	});
}

void SpaceshipsPostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	const PlayersInterpolate& rPlayers = *rFrame.interpolate.pPlayers;
	const PlayersPostRender& rPlayersPostRender = *rFrame.postRender.pPlayers;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if ((rCurrentPostRender.pFlags[i] & kExploding) && rCurrentPostRender.pfDestroyedExplosionTimes[i] <= 0.0f)
		{
			rCurrentPostRender.pfDestroyedExplosionTimes[i] = kfSpaceshipDestroyExplosionInterval;

			float fPercent = rCurrentInterpolate.pfDestroyedTimes[i] / kfSpaceshipDestroyTime;
			XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
			SpawnSpaceshipExplosion(rFrame, rCurrentInterpolate.pVecPositions[i], vecDirection, fPercent);
			continue;
		}

		if (rCurrentPostRender.pFlags[i] & kExploding)
		{
			continue;
		}

		if (rCurrentPostRender.pfArrivalGracePeriods[i] > 0.0f)
		{
			continue;
		}

		XMVECTOR vecNearestPlayer = XMVectorZero();
		if (!NearestAlivePlayerPosition(rPlayers, rPlayersPostRender, rCurrentInterpolate.pVecPositions[i], vecNearestPlayer))
		{
			continue;
		}

		if (!FrameInterpolate::IsVisible(vecNearestPlayer, rCurrentInterpolate.pVecPositions[i]))
		{
			continue;
		}

		XMVECTOR vecToPlayer = XMVectorSubtract(vecNearestPlayer, rCurrentInterpolate.pVecPositions[i]);
		XMVECTOR vecToPlayerNormal = XMVector3Normalize(vecToPlayer);
		float fAngleToPlayer = XMVectorGetX(XMVector3AngleBetweenNormals(rCurrentInterpolate.pVecDirections[i], vecToPlayerNormal));

		bool bSpawnBlaster = fAngleToPlayer <= kfSpawnBlasterPlayerAngle;

		if (rCurrentPostRender.pfNextBlasterSpawnTimes[i] < 0.0f && bSpawnBlaster)
		{
			rCurrentPostRender.pfNextBlasterSpawnTimes[i] = kfBlastersSpawnCooldown;

			XMVECTOR vecDirection = rCurrentInterpolate.pVecDirections[i];
			XMVECTOR vecBlasterVelocity = XMVectorScale(vecDirection, kfBlastersSpeed);
			XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];

			BlastersPostRender::Spawn(rFrame,
			{
				.vecPosition = vecPosition,
				.vecVelocity = vecBlasterVelocity,
				.uiTypeIndex = suiEnemyBlasterTypeIndex,
				.flags = {},
				.alignment = rCurrentPostRender.pAlignments[i],
				.fWindTrailIntensity = game::gWindDepositSpaceshipsBlastersIntensity.mfCurrent,
				.fWindTrailWidth = game::gWindDepositSpaceshipsBlastersWidth.mfCurrent,
				.fWindTrailLengthMultiplier = game::gWindDepositSpaceshipsBlastersLengthMultiplier.mfCurrent,
			});

#if defined(BT_CLIENT)
			engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster514039__newlocknew__blastershot6sytrusrsmplmultiprcsngsinglewavCrc, rStaticData.coordinate, vecPosition, gEnemyBlasterVolume.mfCurrent, gEnemyBlasterPitchMinimum.mfCurrent, gEnemyBlasterPitchRandom.mfCurrent);
#endif
		}
	}
}

#if defined(BT_CLIENT)
void SpaceshipsInterpolate::ClientInitialize(Frame& rFrame, int64_t iIndex)
{
	SpaceshipsInterpolate& rSpaceships = *rFrame.interpolate.pSpaceships;

	rSpaceships.puiWindTrails[iIndex] = {};
	engine::WindTrailsPostRender::Add(rFrame, rSpaceships.puiWindTrails[iIndex]);

	engine::WindTrailsInterpolate::Sync(rFrame.interpolate, rSpaceships.puiWindTrails[iIndex],
	{
		.vecPosition = rSpaceships.pVecPositions[iIndex],
		.fIntensity = game::gWindDepositSpaceshipsIntensity.mfCurrent,
		.fWidth = game::gWindDepositSpaceshipsWidth.mfCurrent,
		.fLengthMultiplier = game::gWindDepositSpaceshipsLengthMultiplier.mfCurrent,
	});
}

void SpaceshipsInterpolate::ClientInitializeAll(Frame& rFrame)
{
	SpaceshipsInterpolate& rSpaceships = *rFrame.interpolate.pSpaceships;
	for (int64_t i = 0; i < rSpaceships.iCount; ++i)
	{
		if (rSpaceships.pfDestroyedTimes[i] >= 0.0f)
		{
			continue;
		}
		ClientInitialize(rFrame, i);
	}
}
#endif // BT_CLIENT

bool SpaceshipsPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo)
{
	if (!common::InsideArea(rInfo.vecPosition, engine::LocalFrameArea()))
	{
		return false;
	}

	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	common::ValidateVector<true >(rInfo.vecPosition);
	common::ValidateVector<false>(rInfo.vecDirection);
	common::ValidateVector<false>(rInfo.vecVelocity);

	engine::GrowPairedCollections(rCurrentInterpolate, rCurrentPostRender, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	int64_t iIndex = engine::AddElement(rCurrentInterpolate, rCurrentPostRender);

	rCurrentInterpolate.pVecPositions[iIndex] = rInfo.vecPosition;
	rCurrentInterpolate.pVecDirections[iIndex] = rInfo.vecDirection;
	rCurrentInterpolate.pfDestroyedTimes[iIndex] = -1.0f; // Sentinel: -1.0f = not exploding
	rCurrentInterpolate.pfDeltaRotations[iIndex] = rInfo.fDeltaRotation;
#if defined(BT_CLIENT)
	rCurrentInterpolate.pfAnimationTimes[iIndex] = 0.0f;
#endif

	rCurrentInterpolate.puiPushers[iIndex] = {};
	engine::PushersPostRender::Add(rFrame, rCurrentInterpolate.puiPushers[iIndex]);

#if defined(BT_CLIENT)
	SpaceshipsInterpolate::ClientInitialize(rFrame, iIndex);
#endif

	// Generate the missile-homing registry ID with one frame UUID after the pusher ID, preserving per-spawn UUID
	// order for replay.
	rCurrentInterpolate.puiRegistryIds[iIndex] = engine::registry_id_t::Generate(rFrame.postRender);

	// Flags describe reactions to the previous frame, so each spawned row starts from defaults.
	rCurrentPostRender.pFlags[iIndex] = {};
	rCurrentPostRender.pVecVelocities[iIndex] = rInfo.vecVelocity;
	rCurrentPostRender.pVecDamageDirections[iIndex] = XMVectorZero();
	rCurrentPostRender.pfHealths[iIndex] = rInfo.fHealth > 0.0f ? rInfo.fHealth : kfSpaceshipHealth;
	rCurrentPostRender.pfDestroyedExplosionTimes[iIndex] = 0.0f;
	rCurrentPostRender.pfNextBlasterSpawnTimes[iIndex] = rInfo.fNextBlasterSpawnTime;
	rCurrentPostRender.pAlignments[iIndex] = rInfo.alignment;
	rCurrentPostRender.pfArrivalGracePeriods[iIndex] = rInfo.fArrivalGracePeriod;

	// Sync owned objects after Add()
	SyncSpaceship(rFrame.interpolate, rCurrentInterpolate.puiPushers[iIndex], rInfo.vecPosition);

	return true;
}

void SpaceshipsPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdateSpaceships);

	SpaceshipsPostRender& __restrict rCurrent = *rFrame.postRender.pSpaceships;
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	const SpaceshipsPostRender& rPrevious = *rPreviousFrame.postRender.pSpaceships;
	const SpaceshipsInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pSpaceships;
	// Steering and health regeneration use previous-frame players; AvoidTerrain and Spawn use current-frame players.
	const PlayersInterpolate& rPlayers = *rPreviousFrame.interpolate.pPlayers;
	const PlayersPostRender& rPlayersPostRender = *rPreviousFrame.postRender.pPlayers;
	float fDeltaTime = rFrame.interpolate.fDeltaTime;

	// Island centers and base height are fixed for this cell's tick; the per-thread workbuffer shares candidates across ships.
	int64_t iIslandCount = std::ssize(rStaticData.islands);
	auto islandCandidatesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMFLOAT4*>(iIslandCount * static_cast<int64_t>(sizeof(XMFLOAT4)));
	for (int64_t i = 0; i < iIslandCount; ++i)
	{
		const engine::IslandPlacement& rPlacement = rStaticData.islands.at(i);
		XMStoreFloat4(&islandCandidatesAllocation.mpData[i], XMVectorSet(rPlacement.f2WorldPosition.x, rPlacement.f2WorldPosition.y, engine::gBaseHeight.mfCurrent, 1.0f));
	}
	std::span<const XMFLOAT4> islandCandidates(static_cast<XMFLOAT4*>(islandCandidatesAllocation.mpData), static_cast<size_t>(iIslandCount));

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Load from PostRender (static fields copied via memcpy in AllocateAndCopy)
		SpaceshipFlags_t flags = rPrevious.pFlags[i];
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		float fHealth = rPrevious.pfHealths[i];
		float fDestroyedExplosionTime = rPrevious.pfDestroyedExplosionTimes[i] - fDeltaTime;
		float fNextBlasterSpawnTime = rPrevious.pfNextBlasterSpawnTimes[i] - fDeltaTime;
		float fArrivalGracePeriod = std::max(0.0f, rPrevious.pfArrivalGracePeriods[i] - fDeltaTime);

		float fDeltaRotation = rPreviousInterpolate.pfDeltaRotations[i];

		if (!(flags & kExploding)) [[likely]]
		{
			if (fArrivalGracePeriod > 0.0f)
			{
				ApplyPusherResponse(rFrame, rCurrentInterpolate, i, vecVelocity);
			}
			else
			{
				XMVECTOR vecNearestPlayer = XMVectorZero();
				bool bPlayerAlive = NearestAlivePlayerPosition(rPlayers, rPlayersPostRender, rCurrentInterpolate.pVecPositions[i], vecNearestPlayer);

				RegenerateHealth(rCurrentInterpolate.pVecPositions[i], bPlayerAlive, vecNearestPlayer, flags, std::chrono::duration<float>(fDeltaTime), fHealth);
				ComputeSteering(islandCandidates, rCurrentInterpolate.pVecPositions[i], rCurrentInterpolate.pVecDirections[i], bPlayerAlive, vecNearestPlayer, fDeltaTime, flags, fDeltaRotation);
				ApplyMovement(rFrame, rCurrentInterpolate, i, flags, fDeltaTime, vecVelocity);
			}
		}
		else
		{
			XMVECTOR vecNearestPlayer = XMVectorZero();
			bool bPlayerAlive = NearestAlivePlayerPosition(rPlayers, rPlayersPostRender, rCurrentInterpolate.pVecPositions[i], vecNearestPlayer);
			RegenerateHealth(rCurrentInterpolate.pVecPositions[i], bPlayerAlive, vecNearestPlayer, flags, std::chrono::duration<float>(fDeltaTime), fHealth);
			vecVelocity = XMVectorScale(XMVectorNegate(rPrevious.pVecDamageDirections[i]), kfDeathKnockbackSpeed);
		}

		ApplyTerrainBounce(rStaticData, rCurrentInterpolate, i, fDeltaTime, fDeltaRotation, vecVelocity);

		fDeltaRotation = common::ClampMagnitude(fDeltaRotation, kfSpaceshipMaxTurnRate);

		rCurrent.pFlags[i] = flags;
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pfHealths[i] = fHealth;
		rCurrent.pfDestroyedExplosionTimes[i] = fDestroyedExplosionTime;
		rCurrent.pfNextBlasterSpawnTimes[i] = fNextBlasterSpawnTime;
		rCurrent.pfArrivalGracePeriods[i] = fArrivalGracePeriod;

		rCurrentInterpolate.pfDeltaRotations[i] = fDeltaRotation;
	}

	SpaceshipsPostRender::AvoidTerrain(rFrame, rPreviousFrame, rStaticData, 0, rFrame.interpolate.pSpaceships->iCount);
}

bool SpaceshipsInterpolate::LogDifferences(const SpaceshipsInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("SpaceshipsInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
		bEqual &= common::LogDifference<"pfDestroyedTimes">(i, pfDestroyedTimes[i], rOther.pfDestroyedTimes[i]);
		bEqual &= common::LogDifference<"puiPushers">(i, puiPushers[i], rOther.puiPushers[i]);
		bEqual &= common::LogDifference<"puiRegistryIds">(i, puiRegistryIds[i], rOther.puiRegistryIds[i]);
		bEqual &= common::LogDifference<"pfDeltaRotations">(i, pfDeltaRotations[i], rOther.pfDeltaRotations[i]);
	}

	return bEqual;
}

bool SpaceshipsPostRender::LogDifferences(const SpaceshipsPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("SpaceshipsPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pFlags">(i, pFlags[i], rOther.pFlags[i]);
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pVecDamageDirections">(i, pVecDamageDirections[i], rOther.pVecDamageDirections[i]);
		bEqual &= common::LogDifference<"pfHealths">(i, pfHealths[i], rOther.pfHealths[i]);
		bEqual &= common::LogDifference<"pfDestroyedExplosionTimes">(i, pfDestroyedExplosionTimes[i], rOther.pfDestroyedExplosionTimes[i]);
		bEqual &= common::LogDifference<"pfNextBlasterSpawnTimes">(i, pfNextBlasterSpawnTimes[i], rOther.pfNextBlasterSpawnTimes[i]);
		bEqual &= common::LogDifference<"pAlignments">(i, pAlignments[i], rOther.pAlignments[i]);
		bEqual &= common::LogDifference<"pfArrivalGracePeriods">(i, pfArrivalGracePeriods[i], rOther.pfArrivalGracePeriods[i]);
	}

	return bEqual;
}

} // namespace game
