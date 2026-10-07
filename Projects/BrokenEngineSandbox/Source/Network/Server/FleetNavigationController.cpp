#include "Pch.h"

#include "Network/Server/FleetNavigationController.h"

#include "Frame/Collections/Players/Players.h"
#include "Network/PlayerEvents.h"
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
		default: return {.iX = 0, .iY = 0};
	}
}

void FleetNavigationController::TickFleetTimers(std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, common::RandomEngine& rRandom)
{
	for (auto& [rGuid, rFleetVector] : rFleets)
	{
		for (Fleet& rFleet : rFleetVector)
		{
			auto flagshipIt = std::ranges::find(rFleet.members, rFleet.flagshipGlobalPlayerId, &FleetMember::globalPlayerId);
			if (!(rFleet.flagshipGlobalPlayerId.iValue != 0) || flagshipIt == rFleet.members.end())
			{
				continue;
			}

			const FleetMember& rFlagship = *flagshipIt;
			if (rFlagship.flags & FleetMemberFlags::kIsDead)
			{
				continue;
			}

			// Drain cellChangeTimerSeconds only while the flagship is at wantedCoordinate, so the cycle is transit plus
			// navigationDelaySeconds of idle time. Drain there even in cardinal mode; an expired timer fires when that mode
			// ends instead of freezing. GameBase::ServerUpdate supplies mfLastDeltaTime = iFullTicks * kfDeltaTime
			// after pause/time-scale resolution. BuildFrameInputs runs this only for advancing updates, including
			// mTimeStep fast-forward/slow-motion scaling, keeping it in tick lockstep.
			if (rFlagship.coordinate == rFleet.wantedCoordinate)
			{
				rFleet.cellChangeTimerSeconds -= std::chrono::duration<float>(gpGame->mfLastDeltaTime);
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

			const PlayersPostRender& rPlayers = *(*gpGame->mCells.at(rFlagship.coordinate).pCurrent).postRender.pPlayers;
			bool bFoundFlagship = false;
			int64_t iFlagshipNavigationDirection = -1;
			for (int64_t k = 0; k < rPlayers.iCount; ++k)
			{
				if (rPlayers.pGlobalPlayerIds[k] == rFlagship.globalPlayerId)
				{
					iFlagshipNavigationDirection = GetNavigationDirection(rPlayers.pFlags[k]);
					bFoundFlagship = true;
					break;
				}
			}
			if (!bFoundFlagship)
			{
				continue;
			}

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
			mPendingFlagshipUpdates.push_back({.clientGuid = rGuid, .fleetGuid = rFleet.guid, .newWantedCoordinate = destination, .iPendingFleetWantedCoordinateTicks = iPendingTicks});
			LOG(kNetwork, kVerbose, "FleetNavigationController::TickFleetTimers Guid: ({},{}) FleetGuid: ({},{}) Direction: {} WantedCoord: ({},{})", rGuid.uiHigh, rGuid.uiLow, rFleet.guid.uiHigh, rFleet.guid.uiLow, iDirection, destination.iX, destination.iY);
		}
	}
}

void FleetNavigationController::ProcessFlagshipUpdates(const std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets)
{
	for (const PendingFlagshipUpdate& rUpdate : mPendingFlagshipUpdates)
	{
		auto fleetIt = rFleets.find(rUpdate.clientGuid);
		if (fleetIt == rFleets.end())
		{
			continue;
		}

		auto matchIt = std::ranges::find(fleetIt->second, rUpdate.fleetGuid, &Fleet::guid);
		if (matchIt == fleetIt->second.end())
		{
			continue;
		}
		const Fleet& rFleet = *matchIt;

		if (!(rFleet.flagshipGlobalPlayerId.iValue != 0) || !std::ranges::contains(rFleet.members, rFleet.flagshipGlobalPlayerId, &FleetMember::globalPlayerId))
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
			bool bMemberIsFlagship = (rMember.globalPlayerId == rFleet.flagshipGlobalPlayerId);

			auto frameInputIt = gpGame->mFrameInputs.find(memberCoordinate);
			if (frameInputIt == gpGame->mFrameInputs.end())
			{
				continue;
			}
			if (!gpGame->mCells.contains(memberCoordinate))
			{
				continue;
			}

			const PlayersPostRender& rPlayers = *(*gpGame->mCells.at(memberCoordinate).pCurrent).postRender.pPlayers;
			for (int64_t k = 0; k < rPlayers.iCount; ++k)
			{
				if (rPlayers.pGlobalPlayerIds[k] == rMember.globalPlayerId)
				{
					int64_t iPlayerUuid = rPlayers.pIds[k].uuid.iValue;
					frameInputIt->second.statusChanges.push_back(
					{
						.eType = StatusChangeType::kUpdateFleet,
						.data = UpdateFleetData
						{
							.iPlayerUuid = iPlayerUuid,
							.bIsFlagship = bMemberIsFlagship,
							.fleetWantedCoordinate = rUpdate.newWantedCoordinate,
							.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(rUpdate.iPendingFleetWantedCoordinateTicks),
						},
					});
					++iMembersUpdated;
					break;
				}
			}
		}
		LOG(kNetwork, kVerbose, "FleetNavigationController::ProcessFlagshipUpdates Guid: ({},{}) FleetGuid: ({},{}) MembersUpdated: {} WantedCoord: ({},{})", rUpdate.clientGuid.uiHigh, rUpdate.clientGuid.uiLow, rUpdate.fleetGuid.uiHigh, rUpdate.fleetGuid.uiLow, iMembersUpdated, rUpdate.newWantedCoordinate.iX, rUpdate.newWantedCoordinate.iY);
	}
	mPendingFlagshipUpdates.clear();
}

void FleetNavigationController::ShiftFlagshipAfterDeath(const engine::ClientGuid& rGuid, Fleet& rFleet)
{
	// The flagship's list position is only the rotation start; every caller has already matched the flagship to a member.
	int64_t iFlagshipPosition = std::ranges::find(rFleet.members, rFleet.flagshipGlobalPlayerId, &FleetMember::globalPlayerId) - rFleet.members.begin();
	int64_t iNewFlagship = -1;
	for (int64_t k = 1; k < std::ssize(rFleet.members); ++k)
	{
		int64_t iCandidate = (iFlagshipPosition + k) % std::ssize(rFleet.members);
		if (!(rFleet.members.at(static_cast<size_t>(iCandidate)).flags & FleetMemberFlags::kIsDead))
		{
			iNewFlagship = iCandidate;
			break;
		}
	}
	if (iNewFlagship < 0)
	{
		return;
	}

	const FleetMember& rNewFlagship = rFleet.members.at(static_cast<size_t>(iNewFlagship));
	rFleet.flagshipGlobalPlayerId = rNewFlagship.globalPlayerId;
	rFleet.wantedCoordinate = rNewFlagship.coordinate;
	rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;
	mPendingFlagshipUpdates.push_back({.clientGuid = rGuid, .fleetGuid = rFleet.guid, .newWantedCoordinate = rFleet.wantedCoordinate});
}

#endif // BT_SERVER

} // namespace game
