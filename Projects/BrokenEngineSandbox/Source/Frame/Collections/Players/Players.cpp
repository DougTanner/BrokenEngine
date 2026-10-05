#include "Players.h"

#include "Data/Texture.h"
#include "Frame/Collections/Explosions/Explosions.h"
#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/FrameStaticData.h"

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
		.iPrimaryLightControllerTypeIndex = engine::ExplosionsInterpolate::suiPrimaryLightControllerTypeIndex,
		.iSecondaryLightControllerTypeIndex = engine::ExplosionsInterpolate::suiSecondaryLightControllerTypeIndex,
		.iPrimaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::suiPrimaryPuffControllerTypeIndex,
		.iSecondaryPuffControllerTypeIndex = engine::ExplosionsInterpolate::suiSecondaryPuffControllerTypeIndex,
		.iTrailTypeIndex = engine::ExplosionsInterpolate::siExplosionTrailTypeIndex,
		.iWindRadialControllerTypeIndex = engine::ExplosionsInterpolate::suiWindRadialControllerTypeIndex,
#endif // BT_CLIENT
		.uiBaseParticleCount = static_cast<uint32_t>(kiExplosionBaseParticleCount),
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
	int64_t iImpactPointLightControllerTypeIndex = suiImpactPointLightControllerTypeIndex;
	engine::PointLightsInterpolate::RegisterControllerType(iImpactPointLightControllerTypeIndex,
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
	suiImpactPointLightControllerTypeIndex = static_cast<uint8_t>(iImpactPointLightControllerTypeIndex);

	// Hits trigger the impact puff controller.
	int64_t iImpactPuffTypeIndex = 0xFF;
	engine::PuffsInterpolate::RegisterType(iImpactPuffTypeIndex,
	{
		.uiCrc = data::kTexturesSmokeBC44jpgCrc,
		.uiColor = 0xFFFFFFFF,
	});
	int64_t iImpactPuffControllerTypeIndex = suiImpactPuffControllerTypeIndex;
	engine::PuffsInterpolate::RegisterControllerType(iImpactPuffControllerTypeIndex,
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
	suiImpactPuffControllerTypeIndex = static_cast<uint8_t>(iImpactPuffControllerTypeIndex);

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

void PlayersPostRender::Destroy([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
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
		return (rCurrentPostRender.pFlags[i] & kExploding) && rCurrentInterpolate.pfDestroyedTimes[i] <= 0.0f;
	},
	[&](int64_t i)
	{
		engine::PushersPostRender::Remove(rFrame, rCurrentInterpolate.pPushers[i]);
		engine::RemoveIndexableElement(rCurrentInterpolate, rCurrentPostRender, rCurrentPostRender.pIds[i], rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	});
}

static void ProcessSpawnStatusChanges([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
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
			XMVECTOR vecSpawnPosition = XMVectorSet(rSpawnData.fSpawnOffsetX, rSpawnData.fSpawnOffsetY, engine::gBaseHeight.mfCurrent, 1.0f);

			// These offsets arrive from outside the simulation (network or harness), so refuse one outside this
			// cell here.
			if (!common::InsideArea(vecSpawnPosition, engine::LocalFrameArea()))
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
				.fArrivalGracePeriod = kArrivalGracePeriod.count(),
				.globalPlayerId = {.iValue = rSpawnData.iGlobalId},
				// Empty unless the spawn request carries an owning client: agent-injected rows are born unowned.
				.clientGuid = rSpawnData.clientGuid,
				.fleetWantedCoordinate = rSpawnData.fleetWantedCoordinate,
				.iPendingFleetWantedCoordinateTicks = rSpawnData.uiPendingFleetWantedCoordinateTicks,
			});
		}
	}
}

void PlayersPostRender::ProcessUpdateStatusChanges([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
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
					// NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores) — read only by the kVerbose LOG after the loop, which compiles out at default log levels
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
				rCurrentPostRender.pfNavigationDelays[iIndex] = rUpdate.navigationDelaySeconds.count();
				rCurrentPostRender.pfFrameChangeTimers[iIndex] = rUpdate.navigationDelaySeconds.count();
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

void PlayersPostRender::Spawn([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const FrameInput& __restrict rFrameInput, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
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
	SpawnDeathExplosions(rFrame);
}

bool PlayersPostRender::Spawn(Frame& __restrict rFrame, const SpawnInfo& rInfo)
{
	// A human-controlled unit outside its own cell is a bug, not gameplay.
	ASSERT(common::InsideArea(rInfo.vecPosition, engine::LocalFrameArea()));

	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	common::ValidateVector<true >(rInfo.vecPosition);
	common::ValidateVector<false>(rInfo.vecDirection);
	common::ValidateVector<false>(rInfo.vecVelocity);

	engine::GrowPairedCollections(rCurrentInterpolate, rCurrentPostRender, rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	auto [iIndex, newId] = engine::AddIndexableElement(rCurrentInterpolate, rCurrentPostRender, rFrame.postRender);

	rCurrentInterpolate.pVecPositions[iIndex] = rInfo.vecPosition;
	rCurrentInterpolate.pVecDirections[iIndex] = rInfo.vecDirection;
	rCurrentInterpolate.pfDestroyedTimes[iIndex] = 0.0f;
	rCurrentInterpolate.pfAnimationTimes[iIndex] = rInfo.fAnimationTime;
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
	rCurrentPostRender.pfNextBlasterFireTimes[iIndex] = rInfo.fNextBlasterFireTime;
	rCurrentPostRender.pfNextSecondarySpawnTimes[iIndex] = rInfo.fNextSecondarySpawnTime;
	rCurrentPostRender.pVecVelocities[iIndex] = rInfo.vecVelocity;
	rCurrentPostRender.pVecWantedDirections[iIndex] = rInfo.vecDirection;
	rCurrentPostRender.pfArmors[iIndex] = rInfo.bTransfer ? rInfo.fArmor : kfPlayerArmor;
	rCurrentPostRender.pfShields[iIndex] = rInfo.bTransfer ? rInfo.fShield : kfPlayerShield;
	rCurrentPostRender.pfShieldCooldowns[iIndex] = rInfo.fShieldCooldown;
	rCurrentPostRender.pfDestroyedExplosionTimes[iIndex] = 0.0f;
	rCurrentPostRender.pfShieldDownSoundCooldowns[iIndex] = rInfo.fShieldDownSoundCooldown;
	rCurrentPostRender.pVecAiDirections[iIndex] = XMVectorZero();
	rCurrentPostRender.pfTransferLockTimers[iIndex] = rInfo.fTransferLockTimer;
	rCurrentPostRender.pfArrivalGracePeriods[iIndex] = rInfo.fArrivalGracePeriod;
	// Always consume random for determinism, even if fFrameChangeTimer is pre-set
	float fRandomTimer = 15.0f + common::Random<10.0f>(rFrame.postRender.randomEngine);
	rCurrentPostRender.pfFrameChangeTimers[iIndex] = (rInfo.fFrameChangeTimer > 0.0f) ? rInfo.fFrameChangeTimer : fRandomTimer;
	// Entering a frame is a fresh spawn for navigation, cross-frame transfers included: the wanted direction comes from the
	// hull direction, and the cached steering, island destination, navigation mode, and waypoint index reset here.
	// Flagship navigates to island destination on enter; non-flagship starts roaming and follows flagship via proximity
	PlayerFlags_t spawnFlags = rInfo.flags;
	SetNavigationDirection(spawnFlags, (rInfo.flags & kIsFlagship) ? 4i64 : -1i64);
	SetNavigationWaypointIndex(spawnFlags, 0);
	rCurrentPostRender.pFlags[iIndex] = spawnFlags;
	rCurrentPostRender.pfNavigationDelays[iIndex] = rInfo.fNavigationDelay;
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
	float fDeltaTime = rFrameInterpolate.fDeltaTime;

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		XMVECTOR vecPosition = rPrevious.pVecPositions[i];
		XMVECTOR vecDirection = rPrevious.pVecDirections[i];
		float fDestroyedTime = rPrevious.pfDestroyedTimes[i];
		float fAnimationTime = rPrevious.pfAnimationTimes[i];

		if (!(rPreviousPostRender.pFlags[i] & kExploding)) [[likely]]
		{
			vecPosition = XMVectorMultiplyAdd(XMVectorReplicate(fDeltaTime), rPreviousPostRender.pVecVelocities[i], vecPosition);
		}
		vecPosition = XMVectorSetZ(vecPosition, engine::gBaseHeight.mfCurrent);
		// Enforce W=1.0 — prevents drift via MultiplyAdd's 4-lane propagation (pos.W += dt * vel.W).
		vecPosition = XMVectorSetW(vecPosition, 1.0f);

		vecDirection = common::RotateTowardsPercent(vecDirection, rPreviousPostRender.pVecWantedDirections[i], common::ExponentialInterpolant(kfRotateTowardsSpeed, fDeltaTime));

		if (rPreviousPostRender.pFlags[i] & kExploding) [[unlikely]]
		{
			fDestroyedTime = std::max(fDestroyedTime - fDeltaTime, 0.0f);
		}

		rCurrent.pVecPositions[i] = vecPosition;
		rCurrent.pVecDirections[i] = vecDirection;
		rCurrent.pfDestroyedTimes[i] = fDestroyedTime;
		rCurrent.pfAnimationTimes[i] = fAnimationTime;

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
		fShieldRotation += fDeltaTime * kfShieldRotationSpeed;
		fShieldShrink = std::clamp(fShieldShrink + (rPreviousPostRender.pfShields[i] > 0.0f ? fDeltaTime * kfShieldShrinkSpeed : -fDeltaTime * kfShieldShrinkSpeed), 0.0f, 1.0f);
		rCurrent.pfShieldRotations[i] = fShieldRotation;
		rCurrent.pfShieldShrinks[i] = fShieldShrink;

		if (engine::gAnimationDataMap.contains(kPlayerModel))
		{
			const engine::AnimationData& rAnimationData = engine::gAnimationDataMap.at(kPlayerModel);
			float fAnimationDuration = rAnimationData.mpAnimations[0].fDuration;
			fAnimationTime += fDeltaTime;
			if (fAnimationTime >= fAnimationDuration)
			{
				fAnimationTime = std::fmod(fAnimationTime, fAnimationDuration);
			}
			rCurrent.pfAnimationTimes[i] = fAnimationTime;
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
			hexShieldVertexIntensities.data[j] = std::max(rPrevious.pHexShieldVertexIntensities[i].data[j] - gHexShieldIntensityDecay.mfCurrent * fDeltaTime, 0.0f);
			hexShieldFragmentIntensities.data[j] = std::max(rPrevious.pHexShieldFragmentIntensities[i].data[j] - gHexShieldIntensityDecay.mfCurrent * fDeltaTime, 0.0f);
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
static thread_local std::vector<float> sCollisionRadii;
static thread_local std::vector<float> sCollisionDamages;
static thread_local std::vector<engine::CollisionFlags_t> sCollisionFlags;

struct PlayerCollisionIntervalScratch
{
	std::vector<float> startTimes;
	std::vector<float> endTimes;
	std::vector<float> maximumTimes;
};

void PlayersPostRender::PreCollision([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
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
	sCollisionRadii.resize(static_cast<size_t>(iCount));
	sCollisionDamages.resize(static_cast<size_t>(iCount));
	sCollisionFlags.resize(static_cast<size_t>(iCount));
	rCollisionScratch.startTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.endTimes.resize(static_cast<size_t>(iCount));
	rCollisionScratch.maximumTimes.resize(static_cast<size_t>(iCount));
	for (int64_t i = 0; i < rCurrentInterpolate.iCount; ++i)
	{
		sCollisionRadii.at(static_cast<size_t>(i)) = kfPlayerRadius;
		sCollisionDamages.at(static_cast<size_t>(i)) = 0.0f; // Player doesn't deal collision damage
		sCollisionFlags.at(static_cast<size_t>(i)) = (rCurrentPostRender.pFlags[i] & kExploding) ? engine::CollisionFlags_t {engine::CollisionFlags::kAlreadyCollided} : engine::CollisionFlags_t {};
		rCollisionScratch.startTimes.at(static_cast<size_t>(i)) = 0.0f;
		rCollisionScratch.endTimes.at(static_cast<size_t>(i)) = 1.0f;
		engine::SegmentHit boundaryHit = engine::TracePointToFrameExit(engine::LocalFrameArea(), rPreviousFrame.interpolate.pPlayers->pVecPositions[i], rCurrentInterpolate.pVecPositions[i], 0.0f, 1.0f);
		rCollisionScratch.maximumTimes.at(static_cast<size_t>(i)) = boundaryHit.bHit ? boundaryHit.fTime : std::numeric_limits<float>::max();
	}

	siCollisionLayerIndex = engine::Collision::AddLayer(
	{
		.pVecStartPositions = rPreviousFrame.interpolate.pPlayers->pVecPositions,
		.pVecEndPositions = rCurrentInterpolate.pVecPositions,
		.pfStartTimes = rCollisionScratch.startTimes.data(),
		.pfEndTimes = rCollisionScratch.endTimes.data(),
		.pfMaxTimes = rCollisionScratch.maximumTimes.data(),
		.pfRadii = sCollisionRadii.data(),
		.pfDamages = sCollisionDamages.data(),
		.pFlags = sCollisionFlags.data(),
		.pVecVelocities = rCurrentPostRender.pVecVelocities,
		.iCount = rCurrentInterpolate.iCount,
		.uiCategory = CollisionCategory::kuiPlayer,
		.uiCollidesWith = CollidesWith::kuiPlayer,
		.pAlignments = rCurrentPostRender.pAlignments,
	});
}

// PlayersPostRender::Update orchestrates per-iteration helpers in PlayersNavigation.cpp and PlayersCombat.cpp.

void PlayersPostRender::Update([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::FrameStaticData& rStaticData)
{
	PlayersPostRender& __restrict rCurrent = *rFrame.postRender.pPlayers;
	const PlayersPostRender& rPrevious = *rPreviousFrame.postRender.pPlayers;
	const PlayersInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pPlayers;
	float fDeltaTime = rFrame.interpolate.fDeltaTime;

	if (rCurrent.iCount == 0)
	{
		return;
	}

	// Frame center: every cell's local frame is centered on the origin.
	// W=1.0 keeps this a proper position — every downstream (frameCenter - vecPosition) and cardinal offset add stays W-clean,
	// so normalize fallbacks don't leak W into the AI direction and on into velocity.
	XMVECTOR vecFrameCenter = XMVectorSet(0.0f, 0.0f, engine::gBaseHeight.mfCurrent, 1.0f);

	for (int64_t i = 0; i < rCurrent.iCount; ++i)
	{
		PlayerFlags_t flags = rPrevious.pFlags[i];
		float fNextBlasterFireTime = rPrevious.pfNextBlasterFireTimes[i];
		float fNextSecondarySpawnTime = rPrevious.pfNextSecondarySpawnTimes[i];
		XMVECTOR vecVelocity = rPrevious.pVecVelocities[i];
		XMVECTOR vecWantedDirection = rPrevious.pVecWantedDirections[i];
		float fArmor = rPrevious.pfArmors[i];
		float fShield = rPrevious.pfShields[i];
		float fShieldCooldown = std::max(0.0f, rPrevious.pfShieldCooldowns[i] - fDeltaTime);
		float fDestroyedExplosionTime = std::max(0.0f, rPrevious.pfDestroyedExplosionTimes[i] - fDeltaTime);
		float fShieldDownSoundCooldown = std::max(0.0f, rPrevious.pfShieldDownSoundCooldowns[i] - fDeltaTime);
		XMVECTOR vecAiDirection = rPrevious.pVecAiDirections[i];
		float fTransferLockTimer = rPrevious.pfTransferLockTimers[i];
		float fArrivalGracePeriod = std::max(0.0f, rPrevious.pfArrivalGracePeriods[i] - fDeltaTime);
		float fFrameChangeTimer = rPrevious.pfFrameChangeTimers[i];
		float fNavigationDelay = rPrevious.pfNavigationDelays[i];
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
		if (fTransferLockTimer > 0.0f)
		{
			fTransferLockTimer -= fDeltaTime;
		}
		else
		{
			ComputeNavigation(rFrame, rPreviousFrame, rStaticData, i, vecPosition, vecFrameCenter, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, flags, fDeltaTime, iNavigationDirection, iNavigationWaypointIndex, vecAiDirection, vecIslandDestination, fFrameChangeTimer);

			bool bLookTargetFound = false;
			XMVECTOR vecLookPosition = XMVectorZero();
			AcquireTarget(rPreviousFrame, vecPosition, flags, bLookTargetFound, vecLookPosition);

			float fAccelerationMultiplier = (1.0f - kfPlayerJitterRange * 0.5f) + common::Random<kfPlayerJitterRange>(rFrame.postRender.randomEngine);
			float fDecayMultiplier = (1.0f - kfPlayerJitterRange * 0.5f) + common::Random<kfPlayerJitterRange>(rFrame.postRender.randomEngine);
			ApplyMovement(iNavigationDirection, vecAiDirection, fDeltaTime, fAccelerationMultiplier, fDecayMultiplier, vecVelocity);

			if (bLookTargetFound)
			{
				UpdateFacing(vecPosition, vecLookPosition, vecWantedDirection);
			}
		}

		// Shield regeneration and pushes also run while transfer-locked.
		RegenerateShield(std::chrono::duration<float>(fDeltaTime), std::chrono::duration<float>(fShieldCooldown), fShield);
		ApplyTerrainPush(rStaticData, vecPosition, vecVelocity);
		ApplyPusherPush(rFrame, rPreviousFrame, i, vecPosition, vecVelocity);

		SetNavigationDirection(flags, iNavigationDirection);
		SetNavigationWaypointIndex(flags, iNavigationWaypointIndex);
		rCurrent.pFlags[i] = flags;
		rCurrent.pfNextBlasterFireTimes[i] = fNextBlasterFireTime;
		rCurrent.pfNextSecondarySpawnTimes[i] = fNextSecondarySpawnTime;
		rCurrent.pVecVelocities[i] = vecVelocity;
		rCurrent.pVecWantedDirections[i] = vecWantedDirection;
		rCurrent.pfArmors[i] = fArmor;
		rCurrent.pfShields[i] = fShield;
		rCurrent.pfShieldCooldowns[i] = fShieldCooldown;
		rCurrent.pfDestroyedExplosionTimes[i] = fDestroyedExplosionTime;
		rCurrent.pfShieldDownSoundCooldowns[i] = fShieldDownSoundCooldown;
		rCurrent.pVecAiDirections[i] = vecAiDirection;
		rCurrent.pfTransferLockTimers[i] = fTransferLockTimer;
		rCurrent.pfArrivalGracePeriods[i] = fArrivalGracePeriod;
		rCurrent.pfFrameChangeTimers[i] = fFrameChangeTimer;
		rCurrent.pfNavigationDelays[i] = fNavigationDelay;
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
		bEqual &= common::LogDifference<"pfDestroyedTimes">(i, pfDestroyedTimes[i], rOther.pfDestroyedTimes[i]);
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
		bEqual &= common::LogDifference<"pfNextBlasterFireTimes">(i, pfNextBlasterFireTimes[i], rOther.pfNextBlasterFireTimes[i]);
		bEqual &= common::LogDifference<"pfNextSecondarySpawnTimes">(i, pfNextSecondarySpawnTimes[i], rOther.pfNextSecondarySpawnTimes[i]);
		bEqual &= common::LogDifference<"pVecVelocities">(i, pVecVelocities[i], rOther.pVecVelocities[i]);
		bEqual &= common::LogDifference<"pVecWantedDirections">(i, pVecWantedDirections[i], rOther.pVecWantedDirections[i]);
		bEqual &= common::LogDifference<"pfArmors">(i, pfArmors[i], rOther.pfArmors[i]);
		bEqual &= common::LogDifference<"pfShields">(i, pfShields[i], rOther.pfShields[i]);
		bEqual &= common::LogDifference<"pfShieldCooldowns">(i, pfShieldCooldowns[i], rOther.pfShieldCooldowns[i]);
		bEqual &= common::LogDifference<"pfDestroyedExplosionTimes">(i, pfDestroyedExplosionTimes[i], rOther.pfDestroyedExplosionTimes[i]);
		bEqual &= common::LogDifference<"pfShieldDownSoundCooldowns">(i, pfShieldDownSoundCooldowns[i], rOther.pfShieldDownSoundCooldowns[i]);
		bEqual &= common::LogDifference<"pVecAiDirections">(i, pVecAiDirections[i], rOther.pVecAiDirections[i]);
		bEqual &= common::LogDifference<"pfTransferLockTimers">(i, pfTransferLockTimers[i], rOther.pfTransferLockTimers[i]);
		bEqual &= common::LogDifference<"pfArrivalGracePeriods">(i, pfArrivalGracePeriods[i], rOther.pfArrivalGracePeriods[i]);
		bEqual &= common::LogDifference<"pfFrameChangeTimers">(i, pfFrameChangeTimers[i], rOther.pfFrameChangeTimers[i]);
		bEqual &= common::LogDifference<"pfNavigationDelays">(i, pfNavigationDelays[i], rOther.pfNavigationDelays[i]);
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
