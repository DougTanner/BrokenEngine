#include "Pch.h"

#include "Network/Server/FleetNavigationController.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

static engine::GridCoord NavigationDirectionOffset(int64_t iNavigationDirection)
{
	switch (iNavigationDirection)
	{
		case 0: return {.iX = 0, .iY = 1};
		case 1: return {.iX = 0, .iY = -1};
		case 2: return {.iX = 1, .iY = 0};
		case 3: return {.iX = -1, .iY = 0};
		default:
			ASSERT(false);
			std::unreachable();
	}
}

// Owners walk in GUID order because each fleet's direction draw consumes the one fleet RNG, and replay playback must
// draw in the recording's order.
void FleetNavigationController::TickFleetTimers(std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, common::RandomEngine& rRandom)
{
	common::ScopedWorkbufferArena ownersArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const engine::ClientGuid& rGuid : SortedGuidKeys(rFleets, ownersArena))
	{
		for (Fleet& rFleet : rFleets.at(rGuid))
		{
			const FleetMember& rFlagship = *std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId);
			if (rFlagship.flags & FleetMemberFlags::kIsDead)
			{
				continue;
			}

			// Drain cellChangeTimerSeconds only while the flagship is at wantedCoordinate, so the cycle is transit plus
			// navigationDelaySeconds of idle time. Drain there even in cardinal mode; an expired timer fires when that mode
			// ends instead of freezing. The fleet step runs once per tick, so each pass drains one tick.
			if (rFlagship.coordinate == rFleet.wantedCoordinate)
			{
				rFleet.cellChangeTimerSeconds -= std::chrono::duration<float>(engine::kfDeltaTime);
			}

			if (rFleet.cellChangeTimerSeconds.count() > 0.0f)
			{
				continue;
			}

			// Cardinal ejection between ticks can move the flagship after the timer last drained.
			if (!(rFlagship.coordinate == rFleet.wantedCoordinate))
			{
				continue;
			}
			if (!gpGame->mCells.contains(rFlagship.coordinate))
			{
				continue;
			}

			const Frame& rFlagshipFrame = *gpGame->mCells.at(rFlagship.coordinate).pCurrent;
			std::optional<ShipLocator> optionalFlagship = FindShipByGlobalId(rFlagshipFrame, rFlagship.globalId);
			if (!optionalFlagship)
			{
				continue;
			}
			int64_t iFlagshipNavigationDirection = VisitShip(rFlagshipFrame, *optionalFlagship, []([[maybe_unused]] const auto& rInterpolate, const auto& rPostRender, int64_t iRow)
			{
				return GetNavigationDirection(rPostRender.pFlags[iRow]);
			});

			if (iFlagshipNavigationDirection >= 0 && iFlagshipNavigationDirection <= 3)
			{
				continue;
			}

			int64_t iDirection = common::Random(3i64, rRandom);
			engine::GridCoord offset = NavigationDirectionOffset(iDirection);
			engine::GridCoord destination {};
			if (!engine::TryAddGridCoordinate(rFlagship.coordinate, offset.iX, offset.iY, destination)) [[unlikely]]
			{
				// A flagship at a numeric coordinate edge has no neighbour that way, so this move never happens
				// rather than wrapping to the far side of the grid. wantedCoordinate already equals the flagship coord,
				// so resetting the timer alone spends a full navigationDelaySeconds before the next draw instead of
				// redrawing a direction every tick.
				rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;
				continue;
			}
			int64_t iPendingTicks = engine::kiTickRate;
			rFleet.wantedCoordinate = destination;
			rFleet.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(iPendingTicks);
			rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;

			// Playback takes every frame change from the recording.
			if (gpGame->mbReplaying)
			{
				continue;
			}

			int64_t iMembersUpdated = 0;
			for (const FleetMember& rMember : rFleet.members)
			{
				if (rMember.flags & FleetMemberFlags::kIsDead)
				{
					continue;
				}

				engine::GridCoord memberCoordinate = rMember.coordinate;

				auto frameInputIt = gpGame->mFrameInputs.find(memberCoordinate);
				if (frameInputIt == gpGame->mFrameInputs.end())
				{
					continue;
				}

				ShipUuid shipUuid = ShipUuidByGlobalId(*gpGame->mCells.at(memberCoordinate).pCurrent, rMember.globalId);
				if (shipUuid.iUuid == 0)
				{
					continue;
				}
				frameInputIt->second.statusChanges.push_back(
				{
					.eType = StatusChangeType::kUpdateFleet,
					.data = UpdateFleetData
					{
						.eType = shipUuid.eType,
						.iUuid = shipUuid.iUuid,
						.fleetWantedCoordinate = destination,
						.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(iPendingTicks),
					},
				});
				++iMembersUpdated;
			}
			LOG(kNetwork, kVerbose, "FleetNavigationController::TickFleetTimers Guid: ({},{}) FleetGuid: ({},{}) Direction: {} WantedCoord: ({},{}) MembersUpdated: {}", rGuid.uiHigh, rGuid.uiLow, rFleet.guid.uiHigh, rFleet.guid.uiLow, iDirection, destination.iX, destination.iY, iMembersUpdated);
		}
	}
}

#endif // BT_SERVER

} // namespace game
