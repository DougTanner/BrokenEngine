#include "Spaceships.h"

#include "Data/Audio.h"
#include "Data/Texture.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/Collection.h"
#include "Frame/CellStaticData.h"
#include "Ui/WrapperBase.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Frame/HealthDamage.h"
#include "Profile/ProfileRows.h"
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
constexpr int64_t kiSpaceshipExplosionTrailCount = 5;

static void RegisterEnemyBlasterType();

// Shared type indices (accessible from SpaceshipsCombat.cpp via extern)
int64_t giSpaceshipExplosionTypeIndex = 0xFF;
#if defined(BT_CLIENT)
static int64_t siSpaceshipHitFlashTypeIndex = 0xFF;
int64_t giSpaceshipHitFlashControllerTypeIndex = 0xFF;

static void RegisterSpaceshipHitFlashEffect();
#endif

constexpr int64_t kiSpaceshipExplosionBaseParticleCount = 16;
constexpr uint32_t kuiSpaceshipExplosionParticleColor = 0xFF0000FF;
constexpr float kfSpaceshipExplosionParticleVelocityMinimum = 1.0f;
constexpr float kfSpaceshipExplosionParticleVelocityRandom = 9.0f;
constexpr float kfSpaceshipExplosionParticleVerticalVelocityMinimum = -5.0f;
constexpr float kfSpaceshipExplosionParticleVerticalVelocityRandom = 10.0f;
constexpr float kfSpaceshipExplosionParticleIntensityDecay = 2.4f;
constexpr float kfSpaceshipExplosionTrailLengthRandom = kfSpaceshipRadius * 1.25f;
constexpr int64_t kiSpaceshipExplosionSecondaryCount = 1;

constexpr float kfSpawnBlasterOmniAngle = 0.1f;
constexpr float kfBlastersSpeed = 50.0f;
constexpr std::chrono::duration<float> kBlastersSpawnCooldown = std::chrono::duration<float>(1.0f);

constexpr float kfEnemyBlasterSize = kfSpaceshipRadius * 0.15f;
// Enemy blaster lighting tuning is supplied by LightingWrappers (gEnemyBlaster*).

#if defined(BT_CLIENT)
constexpr std::chrono::duration<float> kHitFlashDuration(0.3f);
#endif // BT_CLIENT

// Returns false when no omni is both non-exploding and outside arrival grace.
[[nodiscard]] bool XM_CALLCONV NearestAliveOmniPosition(const OmnisInterpolate& rOmnis, const OmnisPostRender& rOmnisPostRender, FXMVECTOR vecFrom, XMVECTOR& rVecResult)
{
	float fClosestDistanceSquared = std::numeric_limits<float>::max();
	bool bFound = false;

	for (int64_t i = 0; i < rOmnis.iCount; ++i)
	{
		if (rOmnisPostRender.pFlags[i] & OmniFlags::kExploding)
		{
			continue;
		}

		if (rOmnisPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(rOmnis.pVecPositions[i], vecFrom)));
		if (fDistanceSquared < fClosestDistanceSquared)
		{
			fClosestDistanceSquared = fDistanceSquared;
			rVecResult = rOmnis.pVecPositions[i];
			bFound = true;
		}
	}

	return bFound;
}

void SpaceshipsInterpolate::Register()
{
	engine::ExplosionsInterpolate::RegisterType(giSpaceshipExplosionTypeIndex,
	{
		.iBaseParticleCount = kiSpaceshipExplosionBaseParticleCount,
		.uiParticleColor = kuiSpaceshipExplosionParticleColor,
		.fParticleVelocityMinimum = kfSpaceshipExplosionParticleVelocityMinimum,
		.fParticleVelocityRandom = kfSpaceshipExplosionParticleVelocityRandom,
		.fParticleVerticalVelocityMinimum = kfSpaceshipExplosionParticleVerticalVelocityMinimum,
		.fParticleVerticalVelocityRandom = kfSpaceshipExplosionParticleVerticalVelocityRandom,
		.fParticleIntensityDecay = kfSpaceshipExplosionParticleIntensityDecay,
		.fTrailLengthRandom = kfSpaceshipExplosionTrailLengthRandom,
		.iSecondaryExplosionCount = kiSpaceshipExplosionSecondaryCount,
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

static int64_t siEnemyBlasterPointLightTypeIndex = 0xFF;
static int64_t siEnemyBlasterTypeIndex = 0xFF;

static void RegisterEnemyBlasterType()
{
#if defined(BT_CLIENT)
	engine::PointLightsInterpolate::RegisterType(siEnemyBlasterPointLightTypeIndex,
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

	BlastersInterpolate::RegisterType(siEnemyBlasterTypeIndex,
	{
		.f2Size = {kfEnemyBlasterSize, kfEnemyBlasterSize},
		.iPointLightTypeIndex = siEnemyBlasterPointLightTypeIndex,
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
	engine::PointLightsInterpolate::RegisterType(siSpaceshipHitFlashTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC74pngCrc,
		.uiColor = 0xFFFFFFFF,
	});

	engine::PointLightsInterpolate::RegisterControllerType(giSpaceshipHitFlashControllerTypeIndex,
	{
		.iBaseTypeIndex = siSpaceshipHitFlashTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.pTimes = {std::chrono::duration<float>(0.0f), kHitFlashDuration, std::chrono::duration<float>(0.0f), std::chrono::duration<float>(0.0f)},
		.pKeyframes =
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
#endif // BT_CLIENT

void SpawnSpaceshipExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent)
{
	XMVECTOR vecJitteredPosition = common::RandomPositionJitter<kfSpaceshipExplosionPositionJitter>(vecPosition, rFrame.postRender.randomEngine);
	XMVECTOR vecJitteredDirection = common::RandomDirectionJitter<kfSpaceshipExplosionDirectionJitter>(vecDirection, rFrame.postRender.randomEngine);

	engine::ExplosionsPostRender::Spawn(rFrame, emitterCoordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick),
	{
		.iTypeIndex = giSpaceshipExplosionTypeIndex,
		.vecPosition = vecJitteredPosition,
		.vecDirection = vecJitteredDirection,
		.flags = {engine::ExplosionFlags::kDestroysSelf, engine::ExplosionFlags::kRed},
		.iTrailCount = kiSpaceshipExplosionTrailCount,
		.fTrailAngle = fPercent * XM_PI,
		.iParticleCount = static_cast<int64_t>(fPercent * kfSpaceshipExplosionParticleCount),
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
	std::chrono::duration<float> deltaTime(rCurrentFrameInterpolate.fDeltaTime);
	// The climb rate is capped at the maximum flight speed.
	float fTargetHeight = engine::gpFlightPlanes->Lowest().fHeightMeters;
	float fClimbStep = kfSpaceshipMaxSpeed * deltaTime.count();

#if defined(BT_CLIENT)
	// Null unless the model's first animation has a positive duration.
	const engine::AnimationData* pAnimationData = nullptr;
	if (engine::gAnimationDataMap.contains(kuiSpaceshipModel))
	{
		const engine::AnimationData& rAnimationData = engine::gAnimationDataMap.at(kuiSpaceshipModel);
		if (rAnimationData.mpAnimations[0].fDuration > 0.0f)
		{
			pAnimationData = &rAnimationData;
		}
	}
#endif

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		std::chrono::duration<float> destroyedTime = rPrevious.pDestroyedTimes[i];
		float fDeltaRotation = rPrevious.pfDeltaRotations[i];
#if defined(BT_CLIENT)
		std::chrono::duration<float> animationTime = rPrevious.pAnimationTimes[i];
#endif

		vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count()), rPreviousPostRender.pVecVelocities[i], vecPosition);
		// PostRender Update stores velocity Z as 0, so this glide is what moves a ship's height; the final
		// assignment lands it bit-exactly on the target and keeps it there.
		float fHeight = XMVectorGetZ(vecPosition);
		float fHeightGap = fTargetHeight - fHeight;
		if (fHeightGap > fClimbStep)
		{
			fHeight += fClimbStep;
		}
		else if (fHeightGap < -fClimbStep)
		{
			fHeight -= fClimbStep;
		}
		else
		{
			fHeight = fTargetHeight;
		}
		vecPosition = XMVectorSetZ(vecPosition, fHeight);
		// Enforce W=1.0 — prevents drift via MultiplyAdd's 4-lane propagation.
		vecPosition = XMVectorSetW(vecPosition, 1.0f);

		vecDirection = XMVector3Normalize(XMVector4Transform(vecDirection, XMMatrixRotationZ(deltaTime.count() * fDeltaRotation)));

		// The -1.0f sentinel means not exploding and must remain unchanged.
		if (destroyedTime > std::chrono::duration<float>::zero())
		{
			destroyedTime = std::max(destroyedTime - deltaTime, std::chrono::duration<float>::zero());
		}

#if defined(BT_CLIENT)
		if (pAnimationData != nullptr)
		{
			animationTime = pAnimationData->AdvanceLoopingTime(animationTime, deltaTime);
		}
#endif // BT_CLIENT

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pDestroyedTimes[i] = destroyedTime;
		rCurrent.pfDeltaRotations[i] = fDeltaRotation;
#if defined(BT_CLIENT)
		rCurrent.pAnimationTimes[i] = animationTime;
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
		engine::RemoveIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.puiWindTrails[i]);
	}
#endif
}

void SpaceshipsPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	engine::CellBounds bounds = engine::ComputeCellBounds(engine::LocalCellArea());

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return rCurrentPostRender.pFlags[i] & kTransfer;
	}, [&](int64_t i)
	{
		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];

		TransferRequest request
		{
			.change = MakeTransfer(SpawnInfo
			{
				.vecPosition = vecPosition,
				.vecDirection = rCurrentInterpolate.pVecDirections[i],
				.vecVelocity = rCurrentPostRender.pVecVelocities[i],
				.alignment = rCurrentPostRender.pAlignments[i],
				.fHealth = rCurrentPostRender.pfHealths[i],
				.nextBlasterSpawnTime = rCurrentPostRender.pNextBlasterSpawnTimes[i],
				.arrivalGracePeriod = kArrivalGracePeriod,
				.fDeltaRotation = rCurrentInterpolate.pfDeltaRotations[i],
			}),
		};
		if (rFrame.postRender.PrepareTransferRequest(bounds, request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Spaceship Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Delta: ({},{}) Health: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iX, rStaticData.coordinate.iY, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDeltaX), static_cast<int32_t>(request.iDeltaY), common::Wb(rCurrentPostRender.pfHealths[i], 1), rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, std::ssize(rFrame.postRender.transferRequests), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		rFrame.postRender.PushTransferRequest(request);

		RemoveOwnedObjects(rFrame, rCurrentInterpolate, i, true);

		engine::DestroyElement(rCurrentInterpolate, rCurrentPostRender, i, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	});
}

void SpaceshipsPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return (rCurrentPostRender.pFlags[i] & kExploding) && !(rCurrentInterpolate.pDestroyedTimes[i] > std::chrono::duration<float>::zero());
	}, [&](int64_t i)
	{
		RemoveOwnedObjects(rFrame, rCurrentInterpolate, i, false);

		engine::DestroyElement(rCurrentInterpolate, rCurrentPostRender, i, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	});
}

void SpaceshipsPostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	SpaceshipsPostRender& rCurrentPostRender = *rFrame.postRender.pSpaceships;

	const OmnisInterpolate& rOmnis = *rFrame.interpolate.pOmnis;
	const OmnisPostRender& rOmnisPostRender = *rFrame.postRender.pOmnis;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if ((rCurrentPostRender.pFlags[i] & kExploding) && rCurrentPostRender.pDestroyedExplosionTimes[i].count() <= 0.0f)
		{
			rCurrentPostRender.pDestroyedExplosionTimes[i] = kSpaceshipDestroyExplosionInterval;

			float fPercent = rCurrentInterpolate.pDestroyedTimes[i] / kSpaceshipDestroyTime;
			XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
			SpawnSpaceshipExplosion(rFrame, rStaticData.coordinate, rCurrentInterpolate.pVecPositions[i], vecDirection, fPercent);
			continue;
		}

		if (rCurrentPostRender.pFlags[i] & kExploding)
		{
			continue;
		}

		if (rCurrentPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		XMVECTOR vecNearestOmni = XMVectorZero();
		if (!NearestAliveOmniPosition(rOmnis, rOmnisPostRender, rCurrentInterpolate.pVecPositions[i], vecNearestOmni))
		{
			continue;
		}

		if (!FrameInterpolate::IsVisible(vecNearestOmni, rCurrentInterpolate.pVecPositions[i]))
		{
			continue;
		}

		XMVECTOR vecToOmni = XMVectorSubtract(vecNearestOmni, rCurrentInterpolate.pVecPositions[i]);
		XMVECTOR vecToOmniNormal = XMVector3Normalize(vecToOmni);
		float fAngleToOmni = XMVectorGetX(XMVector3AngleBetweenNormals(rCurrentInterpolate.pVecDirections[i], vecToOmniNormal));

		bool bSpawnBlaster = fAngleToOmni <= kfSpawnBlasterOmniAngle;

		if (rCurrentPostRender.pNextBlasterSpawnTimes[i] < std::chrono::duration<float>::zero() && bSpawnBlaster)
		{
			rCurrentPostRender.pNextBlasterSpawnTimes[i] = kBlastersSpawnCooldown;

			XMVECTOR vecDirection = rCurrentInterpolate.pVecDirections[i];
			XMVECTOR vecBlasterVelocity = XMVectorScale(vecDirection, kfBlastersSpeed);
			XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];

			BlastersPostRender::Spawn(rFrame,
			{
				.vecPosition = vecPosition,
				.vecVelocity = vecBlasterVelocity,
				.iTypeIndex = siEnemyBlasterTypeIndex,
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
	engine::WindTrailsPostRender::Add(rFrame, rSpaceships.puiWindTrails[iIndex],
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
		if (rSpaceships.pDestroyedTimes[i].count() >= 0.0f)
		{
			continue;
		}
		ClientInitialize(rFrame, i);
	}
}
#endif // BT_CLIENT

bool SpaceshipsPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo)
{
	if (!common::InsideArea(rInfo.vecPosition, engine::LocalCellArea()))
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
	rCurrentInterpolate.pDestroyedTimes[iIndex] = std::chrono::duration<float>(-1.0f); // Sentinel: -1.0f = not exploding
	rCurrentInterpolate.pfDeltaRotations[iIndex] = rInfo.fDeltaRotation;
#if defined(BT_CLIENT)
	rCurrentInterpolate.pAnimationTimes[iIndex] = std::chrono::duration<float>::zero();
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
	rCurrentPostRender.pDestroyedExplosionTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentPostRender.pNextBlasterSpawnTimes[iIndex] = rInfo.nextBlasterSpawnTime;
	rCurrentPostRender.pAlignments[iIndex] = rInfo.alignment;
	rCurrentPostRender.pArrivalGracePeriods[iIndex] = rInfo.arrivalGracePeriod;

	// Sync owned objects after Add()
	SyncSpaceship(rFrame.interpolate, rCurrentInterpolate.puiPushers[iIndex], rInfo.vecPosition);

	return true;
}

void SpaceshipsPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdateSpaceships);

	SpaceshipsPostRender& __restrict rCurrent = *rFrame.postRender.pSpaceships;
	SpaceshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pSpaceships;
	const SpaceshipsPostRender& rPrevious = *rPreviousFrame.postRender.pSpaceships;
	const SpaceshipsInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pSpaceships;
	// Steering and health regeneration use previous-frame omnis; AvoidTerrain and Spawn use current-frame omnis.
	const OmnisInterpolate& rOmnis = *rPreviousFrame.interpolate.pOmnis;
	const OmnisPostRender& rOmnisPostRender = *rPreviousFrame.postRender.pOmnis;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	// Island centers and base height are fixed for this cell's tick; the per-thread workbuffer shares candidates across ships.
	int64_t iIslandCount = std::ssize(rStaticData.islands);
	auto islandCandidatesAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<XMFLOAT4*>(iIslandCount * static_cast<int64_t>(sizeof(XMFLOAT4)));
	for (int64_t i = 0; i < iIslandCount; ++i)
	{
		const engine::IslandPlacement& rPlacement = rStaticData.islands.at(i);
		XMStoreFloat4(&islandCandidatesAllocation.mpData[i], XMVectorSet(rPlacement.f2WorldPosition.x, rPlacement.f2WorldPosition.y, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f));
	}
	std::span<const XMFLOAT4> islandCandidates(islandCandidatesAllocation.mpData, static_cast<size_t>(iIslandCount));

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Load from PostRender (static fields copied via memcpy in AllocateAndCopy)
		SpaceshipFlags_t flags = rPrevious.pFlags[i];
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		float fHealth = rPrevious.pfHealths[i];
		std::chrono::duration<float> destroyedExplosionTime = rPrevious.pDestroyedExplosionTimes[i] - deltaTime;
		std::chrono::duration<float> nextBlasterSpawnTime = rPrevious.pNextBlasterSpawnTimes[i] - deltaTime;
		std::chrono::duration<float> arrivalGracePeriod = std::max(std::chrono::duration<float>::zero(), rPrevious.pArrivalGracePeriods[i] - deltaTime);

		float fDeltaRotation = rPreviousInterpolate.pfDeltaRotations[i];

		if (!(flags & kExploding)) [[likely]]
		{
			if (arrivalGracePeriod > std::chrono::duration<float>::zero())
			{
				ApplyPusherResponse(rFrame, rCurrentInterpolate, i, vecVelocity);
			}
			else
			{
				XMVECTOR vecNearestOmni = XMVectorZero();
				bool bOmniAlive = NearestAliveOmniPosition(rOmnis, rOmnisPostRender, rCurrentInterpolate.pVecPositions[i], vecNearestOmni);

				RegenerateHealth(rCurrentInterpolate.pVecPositions[i], bOmniAlive, vecNearestOmni, flags, deltaTime, fHealth);
				ComputeSteering(islandCandidates, rCurrentInterpolate.pVecPositions[i], rCurrentInterpolate.pVecDirections[i], bOmniAlive, vecNearestOmni, deltaTime, flags, fDeltaRotation);
				ApplyMovement(rFrame, rCurrentInterpolate, i, flags, deltaTime, vecVelocity);
			}
		}
		else
		{
			XMVECTOR vecNearestOmni = XMVectorZero();
			bool bOmniAlive = NearestAliveOmniPosition(rOmnis, rOmnisPostRender, rCurrentInterpolate.pVecPositions[i], vecNearestOmni);
			RegenerateHealth(rCurrentInterpolate.pVecPositions[i], bOmniAlive, vecNearestOmni, flags, deltaTime, fHealth);
			vecVelocity = XMVectorScale(XMVectorNegate(rPrevious.pVecDamageDirections[i]), kfDeathKnockbackSpeed);
		}

		ApplyTerrainBounce(rStaticData, rPreviousInterpolate, rCurrentInterpolate, i, deltaTime, fDeltaRotation, vecVelocity);
		// Last velocity write, after pusher response, knockback, and bounce: a stored Z speed would reach the next
		// tick's ApplyMovement, which rescales its blend to the full 3D speed and would turn it into horizontal speed.
		vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);

		fDeltaRotation = common::ClampMagnitude(fDeltaRotation, kfSpaceshipMaxTurnRate);

		rCurrent.pFlags[i] = flags;
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pfHealths[i] = fHealth;
		rCurrent.pDestroyedExplosionTimes[i] = destroyedExplosionTime;
		rCurrent.pNextBlasterSpawnTimes[i] = nextBlasterSpawnTime;
		rCurrent.pArrivalGracePeriods[i] = arrivalGracePeriod;

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
		bEqual &= common::LogDifference<"pDestroyedTimes">(i, pDestroyedTimes[i], rOther.pDestroyedTimes[i]);
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
		bEqual &= common::LogDifference<"pDestroyedExplosionTimes">(i, pDestroyedExplosionTimes[i], rOther.pDestroyedExplosionTimes[i]);
		bEqual &= common::LogDifference<"pNextBlasterSpawnTimes">(i, pNextBlasterSpawnTimes[i], rOther.pNextBlasterSpawnTimes[i]);
		bEqual &= common::LogDifference<"pAlignments">(i, pAlignments[i], rOther.pAlignments[i]);
		bEqual &= common::LogDifference<"pArrivalGracePeriods">(i, pArrivalGracePeriods[i], rOther.pArrivalGracePeriods[i]);
	}

	return bEqual;
}

} // namespace game
