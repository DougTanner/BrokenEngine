#include "Omnis.h"

#include "Data/Texture.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipHitboxes.h"
#include "Frame/HealthDamage.h"
#if defined(BT_CLIENT)
#include "Frame/Collections/PointLights/PointLights.h"
#include "Frame/Collections/Puffs/Puffs.h"
#include "Ui/LightingWrappers.h"
#include "Ui/ParticleWrappers.h"
#include "Ui/SmokeWrappers.h"
#include "Ui/WindDepositsWrappers.h"
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
#if defined(BT_CLIENT)
		.pWindTrailIntensityWrapper = &gWindDepositOmniBlastersIntensity,
		.pWindTrailWidthWrapper = &gWindDepositOmniBlastersWidth,
		.pWindTrailLengthMultiplierWrapper = &gWindDepositOmniBlastersLengthMultiplier,
#endif
	});

#if defined(BT_CLIENT)
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
		.uiColor = 0xFFFFFFFF,
	});
	engine::PuffsInterpolate::RegisterControllerType(siImpactPuffControllerTypeIndex,
	{
		.iBaseTypeIndex = iImpactPuffTypeIndex,
		.iKeyframeCount = 2,
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

	// The Fighter beam's texture and middle column, tinted green.
	engine::AreaLightsInterpolate::RegisterType(siHealBeamAreaLightTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC72pngCrc,
		.puiColors = {0x00FF00FF, 0x00FF00FF, 0x00FF00FF, 0x00FF00FF}, // Opaque green (RRGGBBAA)
		.pf2TextureCoordinates = {{0.5f, 0.0f}, {0.5f, 1.0f}, {0.5f, 0.0f}, {0.5f, 1.0f}},
		.fLightingIntensity = 200.0f,
		.expandSides = {engine::AreaLightExpandSides::kLeft, engine::AreaLightExpandSides::kRight},
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
		return IsFinishedDying(rCurrentPostRender.pLifecycleFlags[i], rCurrentInterpolate.pDestroyedTimes[i]);
	},
	[&](int64_t i)
	{
		engine::MarkDestroy(rCurrentPostRender.pLifecycleFlags[i]);
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

	XMVECTOR vecPosition = rInfo.bTransfer ? rInfo.vecPosition : XMVectorSetZ(rInfo.vecPosition, EffectiveShipFlightHeight(ShipType::kOmni, rInfo.eController));
	rCurrentInterpolate.pVecPositions[iIndex] = vecPosition;
	rCurrentInterpolate.pVecDirections[iIndex] = rInfo.vecDirection;
	rCurrentInterpolate.pDestroyedTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentInterpolate.pPushers[iIndex] = {};
	engine::PushersPostRender::Add(rFrame, rCurrentInterpolate.pPushers[iIndex]);
	// Generate the missile-homing registry ID with one frame UUID after the pusher ID, preserving per-spawn UUID
	// order for replay.
	rCurrentInterpolate.pRegistryIds[iIndex] = engine::registry_id_t::Generate(rFrame.postRender);
#if defined(BT_CLIENT)
	rCurrentInterpolate.pAnimationTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentInterpolate.pfRotationAccelerationXs[iIndex] = 0.0f;
	rCurrentInterpolate.pfRotationAccelerationYs[iIndex] = 0.0f;
	rCurrentInterpolate.pWindTrails[iIndex] = {};
	rCurrentInterpolate.pHexShields[iIndex] = {};
	rCurrentInterpolate.pHealBeamAreaLights[iIndex] = {};
	rCurrentInterpolate.pfShieldRotations[iIndex] = 0.0f;
	rCurrentInterpolate.pfShieldShrinks[iIndex] = 1.0f;
	rCurrentInterpolate.pHexShieldDirections[iIndex] = {};
	rCurrentInterpolate.pHexShieldVertexIntensities[iIndex] = {};
	rCurrentInterpolate.pHexShieldFragmentIntensities[iIndex] = {};
#endif // BT_CLIENT

	rCurrentPostRender.pIds[iIndex] = newId;
	rCurrentPostRender.pController[iIndex] = rInfo.eController;
	rCurrentPostRender.pOrders[iIndex] = rInfo.orders;
	rCurrentPostRender.pAlignments[iIndex] = rInfo.alignment;
	rCurrentPostRender.pNextBlasterFireTimes[iIndex] = rInfo.nextBlasterFireTime;
	rCurrentPostRender.pVecVelocities[iIndex] = rInfo.vecVelocity;
	rCurrentPostRender.pVecWantedDirections[iIndex] = rInfo.vecDirection;
	rCurrentPostRender.pfArmors[iIndex] = rInfo.bTransfer ? rInfo.fArmor : kfOmniArmor;
	rCurrentPostRender.pfShields[iIndex] = rInfo.bTransfer ? rInfo.fShield : kfOmniShield;
	rCurrentPostRender.pShieldCooldowns[iIndex] = rInfo.shieldCooldown;
#if defined(BT_CLIENT)
	rCurrentPostRender.pDestroyedExplosionTimes[iIndex] = std::chrono::duration<float>::zero();
#endif
	rCurrentPostRender.pShieldDownSoundCooldowns[iIndex] = rInfo.shieldDownSoundCooldown;
	rCurrentPostRender.pVecAiDirections[iIndex] = rInfo.vecDirection;
	rCurrentPostRender.pTransferLockTimers[iIndex] = rInfo.transferLockTimer;
	rCurrentPostRender.pArrivalGracePeriods[iIndex] = rInfo.arrivalGracePeriod;
	rCurrentPostRender.pCellChangeTimers[iIndex] = std::chrono::duration<float>(15.0f + common::Random<10.0f>(rFrame.postRender.randomEngine));
	// Entering a cell is a fresh spawn for navigation, cross-cell transfers included: the wanted direction comes from the
	// hull direction, and the cached steering, island destination, navigation mode, and waypoint index reset here.
	// Flagship navigates to island destination on enter; non-flagship starts roaming and follows its flagship in the same cell
	// kHealing carries across a transfer. The AI direction starts as the heading with kNoAiDirection set until the first
	// bearing, so the Omni does not thrust along it.
	ShipFlags_t spawnFlags = rInfo.flags;
	SetNavigationDirection(spawnFlags, (rInfo.orders & ShipOrders::kIsFlagship) ? 6i64 : -1i64);
	SetNavigationWaypointIndex(spawnFlags, 0);
	spawnFlags.Set(ShipFlags::kNoAiDirection);
	rCurrentPostRender.pFlags[iIndex] = spawnFlags;
	rCurrentPostRender.pVecIslandDestinations[iIndex] = XMVectorZero();
	// Registry ids are per cell, so a transfer arrival starts without a kept target too.
	rCurrentPostRender.pRegistryTargets[iIndex] = {};
	rCurrentPostRender.pClientGuids[iIndex] = rInfo.clientGuid;
	rCurrentPostRender.pGlobalIds[iIndex] = rInfo.globalId;
	rCurrentPostRender.pFlagshipGlobalIds[iIndex] = rInfo.flagshipGlobalId;
	rCurrentPostRender.pFleetWantedCoordinates[iIndex] = rInfo.fleetWantedCoordinate;
	rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[iIndex] = rInfo.uiPendingFleetWantedCoordinateTicks;
#if defined(BT_CLIENT)
	rCurrentPostRender.pVecDebugNavigationWaypoints[iIndex] = XMVectorZero();
#endif // BT_CLIENT

	return true;
}

void OmnisPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	OmnisInterpolate& rCurrentInterpolate = *rFrame.interpolate.pOmnis;
	OmnisPostRender& rCurrentPostRender = *rFrame.postRender.pOmnis;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	siCollisionLayerIndex = AddShipCollisionLayer(rCurrentInterpolate, rCurrentPostRender, *rPreviousFrame.interpolate.pOmnis, ShipType::kOmni, CollisionCategory::kuiOmni, CollidesWith::kuiOmni);
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
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pVecWantedDirections">(i, pVecWantedDirections[i], rOther.pVecWantedDirections[i]);
		bEqual &= common::LogDifference<"pfArmors">(i, pfArmors[i], rOther.pfArmors[i]);
		bEqual &= common::LogDifference<"pfShields">(i, pfShields[i], rOther.pfShields[i]);
		bEqual &= common::LogDifference<"pShieldCooldowns">(i, pShieldCooldowns[i], rOther.pShieldCooldowns[i]);
		bEqual &= common::LogDifference<"pShieldDownSoundCooldowns">(i, pShieldDownSoundCooldowns[i], rOther.pShieldDownSoundCooldowns[i]);
		bEqual &= common::LogDifference<"pVecAiDirections">(i, pVecAiDirections[i], rOther.pVecAiDirections[i]);
		bEqual &= common::LogDifference<"pTransferLockTimers">(i, pTransferLockTimers[i], rOther.pTransferLockTimers[i]);
		bEqual &= common::LogDifference<"pArrivalGracePeriods">(i, pArrivalGracePeriods[i], rOther.pArrivalGracePeriods[i]);
		bEqual &= common::LogDifference<"pCellChangeTimers">(i, pCellChangeTimers[i], rOther.pCellChangeTimers[i]);
		bEqual &= common::LogDifference<"pVecIslandDestinations">(i, pVecIslandDestinations[i], rOther.pVecIslandDestinations[i]);
		bEqual &= common::LogDifference<"pRegistryTargets">(i, pRegistryTargets[i].uuid.iValue, rOther.pRegistryTargets[i].uuid.iValue);
		bEqual &= common::LogDifference<"pGlobalIds">(i, pGlobalIds[i], rOther.pGlobalIds[i]);
		bEqual &= common::LogDifference<"pFlagshipGlobalIds">(i, pFlagshipGlobalIds[i], rOther.pFlagshipGlobalIds[i]);
		bEqual &= common::LogDifference<"pFleetWantedCoordinates.iQ">(i, pFleetWantedCoordinates[i].iQ, rOther.pFleetWantedCoordinates[i].iQ);
		bEqual &= common::LogDifference<"pFleetWantedCoordinates.iR">(i, pFleetWantedCoordinates[i].iR, rOther.pFleetWantedCoordinates[i].iR);
		bEqual &= common::LogDifference<"puiPendingFleetWantedCoordinateTicks">(i, puiPendingFleetWantedCoordinateTicks[i], rOther.puiPendingFleetWantedCoordinateTicks[i]);
	}

	return bEqual;
}

} // namespace game
