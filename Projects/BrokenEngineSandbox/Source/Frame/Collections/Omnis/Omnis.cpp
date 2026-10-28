#include "Omnis.h"

#include "Data/Texture.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Frame/HealthDamage.h"
#include "Frame/TerrainUtils.h"
#include "Ui/ParticleWrappers.h"
#include "Ui/WindDepositsWrappers.h"
#if defined(BT_CLIENT)
#include "Data/Scene.h"
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Ui/LightingWrappers.h"
#include "Ui/SmokeWrappers.h"
#endif

namespace engine
{
template struct Collection<game::OmnisInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<game::OmnisPostRender>;
} // namespace engine

namespace game
{

constexpr float kfBlasterSizeX = kfOmniRadius * 0.3333f;
constexpr float kfBlasterSizeY = kfOmniRadius * 1.0f;


constexpr int64_t kiExplosionBaseParticleCount = 16;
constexpr float kfExplosionParticleVelocityMinimum = 5.0f;
constexpr float kfExplosionParticleVelocityRandom = 15.0f;
constexpr float kfExplosionParticleVerticalVelocityMinimum = 0.0f;
constexpr float kfExplosionParticleVerticalVelocityRandom = 20.0f;
constexpr float kfExplosionParticleIntensityDecay = 2.4f;

#if defined(BT_CLIENT)
constexpr std::chrono::duration<float> kImpactPointLightDuration(0.4f);

constexpr std::chrono::duration<float> kImpactPuffDuration(0.1f);
#endif // BT_CLIENT

constexpr float kfRotateTowardsSpeed = 10.0f;
#if defined(BT_CLIENT)
constexpr float kfShieldShrinkSpeed = 1.5f;
constexpr float kfShieldRotationSpeed = 4.0f;

constexpr float kfHexShieldSizeScale = kfOmniRadius * 1.334f;
constexpr float kfHexShieldColorMix = 0.85f;
constexpr float kfHexShieldFadeThreshold = 0.25f;

// Omni models, one per ShipController (defined in OmnisRender.cpp)
extern const common::crc_t kOmniFleetModel;
extern const common::crc_t kOmniRandomModel;

constexpr float kfRotationTiltFactor = 0.015f;
constexpr float kfRotationTiltMaximum = 0.4f;
#endif // BT_CLIENT

void OmnisInterpolate::Register()
{
#if defined(BT_CLIENT)
	engine::AreaLightsInterpolate::RegisterType(siAreaLightTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC74pngCrc,
		.puiColors = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		.pf2TextureCoordinates = {{1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}},
		.fVisibleIntensity = gOmniAreaLightVisibleIntensity.mfCurrent,
		.fLightingSize = gOmniBlasterLightingArea.mfCurrent,
		.fLightingIntensity = gOmniBlasterLightingIntensity.mfCurrent,
		.pVisibleIntensityWrapper = &gOmniAreaLightVisibleIntensity,
		.pLightingSizeWrapper = &gOmniBlasterLightingArea,
		.pLightingIntensityWrapper = &gOmniBlasterLightingIntensity,
	});
#endif // BT_CLIENT

	BlastersInterpolate::RegisterType(siBlasterTypeIndex,
	{
		.f2Size = {kfBlasterSizeX, kfBlasterSizeY},
		.iAreaLightTypeIndex = siAreaLightTypeIndex,
		.pWindTrailIntensityWrapper = &gWindDepositOmniBlastersIntensity,
		.pWindTrailWidthWrapper = &gWindDepositOmniBlastersWidth,
		.pWindTrailLengthMultiplierWrapper = &gWindDepositOmniBlastersLengthMultiplier,
	});

	engine::ExplosionsInterpolate::RegisterType(siExplosionTypeIndex,
	{
		.iBaseParticleCount = kiExplosionBaseParticleCount,
		.uiParticleColor = 0xFF0000FF,
		.fParticleVelocityMinimum = kfExplosionParticleVelocityMinimum,
		.fParticleVelocityRandom = kfExplosionParticleVelocityRandom,
		.fParticleVerticalVelocityMinimum = kfExplosionParticleVerticalVelocityMinimum,
		.fParticleVerticalVelocityRandom = kfExplosionParticleVerticalVelocityRandom,
		.fParticleIntensityDecay = kfExplosionParticleIntensityDecay,
		.pParticleWidthScale = &gOmniExplosionParticleWidth,
		.pParticleLengthScale = &gOmniExplosionParticleLength,
		.pParticleLengthSpreadScale = &gOmniExplosionParticleLengthSpread,
		.pParticlePositionJitterScale = &gOmniExplosionParticlePositionJitter,
		.pParticleVelocityBaseScale = &gOmniExplosionParticleVelocityBase,
		.pParticleVelocitySpreadScale = &gOmniExplosionParticleVelocitySpread,
		.pParticleVerticalVelocityBaseScale = &gOmniExplosionParticleVerticalVelocityBase,
		.pParticleVerticalVelocitySpreadScale = &gOmniExplosionParticleVerticalVelocitySpread,
		.pParticleVelocityDecayScale = &gOmniExplosionParticleVelocityDecay,
		.pParticleGravityScale = &gOmniExplosionParticleGravity,
		.pParticleVisibleIntensityScale = &gOmniExplosionParticleVisibleIntensity,
		.pParticleIntensitySpreadScale = &gOmniExplosionParticleIntensitySpread,
		.pParticleIntensityDecayScale = &gOmniExplosionParticleIntensityDecay,
		.pParticleIntensityPowerScale = &gOmniExplosionParticleIntensityPower,
	});

#if defined(BT_CLIENT)
	// Hits trigger the impact light controller.
	int64_t iImpactPointLightTypeIndex = 0xFF;
	engine::PointLightsInterpolate::RegisterType(iImpactPointLightTypeIndex,
	{
		.uiCrc = data::kTexturesBC7ExplosionpngCrc,
		.uiColor = 0xFFFFFFFF,
	});
	engine::PointLightsInterpolate::RegisterControllerType(siImpactPointLightControllerTypeIndex,
	{
		.iBaseTypeIndex = iImpactPointLightTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.pTimes = {std::chrono::duration<float>::zero(), kImpactPointLightDuration, std::chrono::duration<float>::zero(), std::chrono::duration<float>::zero()},
		.pKeyframes =
		{
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppVisibleAreaScales = {&gOmniImpactVisibleAreaOne, &gOmniImpactVisibleAreaTwo, nullptr, nullptr},
		.ppVisibleIntensityScales = {&gOmniImpactVisibleIntensityOne, &gOmniImpactVisibleIntensityTwo, nullptr, nullptr},
		.ppLightingAreaScales = {&gOmniImpactLightingAreaOne, &gOmniImpactLightingAreaTwo, nullptr, nullptr},
		.ppLightingIntensityScales = {&gOmniImpactLightingIntensityOne, &gOmniImpactLightingIntensityTwo, nullptr, nullptr},
	});

	// Hits trigger the impact puff controller.
	int64_t iImpactPuffTypeIndex = 0xFF;
	engine::PuffsInterpolate::RegisterType(iImpactPuffTypeIndex,
	{
		.uiCrc = data::kTexturesSmokeBC44jpgCrc,
		.uiColor = 0xFFFFFFFF,
	});
	engine::PuffsInterpolate::RegisterControllerType(siImpactPuffControllerTypeIndex,
	{
		.iBaseTypeIndex = iImpactPuffTypeIndex,
		.iKeyframeCount = 2,
		.bDestroysSelf = true,
		.pTimes = {std::chrono::duration<float>::zero(), kImpactPuffDuration, std::chrono::duration<float>::zero(), std::chrono::duration<float>::zero()},
		.pKeyframes =
		{
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppAreaScales = {&gOmniImpactPuffAreaOne, &gOmniImpactPuffAreaTwo, nullptr, nullptr},
		.ppIntensityScales = {&gOmniImpactPuffIntensityOne, &gOmniImpactPuffIntensityTwo, nullptr, nullptr},
	});

	engine::HexShieldsInterpolate::RegisterType(siHexShieldTypeIndex,
	{
		.uiColor = 0x40FFFF00,        // Cyan with 25% alpha (ABGR)
		.uiLightingColor = 0x40FFFF00, // Cyan (ABGR)
		.fMinimumIntensity = 0.0f,    // Shield invisible when idle
	});
#endif // BT_CLIENT
}

void OmnisPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
#if defined(BT_CLIENT)
		if (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)
		{
			engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.pWindTrails[i]);
			engine::ReleaseIndexableElementAndClearHandle(rFrame, rCurrentInterpolate.pHexShields[i]);
		}
#endif // BT_CLIENT
	}

	// Mark dead omnis for the compaction fold, which removes their pushers with them
	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) && rCurrentInterpolate.pDestroyedTimes[i].count() <= 0.0f;
	},
	[&](int64_t i)
	{
		rCurrentPostRender.pLifecycleFlags[i].Set(engine::LifecycleFlags::kDestroy);
	});
}

void OmnisPostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
#if defined(BT_CLIENT)
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (!(rCurrentInterpolate.pWindTrails[i].uuid.iValue != 0) && !(rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying))
		{
			engine::WindTrailsPostRender::Add(rFrame, rCurrentInterpolate.pWindTrails[i],
			{
				.vecPosition = rCurrentInterpolate.pVecPositions[i],
				.fIntensity = game::gWindDepositOmniIntensity.mfCurrent,
				.fWidth = game::gWindDepositOmniWidth.mfCurrent,
				.fLengthMultiplier = game::gWindDepositOmniLengthMultiplier.mfCurrent,
			});
		}

		if (!(rCurrentInterpolate.pHexShields[i].uuid.iValue != 0) && !(rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying))
		{
			engine::HexShieldsPostRender::Add(rFrame, rCurrentInterpolate.pHexShields[i], OmnisInterpolate::siHexShieldTypeIndex);
		}
	}
#endif // BT_CLIENT

	SpawnBlasters(rFrame, rStaticData.coordinate);
	SpawnMissiles(rFrame, rStaticData.coordinate);
	SpawnDeathExplosions(rFrame, rStaticData.coordinate);
}

bool OmnisPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo)
{
	// An omni outside its own cell is a bug, not gameplay.
	ASSERT(engine::IsInsideCell(rInfo.vecPosition));

	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	common::ValidateVector<true >(rInfo.vecPosition);
	common::ValidateVector<false>(rInfo.vecDirection);
	common::ValidateVector<false>(rInfo.vecVelocity);

	engine::GrowPairedCollections(rCurrentInterpolate, rCurrentPostRender, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	auto [iIndex, newId] = engine::AddIndexableElement(rCurrentInterpolate, rCurrentPostRender, rFrame.postRender);

	rCurrentInterpolate.pVecPositions[iIndex] = rInfo.vecPosition;
	rCurrentInterpolate.pVecDirections[iIndex] = rInfo.vecDirection;
	rCurrentInterpolate.pDestroyedTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentInterpolate.pAnimationTimes[iIndex] = rInfo.animationTime;
	rCurrentInterpolate.pPushers[iIndex] = {};
	engine::PushersPostRender::Add(rFrame, rCurrentInterpolate.pPushers[iIndex]);
	// Generate the missile-homing registry ID with one frame UUID after the pusher ID, preserving per-spawn UUID
	// order for replay.
	rCurrentInterpolate.pRegistryIds[iIndex] = engine::registry_id_t::Generate(rFrame.postRender);
	rCurrentInterpolate.puiFlightPlanes[iIndex] = rInfo.uiFlightPlane;
#if defined(BT_CLIENT)
	rCurrentInterpolate.pfRotationAccelerationXs[iIndex] = 0.0f;
	rCurrentInterpolate.pfRotationAccelerationYs[iIndex] = 0.0f;
	rCurrentInterpolate.pWindTrails[iIndex] = {};
	rCurrentInterpolate.pHexShields[iIndex] = {};
	rCurrentInterpolate.pfShieldRotations[iIndex] = rInfo.fShieldRotation;
	rCurrentInterpolate.pfShieldShrinks[iIndex] = rInfo.fShieldShrink;
	rCurrentInterpolate.pHexShieldDirections[iIndex] = {};
	rCurrentInterpolate.pHexShieldVertexIntensities[iIndex] = {};
	rCurrentInterpolate.pHexShieldFragmentIntensities[iIndex] = {};
#endif // BT_CLIENT

	rCurrentPostRender.pIds[iIndex] = newId;
	rCurrentPostRender.pController[iIndex] = rInfo.eController;
	rCurrentPostRender.pOrders[iIndex] = rInfo.orders;
	rCurrentPostRender.pAlignments[iIndex] = rInfo.alignment;
	rCurrentPostRender.pNextBlasterFireTimes[iIndex] = rInfo.nextBlasterFireTime;
	rCurrentPostRender.pNextSecondarySpawnTimes[iIndex] = rInfo.nextSecondarySpawnTime;
	rCurrentPostRender.pVecVelocities[iIndex] = rInfo.vecVelocity;
	rCurrentPostRender.pVecWantedDirections[iIndex] = rInfo.vecDirection;
	rCurrentPostRender.pfArmors[iIndex] = rInfo.bTransfer ? rInfo.fArmor : kfOmniArmor;
	rCurrentPostRender.pfShields[iIndex] = rInfo.bTransfer ? rInfo.fShield : kfOmniShield;
	rCurrentPostRender.pShieldCooldowns[iIndex] = rInfo.shieldCooldown;
	rCurrentPostRender.pDestroyedExplosionTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentPostRender.pShieldDownSoundCooldowns[iIndex] = rInfo.shieldDownSoundCooldown;
	rCurrentPostRender.pVecAiDirections[iIndex] = XMVectorZero();
	rCurrentPostRender.pTransferLockTimers[iIndex] = rInfo.transferLockTimer;
	rCurrentPostRender.pArrivalGracePeriods[iIndex] = rInfo.arrivalGracePeriod;
	rCurrentPostRender.pCellChangeTimers[iIndex] = std::chrono::duration<float>(15.0f + common::Random<10.0f>(rFrame.postRender.randomEngine));
	// Entering a cell is a fresh spawn for navigation, cross-cell transfers included: the wanted direction comes from the
	// hull direction, and the cached steering, island destination, navigation mode, and waypoint index reset here.
	// Flagship navigates to island destination on enter; non-flagship starts roaming and follows its flagship in the same cell
	ShipFlags_t spawnFlags = rInfo.flags;
	SetNavigationDirection(spawnFlags, (rInfo.orders & ShipOrders::kIsFlagship) ? 6i64 : -1i64);
	SetNavigationWaypointIndex(spawnFlags, 0);
	rCurrentPostRender.pFlags[iIndex] = spawnFlags;
	rCurrentPostRender.pNavigationDelays[iIndex] = rInfo.navigationDelay;
	rCurrentPostRender.pVecIslandDestinations[iIndex] = XMVectorZero();
	rCurrentPostRender.pClientGuids[iIndex] = rInfo.clientGuid;
	rCurrentPostRender.pGlobalIds[iIndex] = rInfo.globalId;
	rCurrentPostRender.pFlagshipGlobalIds[iIndex] = rInfo.flagshipGlobalId;
	rCurrentPostRender.pFleetWantedCoordinates[iIndex] = rInfo.fleetWantedCoordinate;
	rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[iIndex] = rInfo.uiPendingFleetWantedCoordinateTicks;
	rCurrentPostRender.puiPendingWeaponModeTicks[iIndex] = rInfo.uiPendingWeaponModeTicks;
#if defined(BT_CLIENT)
	rCurrentPostRender.pVecDebugNavigationWaypoints[iIndex] = XMVectorZero();
#endif // BT_CLIENT

	return true;
}


void OmnisInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	OmnisInterpolate& rCurrent = *rFrameInterpolate.pOmnis;
	const OmnisInterpolate& rPrevious = *rPreviousFrame.interpolate.pOmnis;
	const OmnisPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pOmnis;
	std::chrono::duration<float> deltaTime(rFrameInterpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		std::chrono::duration<float> destroyedTime = rPrevious.pDestroyedTimes[i];
		std::chrono::duration<float> animationTime = rPrevious.pAnimationTimes[i];
		uint8_t uiFlightPlane = rPrevious.puiFlightPlanes[i];

		if (!(rPreviousPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying)) [[likely]]
		{
			vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count()), rPreviousPostRender.pVecVelocities[i], vecPosition);
		}
		vecPosition = ClimbTowardsFlightPlane(vecPosition, GetTargetFlightPlane(rPreviousPostRender.pOrders[i]), kfOmniClimbRate, deltaTime, uiFlightPlane);
		// Enforce W=1.0 — prevents drift via MultiplyAdd's 4-lane propagation (pos.W += dt * vel.W).
		vecPosition = XMVectorSetW(vecPosition, 1.0f);

		// The hull turns only about Z: it follows the horizontal part of a wanted direction that may aim at another
		// plane, and holds when the target is exactly overhead.
		XMVECTOR vecWantedHorizontal = XMVectorSetZ(rPreviousPostRender.pVecWantedDirections[i], 0.0f);
		if (XMVectorGetX(XMVector3LengthSq(vecWantedHorizontal)) > 0.0f)
		{
			vecDirection = common::RotateTowardsPercent(vecDirection, XMVector3Normalize(vecWantedHorizontal), common::ExponentialInterpolant(kfRotateTowardsSpeed, deltaTime.count()));
		}

		if (rPreviousPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) [[unlikely]]
		{
			destroyedTime = std::max(destroyedTime - deltaTime, std::chrono::duration<float>::zero());
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pDestroyedTimes[i] = destroyedTime;
		rCurrent.pAnimationTimes[i] = animationTime;
		rCurrent.puiFlightPlanes[i] = uiFlightPlane;

		engine::PushersInterpolate::Sync(rFrameInterpolate, rCurrent.pPushers[i],
		{
			.vecPosition = vecPosition,
			.fRadius = kfOmniPusherRadius,
			.fIntensity = kfOmniPusherIntensity,
			.fPower = kfOmniPusherPower,
			.flags = {engine::PusherFlags::kTypeDefault},
		});

#if defined(BT_CLIENT)
		float fRotationAccelerationX = std::clamp(kfRotationTiltFactor * XMVectorGetX(rPreviousPostRender.pVecVelocities[i]), -kfRotationTiltMaximum, kfRotationTiltMaximum);
		float fRotationAccelerationY = std::clamp(-kfRotationTiltFactor * XMVectorGetY(rPreviousPostRender.pVecVelocities[i]), -kfRotationTiltMaximum, kfRotationTiltMaximum);
		rCurrent.pfRotationAccelerationXs[i] = fRotationAccelerationX;
		rCurrent.pfRotationAccelerationYs[i] = fRotationAccelerationY;

		float fShieldRotation = rPrevious.pfShieldRotations[i];
		float fShieldShrink = rPrevious.pfShieldShrinks[i];
		fShieldRotation += deltaTime.count() * kfShieldRotationSpeed;
		fShieldShrink = std::clamp(fShieldShrink + (rPreviousPostRender.pfShields[i] > 0.0f ? deltaTime.count() * kfShieldShrinkSpeed : -deltaTime.count() * kfShieldShrinkSpeed), 0.0f, 1.0f);
		rCurrent.pfShieldRotations[i] = fShieldRotation;
		rCurrent.pfShieldShrinks[i] = fShieldShrink;

		// Each row's clock advances through its own controller's model.
		common::crc_t uiModel = rPreviousPostRender.pController[i] == ShipController::kFleet ? kOmniFleetModel : kOmniRandomModel;
		if (engine::gAnimationDataMap.contains(uiModel))
		{
			animationTime = engine::gAnimationDataMap.at(uiModel).AdvanceLoopingTime(animationTime, deltaTime);
			rCurrent.pAnimationTimes[i] = animationTime;
		}

		if ((rCurrent.pWindTrails[i].uuid.iValue != 0))
		{
			engine::WindTrailsInterpolate::Sync(rFrameInterpolate, rCurrent.pWindTrails[i],
			{
				.vecPosition = vecPosition,
				.fIntensity = game::gWindDepositOmniIntensity.mfCurrent,
				.fWidth = game::gWindDepositOmniWidth.mfCurrent,
				.fLengthMultiplier = game::gWindDepositOmniLengthMultiplier.mfCurrent,
			});
		}

		HexShieldDirections hexShieldDirections = rPrevious.pHexShieldDirections[i];
		HexShieldIntensities hexShieldVertexIntensities {};
		HexShieldIntensities hexShieldFragmentIntensities {};
		for (int64_t j = 0; j < shaders::kiHexShieldDirections; ++j)
		{
			hexShieldVertexIntensities.pfData[j] = std::max(rPrevious.pHexShieldVertexIntensities[i].pfData[j] - gHexShieldIntensityDecay.mfCurrent * deltaTime.count(), 0.0f);
			hexShieldFragmentIntensities.pfData[j] = std::max(rPrevious.pHexShieldFragmentIntensities[i].pfData[j] - gHexShieldIntensityDecay.mfCurrent * deltaTime.count(), 0.0f);
		}
		rCurrent.pHexShieldDirections[i] = hexShieldDirections;
		rCurrent.pHexShieldVertexIntensities[i] = hexShieldVertexIntensities;
		rCurrent.pHexShieldFragmentIntensities[i] = hexShieldFragmentIntensities;

		if ((rCurrent.pHexShields[i].uuid.iValue != 0) && !(rPreviousPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying))
		{
			XMMATRIX matRotation = XMMatrixRotationZ(rCurrent.pfShieldRotations[i]);
			XMFLOAT3X4 f3x4Transform {};
			XMFLOAT3X4 f3x4TransformNormal {};
			XMStoreFloat3x4(&f3x4Transform, matRotation);
			XMStoreFloat3x4(&f3x4TransformNormal, XMMatrixTranspose(XMMatrixInverse(nullptr, matRotation)));

			engine::HexShieldsInterpolate::SyncData syncData
			{
				.vecPosition = rCurrent.pVecPositions[i],
				.pf4Transforms =
				{
					{f3x4Transform._11, f3x4Transform._12, f3x4Transform._13, f3x4Transform._14},
					{f3x4Transform._21, f3x4Transform._22, f3x4Transform._23, f3x4Transform._24},
					{f3x4Transform._31, f3x4Transform._32, f3x4Transform._33, f3x4Transform._34},
				},
				.pf4TransformNormals =
				{
					{f3x4TransformNormal._11, f3x4TransformNormal._12, f3x4TransformNormal._13, f3x4TransformNormal._14},
					{f3x4TransformNormal._21, f3x4TransformNormal._22, f3x4TransformNormal._23, f3x4TransformNormal._24},
					{f3x4TransformNormal._31, f3x4TransformNormal._32, f3x4TransformNormal._33, f3x4TransformNormal._34},
				},
				.pf4Directions = {},
				.pfVertexIntensities = {},
				.pfFragmentIntensities = {},
				.fLightingIntensity = gHexShieldLightingIntensity.mfCurrent,
				.fSize = rCurrent.pfShieldShrinks[i] * kfHexShieldSizeScale,
				.fColorMix = kfHexShieldColorMix,
			};

			// Copy direction arrays (per-slot smoothstep tail so fade-out eases to zero)
			for (int64_t j = 0; j < shaders::kiHexShieldDirections; ++j)
			{
				float fFragmentIntensity = rCurrent.pHexShieldFragmentIntensities[i].pfData[j];
				float fFade = std::clamp(fFragmentIntensity / kfHexShieldFadeThreshold, 0.0f, 1.0f);
				float fSmoothFade = fFade * fFade * (3.0f - 2.0f * fFade);
				syncData.pf4Directions[j] = rCurrent.pHexShieldDirections[i].pf4Data[j];
				syncData.pfVertexIntensities[j] = rCurrent.pHexShieldVertexIntensities[i].pfData[j] * fSmoothFade;
				syncData.pfFragmentIntensities[j] = fFragmentIntensity * fSmoothFade;
			}

			engine::HexShieldsInterpolate::Sync(rFrameInterpolate, rCurrent.pHexShields[i], syncData);
		}

#endif // BT_CLIENT
	}
}


void OmnisPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pOmnis->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.fRadius = kfOmniRadius,
		.uiCategory = CollisionCategory::kuiOmni,
		.uiCollidesWith = CollidesWith::kuiOmni,
		.pAlignments = rCurrentPostRender.pAlignments,
	});

	std::span<engine::CollisionFlags_t> flags = engine::Collision::GetLayerFlags(siCollisionLayerIndex);
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		flags[static_cast<size_t>(i)] = (rCurrentPostRender.pLifecycleFlags[i] & engine::LifecycleFlags::kDying) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {};
	}
}

// OmnisPostRender::Update orchestrates the shared per-row behaviours in ShipBehaviours.cpp and the helpers in OmnisCombat.cpp.

void OmnisPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisPostRender& __restrict rCurrent = *rFrame.postRender.pOmnis;
	const OmnisPostRender& rPrevious = *rPreviousFrame.postRender.pOmnis;
	const OmnisInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pOmnis;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	if (rCurrent.iCount == 0)
	{
		return;
	}

	// Cell center: every cell's local frame is centered on the origin.
	// W=1.0 keeps this a proper position — every downstream (cellCenter - vecPosition) and neighbour offset add stays W-clean,
	// so normalize fallbacks don't leak W into the AI direction and on into velocity.
	XMVECTOR vecCellCenter = XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);

	// Random-spawn destinations: island centers are fixed for this cell's tick; the per-thread workbuffer shares them across ships.
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
		ShipOrders_t orders = rPrevious.pOrders[i];
		ShipFlags_t flags = rPrevious.pFlags[i];
		std::chrono::duration<float> nextBlasterFireTime = rPrevious.pNextBlasterFireTimes[i];
		std::chrono::duration<float> nextSecondarySpawnTime = rPrevious.pNextSecondarySpawnTimes[i];
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		XMVECTOR vecWantedDirection = rPrevious.pVecWantedDirections[i];
		float fArmor = rPrevious.pfArmors[i];
		float fShield = rPrevious.pfShields[i];
		std::chrono::duration<float> shieldCooldown = std::max(std::chrono::duration<float>::zero(), rPrevious.pShieldCooldowns[i] - deltaTime);
		std::chrono::duration<float> destroyedExplosionTime = std::max(std::chrono::duration<float>::zero(), rPrevious.pDestroyedExplosionTimes[i] - deltaTime);
		std::chrono::duration<float> shieldDownSoundCooldown = std::max(std::chrono::duration<float>::zero(), rPrevious.pShieldDownSoundCooldowns[i] - deltaTime);
		XMVECTOR vecAiDirection = rPrevious.pVecAiDirections[i];
		std::chrono::duration<float> transferLockTimer = rPrevious.pTransferLockTimers[i];
		std::chrono::duration<float> arrivalGracePeriod = std::max(std::chrono::duration<float>::zero(), rPrevious.pArrivalGracePeriods[i] - deltaTime);
		std::chrono::duration<float> cellChangeTimer = rPrevious.pCellChangeTimers[i];
		std::chrono::duration<float> navigationDelay = rPrevious.pNavigationDelays[i];
		engine::GridCoord fleetWantedCoordinate = rPrevious.pFleetWantedCoordinates[i];
		int64_t iPendingFleetWantedCoordinateTicks = rPrevious.puiPendingFleetWantedCoordinateTicks[i];
		int64_t iPendingWeaponModeTicks = rPrevious.puiPendingWeaponModeTicks[i];
		int64_t iNavigationDirection = GetNavigationDirection(flags);
		int64_t iNavigationWaypointIndex = GetNavigationWaypointIndex(flags);
		XMVECTOR vecIslandDestination = rPrevious.pVecIslandDestinations[i];
		XMVECTOR vecPosition = rPreviousInterpolate.pVecPositions[i];

#if defined(BT_CLIENT)
		if constexpr (kbDebugRender)
		{
			// Debug nav waypoint persists across the staggered pathfind throttle: ComputeNavigation
			// overwrites it only on a recompute tick, so carry the previous tick's value forward here.
			// Also covers the transfer-lock path, where ComputeNavigation is skipped. Without this the
			// non-recompute ticks render stale buffer contents (line flashes to random positions).
			rCurrent.pVecDebugNavigationWaypoints[i] = rPrevious.pVecDebugNavigationWaypoints[i];
		}
#endif // BT_CLIENT

		CountdownPendingOrders(orders, iPendingFleetWantedCoordinateTicks, iPendingWeaponModeTicks);

		// Transfer lock skips navigation, target acquisition, and movement acceleration.
		if (transferLockTimer > std::chrono::duration<float>::zero())
		{
			transferLockTimer -= deltaTime;
		}
		else
		{
			if (rPrevious.pController[i] == ShipController::kFleet)
			{
				ComputeNavigation(rFrame, rPreviousFrame, rStaticData, {.eType = ShipType::kOmni, .iRow = i}, vecPosition, vecCellCenter, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, orders, rPrevious.pFlagshipGlobalIds[i], deltaTime, iNavigationDirection, iNavigationWaypointIndex, vecAiDirection, vecIslandDestination, cellChangeTimer);
			}
			else
			{
				XMVECTOR vecNearestEnemy = XMVectorZero();
				bool bEnemyFound = FindNearestEnemyShip(rPreviousFrame, vecPosition, rPrevious.pAlignments[i], std::nullopt, vecNearestEnemy);
				bool bHasDestination = false;
				XMVECTOR vecDestination = ComputeFleeOrReturnDestination(islandCandidates, vecPosition, bEnemyFound, vecNearestEnemy, flags, bHasDestination);
				if (bHasDestination)
				{
					vecAiDirection = XMVector3Normalize(XMVectorSetZ(XMVectorSubtract(vecDestination, vecPosition), 0.0f));
				}
			}

			bool bLookTargetFound = false;
			XMVECTOR vecLookPosition = XMVectorZero();
			AcquireTarget(rPreviousFrame, vecPosition, rPrevious.pAlignments[i], kfOmniBlastersSpeed, kfBlasterTargetRange, kfMissileTargetRange, flags, bLookTargetFound, vecLookPosition);

			float fAccelerationMultiplier = (1.0f - kfOmniJitterRange * 0.5f) + common::Random<kfOmniJitterRange>(rFrame.postRender.randomEngine);
			float fDecayMultiplier = (1.0f - kfOmniJitterRange * 0.5f) + common::Random<kfOmniJitterRange>(rFrame.postRender.randomEngine);
			ApplyAcceleratedMovement(iNavigationDirection, vecAiDirection, deltaTime, fAccelerationMultiplier, fDecayMultiplier, kOmniMovement, vecVelocity);

			if (bLookTargetFound)
			{
				UpdateFacing(vecPosition, vecLookPosition, vecWantedDirection);
			}
		}

		// Shield regeneration and pushes also run while transfer-locked. Terrain contact uses the sweep end's height.
		float fHeight = XMVectorGetZ(rFrame.interpolate.pOmnis->pVecPositions[i]);
		bool bTerrainContact = !SkipsTerrainContact(fHeight);
		RegenerateShield(deltaTime, shieldCooldown, kfOmniShield, kfOmniShieldRegeneration, fShield);
		if (bTerrainContact)
		{
			ApplyTerrainPush(rStaticData, vecPosition, kfOmniRadius, fHeight, kfPushMargin, vecVelocity);
		}
		float fPositionCorrectionX = 0.0f;
		ApplyPusherPush(rFrame, rFrame.interpolate.pOmnis->pPushers[i], kfOmniMaximumPusherPushVelocity, vecVelocity, fPositionCorrectionX);
		if (bTerrainContact)
		{
			ResolveTerrainContact(rStaticData, {.eType = ShipType::kOmni, .iRow = i}, vecPosition, rFrame.interpolate.pOmnis->pVecPositions[i], kfOmniRadius, vecVelocity, GetTargetFlightPlane(orders));
		}
		rFrame.interpolate.pOmnis->pVecPositions[i] = XMVectorSetX(rFrame.interpolate.pOmnis->pVecPositions[i], XMVectorGetX(rFrame.interpolate.pOmnis->pVecPositions[i]) + fPositionCorrectionX);
		// Only the climb moves height.
		vecVelocity = XMVectorSetZ(vecVelocity, 0.0f);

		SetNavigationDirection(flags, iNavigationDirection);
		SetNavigationWaypointIndex(flags, iNavigationWaypointIndex);
		rCurrent.pOrders[i] = orders;
		rCurrent.pFlags[i] = flags;
		rCurrent.pNextBlasterFireTimes[i] = nextBlasterFireTime;
		rCurrent.pNextSecondarySpawnTimes[i] = nextSecondarySpawnTime;
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pVecWantedDirections[i] = vecWantedDirection;
		rCurrent.pfArmors[i] = fArmor;
		rCurrent.pfShields[i] = fShield;
		rCurrent.pShieldCooldowns[i] = shieldCooldown;
		rCurrent.pDestroyedExplosionTimes[i] = destroyedExplosionTime;
		rCurrent.pShieldDownSoundCooldowns[i] = shieldDownSoundCooldown;
		rCurrent.pVecAiDirections[i] = vecAiDirection;
		rCurrent.pTransferLockTimers[i] = transferLockTimer;
		rCurrent.pArrivalGracePeriods[i] = arrivalGracePeriod;
		rCurrent.pCellChangeTimers[i] = cellChangeTimer;
		rCurrent.pNavigationDelays[i] = navigationDelay;
		rCurrent.pVecIslandDestinations[i] = vecIslandDestination;
		rCurrent.pFleetWantedCoordinates[i] = fleetWantedCoordinate;
		rCurrent.puiPendingFleetWantedCoordinateTicks[i] = static_cast<uint8_t>(iPendingFleetWantedCoordinateTicks);
		rCurrent.puiPendingWeaponModeTicks[i] = static_cast<uint8_t>(iPendingWeaponModeTicks);
	}
}

bool OmnisInterpolate::LogDifferences(const OmnisInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("OmnisInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
		bEqual &= common::LogDifference<"pDestroyedTimes">(i, pDestroyedTimes[i], rOther.pDestroyedTimes[i]);
		bEqual &= common::LogDifference<"pPushers">(i, pPushers[i], rOther.pPushers[i]);
		bEqual &= common::LogDifference<"pRegistryIds">(i, pRegistryIds[i], rOther.pRegistryIds[i]);
		bEqual &= common::LogDifference<"puiFlightPlanes">(i, puiFlightPlanes[i], rOther.puiFlightPlanes[i]);
	}

	return bEqual;
}

bool OmnisPostRender::LogDifferences(const OmnisPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("OmnisPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pIds">(i, pIds[i].uuid.iValue, rOther.pIds[i].uuid.iValue);
		bEqual &= common::LogDifference<"pLifecycleFlags">(i, pLifecycleFlags[i], rOther.pLifecycleFlags[i]);
		bEqual &= common::LogDifference<"pController">(i, std::to_underlying(pController[i]), std::to_underlying(rOther.pController[i]));
		bEqual &= common::LogDifference<"pOrders">(i, pOrders[i], rOther.pOrders[i]);
		bEqual &= common::LogDifference<"pFlags">(i, pFlags[i], rOther.pFlags[i]);
		bEqual &= common::LogDifference<"pAlignments">(i, pAlignments[i], rOther.pAlignments[i]);
		bEqual &= common::LogDifference<"pNextBlasterFireTimes">(i, pNextBlasterFireTimes[i], rOther.pNextBlasterFireTimes[i]);
		bEqual &= common::LogDifference<"pNextSecondarySpawnTimes">(i, pNextSecondarySpawnTimes[i], rOther.pNextSecondarySpawnTimes[i]);
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pVecWantedDirections">(i, pVecWantedDirections[i], rOther.pVecWantedDirections[i]);
		bEqual &= common::LogDifference<"pfArmors">(i, pfArmors[i], rOther.pfArmors[i]);
		bEqual &= common::LogDifference<"pfShields">(i, pfShields[i], rOther.pfShields[i]);
		bEqual &= common::LogDifference<"pShieldCooldowns">(i, pShieldCooldowns[i], rOther.pShieldCooldowns[i]);
		bEqual &= common::LogDifference<"pDestroyedExplosionTimes">(i, pDestroyedExplosionTimes[i], rOther.pDestroyedExplosionTimes[i]);
		bEqual &= common::LogDifference<"pShieldDownSoundCooldowns">(i, pShieldDownSoundCooldowns[i], rOther.pShieldDownSoundCooldowns[i]);
		bEqual &= common::LogDifference<"pVecAiDirections">(i, pVecAiDirections[i], rOther.pVecAiDirections[i]);
		bEqual &= common::LogDifference<"pTransferLockTimers">(i, pTransferLockTimers[i], rOther.pTransferLockTimers[i]);
		bEqual &= common::LogDifference<"pArrivalGracePeriods">(i, pArrivalGracePeriods[i], rOther.pArrivalGracePeriods[i]);
		bEqual &= common::LogDifference<"pCellChangeTimers">(i, pCellChangeTimers[i], rOther.pCellChangeTimers[i]);
		bEqual &= common::LogDifference<"pNavigationDelays">(i, pNavigationDelays[i], rOther.pNavigationDelays[i]);
		bEqual &= common::LogDifference<"pVecIslandDestinations">(i, pVecIslandDestinations[i], rOther.pVecIslandDestinations[i]);
		bEqual &= common::LogDifference<"pClientGuids.uiHigh">(i, pClientGuids[i].uiHigh, rOther.pClientGuids[i].uiHigh);
		bEqual &= common::LogDifference<"pClientGuids.uiLow">(i, pClientGuids[i].uiLow, rOther.pClientGuids[i].uiLow);
		bEqual &= common::LogDifference<"pGlobalIds">(i, pGlobalIds[i], rOther.pGlobalIds[i]);
		bEqual &= common::LogDifference<"pFlagshipGlobalIds">(i, pFlagshipGlobalIds[i], rOther.pFlagshipGlobalIds[i]);
		bEqual &= common::LogDifference<"pFleetWantedCoordinates.iQ">(i, pFleetWantedCoordinates[i].iQ, rOther.pFleetWantedCoordinates[i].iQ);
		bEqual &= common::LogDifference<"pFleetWantedCoordinates.iR">(i, pFleetWantedCoordinates[i].iR, rOther.pFleetWantedCoordinates[i].iR);
		bEqual &= common::LogDifference<"puiPendingFleetWantedCoordinateTicks">(i, puiPendingFleetWantedCoordinateTicks[i], rOther.puiPendingFleetWantedCoordinateTicks[i]);
		bEqual &= common::LogDifference<"puiPendingWeaponModeTicks">(i, puiPendingWeaponModeTicks[i], rOther.puiPendingWeaponModeTicks[i]);
	}

	return bEqual;
}

} // namespace game
