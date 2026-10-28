#include "Battleships.h"

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
template struct Collection<game::BattleshipsInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<game::BattleshipsPostRender>;
} // namespace engine

namespace game
{

constexpr float kfDeathKnockbackSpeed = 20.0f;

constexpr float kfBattleshipExplosionParticleCount = 8.0f;
constexpr float kfBattleshipExplosionSizeStart = kfBattleshipRadius * 0.625f;
constexpr float kfBattleshipExplosionSizeEnd = kfBattleshipRadius * 0.25f;
constexpr float kfBattleshipExplosionSmoke = 0.5f;
constexpr float kfBattleshipExplosionPositionJitter = kfBattleshipRadius * 0.375f;
constexpr float kfBattleshipExplosionDirectionJitter = 0.5f;
constexpr int64_t kiBattleshipExplosionTrailCount = 5;

static void RegisterBattleshipBlasterType();

// Shared type indices (accessible from BattleshipsCombat.cpp via extern)
int64_t giBattleshipExplosionTypeIndex = 0xFF;
#if defined(BT_CLIENT)
static int64_t siBattleshipHitFlashTypeIndex = 0xFF;
int64_t giBattleshipHitFlashControllerTypeIndex = 0xFF;

static void RegisterBattleshipHitFlashEffect();
#endif

constexpr int64_t kiBattleshipExplosionBaseParticleCount = 16;
constexpr uint32_t kuiBattleshipExplosionParticleColor = 0xFF0000FF;
constexpr float kfBattleshipExplosionParticleVelocityMinimum = 1.0f;
constexpr float kfBattleshipExplosionParticleVelocityRandom = 9.0f;
constexpr float kfBattleshipExplosionParticleVerticalVelocityMinimum = -5.0f;
constexpr float kfBattleshipExplosionParticleVerticalVelocityRandom = 10.0f;
constexpr float kfBattleshipExplosionParticleIntensityDecay = 2.4f;
constexpr float kfBattleshipExplosionTrailLengthRandom = kfBattleshipRadius * 1.25f;
constexpr int64_t kiBattleshipExplosionSecondaryCount = 1;

constexpr float kfBlastersSpeed = 50.0f;
constexpr std::chrono::duration<float> kBlastersSpawnCooldown = std::chrono::duration<float>(1.0f);

constexpr float kfBattleshipBlasterSize = kfBattleshipRadius * 0.15f;
// Battleship blaster lighting reuses the Fighter tuning in LightingWrappers (gFighterBlaster*).

#if defined(BT_CLIENT)
constexpr std::chrono::duration<float> kHitFlashDuration(0.3f);
#endif // BT_CLIENT

void BattleshipsInterpolate::Register()
{
	engine::ExplosionsInterpolate::RegisterType(giBattleshipExplosionTypeIndex,
	{
		.iBaseParticleCount = kiBattleshipExplosionBaseParticleCount,
		.uiParticleColor = kuiBattleshipExplosionParticleColor,
		.fParticleVelocityMinimum = kfBattleshipExplosionParticleVelocityMinimum,
		.fParticleVelocityRandom = kfBattleshipExplosionParticleVelocityRandom,
		.fParticleVerticalVelocityMinimum = kfBattleshipExplosionParticleVerticalVelocityMinimum,
		.fParticleVerticalVelocityRandom = kfBattleshipExplosionParticleVerticalVelocityRandom,
		.fParticleIntensityDecay = kfBattleshipExplosionParticleIntensityDecay,
		.fTrailLengthRandom = kfBattleshipExplosionTrailLengthRandom,
		.iSecondaryExplosionCount = kiBattleshipExplosionSecondaryCount,
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

	RegisterBattleshipBlasterType();
#if defined(BT_CLIENT)
	RegisterBattleshipHitFlashEffect();
#endif
}

static int64_t siBattleshipBlasterPointLightTypeIndex = 0xFF;
static int64_t siBattleshipBlasterTypeIndex = 0xFF;

static void RegisterBattleshipBlasterType()
{
#if defined(BT_CLIENT)
	engine::PointLightsInterpolate::RegisterType(siBattleshipBlasterPointLightTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC72pngCrc,
		.uiColor = 0xFFFFFFFF,
		.fVisibleArea = kfBattleshipBlasterSize,
		.bCameraAligned = true,
		.pVisibleIntensityWrapper = &gFighterBlasterVisibleIntensity,
		.pLightingAreaWrapper = &gFighterBlasterLightingArea,
		.pLightingIntensityWrapper = &gFighterBlasterLightingIntensity,
	});
#endif // BT_CLIENT

	BlastersInterpolate::RegisterType(siBattleshipBlasterTypeIndex,
	{
		.f2Size = {kfBattleshipBlasterSize, kfBattleshipBlasterSize},
		.iPointLightTypeIndex = siBattleshipBlasterPointLightTypeIndex,
		.pWindTrailIntensityWrapper = &gWindDepositFightersBlastersIntensity,
		.pWindTrailWidthWrapper = &gWindDepositFightersBlastersWidth,
		.pWindTrailLengthMultiplierWrapper = &gWindDepositFightersBlastersLengthMultiplier,
	});
}

// The registry binds this collection's position column directly, so its ID needs no per-tick sync.
static void XM_CALLCONV SyncBattleship(FrameInterpolate& rFrameInterpolate, engine::pusher_t uiPusher, FXMVECTOR vecPosition)
{
	engine::PushersInterpolate::Sync(rFrameInterpolate, uiPusher,
	{
		.vecPosition = vecPosition,
		.fRadius = kfBattleshipPusherRadius,
		.fIntensity = kfBattleshipPusherIntensity,
		.fPower = kfBattleshipPusherPower,
		.flags = {engine::PusherFlags::kTypeDefault},
	});
}

#if defined(BT_CLIENT)
static void RegisterBattleshipHitFlashEffect()
{
	engine::PointLightsInterpolate::RegisterType(siBattleshipHitFlashTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC74pngCrc,
		.uiColor = 0xFFFFFFFF,
	});

	engine::PointLightsInterpolate::RegisterControllerType(giBattleshipHitFlashControllerTypeIndex,
	{
		.iBaseTypeIndex = siBattleshipHitFlashTypeIndex,
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

void SpawnBattleshipExplosion(Frame& __restrict rFrame, engine::GridCoord emitterCoordinate, XMVECTOR vecPosition, XMVECTOR vecDirection, float fPercent)
{
	XMVECTOR vecJitteredPosition = common::RandomPositionJitter<kfBattleshipExplosionPositionJitter>(vecPosition, rFrame.postRender.randomEngine);
	XMVECTOR vecJitteredDirection = common::RandomDirectionJitter<kfBattleshipExplosionDirectionJitter>(vecDirection, rFrame.postRender.randomEngine);

	engine::ExplosionsPostRender::Spawn(rFrame, emitterCoordinate, engine::TickToSimulationTime(rFrame.interpolate.iTick),
	{
		.iTypeIndex = giBattleshipExplosionTypeIndex,
		.vecPosition = vecJitteredPosition,
		.vecDirection = vecJitteredDirection,
		.flags = {engine::ExplosionFlags::kRed},
		.lifecycleFlags = {engine::LifecycleFlags::kDying},
		.iTrailCount = kiBattleshipExplosionTrailCount,
		.fTrailAngle = fPercent * XM_PI,
		.iParticleCount = static_cast<int64_t>(fPercent * kfBattleshipExplosionParticleCount),
		.fParticleAngle = fPercent * XM_PIDIV2,
		.fSizePercent = fPercent * kfBattleshipExplosionSizeStart + (1.0f - fPercent) * kfBattleshipExplosionSizeEnd,
		.fSmokePercent = fPercent * kfBattleshipExplosionSmoke,
		.fTimePercent = fPercent,
	});
}

#if defined(BT_CLIENT)
// Battleship models, one per ShipController (defined in BattleshipsRender.cpp)
extern const common::crc_t kBattleshipFleetModel;
extern const common::crc_t kBattleshipRandomModel;

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

void BattleshipsInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rCurrentFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateUpdateBattleships);

	BattleshipsInterpolate& rCurrent = *rCurrentFrameInterpolate.pBattleships;
	const BattleshipsInterpolate& rPrevious = *rPreviousFrame.interpolate.pBattleships;
	const BattleshipsPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pBattleships;
	std::chrono::duration<float> deltaTime(rCurrentFrameInterpolate.fDeltaTime);

#if defined(BT_CLIENT)
	// Indexed by ShipController: each row's clock advances through its own model's animation.
	std::array<const engine::AnimationData*, std::to_underlying(ShipController::kCount)> animationDatas {LoopingAnimationData(kBattleshipFleetModel), LoopingAnimationData(kBattleshipRandomModel)};
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
		// PostRender Update stores velocity Z as 0, so the climb is what moves a ship's height.
		vecPosition = ClimbTowardsFlightPlane(vecPosition, GetTargetFlightPlane(rPreviousPostRender.pOrders[i]), kfBattleshipClimbRate, deltaTime, uiFlightPlane);
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
		rCurrent.puiFlightPlanes[i] = uiFlightPlane;
#if defined(BT_CLIENT)
		rCurrent.pAnimationTimes[i] = animationTime;
#endif

		// Sync owned objects (IDs copied in AllocateAndCopy)
		SyncBattleship(rCurrentFrameInterpolate, rCurrent.pPushers[i], vecPosition);

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

void BattleshipsInterpolate::AllocateAndCopy(BattleshipsInterpolate& rCurrent, const BattleshipsInterpolate& rPrevious)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerInterpolateAllocateAndCopyBattleships);
	engine::AllocateAndCopyMembers(rCurrent, rPrevious);
}

void BattleshipsPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

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
				.nextRearBlasterSpawnTime = rCurrentPostRender.pNextRearBlasterSpawnTimes[i],
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
			LOG(kDefault, kError, "Battleship Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Direction: {} Health: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iQ, rStaticData.coordinate.iR, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDirection), common::Wb(rCurrentPostRender.pfHealths[i], 1), rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, std::ssize(rFrame.postRender.transferRequests), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		rFrame.postRender.PushTransferRequest(request);
	});
}

void BattleshipsPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && !(rCurrentInterpolate.pDestroyedTimes[i] > std::chrono::duration<float>::zero());
	}, [&](int64_t i)
	{
		rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDestroy);
	});
}

// One gun: once its cooldown has run out, shoots flat from the ship's position at the nearest visible enemy on the
// ship's flight plane within the arc about vecAxisNormal.
static void XM_CALLCONV FireGun(Frame& __restrict rFrame, [[maybe_unused]] engine::GridCoord emitterCoordinate, int64_t i, FXMVECTOR vecAxisNormal, std::chrono::duration<float>& rNextBlasterSpawnTime)
{
	const BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	const BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	if (!(rNextBlasterSpawnTime < std::chrono::duration<float>::zero()))
	{
		return;
	}

	XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];
	XMVECTOR vecTarget = XMVectorZero();
	if (!FindNearestEnemyShipInArc(rFrame, vecPosition, vecAxisNormal, kfBattleshipBlasterArcHalfAngle, rCurrentPostRender.pAlignments[i], rCurrentInterpolate.puiFlightPlanes[i], vecTarget))
	{
		return;
	}

	rNextBlasterSpawnTime = kBlastersSpawnCooldown;

	XMVECTOR vecBlasterVelocity = XMVectorScale(XMVector3Normalize(XMVectorSetZ(XMVectorSubtract(vecTarget, vecPosition), 0.0f)), kfBlastersSpeed);

	BlastersPostRender::Spawn(rFrame,
	{
		.vecPosition = vecPosition,
		.vecVelocity = vecBlasterVelocity,
		.iTypeIndex = siBattleshipBlasterTypeIndex,
		.alignment = rCurrentPostRender.pAlignments[i],
	});

#if defined(BT_CLIENT)
	engine::gpAudioManager->PlayOneShot3d(rFrame, data::kAudioBlaster514039__newlocknew__blastershot6sytrusrsmplmultiprcsngsinglewavCrc, emitterCoordinate, vecPosition, gFighterBlasterVolume.mfCurrent, gFighterBlasterPitchMinimum.mfCurrent, gFighterBlasterPitchRandom.mfCurrent);
#endif
}

void BattleshipsPostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if ((rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && rCurrentPostRender.pDestroyedExplosionTimes[i].count() <= 0.0f)
		{
			rCurrentPostRender.pDestroyedExplosionTimes[i] = kBattleshipDestroyExplosionInterval;

			float fPercent = rCurrentInterpolate.pDestroyedTimes[i] / kBattleshipDestroyTime;
			XMVECTOR vecDirection = XMVector3Normalize(rCurrentPostRender.pVecVelocities[i]);
			SpawnBattleshipExplosion(rFrame, rStaticData.coordinate, rCurrentInterpolate.pVecPositions[i], vecDirection, fPercent);
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
		if (rCurrentInterpolate.puiFlightPlanes[i] == kuiNoFlightPlane)
		{
			continue;
		}

		// Front gun first, then rear: the two arcs are disjoint, so one enemy is in reach of at most one gun.
		XMVECTOR vecDirection = rCurrentInterpolate.pVecDirections[i];
		FireGun(rFrame, rStaticData.coordinate, i, vecDirection, rCurrentPostRender.pNextBlasterSpawnTimes[i]);
		FireGun(rFrame, rStaticData.coordinate, i, XMVectorNegate(vecDirection), rCurrentPostRender.pNextRearBlasterSpawnTimes[i]);
	}
}

#if defined(BT_CLIENT)
void BattleshipsInterpolate::ClientInitialize(Frame& rFrame, int64_t iIndex)
{
	BattleshipsInterpolate& rBattleships = *rFrame.interpolate.pBattleships;

	rBattleships.pWindTrails[iIndex] = {};
	engine::WindTrailsPostRender::Add(rFrame, rBattleships.pWindTrails[iIndex],
	{
		.vecPosition = rBattleships.pVecPositions[iIndex],
		.fIntensity = game::gWindDepositFightersIntensity.mfCurrent,
		.fWidth = game::gWindDepositFightersWidth.mfCurrent,
		.fLengthMultiplier = game::gWindDepositFightersLengthMultiplier.mfCurrent,
	});
}

void BattleshipsInterpolate::ClientInitializeAll(Frame& rFrame)
{
	BattleshipsInterpolate& rBattleships = *rFrame.interpolate.pBattleships;
	for (int64_t i = 0; i < rBattleships.iCount; ++i)
	{
		if (rBattleships.pDestroyedTimes[i].count() >= 0.0f)
		{
			continue;
		}
		ClientInitialize(rFrame, i);
	}
}
#endif // BT_CLIENT

bool BattleshipsPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo)
{
	if (!engine::IsInsideCell(rInfo.vecPosition))
	{
		return false;
	}

	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	BattleshipsPostRender& rCurrentPostRender = *rFrame.postRender.pBattleships;

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
#endif

	rCurrentInterpolate.pPushers[iIndex] = {};
	engine::PushersPostRender::Add(rFrame, rCurrentInterpolate.pPushers[iIndex]);

#if defined(BT_CLIENT)
	BattleshipsInterpolate::ClientInitialize(rFrame, iIndex);
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
	rCurrentPostRender.pfHealths[iIndex] = rInfo.bTransfer ? rInfo.fHealth : kfBattleshipHealth;
	rCurrentPostRender.pDestroyedExplosionTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentPostRender.pNextBlasterSpawnTimes[iIndex] = rInfo.nextBlasterSpawnTime;
	rCurrentPostRender.pNextRearBlasterSpawnTimes[iIndex] = rInfo.nextRearBlasterSpawnTime;
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
	SyncBattleship(rFrame.interpolate, rCurrentInterpolate.pPushers[iIndex], rInfo.vecPosition);

	return true;
}

void BattleshipsPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdateBattleships);

	BattleshipsPostRender& __restrict rCurrent = *rFrame.postRender.pBattleships;
	BattleshipsInterpolate& rCurrentInterpolate = *rFrame.interpolate.pBattleships;
	const BattleshipsPostRender& rPrevious = *rPreviousFrame.postRender.pBattleships;
	const BattleshipsInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pBattleships;
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
		std::chrono::duration<float> nextRearBlasterSpawnTime = rPrevious.pNextRearBlasterSpawnTimes[i] - deltaTime;
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
		float fPositionCorrectionX = 0.0f;

		if (!(lifecycleFlags & engine::LifecycleFlags::kDying)) [[likely]]
		{
			if (arrivalGracePeriod > std::chrono::duration<float>::zero())
			{
				ApplyPusherPush(rFrame, rCurrentInterpolate.pPushers[i], kfBattleshipMaxPusherPushVelocity, vecVelocity, fPositionCorrectionX);
			}
			else if (rPrevious.pController[i] == ShipController::kFleet)
			{
				ComputeNavigation(rFrame, rPreviousFrame, rStaticData, {.eType = ShipType::kBattleship, .iRow = i}, vecPosition, vecCellCenter, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, rPrevious.pOrders[i], rPrevious.pFlagshipGlobalIds[i], deltaTime, iNavigationDirection, iNavigationWaypointIndex, vecAiDirection, vecIslandDestination, cellChangeTimer);
				SteerHeadingTowards(vecPosition, vecDirection, XMVectorAdd(vecPosition, vecAiDirection), true, kfBattleshipChaseTurnRate, kfBattleshipSteeringSmoothing, kfBattleshipSteeringDecay, deltaTime, fDeltaRotation);
				ApplyHeadingMovement(vecDirection, flags, deltaTime, kBattleshipMovement, vecVelocity);
				ApplyPusherPush(rFrame, rCurrentInterpolate.pPushers[i], kfBattleshipMaxPusherPushVelocity, vecVelocity, fPositionCorrectionX);
			}
			else
			{
				XMVECTOR vecNearestEnemy = XMVectorZero();
				bool bEnemyFound = FindNearestEnemyShip(rPreviousFrame, vecPosition, rPrevious.pAlignments[i], std::nullopt, vecNearestEnemy);

				RegenerateHealth(vecPosition, bEnemyFound, vecNearestEnemy, deltaTime, kfBattleshipHealth, fHealth);
				bool bHasDestination = false;
				XMVECTOR vecDestination = ComputeFleeOrReturnDestination(islandCandidates, vecPosition, bEnemyFound, vecNearestEnemy, flags, bHasDestination);
				float fTurnRate = ((flags & ShipFlags::kFlee) && !(flags & ShipFlags::kReturnToIslandCenter)) ? -kfBattleshipFleeTurnRate : kfBattleshipChaseTurnRate;
				SteerHeadingTowards(vecPosition, vecDirection, vecDestination, bHasDestination, fTurnRate, kfBattleshipSteeringSmoothing, kfBattleshipSteeringDecay, deltaTime, fDeltaRotation);
				ApplyHeadingMovement(vecDirection, flags, deltaTime, kBattleshipMovement, vecVelocity);
				ApplyPusherPush(rFrame, rCurrentInterpolate.pPushers[i], kfBattleshipMaxPusherPushVelocity, vecVelocity, fPositionCorrectionX);
			}
		}
		else
		{
			vecVelocity = XMVectorScale(XMVectorNegate(rPrevious.pVecDamageDirections[i]), kfDeathKnockbackSpeed);
		}

		if (!SkipsTerrainContact(XMVectorGetZ(rCurrentInterpolate.pVecPositions[i])))
		{
			ApplyTerrainBounce(rStaticData, {.eType = ShipType::kBattleship, .iRow = i}, rPreviousInterpolate.pVecPositions[i], rCurrentInterpolate.pVecPositions[i], vecDirection, kfBattleshipRadius, deltaTime, fDeltaRotation, vecVelocity, GetTargetFlightPlane(rPrevious.pOrders[i]));
		}
		rCurrentInterpolate.pVecPositions[i] = XMVectorSetX(rCurrentInterpolate.pVecPositions[i], XMVectorGetX(rCurrentInterpolate.pVecPositions[i]) + fPositionCorrectionX);
		// Last velocity write, after pusher response, knockback, and bounce: a stored Z speed would reach the next
		// tick's ApplyHeadingMovement, which rescales its blend to the full 3D speed and would turn it into horizontal speed.
		vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);

		fDeltaRotation = common::ClampMagnitude(fDeltaRotation, kfBattleshipMaxTurnRate);

		SetNavigationDirection(flags, iNavigationDirection);
		SetNavigationWaypointIndex(flags, iNavigationWaypointIndex);
		rCurrent.pFlags[i] = flags;
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pfHealths[i] = fHealth;
		rCurrent.pDestroyedExplosionTimes[i] = destroyedExplosionTime;
		rCurrent.pNextBlasterSpawnTimes[i] = nextBlasterSpawnTime;
		rCurrent.pNextRearBlasterSpawnTimes[i] = nextRearBlasterSpawnTime;
		rCurrent.pArrivalGracePeriods[i] = arrivalGracePeriod;
		rCurrent.pFleetWantedCoordinates[i] = fleetWantedCoordinate;
		rCurrent.puiPendingFleetWantedCoordinateTicks[i] = static_cast<uint8_t>(iPendingFleetWantedCoordinateTicks);
		rCurrent.pNavigationDelays[i] = navigationDelay;
		rCurrent.pCellChangeTimers[i] = cellChangeTimer;
		rCurrent.pVecAiDirections[i] = vecAiDirection;
		rCurrent.pVecIslandDestinations[i] = vecIslandDestination;

		rCurrentInterpolate.pfDeltaRotations[i] = fDeltaRotation;
	}

	BattleshipsPostRender::AvoidTerrain(rFrame, rPreviousFrame, rStaticData, 0, rFrame.interpolate.pBattleships->iCount);
}

bool BattleshipsInterpolate::LogDifferences(const BattleshipsInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("BattleshipsInterpolate");
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

bool BattleshipsPostRender::LogDifferences(const BattleshipsPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("BattleshipsPostRender");
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
		bEqual &= common::LogDifference<"pNextRearBlasterSpawnTimes">(i, pNextRearBlasterSpawnTimes[i], rOther.pNextRearBlasterSpawnTimes[i]);
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
