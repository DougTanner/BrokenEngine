#include "Players.h"

#include "Data/Texture.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"

#include "Frame/Collections/Blasters/Blasters.h"
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
template struct Collection<game::PlayersInterpolate, CollectionFlags::kIdToIndex>;
template struct Collection<game::PlayersPostRender>;
} // namespace engine

namespace game
{

using enum PlayerFlags;

constexpr float kfBlasterSizeX = kfPlayerRadius * 0.3333f;
constexpr float kfBlasterSizeY = kfPlayerRadius * 1.0f;


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

constexpr float kfHexShieldSizeScale = kfPlayerRadius * 0.0667f;
constexpr float kfHexShieldColorMix = 0.85f;
constexpr float kfHexShieldFadeThreshold = 0.25f;

// Player model (defined in PlayersRender.cpp)
extern const common::crc_t kPlayerModel;

constexpr float kfRotationTiltFactor = 0.015f;
constexpr float kfRotationTiltMaximum = 0.4f;
#endif // BT_CLIENT

void PlayersInterpolate::Register()
{
#if defined(BT_CLIENT)
	engine::AreaLightsInterpolate::RegisterType(siAreaLightTypeIndex,
	{
		.uiCrc = data::kTexturesBlasterBC74pngCrc,
		.puiColors = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
		.pf2TextureCoordinates = {{1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}},
		.fVisibleIntensity = gPlayerAreaLightVisibleIntensity.mfCurrent,
		.fLightingSize = gPlayerBlasterLightingArea.mfCurrent,
		.fLightingIntensity = gPlayerBlasterLightingIntensity.mfCurrent,
		.pVisibleIntensityWrapper = &gPlayerAreaLightVisibleIntensity,
		.pLightingSizeWrapper = &gPlayerBlasterLightingArea,
		.pLightingIntensityWrapper = &gPlayerBlasterLightingIntensity,
	});
#endif // BT_CLIENT

	BlastersInterpolate::RegisterType(siBlasterTypeIndex,
	{
		.f2Size = {kfBlasterSizeX, kfBlasterSizeY},
		.iAreaLightTypeIndex = siAreaLightTypeIndex,
	});

	engine::ExplosionsInterpolate::RegisterType(siExplosionTypeIndex,
	{
#if defined(BT_CLIENT)
		.iPrimaryLightControllerTypeIndex = engine::ExplosionsInterpolate::siPrimaryLightControllerTypeIndex,
		.iSecondaryLightControllerTypeIndex = engine::ExplosionsInterpolate::siSecondaryLightControllerTypeIndex,
		.iPrimaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::siPrimaryPuffControllerTypeIndex,
		.iSecondaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::siSecondaryPuffControllerTypeIndex,
		.iTrailTypeIndex = engine::ExplosionsInterpolate::siExplosionTrailTypeIndex,
		.iWindRadialControllerTypeIndex = engine::ExplosionsInterpolate::siWindRadialControllerTypeIndex,
#endif // BT_CLIENT
		.iBaseParticleCount = kiExplosionBaseParticleCount,
		.uiParticleColor = 0xFF0000FF,
		.fParticleVelocityMinimum = kfExplosionParticleVelocityMinimum,
		.fParticleVelocityRandom = kfExplosionParticleVelocityRandom,
		.fParticleVerticalVelocityMinimum = kfExplosionParticleVerticalVelocityMinimum,
		.fParticleVerticalVelocityRandom = kfExplosionParticleVerticalVelocityRandom,
		.fParticleIntensityDecay = kfExplosionParticleIntensityDecay,
		.pParticleWidthScale = &gPlayerExplosionParticleWidth,
		.pParticleLengthScale = &gPlayerExplosionParticleLength,
		.pParticleLengthSpreadScale = &gPlayerExplosionParticleLengthSpread,
		.pParticlePositionJitterScale = &gPlayerExplosionParticlePositionJitter,
		.pParticleVelocityBaseScale = &gPlayerExplosionParticleVelocityBase,
		.pParticleVelocitySpreadScale = &gPlayerExplosionParticleVelocitySpread,
		.pParticleVerticalVelocityBaseScale = &gPlayerExplosionParticleVerticalVelocityBase,
		.pParticleVerticalVelocitySpreadScale = &gPlayerExplosionParticleVerticalVelocitySpread,
		.pParticleVelocityDecayScale = &gPlayerExplosionParticleVelocityDecay,
		.pParticleGravityScale = &gPlayerExplosionParticleGravity,
		.pParticleVisibleIntensityScale = &gPlayerExplosionParticleVisibleIntensity,
		.pParticleIntensitySpreadScale = &gPlayerExplosionParticleIntensitySpread,
		.pParticleIntensityDecayScale = &gPlayerExplosionParticleIntensityDecay,
		.pParticleIntensityPowerScale = &gPlayerExplosionParticleIntensityPower,
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
		.times = {std::chrono::duration<float>::zero(), kImpactPointLightDuration, std::chrono::duration<float>::zero(), std::chrono::duration<float>::zero()},
		.keyframes =
		{
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{.fVisibleArea = 1.0f, .fVisibleIntensity = 1.0f, .fLightingArea = 1.0f, .fLightingIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppVisibleAreaScales = {&gPlayerImpactVisibleAreaOne, &gPlayerImpactVisibleAreaTwo, nullptr, nullptr},
		.ppVisibleIntensityScales = {&gPlayerImpactVisibleIntensityOne, &gPlayerImpactVisibleIntensityTwo, nullptr, nullptr},
		.ppLightingAreaScales = {&gPlayerImpactLightingAreaOne, &gPlayerImpactLightingAreaTwo, nullptr, nullptr},
		.ppLightingIntensityScales = {&gPlayerImpactLightingIntensityOne, &gPlayerImpactLightingIntensityTwo, nullptr, nullptr},
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
		.times = {std::chrono::duration<float>::zero(), kImpactPuffDuration, std::chrono::duration<float>::zero(), std::chrono::duration<float>::zero()},
		.keyframes =
		{
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{.fArea = 1.0f, .fIntensity = 1.0f, .fRotation = 0.0f},
			{},
			{},
		},
		.ppAreaScales = {&gPlayerImpactPuffAreaOne, &gPlayerImpactPuffAreaTwo, nullptr, nullptr},
		.ppIntensityScales = {&gPlayerImpactPuffIntensityOne, &gPlayerImpactPuffIntensityTwo, nullptr, nullptr},
	});

	engine::HexShieldsInterpolate::RegisterType(siHexShieldTypeIndex,
	{
		.uiColor = 0x40FFFF00,        // Cyan with 25% alpha (ABGR)
		.uiLightingColor = 0x40FFFF00, // Cyan (ABGR)
		.fMinimumIntensity = 0.0f,    // Shield invisible when idle
	});
#endif // BT_CLIENT
}

#if defined(BT_CLIENT)
void PlayersInterpolate::RemoveOwnedVisuals(Frame& rFrame, PlayersInterpolate& rCurrentInterpolate, int64_t i)
{
	if ((rCurrentInterpolate.pWindTrails[i].uuid.iValue != 0))
	{
		engine::RemoveIndexableElementAndClearHandle(rFrame.interpolate.windTrails, rFrame.postRender.windTrails, rCurrentInterpolate.pWindTrails[i], rFrame.interpolate.windTrails.Members(), rFrame.postRender.windTrails.Members());
	}
	if ((rCurrentInterpolate.pHexShields[i].uuid.iValue != 0))
	{
		engine::HexShieldsPostRender::Remove(rFrame, rCurrentInterpolate.pHexShields[i]);
	}
}
#endif // BT_CLIENT

void PlayersPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
#if defined(BT_CLIENT)
		if (rCurrentPostRender.pFlags[i] & kExploding)
		{
			PlayersInterpolate::RemoveOwnedVisuals(rFrame, rCurrentInterpolate, i);
		}
#endif // BT_CLIENT
	}

	// Remove dead players (reverse iteration for swap-and-pop safety)
	engine::DestroySweep(rCurrentInterpolate, rCurrentPostRender, [&](int64_t i)
	{
		return (rCurrentPostRender.pFlags[i] & kExploding) && rCurrentInterpolate.pDestroyedTimes[i].count() <= 0.0f;
	},
	[&](int64_t i)
	{
		engine::PushersPostRender::Remove(rFrame, rCurrentInterpolate.pPushers[i]);
		engine::RemoveIndexableElement(rCurrentInterpolate, rCurrentPostRender, rCurrentPostRender.pIds[i], rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	});
}

static void ProcessSpawnStatusChanges([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	for (const StatusChange& rStatusChange : rFrameInput.statusChanges)
	{
		if (rStatusChange.eType == StatusChangeType::kDestroyPlayer)
		{
			int64_t iPlayerUuid = std::get<DestroyPlayerData>(rStatusChange.data).iPlayerUuid;
			player_t destroyId {engine::Uuid {iPlayerUuid}};

			auto it = rCurrentInterpolate.idToIndexMap.find(destroyId);
			if (it != rCurrentInterpolate.idToIndexMap.end())
			{
				engine::PushersPostRender::Remove(rFrame, rCurrentInterpolate.pPushers[it->second]);
#if defined(BT_CLIENT)
				PlayersInterpolate::RemoveOwnedVisuals(rFrame, rCurrentInterpolate, it->second);
#endif // BT_CLIENT

				engine::RemoveIndexableElement(rCurrentInterpolate, rCurrentPostRender, destroyId, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
			}
			continue;
		}

		if (rStatusChange.eType == StatusChangeType::kSpawnPlayer)
		{
			const SpawnPlayerData& rSpawnData = std::get<SpawnPlayerData>(rStatusChange.data);
			PlayerFlags_t spawnFlags {PlayerFlags::kBlasterSpawnLeft};
			if (rSpawnData.bIsFlagship)
			{
				spawnFlags.Set(kIsFlagship);
			}

			// The cell's local frame is centered on the origin, so the spawn offset is the local position.
			XMVECTOR vecSpawnPosition = XMVectorSet(rSpawnData.fSpawnOffsetX, rSpawnData.fSpawnOffsetY, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);

			// These offsets arrive from outside the simulation (network or harness), so refuse one outside this
			// cell here.
			if (!common::InsideArea(vecSpawnPosition, engine::LocalCellArea()))
			{
				continue;
			}

			PlayersPostRender::Spawn(rFrame,
			{
				.vecPosition = vecSpawnPosition,
				.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f),
				.vecVelocity = XMVectorZero(),
				.alignment = rFrame.postRender.playerAlignment,
				.flags = spawnFlags,
				.arrivalGracePeriod = kArrivalGracePeriod,
				.globalPlayerId = {.iValue = rSpawnData.iGlobalId},
				// Empty unless the spawn request carries an owning client: agent-injected rows are born unowned.
				.clientGuid = rSpawnData.clientGuid,
				.fleetWantedCoordinate = rSpawnData.fleetWantedCoordinate,
				.iPendingFleetWantedCoordinateTicks = rSpawnData.uiPendingFleetWantedCoordinateTicks,
			});
		}
	}
}

void PlayersPostRender::ProcessUpdateStatusChanges([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	int64_t iUpdateFleetCount = 0;
	engine::GridCoord updateFleetNewCoordinate {};
	int64_t iUpdateFleetFlagshipGlobalId = 0;

	for (const StatusChange& rStatusChange : rFrameInput.statusChanges)
	{
		if (rStatusChange.eType == StatusChangeType::kUpdateFleet)
		{
			const UpdateFleetData& rUpdate = std::get<UpdateFleetData>(rStatusChange.data);
			player_t updateId {engine::Uuid {rUpdate.iPlayerUuid}};
			auto it = rCurrentInterpolate.idToIndexMap.find(updateId);
			if (it != rCurrentInterpolate.idToIndexMap.end())
			{
				int64_t iIndex = it->second;
				if (rUpdate.bIsFlagship)
				{
					rCurrentPostRender.pFlags[iIndex].Set(kIsFlagship);
					iUpdateFleetFlagshipGlobalId = rCurrentPostRender.pGlobalPlayerIds[iIndex].iValue;
				}
				else
				{
					rCurrentPostRender.pFlags[iIndex].Set(kIsFlagship, false);
				}
				++iUpdateFleetCount;
				updateFleetNewCoordinate = rUpdate.fleetWantedCoordinate;
				rCurrentPostRender.pFleetWantedCoordinates[iIndex] = rUpdate.fleetWantedCoordinate;
				rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[iIndex] = rUpdate.uiPendingFleetWantedCoordinateTicks;
			}
			continue;
		}

		if (rStatusChange.eType == StatusChangeType::kUpdatePlayer)
		{
			const UpdatePlayerData& rUpdate = std::get<UpdatePlayerData>(rStatusChange.data);
			player_t updateId {engine::Uuid {rUpdate.iPlayerUuid}};

			auto it = rCurrentInterpolate.idToIndexMap.find(updateId);
			if (it != rCurrentInterpolate.idToIndexMap.end())
			{
				int64_t iIndex = it->second;
				// Weapon mode change is deferred via countdown ticks
				rCurrentPostRender.pFlags[iIndex].Set(kPendingUseMissiles, rUpdate.bUseMissiles);
				rCurrentPostRender.puiPendingWeaponModeTicks[iIndex] = rUpdate.uiPendingWeaponModeTicks;
				rCurrentPostRender.pNavigationDelays[iIndex] = rUpdate.navigationDelaySeconds;
				rCurrentPostRender.pCellChangeTimers[iIndex] = rUpdate.navigationDelaySeconds;
			}
			else
			{
				LOG(kNetwork, kWarning, "ProcessUpdateStatusChanges::kUpdatePlayer Uuid: {} NOT FOUND in idToIndexMap", rUpdate.iPlayerUuid);
			}
			continue;
		}
	}

	if (iUpdateFleetCount > 0)
	{
		LOG(kNetwork, kVerbose, "ProcessUpdateStatusChanges::kUpdateFleet Coord: ({},{}) NewWantedCoord: ({},{}) Players: {} Flagship: {}", rStaticData.coordinate.iX, rStaticData.coordinate.iY, updateFleetNewCoordinate.iX, updateFleetNewCoordinate.iY, iUpdateFleetCount, iUpdateFleetFlagshipGlobalId);
	}
}

void PlayersPostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	ProcessSpawnStatusChanges(rFrame, rFrameInput, rStaticData);

#if defined(BT_CLIENT)
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		if (!(rCurrentInterpolate.pWindTrails[i].uuid.iValue != 0) && !(rCurrentPostRender.pFlags[i] & kExploding))
		{
			engine::WindTrailsPostRender::Add(rFrame, rCurrentInterpolate.pWindTrails[i]);
			engine::WindTrailsInterpolate::Sync(rFrame.interpolate, rCurrentInterpolate.pWindTrails[i],
			{
				.vecPosition = rCurrentInterpolate.pVecPositions[i],
				.fIntensity = game::gWindDepositPlayerIntensity.mfCurrent,
				.fWidth = game::gWindDepositPlayerWidth.mfCurrent,
				.fLengthMultiplier = game::gWindDepositPlayerLengthMultiplier.mfCurrent,
			});
		}

		if (!(rCurrentInterpolate.pHexShields[i].uuid.iValue != 0) && !(rCurrentPostRender.pFlags[i] & kExploding))
		{
			engine::HexShieldsPostRender::Add(rFrame, rCurrentInterpolate.pHexShields[i], PlayersInterpolate::siHexShieldTypeIndex);
		}
	}
#endif // BT_CLIENT

	SpawnBlasters(rFrame, rStaticData.coordinate);
	SpawnMissiles(rFrame, rStaticData.coordinate);
	SpawnDeathExplosions(rFrame, rStaticData.coordinate);
}

bool PlayersPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo)
{
	// A human-controlled unit outside its own cell is a bug, not gameplay.
	ASSERT(common::InsideArea(rInfo.vecPosition, engine::LocalCellArea()));

	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

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
	rCurrentPostRender.pAlignments[iIndex] = rInfo.alignment;
	rCurrentPostRender.pNextBlasterFireTimes[iIndex] = rInfo.nextBlasterFireTime;
	rCurrentPostRender.pNextSecondarySpawnTimes[iIndex] = rInfo.nextSecondarySpawnTime;
	rCurrentPostRender.pVecVelocities[iIndex] = rInfo.vecVelocity;
	rCurrentPostRender.pVecWantedDirections[iIndex] = rInfo.vecDirection;
	rCurrentPostRender.pfArmors[iIndex] = rInfo.bTransfer ? rInfo.fArmor : kfPlayerArmor;
	rCurrentPostRender.pfShields[iIndex] = rInfo.bTransfer ? rInfo.fShield : kfPlayerShield;
	rCurrentPostRender.pShieldCooldowns[iIndex] = rInfo.shieldCooldown;
	rCurrentPostRender.pDestroyedExplosionTimes[iIndex] = std::chrono::duration<float>::zero();
	rCurrentPostRender.pShieldDownSoundCooldowns[iIndex] = rInfo.shieldDownSoundCooldown;
	rCurrentPostRender.pVecAiDirections[iIndex] = XMVectorZero();
	rCurrentPostRender.pTransferLockTimers[iIndex] = rInfo.transferLockTimer;
	rCurrentPostRender.pArrivalGracePeriods[iIndex] = rInfo.arrivalGracePeriod;
	rCurrentPostRender.pCellChangeTimers[iIndex] = std::chrono::duration<float>(15.0f + common::Random<10.0f>(rFrame.postRender.randomEngine));
	// Entering a cell is a fresh spawn for navigation, cross-cell transfers included: the wanted direction comes from the
	// hull direction, and the cached steering, island destination, navigation mode, and waypoint index reset here.
	// Flagship navigates to island destination on enter; non-flagship starts roaming and follows flagship via proximity
	PlayerFlags_t spawnFlags = rInfo.flags;
	SetNavigationDirection(spawnFlags, (rInfo.flags & kIsFlagship) ? 4i64 : -1i64);
	SetNavigationWaypointIndex(spawnFlags, 0);
	rCurrentPostRender.pFlags[iIndex] = spawnFlags;
	rCurrentPostRender.pNavigationDelays[iIndex] = rInfo.navigationDelay;
	rCurrentPostRender.pVecIslandDestinations[iIndex] = XMVectorZero();
	rCurrentPostRender.pClientGuids[iIndex] = rInfo.clientGuid;
	rCurrentPostRender.pGlobalPlayerIds[iIndex] = rInfo.globalPlayerId;
	rCurrentPostRender.pFleetWantedCoordinates[iIndex] = rInfo.fleetWantedCoordinate;
	rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[iIndex] = static_cast<uint8_t>(rInfo.iPendingFleetWantedCoordinateTicks);
	rCurrentPostRender.puiPendingWeaponModeTicks[iIndex] = static_cast<uint8_t>(rInfo.iPendingWeaponModeTicks);
#if defined(BT_CLIENT)
	rCurrentPostRender.pVecDebugNavigationWaypoints[iIndex] = XMVectorZero();
#endif // BT_CLIENT

	return true;
}


void PlayersInterpolate::Update([[maybe_unused]] FrameInterpolate& __restrict rFrameInterpolate, [[maybe_unused]] const Frame& __restrict rPreviousFrame)
{
	PlayersInterpolate& rCurrent = *rFrameInterpolate.pPlayers;
	const PlayersInterpolate& rPrevious = *rPreviousFrame.interpolate.pPlayers;
	const PlayersPostRender& rPreviousPostRender = *rPreviousFrame.postRender.pPlayers;
	std::chrono::duration<float> deltaTime(rFrameInterpolate.fDeltaTime);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		std::chrono::duration<float> destroyedTime = rPrevious.pDestroyedTimes[i];
		std::chrono::duration<float> animationTime = rPrevious.pAnimationTimes[i];

		if (!(rPreviousPostRender.pFlags[i] & kExploding)) [[likely]]
		{
			vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(deltaTime.count()), rPreviousPostRender.pVecVelocities[i], vecPosition);
		}
		vecPosition = XMVectorSetZ(vecPosition, engine::gpFlightPlanes->Lowest().fHeightMeters);
		// Enforce W=1.0 — prevents drift via MultiplyAdd's 4-lane propagation (pos.W += dt * vel.W).
		vecPosition = XMVectorSetW(vecPosition, 1.0f);

		vecDirection = common::RotateTowardsPercent(vecDirection, rPreviousPostRender.pVecWantedDirections[i], common::ExponentialInterpolant(kfRotateTowardsSpeed, deltaTime.count()));

		if (rPreviousPostRender.pFlags[i] & kExploding) [[unlikely]]
		{
			destroyedTime = std::max(destroyedTime - deltaTime, std::chrono::duration<float>::zero());
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pDestroyedTimes[i] = destroyedTime;
		rCurrent.pAnimationTimes[i] = animationTime;

		engine::PushersInterpolate::Sync(rFrameInterpolate, rCurrent.pPushers[i],
		{
			.vecPosition = vecPosition,
			.fRadius = kfPlayerPusherRadius,
			.fIntensity = kfPlayerPusherIntensity,
			.fPower = kfPlayerPusherPower,
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

		if (engine::gAnimationDataMap.contains(kPlayerModel))
		{
			const engine::AnimationData& rAnimationData = engine::gAnimationDataMap.at(kPlayerModel);
			std::chrono::duration<float> animationDuration(rAnimationData.mpAnimations[0].fDuration);
			animationTime += deltaTime;
			if (animationTime.count() >= animationDuration.count())
			{
				animationTime = std::chrono::duration<float>(std::fmod(animationTime.count(), animationDuration.count()));
			}
			rCurrent.pAnimationTimes[i] = animationTime;
		}

		if ((rCurrent.pWindTrails[i].uuid.iValue != 0))
		{
			engine::WindTrailsInterpolate::Sync(rFrameInterpolate, rCurrent.pWindTrails[i],
			{
				.vecPosition = vecPosition,
				.fIntensity = game::gWindDepositPlayerIntensity.mfCurrent,
				.fWidth = game::gWindDepositPlayerWidth.mfCurrent,
				.fLengthMultiplier = game::gWindDepositPlayerLengthMultiplier.mfCurrent,
			});
		}

		HexShieldDirections hexShieldDirections = rPrevious.pHexShieldDirections[i];
		HexShieldIntensities hexShieldVertexIntensities {};
		HexShieldIntensities hexShieldFragmentIntensities {};
		for (int64_t j = 0; j < shaders::kiHexShieldDirections; ++j)
		{
			hexShieldVertexIntensities.data[j] = std::max(rPrevious.pHexShieldVertexIntensities[i].data[j] - gHexShieldIntensityDecay.mfCurrent * deltaTime.count(), 0.0f);
			hexShieldFragmentIntensities.data[j] = std::max(rPrevious.pHexShieldFragmentIntensities[i].data[j] - gHexShieldIntensityDecay.mfCurrent * deltaTime.count(), 0.0f);
		}
		rCurrent.pHexShieldDirections[i] = hexShieldDirections;
		rCurrent.pHexShieldVertexIntensities[i] = hexShieldVertexIntensities;
		rCurrent.pHexShieldFragmentIntensities[i] = hexShieldFragmentIntensities;

		if ((rCurrent.pHexShields[i].uuid.iValue != 0) && !(rPreviousPostRender.pFlags[i] & kExploding))
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
				float fFragmentIntensity = rCurrent.pHexShieldFragmentIntensities[i].data[j];
				float fFade = std::clamp(fFragmentIntensity / kfHexShieldFadeThreshold, 0.0f, 1.0f);
				float fSmoothFade = fFade * fFade * (3.0f - 2.0f * fFade);
				syncData.pf4Directions[j] = rCurrent.pHexShieldDirections[i].data[j];
				syncData.pfVertexIntensities[j] = rCurrent.pHexShieldVertexIntensities[i].data[j] * fSmoothFade;
				syncData.pfFragmentIntensities[j] = fFragmentIntensity * fSmoothFade;
			}

			engine::HexShieldsInterpolate::Sync(rFrameInterpolate, rCurrent.pHexShields[i], syncData);
		}

#endif // BT_CLIENT
	}
}


// Player collision arrays
// thread_local: parallel per-Frame tick via Dispatch
static thread_local std::vector<engine::CollisionFlags_t> sCollisionFlags;

struct PlayerCollisionIntervalScratch
{
	std::vector<float> maximumTimes;
};

void PlayersPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	// Function-local TLS defers construction until first use; default construction is allocation-free
	// (empty vectors), so it is safe even before allocator startup completes. Growth sites suppress tracking.
	static thread_local PlayerCollisionIntervalScratch sScratch;
	PlayerCollisionIntervalScratch& rCollisionScratch = sScratch;
	// Heap: static vectors resized each frame, only allocates on first call or when count grows (capacity retained).
	// .data() pointers are passed to AddLayer and must survive until PostCollision, so workbuffer can't be used
	ScopedSuppressAllocationTracking suppress;

	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	if (rCurrentInterpolate.iCount == 0)
	{
		return;
	}

	int64_t iCount = rCurrentInterpolate.iCount;
	sCollisionFlags.resize(static_cast<size_t>(iCount));
	rCollisionScratch.maximumTimes.resize(static_cast<size_t>(iCount));
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		sCollisionFlags.at(static_cast<size_t>(i)) = (rCurrentPostRender.pFlags[i] & kExploding) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {};
		engine::SegmentHit boundaryHit = engine::TracePointToCellExit(engine::LocalCellArea(), rPreviousFrame.interpolate.pPlayers->pVecPositions[i], rCurrentInterpolate.pVecPositions[i]);
		rCollisionScratch.maximumTimes.at(static_cast<size_t>(i)) = boundaryHit.bHit ? boundaryHit.fTime : std::numeric_limits<float>::max();
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pPlayers->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pfMaxTimes = rCollisionScratch.maximumTimes.data(),
		.pFlags = sCollisionFlags.data(),
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.fRadius = kfPlayerRadius,
		.fDamage = 0.0f, // Player doesn't deal collision damage
		.uiCategory = CollisionCategory::kuiPlayer,
		.uiCollidesWith = CollidesWith::kuiPlayer,
		.pAlignments = rCurrentPostRender.pAlignments,
	});
}

// PlayersPostRender::Update orchestrates per-iteration helpers in PlayersNavigation.cpp and PlayersCombat.cpp.

void PlayersPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	PlayersPostRender& __restrict rCurrent = *rFrame.postRender.pPlayers;
	const PlayersPostRender& rPrevious = *rPreviousFrame.postRender.pPlayers;
	const PlayersInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pPlayers;
	std::chrono::duration<float> deltaTime(rFrame.interpolate.fDeltaTime);

	if (rCurrent.iCount == 0)
	{
		return;
	}

	// Cell center: every cell's local frame is centered on the origin.
	// W=1.0 keeps this a proper position — every downstream (cellCenter - vecPosition) and cardinal offset add stays W-clean,
	// so normalize fallbacks don't leak W into the AI direction and on into velocity.
	XMVECTOR vecCellCenter = XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		PlayerFlags_t flags = rPrevious.pFlags[i];
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

		if (iPendingFleetWantedCoordinateTicks > 0)
		{
			iPendingFleetWantedCoordinateTicks--;
		}
		if (iPendingWeaponModeTicks > 0)
		{
			iPendingWeaponModeTicks--;
			if (iPendingWeaponModeTicks == 0)
			{
				if (flags & kPendingUseMissiles)
				{
					flags.Set(kUseMissiles);
				}
				else
				{
					flags.Set(kUseMissiles, false);
				}
			}
		}

		// Transfer lock skips navigation, target acquisition, and movement acceleration.
		if (transferLockTimer > std::chrono::duration<float>::zero())
		{
			transferLockTimer -= deltaTime;
		}
		else
		{
			ComputeNavigation(rFrame, rPreviousFrame, rStaticData, i, vecPosition, vecCellCenter, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, flags, deltaTime, iNavigationDirection, iNavigationWaypointIndex, vecAiDirection, vecIslandDestination, cellChangeTimer);

			bool bLookTargetFound = false;
			XMVECTOR vecLookPosition = XMVectorZero();
			AcquireTarget(rPreviousFrame, vecPosition, flags, bLookTargetFound, vecLookPosition);

			float fAccelerationMultiplier = (1.0f - kfPlayerJitterRange * 0.5f) + common::Random<kfPlayerJitterRange>(rFrame.postRender.randomEngine);
			float fDecayMultiplier = (1.0f - kfPlayerJitterRange * 0.5f) + common::Random<kfPlayerJitterRange>(rFrame.postRender.randomEngine);
			ApplyMovement(iNavigationDirection, vecAiDirection, deltaTime, fAccelerationMultiplier, fDecayMultiplier, vecVelocity);

			if (bLookTargetFound)
			{
				UpdateFacing(vecPosition, vecLookPosition, vecWantedDirection);
			}
		}

		// Shield regeneration and pushes also run while transfer-locked.
		RegenerateShield(deltaTime, shieldCooldown, fShield);
		ApplyTerrainPush(rStaticData, vecPosition, vecVelocity);
		ApplyPusherPush(rFrame, rPreviousFrame, i, vecPosition, vecVelocity);
		ResolveTerrainContact(rStaticData, i, vecPosition, rFrame.interpolate.pPlayers->pVecPositions[i], vecVelocity);

		SetNavigationDirection(flags, iNavigationDirection);
		SetNavigationWaypointIndex(flags, iNavigationWaypointIndex);
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

bool PlayersInterpolate::LogDifferences(const PlayersInterpolate& rOther) const
{
	common::ScopedLogDifferenceContext context("PlayersInterpolate");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pVecPositions">(i, pVecPositions[i], rOther.pVecPositions[i]);
		bEqual &= common::LogDifference<"pVecDirections">(i, pVecDirections[i], rOther.pVecDirections[i]);
		bEqual &= common::LogDifference<"pDestroyedTimes">(i, pDestroyedTimes[i], rOther.pDestroyedTimes[i]);
		bEqual &= common::LogDifference<"puiPushers">(i, pPushers[i], rOther.pPushers[i]);
	}

	return bEqual;
}

bool PlayersPostRender::LogDifferences(const PlayersPostRender& rOther) const
{
	common::ScopedLogDifferenceContext context("PlayersPostRender");
	bool bEqual = true;
	bEqual &= Collection::LogDifferences(rOther);

	for (int64_t i = 0; i < std::min(iCount, rOther.iCount); ++i)
	{
		bEqual &= common::LogDifference<"pIds">(i, pIds[i].uuid.iValue, rOther.pIds[i].uuid.iValue);
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
		bEqual &= common::LogDifference<"pGlobalPlayerIds">(i, pGlobalPlayerIds[i], rOther.pGlobalPlayerIds[i]);
		bEqual &= common::LogDifference<"pFleetWantedCoords.iX">(i, pFleetWantedCoordinates[i].iX, rOther.pFleetWantedCoordinates[i].iX);
		bEqual &= common::LogDifference<"pFleetWantedCoords.iY">(i, pFleetWantedCoordinates[i].iY, rOther.pFleetWantedCoordinates[i].iY);
		bEqual &= common::LogDifference<"puiPendingFleetWantedCoordTicks">(i, puiPendingFleetWantedCoordinateTicks[i], rOther.puiPendingFleetWantedCoordinateTicks[i]);
		bEqual &= common::LogDifference<"puiPendingWeaponModeTicks">(i, puiPendingWeaponModeTicks[i], rOther.puiPendingWeaponModeTicks[i]);
	}

	return bEqual;
}

} // namespace game
