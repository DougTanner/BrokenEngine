#include "Players.h"

#include "Frame/Collections/Pushers/Pushers.h"
#include "Frame/CellStaticData.h"
#include "Frame/IslandTerrain.h"
#include "Frame/NavQuery.h"
#include "Ui/WrapperBase.h"

#include "Frame/TerrainUtils.h"
#include "Profile/ProfileManager.h"

namespace game
{

using enum PlayerFlags;

// Terrain collision
constexpr float kfTerrainPushVelocity = 15.0f;
constexpr float kfMaximumPushVelocity = 20.0f;

// Flagship follow
constexpr float kfFlagshipFollowDistanceSquared = 50.0f * 50.0f;
constexpr float kfFlagshipCloseDistanceSquared = 12.5f * 12.5f;

// Cadence recomputations are staggered by persisted global player ID; mode changes, destination selection,
// direction reseeding, and blocked position/lookahead probes can force earlier recomputation.
constexpr int64_t kiNavigationRecomputeInterval = 16;

// Off-cadence probes follow the held steering bearing to request a path before it enters an obstacle.
// Blocked probes can trigger per-tick recomputation; open water retains the cadence.
constexpr float kfNavigationLookahead = 8.0f;

// Returns the index into rStaticData.islands for the next navigation waypoint:
// 0 -> largest-area island in this frame, 1 -> smallest-area, 2+ -> the supplied random pick.
// Largest/smallest scan the frame's placements deterministically (no RNG, first-found tie-break);
// area uses the same anisotropic quad footprint the destination point is generated from.
static int64_t SelectIslandPlacement(const engine::CellStaticData& rStaticData, int64_t iWaypointIndex, int64_t iRandomPick)
{
	if (iWaypointIndex >= 2)
	{
		return iRandomPick;
	}

	bool bWantLargest = (iWaypointIndex == 0);
	int64_t iSelected = 0;
	float fSelectedArea = -1.0f;
	for (int64_t j = 0; j < std::ssize(rStaticData.islands); ++j)
	{
		const engine::IslandTemplate& rTemplate = engine::gpIslandTerrain->mIslands.at(rStaticData.islands.at(static_cast<size_t>(j)).islandCrc);
		float fArea = rTemplate.fQuadFootprintX * rTemplate.fQuadFootprintY;
		if (fSelectedArea < 0.0f || (bWantLargest ? (fArea > fSelectedArea) : (fArea < fSelectedArea)))
		{
			fSelectedArea = fArea;
			iSelected = j;
		}
	}
	return iSelected;
}

static void XM_CALLCONV UpdateFleetAndFlagshipNavigation(Frame& __restrict rFrame, const PlayersPostRender& rCurrent, const PlayersPostRender& rPrevious, const PlayersInterpolate& rPreviousInterpolate, int64_t i, FXMVECTOR vecPosition, const engine::CellStaticData& rStaticData, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, PlayerFlags_t flags, float fDeltaTime, int64_t& riNavigationDirection, XMVECTOR& rVecIslandDestination, float& rfCellChangeTimer)
{
	// Fleet navigation: navigate toward fleet's wanted coord after countdown expires
	if (!(fleetWantedCoordinate == rStaticData.coordinate) && iPendingFleetWantedCoordinateTicks == 0)
	{
		// Two coords anywhere in the signed-int32 identity range can separate by more than int32 holds, so each converts to int64 ahead of the subtraction.
		int64_t iDeltaX = static_cast<int64_t>(fleetWantedCoordinate.iX) - static_cast<int64_t>(rStaticData.coordinate.iX);
		int64_t iDeltaY = static_cast<int64_t>(fleetWantedCoordinate.iY) - static_cast<int64_t>(rStaticData.coordinate.iY);

		// Check if already heading in a valid direction toward wanted coord
		bool bAlreadyValid = false;
		if (riNavigationDirection >= 0 && riNavigationDirection <= 3)
		{
			switch (riNavigationDirection)
			{
				case 0: bAlreadyValid = iDeltaY > 0; break;
				case 1: bAlreadyValid = iDeltaY < 0; break;
				case 2: bAlreadyValid = iDeltaX > 0; break;
				case 3: bAlreadyValid = iDeltaX < 0; break;
				default: break;
			}
		}

		if (!bAlreadyValid)
		{
			int64_t iRandom = static_cast<int64_t>(common::Random(3i64, rFrame.postRender.randomEngine));
			if (iDeltaX != 0 && iDeltaY != 0)
			{
				riNavigationDirection = (iRandom < 2)
					? (iDeltaY > 0 ? 0 : 1)
					: (iDeltaX > 0 ? 2 : 3);
			}
			else if (iDeltaY != 0)
			{
				riNavigationDirection = iDeltaY > 0 ? 0 : 1;
			}
			else
			{
				riNavigationDirection = iDeltaX > 0 ? 2 : 3;
			}
			rVecIslandDestination = XMVectorZero();
			LOG(kNavData, kVerbose, "Player {} GlobalId: {} fleet override NavDir: {} WantedCoord: ({},{}) CellCoord: ({},{})", i, rCurrent.pGlobalPlayerIds[i], riNavigationDirection, fleetWantedCoordinate.iX, fleetWantedCoordinate.iY, rStaticData.coordinate.iX, rStaticData.coordinate.iY);
		}
	}

	// Flagship proximity: non-flagship in same cell follows flagship
	if (!(flags & kIsFlagship) && (fleetWantedCoordinate == rStaticData.coordinate))
	{
		for (int64_t j = 0; j < rPrevious.iCount; ++j)
		{
			if (j == i)
			{
				continue;
			}
			if (!(rPrevious.pFlags[j] & kIsFlagship))
			{
				continue;
			}

			XMVECTOR vecFlagshipPosition = rPreviousInterpolate.pVecPositions[j];
			float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, vecFlagshipPosition)));

			if (fDistanceSquared > kfFlagshipFollowDistanceSquared)
			{
				riNavigationDirection = 5;
				rVecIslandDestination = XMVectorSetW(vecFlagshipPosition, 1.0f);
			}
			else if (riNavigationDirection == 5)
			{
				rVecIslandDestination = XMVectorSetW(vecFlagshipPosition, 1.0f);
				if (fDistanceSquared < kfFlagshipCloseDistanceSquared)
				{
					riNavigationDirection = 4;
					rVecIslandDestination = XMVectorZero();
				}
			}
			break;
		}
	}

	if ((flags & kIsFlagship) && riNavigationDirection == 5 && (fleetWantedCoordinate == rStaticData.coordinate))
	{
		riNavigationDirection = 4;
		rVecIslandDestination = XMVectorZero();
	}

	// Cell change timer: cycle back to island destination when roaming
	if (riNavigationDirection == -1)
	{
		rfCellChangeTimer -= fDeltaTime;
		if (rfCellChangeTimer <= 0.0f)
		{
			riNavigationDirection = 4;
		}
	}
}

static bool ShouldRecomputeNavigation(const Frame& rFrame, const PlayersPostRender& rCurrent, int64_t i, FXMVECTOR vecPosition, FXMVECTOR vecArtificialIntelligenceDirection, int64_t iEntryNavigationDirection, bool bReseededDirection, int64_t iNavigationDirection, const engine::CellStaticData& rStaticData)
{
	// Cadence, mode changes, direction reseeding, and blocked position/lookahead probes request pathfinding.
	// Destination selection in mode 4 also forces it; otherwise cached steering is reused without NavQueryDirection.
	// Random draws, arrival checks, and mode transitions remain unconditional.
	bool bRecompute = bReseededDirection || (iNavigationDirection != iEntryNavigationDirection)
	               || (((rFrame.interpolate.iTick + rCurrent.pGlobalPlayerIds[i].iValue) % kiNavigationRecomputeInterval) == 0);

	// The throttle carries the shared world-space bearing for up to kiNavigationRecomputeInterval ticks. Recompute when
	// either the kfNavigationLookahead point or current position is inside a nav polygon: lookahead alone misses ships
	// already stranded inside. NavigationThresholdElevation and ApplyTerrainPush use
	// gBaseHeight - kfPlayerRadius - kfPushMargin; the nav polygon inflates that contour, leaving a no-nav band
	// with zero terrain push. rVecArtificialIntelligenceDirection, vecPosition, and server-built, wire-shipped navigationData match on
	// client/server, and this check draws no RNG. Roam mode -1 is exempt because ComputeArtificialIntelligenceSteering re-steers
	// every tick.
	if (!bRecompute && iNavigationDirection >= 0 && XMVectorGetX(XMVector3LengthSq(vecArtificialIntelligenceDirection)) > 0.001f)
	{
		XMVECTOR vecLookahead = XMVectorMultiplyAdd(XMVectorReplicate(kfNavigationLookahead), vecArtificialIntelligenceDirection, vecPosition);
		if (engine::NavQueryPointBlocked(vecLookahead, rStaticData.navigationData) || engine::NavQueryPointBlocked(vecPosition, rStaticData.navigationData))
		{
			bRecompute = true;
		}
	}

	return bRecompute;
}

static void XM_CALLCONV RecomputeNavigationPath([[maybe_unused]] PlayersPostRender& rCurrent, [[maybe_unused]] int64_t i, FXMVECTOR vecPosition, FXMVECTOR vecDestination, const engine::CellStaticData& rStaticData, XMVECTOR& rVecArtificialIntelligenceDirection)
{
	XMVECTOR vecDebugWaypoint = XMVectorZero();
	XMVECTOR vecNavigationDirection = XMVectorZero();
#if defined(BT_SERVER)
	bool bEnteredAStar = false;
#endif // BT_SERVER
	{
		engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdateNavigationQuery);
#if defined(BT_SERVER)
		vecNavigationDirection = engine::NavQueryDirection(vecPosition, vecDestination, rStaticData.navigationData, &vecDebugWaypoint, &bEnteredAStar);
#else
		vecNavigationDirection = engine::NavQueryDirection(vecPosition, vecDestination, rStaticData.navigationData, &vecDebugWaypoint);
#endif // BT_SERVER
	}
#if defined(BT_SERVER)
	if constexpr (kbProfiling)
	{
		gpProfileManager->AddRawCpuTimerAuxiliaryCount(game::kCpuTimerPostRenderUpdateNavigationQuery, static_cast<int64_t>(bEnteredAStar));
	}
#endif // BT_SERVER
	if (XMVectorGetX(XMVector3LengthSq(vecNavigationDirection)) > 0.001f)
	{
		rVecArtificialIntelligenceDirection = vecNavigationDirection;
	}
	else
	{
		rVecArtificialIntelligenceDirection = XMVector3Normalize(XMVectorSetZ(XMVectorSubtract(vecDestination, vecPosition), 0.0f));
	}
#if defined(BT_CLIENT)
	if constexpr (kbDebugRender)
	{
		rCurrent.pVecDebugNavigationWaypoints[i] = vecDebugWaypoint;
	}
#endif // BT_CLIENT
}

void PlayersPostRender::Transfer([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData)
{
	PlayersInterpolate& rCurrentInterpolate = *rFrame.interpolate.pPlayers;
	PlayersPostRender& rCurrentPostRender = *rFrame.postRender.pPlayers;

	engine::CellBounds bounds = engine::ComputeCellBounds(engine::LocalCellArea());

	// Reverse iteration for swap-and-pop safety with RemoveIndexableElement
	for (int64_t i = rCurrentInterpolate.iCount - 1; i >= 0; --i)
	{
		if (!(rCurrentPostRender.pFlags[i] & kTransfer)) [[likely]]
		{
			continue;
		}

		XMVECTOR vecPosition = rCurrentInterpolate.pVecPositions[i];

		// Build transfer request
		TransferRequest request
		{
			.eType = StatusChangeType::kTransferPlayer,
			.data =
			{
				.vecPosition = vecPosition,
				.vecDirection = rCurrentInterpolate.pVecDirections[i],
				.vecVelocity = rCurrentPostRender.pVecVelocities[i],
				.alignment = rCurrentPostRender.pAlignments[i],
				.fHealth = rCurrentPostRender.pfArmors[i],
				.fShield = rCurrentPostRender.pfShields[i],
				.nextBlasterFireTimeSeconds = std::chrono::duration<float>(rCurrentPostRender.pfNextBlasterFireTimes[i]),
				.nextSecondarySpawnTimeSeconds = std::chrono::duration<float>(rCurrentPostRender.pfNextSecondarySpawnTimes[i]),
				.shieldCooldownSeconds = std::chrono::duration<float>(rCurrentPostRender.pfShieldCooldowns[i]),
				.shieldDownSoundCooldownSeconds = std::chrono::duration<float>(rCurrentPostRender.pfShieldDownSoundCooldowns[i]),
				.animationTimeSeconds = std::chrono::duration<float>(rCurrentInterpolate.pfAnimationTimes[i]),
				.uiPlayerFlags = static_cast<uint16_t>(std::to_underlying(rCurrentPostRender.pFlags[i].meFlags) & ~std::to_underlying(kTransfer)),
				.navigationDelaySeconds = std::chrono::duration<float>(rCurrentPostRender.pfNavigationDelays[i]),
			},
		};
		request.data.globalPlayerId = rCurrentPostRender.pGlobalPlayerIds[i];
		request.data.fleetWantedCoordinate = rCurrentPostRender.pFleetWantedCoordinates[i];
		request.data.uiPendingFleetWantedCoordinateTicks = rCurrentPostRender.puiPendingFleetWantedCoordinateTicks[i];
		request.data.uiPendingWeaponModeTicks = rCurrentPostRender.puiPendingWeaponModeTicks[i];
		request.data.uiClientGuidHigh = rCurrentPostRender.pClientGuids[i].uiHigh;
		request.data.uiClientGuidLow = rCurrentPostRender.pClientGuids[i].uiLow;
		if (PrepareTransferRequest(rFrame.postRender, bounds, request)) [[unlikely]]
		{
			LOG(kDefault, kError, "Player Transfer capacity hit Tick: {} Source: ({},{}) Index: {} Position: {} Velocity: {} Delta: ({},{}) GlobalPlayerId: {} Alignment: {} SourceCount: {} Pushed: {} Capacity: {}", rFrame.interpolate.iTick, rStaticData.coordinate.iX, rStaticData.coordinate.iY, i, common::WbV2(vecPosition, 1), common::WbV2(rCurrentPostRender.pVecVelocities[i], 1), static_cast<int32_t>(request.iDeltaX), static_cast<int32_t>(request.iDeltaY), rCurrentPostRender.pGlobalPlayerIds[i], rCurrentPostRender.pAlignments[i], rCurrentInterpolate.iCount, std::ssize(rFrame.postRender.transferRequests), rFrame.postRender.transferRequests.capacity());
			DEBUG_BREAK();
		}
		PushTransferRequest(rFrame.postRender, request);

		engine::PushersPostRender::Remove(rFrame, rCurrentInterpolate.pPushers[i]);
#if defined(BT_CLIENT)
		PlayersInterpolate::RemoveOwnedVisuals(rFrame, rCurrentInterpolate, i);
#endif // BT_CLIENT

		engine::RemoveIndexableElement(rCurrentInterpolate, rCurrentPostRender, rCurrentPostRender.pIds[i], rCurrentInterpolate.Members(), rCurrentPostRender.Members());
	}
}

void XM_CALLCONV PlayersPostRender::ComputeNavigation([[maybe_unused]] Frame& __restrict rFrame, [[maybe_unused]] const Frame& __restrict rPreviousFrame, [[maybe_unused]] const engine::CellStaticData& rStaticData, int64_t i, FXMVECTOR vecPosition, FXMVECTOR vecCellCenter, engine::GridCoord fleetWantedCoordinate, int64_t iPendingFleetWantedCoordinateTicks, PlayerFlags_t flags, float fDeltaTime, int64_t& riNavigationDirection, int64_t& riNavigationWaypointIndex, XMVECTOR& rVecArtificialIntelligenceDirection, XMVECTOR& rVecIslandDestination, float& rfCellChangeTimer)
{
	PlayersPostRender& __restrict rCurrent = *rFrame.postRender.pPlayers;
	const PlayersPostRender& rPrevious = *rPreviousFrame.postRender.pPlayers;
	const PlayersInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pPlayers;

	// Throttle bookkeeping: capture mode before the fleet/flagship/timer blocks can change it, and note
	// when the direction is re-seeded. Both force an immediate pathfind below (see bRecompute).
	int64_t iEntryNavigationDirection = riNavigationDirection;
	bool bReseededDirection = false;

	// Initialize direction if zero (first spawn or after reset)
	if (XMVectorGetX(XMVector3LengthSq(rVecArtificialIntelligenceDirection)) < 0.001f)
	{
		float fAngle = common::Random<XM_2PI>(rFrame.postRender.randomEngine);
		rVecArtificialIntelligenceDirection = XMVector4Transform(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMMatrixRotationZ(fAngle));
		bReseededDirection = true;
	}

	UpdateFleetAndFlagshipNavigation(rFrame, rCurrent, rPrevious, rPreviousInterpolate, i, vecPosition, rStaticData, fleetWantedCoordinate, iPendingFleetWantedCoordinateTicks, flags, fDeltaTime, riNavigationDirection, rVecIslandDestination, rfCellChangeTimer);

	bool bRecompute = ShouldRecomputeNavigation(rFrame, rCurrent, i, vecPosition, rVecArtificialIntelligenceDirection, iEntryNavigationDirection, bReseededDirection, riNavigationDirection, rStaticData);

	if (riNavigationDirection == 5)
	{
		// Missing flagships leave the proximity scan empty. OnPlayerDeath shifts the flagship after removal;
		// ProcessFlagshipUpdates queues kUpdateFleet, whose Update handler writes kIsFlagship.
		// Followers normally stall one tick before replacement or fleet override. A same-cell promoted flagship
		// recovers on D+2 via mode 4, clearing its stale destination without advancing the rally timer.
		// Agent-injected players have no Fleet updates. Mode 5 follows without arrival or missing-flagship fallback.
		// It consumes three draws (island pick, footprint X/Y) each tick with islands; mode 4 draws only when
		// selecting a destination. Both skip draws without islands. Every Random call advances once regardless of bound.
		if (!rStaticData.islands.empty())
		{
			common::Random(std::ssize(rStaticData.islands) - 1, rFrame.postRender.randomEngine);
			const engine::IslandPlacement& rRandomPlacement = rStaticData.islands.at(static_cast<size_t>(i % std::ssize(rStaticData.islands)));
			const engine::IslandTemplate& rRandomTemplate = engine::gpIslandTerrain->mIslands.at(rRandomPlacement.islandCrc);
			common::Random(rRandomTemplate.fQuadFootprintX, rFrame.postRender.randomEngine);
			common::Random(rRandomTemplate.fQuadFootprintY, rFrame.postRender.randomEngine);
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rCurrent, i, vecPosition, rVecIslandDestination, rStaticData, rVecArtificialIntelligenceDirection);
		}
	}
	else if (riNavigationDirection == 4)
	{
		// Navigate to island destination
		if (XMVectorGetW(rVecIslandDestination) == 0.0f)
		{
			// Waypoints select the largest, smallest, then random island by quad-footprint area.
			// All waypoint indices consume the island-pick draw before the two footprint draws, keeping each
			// destination selection at three draws. The sampled point lies in the selected island's quad-footprint
			// AABB; island-free cells skip draws and target the cell center.
			if (rStaticData.islands.empty())
			{
				rVecIslandDestination = vecCellCenter;
				bRecompute = true;
			}
			else
			{
				int64_t iRandomPick = common::Random(std::ssize(rStaticData.islands) - 1, rFrame.postRender.randomEngine);
				int64_t iPlacement = SelectIslandPlacement(rStaticData, riNavigationWaypointIndex, iRandomPick);
				if (riNavigationWaypointIndex < 2)
				{
					++riNavigationWaypointIndex;
				}
				const engine::IslandPlacement& rPlacement = rStaticData.islands.at(static_cast<size_t>(iPlacement));
				const engine::IslandTemplate& rTemplate = engine::gpIslandTerrain->mIslands.at(rPlacement.islandCrc);
				float fIslandMinimumX = rPlacement.f2WorldPosition.x - 0.5f * rTemplate.fQuadFootprintX;
				float fIslandMinimumY = rPlacement.f2WorldPosition.y - 0.5f * rTemplate.fQuadFootprintY;

				float fX = fIslandMinimumX + common::Random(rTemplate.fQuadFootprintX, rFrame.postRender.randomEngine);
				float fY = fIslandMinimumY + common::Random(rTemplate.fQuadFootprintY, rFrame.postRender.randomEngine);
				rVecIslandDestination = XMVectorSet(fX, fY, engine::gBaseHeight.mfCurrent, 1.0f);

				// Snap to navigable area if inside an obstacle
				rVecIslandDestination = engine::NavQuerySnapToNavigable(rVecIslandDestination, rStaticData.navigationData);
				LOG(kNavData, kVerbose, "Player {} NavSwitch: island dest generated pos={} dest={}", i, common::WbV2(vecPosition, 1), common::WbV2(rVecIslandDestination, 1));
				bRecompute = true; // new destination -> path immediately
			}
		}

		if (bRecompute)
		{
			RecomputeNavigationPath(rCurrent, i, vecPosition, rVecIslandDestination, rStaticData, rVecArtificialIntelligenceDirection);
		}

		// Arrival check (unconditional — independent of the pathfind throttle). Zeroing the destination
		// makes the next tick's mode-4 entry block immediately pick the next island in the
		// largest -> smallest -> random sequence (no idle between islands). Staying in mode 4 keeps the
		// tour progressing; the fleet still leaves the cell when the server cell-change timer fires.
		float fDistanceSquared = XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(vecPosition, rVecIslandDestination)));
		if (fDistanceSquared < 100.0f)
		{
			LOG(kNavData, kVerbose, "Player {} NavSwitch: arrived at island dest pos={} dest={}", i, common::WbV2(vecPosition, 1), common::WbV2(rVecIslandDestination, 1));
			rVecIslandDestination = XMVectorZero();
		}
	}
	else if (riNavigationDirection >= 0)
	{
		// Navigate toward neighboring cell center
		rVecIslandDestination = XMVectorZero();
		XMVECTOR vecDestination = vecCellCenter;
		switch (riNavigationDirection)
		{
			case 0:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(0.0f, engine::kfCellHeight, 0.0f, 0.0f));
				break;
			case 1:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(0.0f, -engine::kfCellHeight, 0.0f, 0.0f));
				break;
			case 2:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(engine::kfCellWidth, 0.0f, 0.0f, 0.0f));
				break;
			case 3:
				vecDestination = XMVectorAdd(vecCellCenter, XMVectorSet(-engine::kfCellWidth, 0.0f, 0.0f, 0.0f));
				break;
			default:
				break;
		}

		if (bRecompute)
		{
			XMVECTOR vecDebugWaypoint = XMVectorZero();
			XMVECTOR vecNavigationDirection = XMVectorZero();
#if defined(BT_SERVER)
			bool bEnteredAStar = false;
#endif // BT_SERVER
			{
				engine::ScopedCpuProfile scopedCpuProfile(game::kCpuTimerPostRenderUpdateNavigationQuery);
#if defined(BT_SERVER)
				vecNavigationDirection = engine::NavQueryDirection(vecPosition, vecDestination, rStaticData.navigationData, kbDebugRender ? &vecDebugWaypoint : nullptr, &bEnteredAStar);
#else
				vecNavigationDirection = engine::NavQueryDirection(vecPosition, vecDestination, rStaticData.navigationData, kbDebugRender ? &vecDebugWaypoint : nullptr);
#endif // BT_SERVER
			}
#if defined(BT_SERVER)
			if constexpr (kbProfiling)
			{
				gpProfileManager->AddRawCpuTimerAuxiliaryCount(game::kCpuTimerPostRenderUpdateNavigationQuery, static_cast<int64_t>(bEnteredAStar));
			}
#endif // BT_SERVER
			float fNavigationLengthSquared = XMVectorGetX(XMVector3LengthSq(vecNavigationDirection));
			if (fNavigationLengthSquared > 0.001f)
			{
				rVecArtificialIntelligenceDirection = vecNavigationDirection;
			}
			else
			{
				rVecArtificialIntelligenceDirection = XMVector3Normalize(XMVectorSetZ(XMVectorSubtract(vecDestination, vecPosition), 0.0f));
				LOG(kNavData, kWarning, "Player {} navQuery returned zero, fallback dir={}", i, common::WbV2(rVecArtificialIntelligenceDirection, 1));
			}
#if defined(BT_CLIENT)
			if constexpr (kbDebugRender)
			{
				rCurrent.pVecDebugNavigationWaypoints[i] = vecDebugWaypoint;
			}
#endif // BT_CLIENT
		}
	}
	else
	{
		rVecIslandDestination = XMVectorZero();
		auto [vecNewArtificialIntelligenceDirection] = ComputeArtificialIntelligenceSteering(rStaticData, vecPosition, rVecArtificialIntelligenceDirection, vecCellCenter, fDeltaTime, i % 2 == 0);
		rVecArtificialIntelligenceDirection = vecNewArtificialIntelligenceDirection;
#if defined(BT_CLIENT)
		if constexpr (kbDebugRender)
		{
			rCurrent.pVecDebugNavigationWaypoints[i] = XMVectorZero();
		}
#endif // BT_CLIENT
	}
}

void XM_CALLCONV PlayersPostRender::ApplyMovement(int64_t iNavigationDirection, FXMVECTOR vecArtificialIntelligenceDirection, float fDeltaTime, float fAccelerationMultiplier, float fDecayMultiplier, XMVECTOR& rVecVelocity)
{
	float fAcceleration = (iNavigationDirection == 5 ? kfPlayerCatchUpAcceleration : kfPlayerAcceleration) * fAccelerationMultiplier;
	float fMaximumSpeed = iNavigationDirection == 5 ? kfPlayerCatchUpMaximumSpeed : kfPlayerMaximumSpeed;
	rVecVelocity = engine::ApplyMovement(rVecVelocity, vecArtificialIntelligenceDirection, fDeltaTime, fAcceleration, kfPlayerDrag * fDecayMultiplier, fMaximumSpeed);
}

void XM_CALLCONV PlayersPostRender::ApplyTerrainPush(const engine::CellStaticData& rStaticData, FXMVECTOR vecPosition, XMVECTOR& rVecVelocity)
{
	// Terrain collision - add velocity away from terrain, gentle at first then ramping up
	float fElevation = engine::gpIslandTerrain->MakeCellElevationSampler(rStaticData).Sample(vecPosition);
	float fPushHeight = engine::gBaseHeight.mfCurrent - kfPlayerRadius - kfPushMargin;
	if (fElevation >= fPushHeight) [[unlikely]]
	{
		XMVECTOR vecTerrainNormal = XMVector3Normalize(XMVectorSetZ(engine::gpIslandTerrain->CellNormal(rStaticData, vecPosition), 0.0f));
		float fPenetration = fElevation - fPushHeight;
		float fPushStrength = fPenetration * fPenetration * kfTerrainPushVelocity;
		rVecVelocity = engine::ApplyClampedPush(rVecVelocity, vecTerrainNormal, fPushStrength, kfMaximumPushVelocity);
	}
}

void XM_CALLCONV PlayersPostRender::ApplyPusherPush(const Frame& __restrict rFrame, const Frame& __restrict rPreviousFrame, int64_t i, FXMVECTOR vecPosition, XMVECTOR& rVecVelocity)
{
	const PlayersInterpolate& rPreviousInterpolate = *rPreviousFrame.interpolate.pPlayers;

	// Player-to-player push (prevents overlap)
	XMVECTOR vecPush = engine::PushersInterpolate::ApplyPush(rFrame.interpolate, vecPosition, rPreviousInterpolate.pPushers[i]);
	float fPushLength = XMVectorGetX(XMVector3Length(vecPush));
	if (fPushLength > 0.0f)
	{
		XMVECTOR vecPushDirection = XMVectorDivide(vecPush, XMVectorReplicate(fPushLength));
		rVecVelocity = engine::ApplyClampedPush(rVecVelocity, vecPushDirection, fPushLength, kfPlayerMaximumPusherPushVelocity);
	}
}

} // namespace game
