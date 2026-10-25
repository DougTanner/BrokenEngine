#include "Fighters.h"

#include "Data/Audio.h"
#include "Data/Texture.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/Collections/Collection.h"
#include "Frame/CellStaticData.h"
#include "Ui/WrapperBase.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Ships/ShipQueries.h"
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
template struct Collection<game::FightersInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<game::FightersPostRender>;
} // namespace engine

namespace game
{

constexpr float kfDeathKnockbackSpeed = 20.0f;

constexpr float kfFighterExplosionParticleCount = 8.0f;
constexpr float kfFighterExplosionSizeStart = kfFighterRadius * 0.625f;
constexpr float kfFighterExplosionSizeEnd = kfFighterRadius * 0.25f;
constexpr float kfFighterExplosionSmoke = 0.5f;
constexpr float kfFighterExplosionPositionJitter = kfFighterRadius * 0.375f;
constexpr float kfFighterExplosionDirectionJitter = 0.5f;
constexpr int64_t kiFighterExplosionTrailCount = 5;

static void RegisterFighterBlasterType();

// Shared type indices (accessible from FightersCombat.cpp via extern)
int64_t giFighterExplosionTypeIndex = 0xFF;
#if defined(BT_CLIENT)
static int64_t siFighterHitFlashTypeIndex = 0xFF;
int64_t giFighterHitFlashControllerTypeIndex = 0xFF;

static void RegisterFighterHitFlashEffect();
#endif

constexpr int64_t kiFighterExplosionBaseParticleCount = 16;
constexpr uint32_t kuiFighterExplosionParticleColor = 0xFF0000FF;
constexpr float kfFighterExplosionParticleVelocityMinimum = 1.0f;
constexpr float kfFighterExplosionParticleVelocityRandom = 9.0f;
constexpr float kfFighterExplosionParticleVerticalVelocityMinimum = -5.0f;
constexpr float kfFighterExplosionParticleVerticalVelocityRandom = 10.0f;
constexpr float kfFighterExplosionParticleIntensityDecay = 2.4f;
constexpr float kfFighterExplosionTrailLengthRandom = kfFighterRadius * 1.25f;
constexpr int64_t kiFighterExplosionSecondaryCount = 1;

constexpr float kfSpawnBlasterEnemyAngle = 0.1f;
constexpr float kfBlastersSpeed = 50.0f;
constexpr std::chrono::duration<float> kBlastersSpawnCooldown = std::chrono::duration<float>(1.0f);

constexpr float kfFighterBlasterSize = kfFighterRadius * 0.15f;
// Fighter blaster lighting tuning is supplied by LightingWrappers (gFighterBlaster*).

#if defined(BT_CLIENT)
constexpr std::chrono::duration<float> kHitFlashDuration(0.3f);
#endif // BT_CLIENT

void FightersInterpolate::Register()
{
	engine::ExplosionsInterpolate::RegisterType(giFighterExplosionTypeIndex,
	{
		.iBaseParticleCount = kiFighterExplosionBaseParticleCount,
		.uiParticleColor = kuiFighterExplosionParticleColor,
		.fParticleVelocityMinimum = kfFighterExplosionParticleVelocityMinimum,
		.fParticleVelocityRandom = kfFighterExplosionParticleVelocityRandom,
		.fParticleVerticalVelocityMinimum = kfFighterExplosionParticleVerticalVelocityMinimum,
		.fParticleVerticalVelocityRandom = kfFighterExplosionParticleVerticalVelocityRandom,
		.fParticleIntensityDecay = kfFighterExplosionParticleIntensityDecay,
		.fTrailLengthRandom = kfFighterExplosionTrailLengthRandom,
		.iSecondaryExplosionCount = kiFighterExplosionSecondaryCount,
		.pParticleWidthScale = &gFighterExplosionParticleWidth,
		.pParticleLengthScale = &gFighterExplosionParticleLength,
		.pParticleLengthSpreadScale = &gFighterExplosionParticleLengthSpread,
		.pParticlePositionJitterScale = &gFighterExplosionParticlePositionJitter,
		.pParticleVelocityBaseScale = &gFighterExplosionParticleVelocityBase,
		.pParticleVelocitySpreadScale = &gFighterExplosionParticleVelocitySpread,
		.pParticleVerticalVelocityBaseScale = &gFighterExplosionParticleVerticalVelocityBase,
		.pParticleVerticalVelocitySpreadScale = &gFighterExplosionParticleVerticalVelocitySpread,
		.pParticleVelocityDecayScale = &gFighterExplosionParticleVelocityDecay,
		.pParticleGravityScale = &gFighterExplosionParticleGravity,
		.pParticleVisibleIntensityScale = &gFighterExplosionParticleVisibleIntensity,
		.pParticleIntensitySpreadScale = &gFighterExplosionParticleIntensitySpread,
		.pParticleIntensityDecayScale = &gFighterExplosionParticleIntensityDecay,
		.pParticleIntensityPowerScale = &gFighterExplosionParticleIntensityPower,
	});

	RegisterFighterBlasterType();
#if defined(BT_CLIENT)
	RegisterFighterHitFlashEffect();
#endif
}

static int64_t siFighterBlasterPointLightTypeIndex = 0xFF;
static int64_t siFighterBlasterTypeIndex = 0xFF;

static void RegisterFighterBlasterType()
{
#if defined(BT_CLIENT)
	engine::PointLightsInterpolate::RegisterType(siFighterBlasterPointLightTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC72pngCrc,
		.uiColor = 0xFFFFFFFF,
		.fVisibleArea = kfFighterBlasterSize,
		.bCameraAligned = true,
		.pVisibleIntensityWrapper = &gFighterBlasterVisibleIntensity,
		.pLightingAreaWrapper = &gFighterBlasterLightingArea,
		.pLightingIntensityWrapper = &gFighterBlasterLightingIntensity,
	});
#endif // BT_CLIENT

	BlastersInterpolate::RegisterType(siFighterBlasterTypeIndex,
	{
		.f2Size = {kfFighterBlasterSize, kfFighterBlasterSize},
		.iPointLightTypeIndex = siFighterBlasterPointLightTypeIndex,
		.pWindTrailIntensityWrapper = &gWindDepositFightersBlastersIntensity,
		.pWindTrailWidthWrapper = &gWindDepositFightersBlastersWidth,
		.pWindTrailLengthMultiplierWrapper = &gWindDepositFightersBlastersLengthMultiplier,
	});
}

// The registry binds this collection's position column directly, so its ID needs no per-tick sync.
static void XM_CALLCONV SyncFighter(FrameInterpolate& rFrameInterpolate, engine::pusher_t uiPusher, FXMVECTOR vecPosition)
{
	engine::PushersInterpolate::Sync(rFrameInterpolate, uiPusher,
	{
		.vecPosition = vecPosition,
		.fRadius = kfFighterPusherRadius,
		.fIntensity = kfFighterPusherIntensity,
		.fPower = kfFighterPusherPower,
		.flags = {engine::PusherFlags::kTypeDefault},
	});
}

#if defined(BT_CLIENT)
static void RegisterFighterHitFlashEffect()
{
	engine::PointLightsInterpolate::RegisterType(siFighterHitFlashTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC74pngCrc,
		.uiColor = 0xFFFFFFFF,
	});

	engine::PointLightsInterpolate::RegisterControllerType(giFighterHitFlashControllerTypeIndex,
	{
		.iBaseTypeIndex = siFighterHitFlashTypeIndex,
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

void SpawnFighterExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent)
{
	XMVECTOR vecJitteredPosition = common::RandomPositionJitter<kfFighterExplosionPositionJitter>(vecPosition, rFrame.postRender.randomEngine);
	XMVECTOR vecJitteredDirection = common::RandomDirectionJitter<kfFighterExplosionDirectionJitter>(vecDirection, rFrame.postRender.randomEngine);

	engine::ExplosionsPostRender::Spawn(rFrame, emitterCoordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick),
	{
		.iTypeIndex = giFighterExplosionTypeIndex,
		.vecPosition = vecJitteredPosition,
		.vecDirection = vecJitteredDirection,
		.flags = {engine::ExplosionFlags::kRed},
		.lifecycleFlags = {engine::LifecycleFlags::kDying},
		.iTrailCount = kiFighterExplosionTrailCount,
		.fTrailAngle = fPercent * XM_PI,
		.iParticleCount = static_cast<int64_t>(fPercent * kfFighterExplosionParticleCount),
		.fParticleAngle = fPercent * XM_PIDIV2,
		.fSizePercent = fPercent * kfFighterExplosionSizeStart + (1.0f - fPercent) * kfFighterExplosionSizeEnd,
		.fSmokePercent = fPercent * kfFighterExplosionSmoke,
		.fTimePercent = fPercent,
	});
}

#if defined(BT_CLIENT)
// Fighter models, one per ShipController (defined in FightersRender.cpp)
extern const common::crc_t kFighterFleetModel;
extern const common::crc_t kFighterRandomModel;

// Null unless the model's first animation has a positive duration.
static const engine::AnimationData* LoopingAnimationData(common::crc_t uiModel)
{
	if (engine::gAnimationDataMap.contains(uiModel))
	{
		const engine::AnimationData& rAnimationData = engine::gAnimationDataMap.at(uiModel);
		if (rAnimationData.mpAnimations[0].fDuration > 0.0f)
		{
			return &rAnimationData;
		}
	}
	return nullptr;
}
#endif

void FightersInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rCurrentFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateUpdateFighters);

	FightersInterpolate& rCurrent = *rCurrentFrameInterpolate.pFighters;
	const FightersInterpolate& rPrevious = *rPreviousFrame.interpolate.pFighters;
	const FightersPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pFighters;
	std::chrono::duration<float> deltaTime(rCurrentFrameInterpolate.fDeltaTime);

#if defined(BT_CLIENT)
	float fClimbStep = kfFighterClimbRate * deltaTime.count();
	// Indexed by ShipController: each row's clock advances through its own model's animation.
	std::array<const engine::AnimationData*, std::to_underlying(ShipController::kCount)> animationDatas {LoopingAnimationData(kFighterFleetModel), LoopingAnimationData(kFighterRandomModel)};
#endif

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		std::chrono::duration<float> destroyedTime = rPrevious.pDestroyedTimes[i];
		float fDeltaRotation = rPrevious.pfDeltaRotations[i];
		uint8_t uiFlightPlane = rPrevious.puiFlightPlanes[i];
#if defined(BT_CLIENT)
		std::chrono::duration<float> animationTime = rPrevious.pAnimationTimes[i];
		const engine::AnimationData* pAnimationData = animationDatas[std::to_underlying(rPreviousPostRender.pController[i])];
#endif

		vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count()), rPreviousPostRender.pVecVelocities[i], vecPosition);
#if defined(BT_CLIENT)
		float fHeightBeforeClimb = XMVectorGetZ(vecPosition);
#endif
		// PostRender Update stores velocity Z as 0, so the climb is what moves a ship's height.
		vecPosition = ClimbTowardsFlightPlane(vecPosition, GetTargetFlightPlane(rPreviousPostRender.pOrders[i]), kfFighterClimbRate, deltaTime, uiFlightPlane);
		// Enforce W=1.0 — prevents drift via MultiplyAdd's 4-lane propagation.
		vecPosition = XMVectorSetW(vecPosition, 1.0f);
#if defined(BT_CLIENT)
		// Full tilt at the full climb rate, none on a plane; a zero-length render step keeps the last tilt.
		float fClimbTilt = fClimbStep > 0.0f ? kfFighterClimbTilt * ((XMVectorGetZ(vecPosition) - fHeightBeforeClimb) / fClimbStep) : rPrevious.pfClimbTilts[i];
#endif

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
		rCurrent.puiFlightPlanes[i] = uiFlightPlane;
#if defined(BT_CLIENT)
		rCurrent.pAnimationTimes[i] = animationTime;
		rCurrent.pfClimbTilts[i] = fClimbTilt;
#endif

		// Sync owned objects (IDs copied in AllocateAndCopy)
		SyncFighter(rCurrentFrameInterpolate, rCurrent.pPushers[i], vecPosition);

#if defined(BT_CLIENT)
		if ((rCurrent.pWindTrails[i].uuid.iValue != 0))
		{
			engine::WindTrailsInterpolate::Sync(rCurrentFrameInterpolate, rCurrent.pWindTrails[i],
			{
				.vecPosition = vecPosition,
				.fIntensity = game::gWindDepositFightersIntensity.mfCurrent,
				.fWidth = game::gWindDepositFightersWidth.mfCurrent,
				.fLengthMultiplier = game::gWindDepositFightersLengthMultiplier.mfCurrent,
			});
		}
#endif // BT_CLIENT
	}
}

void FightersInterpolate::AllocateAndCopy(FightersInterpolate& rCurrent, const FightersInterpolate& rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateAllocateAndCopyFighters);
	engine::AllocateAndCopyMembers(rCurrent, rPrevious);
}

void FightersPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kTransfer;
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
				.eController = rCurrentPostRender.pController[i],
				.orders = rCurrentPostRender.pOrders[i],
				.fHealth = rCurrentPostRender.pfHealths[i],
				.nextBlasterSpawnTime = rCurrentPostRender.pNextBlasterSpawnTimes[i],
				.arrivalGracePeriod = kArrivalGracePeriod,
				.fDeltaRotation = rCurrentInterpolate.pfDeltaRotations[i],
				.navigationDelay = rCurrentPostRender.pNavigationDelays[i],
				.globalId = rCurrentPostRender.pGlobalIds[i],
				.clientGuid = rCurrentPostRender.pClientGuids[i],
				.flagshipGlobalId = rCurrentPostRender.pFlagshipGlobalIds[i],
				.fleetWantedCoordinate = rCurrentPostRender.pFleetWantedCoordinates[i],
				.uiPendingFleetWantedCoordinateTicks = rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[i],
				.uiFlightPlane = rCurrentInterpolate.puiFlightPlanes[i],
				.bTransfer = true,
			}),
		};
		if (!rFrame.postRender.PrepareTransferRequest(request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Fighter Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Direction: {} Health: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iQ, rStaticData.coordinate.iR, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDirection), common::Wb(rCurrentPostRender.pfHealths[i], 1), rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, std::ssize(rFrame.postRender.transferRequests), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		rFrame.postRender.PushTransferRequest(request);
	});
}

void FightersPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && !(rCurrentInterpolate.pDestroyedTimes[i] > std::chrono::duration<float>::zero());
	}, [&](int64_t i)
	{
		rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDestroy);
	});
}

void FightersPostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if ((rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && rCurrentPostRender.pDestroyedExplosionTimes[i].count() <= 0.0f)
		{
			rCurrentPostRender.pDestroyedExplosionTimes[i] = kFighterDestroyExplosionInterval;

			float fPercent = rCurrentInterpolate.pDestroyedTimes[i] / kFighterDestroyTime;
			XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
			SpawnFighterExplosion(rFrame, rStaticData.coordinate, rCurrentInterpolate.pVecPositions[i], vecDirection, fPercent);
			continue;
		}

		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			continue;
		}

		if (rCurrentPostRender.pArrivalGracePeriods[i] > std::chrono::duration<float>::zero())
		{
			continue;
		}

		// A ship climbing between planes stores no plane and holds fire.
		uint8_t uiFlightPlane = rCurrentInterpolate.puiFlightPlanes[i];
		if (uiFlightPlane == kuiNoFlightPlane)
		{
			continue;
		}

		XMVECTOR vecNearestEnemy = XMVectorZero();
		if (!FindNearestEnemyShip(rFrame, rCurrentInterpolate.pVecPositions[i], rCurrentPostRender.pAlignments[i], uiFlightPlane, vecNearestEnemy))
		{
			continue;
		}

		if (!FrameInterpolate::IsVisible(vecNearestEnemy, rCurrentInterpolate.pVecPositions[i]))
		{
			continue;
		}

		// Horizontal offset: the blaster flies flat along the heading, so only the heading-plane angle decides a hit.
		XMVECTOR vecToEnemy = XMVectorSetZ(XMVectorSubtract(vecNearestEnemy, rCurrentInterpolate.pVecPositions[i]), 0.0f);
		XMVECTOR vecToEnemyNormal = XMVector3Normalize(vecToEnemy);
		float fAngleToEnemy = XMVectorGetX(XMVector3AngleBetweenNormals(rCurrentInterpolate.pVecDirections[i], vecToEnemyNormal));

		bool bSpawnBlaster = fAngleToEnemy <= kfSpawnBlasterEnemyAngle;

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
				.iTypeIndex = siFighterBlasterTypeIndex,
				.alignment = rCurrentPostRender.pAlignments[i],
			});

#if defined(BT_CLIENT)
			engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster514039__newlocknew__blastershot6sytrusrsmplmultiprcsngsinglewavCrc, rStaticData.coordinate, vecPosition, gFighterBlasterVolume.mfCurrent, gFighterBlasterPitchMinimum.mfCurrent, gFighterBlasterPitchRandom.mfCurrent);
#endif
		}
	}
}

#if defined(BT_CLIENT)
void FightersInterpolate::ClientInitialize(Frame& rFrame, int64_t iIndex)
{
	FightersInterpolate& rFighters = *rFrame.interpolate.pFighters;

	rFighters.pWindTrails[iIndex] = {};
	engine::WindTrailsPostRender::Add(rFrame, rFighters.pWindTrails[iIndex],
	{
		.vecPosition = rFighters.pVecPositions[iIndex],
		.fIntensity = game::gWindDepositFightersIntensity.mfCurrent,
		.fWidth = game::gWindDepositFightersWidth.mfCurrent,
		.fLengthMultiplier = game::gWindDepositFightersLengthMultiplier.mfCurrent,
	});
}

void FightersInterpolate::ClientInitializeAll(Frame& rFrame)
{
	FightersInterpolate& rFighters = *rFrame.interpolate.pFighters;
	for (int64_t i = 0; i < rFighters.iCount; ++i)
	{
		if (rFighters.pDestroyedTimes[i].count() >= 0.0f)
		{
			continue;
		}
		ClientInitialize(rFrame, i);
	}
}
#endif // BT_CLIENT

bool FightersPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo)
{
	if (!engine::IsInsideCell(rInfo.vecPosition))
	{
		return false;
	}

	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	FightersPostRender& rCurrentPostRender = *rFrame.postRender.pFighters;

	common::ValidateVector<true >(rInfo.vecPosition);
	common::ValidateVector<false>(rInfo.vecDirection);
	common::ValidateVector<false>(rInfo.vecVelocity);

	engine::GrowPairedCollections(rCurrentInterpolate, rCurrentPostRender, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	auto [iIndex, newId] = engine::AddIndexableElement(rCurrentInterpolate, rCurrentPostRender, rFrame.postRender);

	rCurrentInterpolate.pVecPositions[iIndex] = rInfo.vecPosition;
	rCurrentInterpolate.pVecDirections[iIndex] = rInfo.vecDirection;
	rCurrentInterpolate.pDestroyedTimes[iIndex] = std::chrono::duration<float>(-1.0f); // Sentinel: -1.0f = not exploding
	rCurrentInterpolate.pfDeltaRotations[iIndex] = rInfo.fDeltaRotation;
	rCurrentInterpolate.puiFlightPlanes[iIndex] = rInfo.uiFlightPlane;
#if defined(BT_CLIENT)
	rCurrentInterpolate.pAnimationTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentInterpolate.pfClimbTilts[iIndex] = 0.0f;
#endif

	rCurrentInterpolate.pPushers[iIndex] = {};
	engine::PushersPostRender::Add(rFrame, rCurrentInterpolate.pPushers[iIndex]);

#if defined(BT_CLIENT)
	FightersInterpolate::ClientInitialize(rFrame, iIndex);
#endif

	// Generate the missile-homing registry ID with one frame UUID after the pusher ID, preserving per-spawn UUID
	// order for replay.
	rCurrentInterpolate.pRegistryIds[iIndex] = engine::registry_id_t::Generate(rFrame.postRender);

	rCurrentPostRender.pIds[iIndex] = newId;
	rCurrentPostRender.pController[iIndex] = rInfo.eController;
	rCurrentPostRender.pOrders[iIndex] = rInfo.orders;
	// Flags describe reactions to the previous frame, so each spawned row starts from defaults. Entering a cell is a
	// fresh spawn for navigation, cross-cell transfers included: a flagship navigates to an island destination and every
	// other row starts roaming.
	ShipFlags_t spawnFlags {};
	SetNavigationDirection(spawnFlags, (rInfo.orders & ShipOrders::kIsFlagship) ? 6i64 : -1i64);
	rCurrentPostRender.pFlags[iIndex] = spawnFlags;
	rCurrentPostRender.pVecVelocities[iIndex] = rInfo.vecVelocity;
	rCurrentPostRender.pVecDamageDirections[iIndex] = XMVectorZero();
	rCurrentPostRender.pfHealths[iIndex] = rInfo.bTransfer ? rInfo.fHealth : kfFighterHealth;
	rCurrentPostRender.pDestroyedExplosionTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentPostRender.pNextBlasterSpawnTimes[iIndex] = rInfo.nextBlasterSpawnTime;
	rCurrentPostRender.pAlignments[iIndex] = rInfo.alignment;
	rCurrentPostRender.pArrivalGracePeriods[iIndex] = rInfo.arrivalGracePeriod;
	rCurrentPostRender.pGlobalIds[iIndex] = rInfo.globalId;
	rCurrentPostRender.pClientGuids[iIndex] = rInfo.clientGuid;
	rCurrentPostRender.pFlagshipGlobalIds[iIndex] = rInfo.flagshipGlobalId;
	rCurrentPostRender.pFleetWantedCoordinates[iIndex] = rInfo.fleetWantedCoordinate;
	rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[iIndex] = rInfo.uiPendingFleetWantedCoordinateTicks;
	rCurrentPostRender.pNavigationDelays[iIndex] = rInfo.navigationDelay;
	rCurrentPostRender.pCellChangeTimers[iIndex] = std::chrono::duration<float>(15.0f + common::Random<10.0f>(rFrame.postRender.randomEngine));
	rCurrentPostRender.pVecAiDirections[iIndex] = XMVectorZero();
	rCurrentPostRender.pVecIslandDestinations[iIndex] = XMVectorZero();
#if defined(BT_CLIENT)
	rCurrentPostRender.pVecDebugNavigationWaypoints[iIndex] = XMVectorZero();
#endif // BT_CLIENT

	// Sync owned objects after Add()
	SyncFighter(rFrame.interpolate, rCurrentInterpolate.pPushers[iIndex], rInfo.vecPosition);

	return true;
}

// The collision body's 3D sphere test against every other ship row of the previous frame, any alignment or lifecycle
// state.
static bool XM_CALLCONV OverlapsAnotherShip(const Frame& rPreviousFrame, int64_t iFighterRow, FXMVECTOR vecPosition)
{
	bool bOverlaps = false;
	ForEachShipType(rPreviousFrame, [&](ShipType eType, const auto& rShipInterpolate, [[maybe_unused]] const auto& rShipPostRender)
	{
		float fRadius = eType == ShipType::kFighter ? kfFighterRadius : eType == ShipType::kOmni ? kfOmniRadius : kfBattleshipRadius;
		float fCombinedRadius = kfFighterRadius + fRadius;
		for (int64_t i = 0; i < rShipInterpolate.iCount && !bOverlaps; ++i)
		{
			if (eType == ShipType::kFighter && i == iFighterRow)
			{
				continue;
			}
			bOverlaps = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, rShipInterpolate.pVecPositions[i]))) <= fCombinedRadius * fCombinedRadius;
		}
	});
	return bOverlaps;
}

void FightersPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdateFighters);

	FightersPostRender& __restrict rCurrent = *rFrame.postRender.pFighters;
	FightersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pFighters;
	const FightersPostRender& rPrevious = *rPreviousFrame.postRender.pFighters;
	const FightersInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pFighters;
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

	// Cell center: every cell's local frame is centered on the origin; W=1.0 keeps it a proper position.
	XMVECTOR vecCellCenter = XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		// Load from PostRender (static fields copied via memcpy in AllocateAndCopy)
		ShipFlags_t flags = rPrevious.pFlags[i];
		engine::LifecycleFlags_t lifecycleFlags = rPrevious.pLifecycleFlags[i];
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		float fHealth = rPrevious.pfHealths[i];
		std::chrono::duration<float> destroyedExplosionTime = rPrevious.pDestroyedExplosionTimes[i] - deltaTime;
		std::chrono::duration<float> nextBlasterSpawnTime = rPrevious.pNextBlasterSpawnTimes[i] - deltaTime;
		std::chrono::duration<float> arrivalGracePeriod = std::max(std::chrono::duration<float>::zero(), rPrevious.pArrivalGracePeriods[i] - deltaTime);
		engine::GridCoord fleetWantedCoordinate = rPrevious.pFleetWantedCoordinates[i];
		int64_t iPendingFleetWantedCoordinateTicks = rPrevious.puiPendingFleetWantedCoordinateTicks[i];
		std::chrono::duration<float> navigationDelay = rPrevious.pNavigationDelays[i];
		std::chrono::duration<float> cellChangeTimer = rPrevious.pCellChangeTimers[i];
		XMVECTOR vecAiDirection = rPrevious.pVecAiDirections[i];
		XMVECTOR vecIslandDestination = rPrevious.pVecIslandDestinations[i];
		int64_t iNavigationDirection = GetNavigationDirection(flags);
		int64_t iNavigationWaypointIndex = GetNavigationWaypointIndex(flags);

		float fDeltaRotation = rPreviousInterpolate.pfDeltaRotations[i];
		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];
		XMVECTOR vecDirection = rCurrentInterpolate.pVecDirections[i];

#if defined(BT_CLIENT)
		if constexpr (kbDebugRender)
		{
			// ComputeNavigation overwrites the debug waypoint only on a recompute tick, so carry it forward here.
			rCurrent.pVecDebugNavigationWaypoints[i] = rPrevious.pVecDebugNavigationWaypoints[i];
		}
#endif // BT_CLIENT

		CountdownPendingOrders(iPendingFleetWantedCoordinateTicks);

		if (!(lifecycleFlags & engine::LifecycleFlags::kDying)) [[likely]]
		{
			if (arrivalGracePeriod > std::chrono::duration<float>::zero())
			{
				ApplyPusherPush(rFrame, vecPosition, rCurrentInterpolate.pPushers[i], kfFighterMaxPusherPushVelocity, vecVelocity);
			}
			else if (rPrevious.pController[i] == ShipController::kFleet)
			{
				ComputeNavigation(rFrame, rPreviousFrame, rStaticData, {.eType = ShipType::kFighter, .iRow = i}, vecPosition, vecCellCenter, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, rPrevious.pOrders[i], rPrevious.pFlagshipGlobalIds[i], deltaTime, iNavigationDirection, iNavigationWaypointIndex, vecAiDirection, vecIslandDestination, cellChangeTimer);
				SteerHeadingTowards(vecPosition, vecDirection, XMVectorAdd(vecPosition, vecAiDirection), true, kfFighterChaseTurnRate, kfFighterSteeringSmoothing, kfFighterSteeringDecay, deltaTime, fDeltaRotation);
				ApplyHeadingMovement(vecDirection, flags, deltaTime, kFighterMovement, vecVelocity);
				ApplyPusherPush(rFrame, vecPosition, rCurrentInterpolate.pPushers[i], kfFighterMaxPusherPushVelocity, vecVelocity);
			}
			else
			{
				XMVECTOR vecNearestEnemy = XMVectorZero();
				bool bEnemyFound = FindNearestEnemyShip(rPreviousFrame, vecPosition, rPrevious.pAlignments[i], std::nullopt, vecNearestEnemy);

				RegenerateHealth(vecPosition, bEnemyFound, vecNearestEnemy, deltaTime, kfFighterHealth, fHealth);
				bool bHasDestination = false;
				XMVECTOR vecDestination = ComputeFleeOrReturnDestination(islandCandidates, vecPosition, bEnemyFound, vecNearestEnemy, flags, bHasDestination);
				float fTurnRate = ((flags & ShipFlags::kFlee) && !(flags & ShipFlags::kReturnToIslandCenter)) ? -kfFighterFleeTurnRate : kfFighterChaseTurnRate;
				SteerHeadingTowards(vecPosition, vecDirection, vecDestination, bHasDestination, fTurnRate, kfFighterSteeringSmoothing, kfFighterSteeringDecay, deltaTime, fDeltaRotation);
				ApplyHeadingMovement(vecDirection, flags, deltaTime, kFighterMovement, vecVelocity);
				ApplyPusherPush(rFrame, vecPosition, rCurrentInterpolate.pPushers[i], kfFighterMaxPusherPushVelocity, vecVelocity);
			}

			if ((rPrevious.pOrders[i] & ShipOrders::kSpawning) && !OverlapsAnotherShip(rPreviousFrame, i, vecPosition))
			{
				rCurrent.pOrders[i].Set(ShipOrders::kSpawning, false);
			}
		}
		else
		{
			vecVelocity = XMVectorScale(XMVectorNegate(rPrevious.pVecDamageDirections[i]), kfDeathKnockbackSpeed);
		}

		if (!SkipsTerrainContact(XMVectorGetZ(rCurrentInterpolate.pVecPositions[i])))
		{
			ApplyTerrainBounce(rStaticData, {.eType = ShipType::kFighter, .iRow = i}, rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], vecDirection, kfFighterRadius, deltaTime, fDeltaRotation, vecVelocity, GetTargetFlightPlane(rPrevious.pOrders[i]));
		}
		// Last velocity write, after pusher response, knockback, and bounce: a stored Z speed would reach the next
		// tick's ApplyHeadingMovement, which rescales its blend to the full 3D speed and would turn it into horizontal speed.
		vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);

		fDeltaRotation = common::ClampMagnitude(fDeltaRotation, kfFighterMaxTurnRate);

		SetNavigationDirection(flags, iNavigationDirection);
		SetNavigationWaypointIndex(flags, iNavigationWaypointIndex);
		rCurrent.pFlags[i] = flags;
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pfHealths[i] = fHealth;
		rCurrent.pDestroyedExplosionTimes[i] = destroyedExplosionTime;
		rCurrent.pNextBlasterSpawnTimes[i] = nextBlasterSpawnTime;
		rCurrent.pArrivalGracePeriods[i] = arrivalGracePeriod;
		rCurrent.pFleetWantedCoordinates[i] = fleetWantedCoordinate;
		rCurrent.puiPendingFleetWantedCoordinateTicks[i] = static_cast<uint8_t>(iPendingFleetWantedCoordinateTicks);
		rCurrent.pNavigationDelays[i] = navigationDelay;
		rCurrent.pCellChangeTimers[i] = cellChangeTimer;
		rCurrent.pVecAiDirections[i] = vecAiDirection;
		rCurrent.pVecIslandDestinations[i] = vecIslandDestination;

		rCurrentInterpolate.pfDeltaRotations[i] = fDeltaRotation;
	}

	FightersPostRender::AvoidTerrain(rFrame, rPreviousFrame, rStaticData, 0, rFrame.interpolate.pFighters->iCount);
}

bool FightersInterpolate::LogDifferences(const FightersInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("FightersInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
		bEqual &= common::LogDifference<"pDestroyedTimes">(i, pDestroyedTimes[i], rOther.pDestroyedTimes[i]);
		bEqual &= common::LogDifference<"pPushers">(i, pPushers[i], rOther.pPushers[i]);
		bEqual &= common::LogDifference<"pRegistryIds">(i, pRegistryIds[i], rOther.pRegistryIds[i]);
		bEqual &= common::LogDifference<"pfDeltaRotations">(i, pfDeltaRotations[i], rOther.pfDeltaRotations[i]);
		bEqual &= common::LogDifference<"puiFlightPlanes">(i, puiFlightPlanes[i], rOther.puiFlightPlanes[i]);
	}

	return bEqual;
}

bool FightersPostRender::LogDifferences(const FightersPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("FightersPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pIds">(i, pIds[i].uuid.iValue, rOther.pIds[i].uuid.iValue);
		bEqual &= common::LogDifference<"pLifecycleFlags">(i, pLifecycleFlags[i], rOther.pLifecycleFlags[i]);
		bEqual &= common::LogDifference<"pController">(i, std::to_underlying(pController[i]), std::to_underlying(rOther.pController[i]));
		bEqual &= common::LogDifference<"pOrders">(i, pOrders[i], rOther.pOrders[i]);
		bEqual &= common::LogDifference<"pFlags">(i, pFlags[i], rOther.pFlags[i]);
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pVecDamageDirections">(i, pVecDamageDirections[i], rOther.pVecDamageDirections[i]);
		bEqual &= common::LogDifference<"pfHealths">(i, pfHealths[i], rOther.pfHealths[i]);
		bEqual &= common::LogDifference<"pDestroyedExplosionTimes">(i, pDestroyedExplosionTimes[i], rOther.pDestroyedExplosionTimes[i]);
		bEqual &= common::LogDifference<"pNextBlasterSpawnTimes">(i, pNextBlasterSpawnTimes[i], rOther.pNextBlasterSpawnTimes[i]);
		bEqual &= common::LogDifference<"pAlignments">(i, pAlignments[i], rOther.pAlignments[i]);
		bEqual &= common::LogDifference<"pArrivalGracePeriods">(i, pArrivalGracePeriods[i], rOther.pArrivalGracePeriods[i]);
		bEqual &= common::LogDifference<"pGlobalIds">(i, pGlobalIds[i], rOther.pGlobalIds[i]);
		bEqual &= common::LogDifference<"pClientGuids.uiHigh">(i, pClientGuids[i].uiHigh, rOther.pClientGuids[i].uiHigh);
		bEqual &= common::LogDifference<"pClientGuids.uiLow">(i, pClientGuids[i].uiLow, rOther.pClientGuids[i].uiLow);
		bEqual &= common::LogDifference<"pFlagshipGlobalIds">(i, pFlagshipGlobalIds[i], rOther.pFlagshipGlobalIds[i]);
		bEqual &= common::LogDifference<"pFleetWantedCoordinates.iQ">(i, pFleetWantedCoordinates[i].iQ, rOther.pFleetWantedCoordinates[i].iQ);
		bEqual &= common::LogDifference<"pFleetWantedCoordinates.iR">(i, pFleetWantedCoordinates[i].iR, rOther.pFleetWantedCoordinates[i].iR);
		bEqual &= common::LogDifference<"puiPendingFleetWantedCoordinateTicks">(i, puiPendingFleetWantedCoordinateTicks[i], rOther.puiPendingFleetWantedCoordinateTicks[i]);
		bEqual &= common::LogDifference<"pNavigationDelays">(i, pNavigationDelays[i], rOther.pNavigationDelays[i]);
		bEqual &= common::LogDifference<"pCellChangeTimers">(i, pCellChangeTimers[i], rOther.pCellChangeTimers[i]);
		bEqual &= common::LogDifference<"pVecAiDirections">(i, pVecAiDirections[i], rOther.pVecAiDirections[i]);
		bEqual &= common::LogDifference<"pVecIslandDestinations">(i, pVecIslandDestinations[i], rOther.pVecIslandDestinations[i]);
	}

	return bEqual;
}

} // namespace game
