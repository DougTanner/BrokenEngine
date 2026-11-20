#include "Pch.h"

#if defined(BT_SERVER)

#include "Network/Server/FleetRows.h"

#include "Network/Server/ServerTransferManager.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipQueries.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

// Pushes into each live member's cell input the status change rMakeStatusChange builds for the member's row.
template <typename MEMBERS, typename MAKE_STATUS_CHANGE>
static void QueueLiveMemberStatusChanges(MEMBERS&& rMembers, const MAKE_STATUS_CHANGE& rMakeStatusChange)
{
	if (gpGame->mbReplaying)
	{
		return;
	}

	for (const FleetMember& rMember : rMembers)
	{
		if (rMember.flags & FleetMemberFlags::kIsDead)
		{
			continue;
		}

		ShipUuid shipUuid = ShipUuidByGlobalId(*gpGame->mCells.at(rMember.coordinate).pCurrent, rMember.globalId);
		ASSERT(shipUuid.iUuid != 0);
		gpGame->mFrameInputs.at(rMember.coordinate).statusChanges.push_back(rMakeStatusChange(shipUuid));
	}
}

// The fleet step runs while miTickCounter names the tick being prepared; that tick's post-tick batch creates the row. The
// owner's GUID rides the spawn, so the row is born owned.
template <typename SPAWN_INFO>
void QueueFleetSpawn(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, engine::GridCoord coordinate, SPAWN_INFO spawnInfo)
{
	bool bIsFlagship = rMember.globalId == rFleet.flagshipGlobalId;
	ShipOrders_t orders = rFleet.pSquadronOrders[std::to_underlying(rMember.eType)][rMember.uiSquadron];
	if ((rMember.eType == ShipType::kFighter || rMember.eType == ShipType::kOmni) && rMember.uiSquadron == 1)
	{
		orders.Set(ShipOrders::kSquadron1);
	}
	if (bIsFlagship)
	{
		orders.Set(ShipOrders::kIsFlagship);
	}
	SetTargetFlightPlane(orders, DefaultFlightPlane(rMember.eType));

	spawnInfo.alignment = gpGame->mFleetAlignment;
	spawnInfo.eController = ShipController::kFleet;
	spawnInfo.orders = orders;
	spawnInfo.globalId = rMember.globalId;
	spawnInfo.clientGuid = rClientGuid;
	spawnInfo.flagshipGlobalId = rFleet.flagshipGlobalId;
	spawnInfo.fleetWantedCoordinate = rFleet.wantedCoordinate;
	spawnInfo.uiPendingFleetWantedCoordinateTicks = rFleet.uiPendingFleetWantedCoordinateTicks;
	if (!gpGame->mbReplaying)
	{
		gpServerSession->mpTransferManager->QueueSpawn(coordinate, gpGame->miTickCounter, MakeSpawn(spawnInfo));
	}
	LOG(kNetwork, kVerbose, "QueueFleetSpawn Type: {} Guid: ({},{}) GlobalId: {} Coord: ({},{}) Flagship: {} FlagshipGlobalId: {}", ShipTypeName(rMember.eType), rClientGuid.uiHigh, rClientGuid.uiLow, rMember.globalId.iValue, coordinate.iQ, coordinate.iR, bIsFlagship, rFleet.flagshipGlobalId.iValue);
}

template void QueueFleetSpawn(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, engine::GridCoord coordinate, FightersPostRender::SpawnInfo spawnInfo);
template void QueueFleetSpawn(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, engine::GridCoord coordinate, OmnisPostRender::SpawnInfo spawnInfo);
template void QueueFleetSpawn(const engine::ClientGuid& rClientGuid, const Fleet& rFleet, const FleetMember& rMember, engine::GridCoord coordinate, BattleshipsPostRender::SpawnInfo spawnInfo);

void QueueUpdateFleet(const Fleet& rFleet)
{
	QueueLiveMemberStatusChanges(rFleet.members, [&rFleet](ShipUuid shipUuid)
	{
		return StatusChange
		{
			.eType = StatusChangeType::kUpdateFleet,
			.data = UpdateFleetData
			{
				.eType = shipUuid.eType,
				.iUuid = shipUuid.iUuid,
				.fleetWantedCoordinate = rFleet.wantedCoordinate,
				.uiPendingFleetWantedCoordinateTicks = rFleet.uiPendingFleetWantedCoordinateTicks,
			},
		};
	});
}

void QueueUpdateSquadronOrders(const Fleet& rFleet, ShipType eType, uint8_t uiSquadron)
{
	ShipOrders_t orders = rFleet.pSquadronOrders[std::to_underlying(eType)][uiSquadron];
	QueueLiveMemberStatusChanges(rFleet.members | std::views::filter([eType, uiSquadron](const FleetMember& rMember)
	{
		return rMember.eType == eType && rMember.uiSquadron == uiSquadron;
	}), [orders](ShipUuid shipUuid)
	{
		return StatusChange {.eType = StatusChangeType::kUpdateSquadronOrders, .data = UpdateSquadronOrdersData {.eType = shipUuid.eType, .iUuid = shipUuid.iUuid, .orders = orders}};
	});
}

void QueueDestroyShip(const FleetMember& rMember)
{
	QueueLiveMemberStatusChanges(std::span<const FleetMember>(&rMember, 1), [](ShipUuid shipUuid)
	{
		return StatusChange {.eType = StatusChangeType::kDestroyShip, .data = DestroyShipData {.eType = shipUuid.eType, .iUuid = shipUuid.iUuid}};
	});
}

} // namespace game

#endif // BT_SERVER
