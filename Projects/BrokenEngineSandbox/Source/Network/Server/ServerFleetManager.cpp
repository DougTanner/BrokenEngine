#include "Pch.h"

#if defined(BT_SERVER)

#include "Network/Server/ServerFleetManager.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Network/Server/FleetRows.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

// 32 ticks at engine::kiTickRate; exact in float, so the countdown expires on a tick boundary.
constexpr std::chrono::duration<float> kRespawnDelay = 1s;

ServerFleetManager::ServerFleetManager()
{
	mRandomEngine.TimeSeed();
}

FleetMemberLocation ServerFleetManager::FindFleetMember(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId)
{
	auto it = mFleets.find(rClientGuid);
	if (it == mFleets.end())
	{
		return {};
	}
	for (Fleet& rFleet : it->second)
	{
		auto memberIt = std::ranges::find(rFleet.members, globalId, &FleetMember::globalId);
		if (memberIt != rFleet.members.end())
		{
			return {.pFleet = &rFleet, .pMember = &*memberIt};
		}
	}
	return {};
}

// Every member is born dead with an expired countdown, so TickRespawnTimers queues its first spawn. The first member of an
// empty squadron resets the squadron's settings to its type's default, so an emptied squadron's settings do not carry over.
FleetMember& ServerFleetManager::AddFleetMember(const engine::ClientGuid& rClientGuid, Fleet& rFleet, ShipType eType, uint8_t uiSquadron)
{
	if (CountFleetSquadronMembers(rFleet, eType, uiSquadron) == 0)
	{
		rFleet.pSquadronOrders[std::to_underlying(eType)][uiSquadron] = kpDefaultSquadronOrders[std::to_underlying(eType)];
	}
	engine::GlobalId memberGlobalId {.iValue = gpGame->miNextGlobalId++};
	FleetMember& rMember = rFleet.members.emplace_back(FleetMember {.globalId = memberGlobalId, .eType = eType, .flags = FleetMemberFlags_t {FleetMemberFlags::kIsDead}, .uiSquadron = uiSquadron, .coordinate = engine::kOriginCoordinate});
	LOG(kNetwork, kDebug, "ServerFleetManager::AddFleetMember Guid: ({},{}) FleetGuid: ({},{}) Type: {} Squadron: {} Member: {}", rClientGuid.uiHigh, rClientGuid.uiLow, rFleet.guid.uiHigh, rFleet.guid.uiLow, ShipTypeName(eType), uiSquadron, memberGlobalId.iValue);
	return rMember;
}

// The fleet's Battleship is its first member and its flagship.
void ServerFleetManager::CreateFleet(const engine::ClientGuid& rClientGuid)
{
	Fleet& rNewFleet = mFleets.try_emplace(rClientGuid).first->second.emplace_back();
	rNewFleet.guid.uiHigh = common::RandomNext(mRandomEngine);
	rNewFleet.guid.uiLow = common::RandomNext(mRandomEngine);
	engine::GlobalId battleshipGlobalId = AddFleetMember(rClientGuid, rNewFleet, ShipType::kBattleship, 0).globalId;
	rNewFleet.flagshipGlobalId = battleshipGlobalId;
	rNewFleet.wantedCoordinate = engine::kOriginCoordinate;
	rNewFleet.cellChangeTimerSeconds = std::chrono::duration<float>(common::Random(rNewFleet.navigationDelaySeconds.count(), mRandomEngine));
	LOG(kNetwork, kDebug, "ServerFleetManager::CreateFleet Guid: ({},{}) FleetCount: {} FleetGuid: ({},{}) Battleship: {}", rClientGuid.uiHigh, rClientGuid.uiLow, std::ssize(mFleets.at(rClientGuid)), rNewFleet.guid.uiHigh, rNewFleet.guid.uiLow, battleshipGlobalId.iValue);
}

void ServerFleetManager::ProcessCreateFleetRequests()
{
	// Heap: mFleets map and fleet-list grow on create
	ScopedSuppressAllocationTracking suppress;

	for (const PendingCreateFleetRequest& rRequest : mPendingCreateFleetRequests)
	{
		const engine::ClientConnection* pClient = engine::gpServer->FindClient(rRequest.iClientId);
		engine::ClientGuid guid = pClient->clientGuid;
		auto it = mFleets.find(guid);
		if (it != mFleets.end() && std::ssize(it->second) >= kiMaximumFleetsPerClient)
		{
			LOG(kNetwork, kWarning, "ServerFleetManager::ProcessCreateFleetRequests Client: {} at fleet cap {}, ignoring create", rRequest.iClientId, kiMaximumFleetsPerClient);
			continue;
		}
		CreateFleet(guid);
	}

	// Drain on process: AfterNetworkPoll runs twice per update, and a retained request would create a
	// second fleet on the tick-boundary poll.
	mPendingCreateFleetRequests.clear();
}

void ServerFleetManager::ProcessSpawnIntoFleetRequests()
{
	// Heap: fleet member list grows on spawn-into
	ScopedSuppressAllocationTracking suppress;

	for (const PendingSpawnIntoFleetRequest& rRequest : mPendingSpawnIntoFleetRequests)
	{
		const engine::ClientConnection* pClient = engine::gpServer->FindClient(rRequest.iClientId);
		engine::ClientGuid guid = pClient->clientGuid;
		auto it = mFleets.find(guid);
		if (it == mFleets.end())
		{
			continue;
		}

		auto fleetIt = std::ranges::find(it->second, rRequest.fleetGuid, &Fleet::guid);
		if (fleetIt == it->second.end())
		{
			continue;
		}

		Fleet& rFleet = *fleetIt;
		uint8_t uiSquadron = FindOpenFleetSquadron(rFleet, rRequest.eType);
		if (uiSquadron == kuiNoOpenFleetSquadron)
		{
			LOG(kNetwork, kWarning, "ServerFleetManager::ProcessSpawnIntoFleetRequests Client: {} FleetGuid: ({},{}) Type: {} no open squadron, ignoring spawn", rRequest.iClientId, rRequest.fleetGuid.uiHigh, rRequest.fleetGuid.uiLow, ShipTypeName(rRequest.eType));
			continue;
		}

		AddFleetMember(guid, rFleet, rRequest.eType, uiSquadron);
	}

	mPendingSpawnIntoFleetRequests.clear();
}

void ServerFleetManager::ProcessDestroyFleetMemberRequests()
{
	// Heap: cell FrameInput statusChanges grow for accepted requests.
	ScopedSuppressAllocationTracking suppress;

	for (const PendingDestroyFleetMemberRequest& rRequest : mPendingDestroyFleetMemberRequests)
	{
		const engine::ClientConnection* pClient = engine::gpServer->FindClient(rRequest.iClientId);
		FleetMemberLocation location = FindFleetMember(pClient->clientGuid, rRequest.globalId);
		if (location.pMember == nullptr)
		{
			continue;
		}

		if (location.pMember->eType == ShipType::kBattleship)
		{
			continue;
		}
		QueueDestroyShip(*location.pMember);
		location.pFleet->members.erase(location.pFleet->members.begin() + (location.pMember - location.pFleet->members.data()));
	}
	mPendingDestroyFleetMemberRequests.clear();
}

// The named Fighter or Omni member's squadron slot takes the request's settings, and each live member row of the squadron
// gets them through its cell's frame input.
void ServerFleetManager::ProcessSquadronOrdersRequests()
{
	// Heap: cell FrameInput statusChanges grow for accepted requests.
	ScopedSuppressAllocationTracking suppress;

	for (const PendingSquadronOrdersRequest& rRequest : mPendingSquadronOrdersRequests)
	{
		const engine::ClientConnection* pClient = engine::gpServer->FindClient(rRequest.iClientId);
		FleetMemberLocation location = FindFleetMember(pClient->clientGuid, rRequest.globalId);
		if (location.pMember == nullptr)
		{
			continue;
		}
		ShipType eType = location.pMember->eType;
		uint8_t uiSquadron = location.pMember->uiSquadron;
		ShipOrders_t& rSlot = location.pFleet->pSquadronOrders[std::to_underlying(eType)][uiSquadron];
		// A setting of another type, or no change, is ignored; a Battleship's allowed bits are empty.
		if (rRequest.orders.Mask(kpSquadronOrderBits[std::to_underlying(eType)]) != std::to_underlying(rRequest.orders.meFlags))
		{
			continue;
		}

		if (rRequest.orders == rSlot)
		{
			continue;
		}
		rSlot = rRequest.orders;
		QueueUpdateSquadronOrders(*location.pFleet, eType, uiSquadron);
	}
	mPendingSquadronOrdersRequests.clear();
}

void ServerFleetManager::TickFleetTimers()
{
	// Heap: FrameInput statusChanges vectors of the cells holding the fleet's live members grow when a flagship picks a new direction
	ScopedSuppressAllocationTracking suppress;

	mNavigation.TickFleetTimers(mFleets, mRandomEngine);
}

// The queue order — owners in GUID order, fleets and members in list order — is the order this tick's spawns join its
// post-tick batch, which the simulation and the CRC depend on. Playback queues none, but its rerun must walk the same
// order: the spawn search is greedy over the points this pass took, so the order decides which members stay dead.
void ServerFleetManager::TickRespawnTimers()
{
	// Heap: transfer-manager spawn-queue push_back
	ScopedSuppressAllocationTracking suppress;

	common::ScopedWorkbufferArena ownersArena = common::gpThreadLocal->mWorkbuffer.Push();
	std::span<const engine::ClientGuid> owners = SortedGuidKeys(mFleets, ownersArena);
	// The spawns queued earlier in this pass are not yet rows of the origin's frame, so each fleet spawn search also
	// keeps clear of the points they took. Opened after the owner keys, so it is the top frame for every PushBack.
	common::ScopedWorkbufferArena takenSpawnPoints = common::gpThreadLocal->mWorkbuffer.Push();
	for (const engine::ClientGuid& rGuid : owners)
	{
		for (Fleet& rFleet : mFleets.at(rGuid))
		{
			int64_t iNextFighterLaunchBay = 0;
			bool bFighterLaunchReady = rFleet.iNextFighterLaunchTick <= gpGame->miTickCounter;
			bool bAnyFighterLaunched = false;
			for (FleetMember& rMember : rFleet.members)
			{
				if (!(rMember.flags & FleetMemberFlags::kIsDead))
				{
					continue;
				}

				rMember.respawnTimerSeconds -= std::chrono::duration<float>(engine::kfDeltaTime);
				if (rMember.respawnTimerSeconds.count() > 0.0f)
				{
					continue;
				}

				if (rMember.eType == ShipType::kFighter && !bFighterLaunchReady)
				{
					continue;
				}

				std::optional<engine::GridCoord> optionalSpawnCoordinate = gpServerSession->mpClientManager->SpawnFleetMember(rGuid, rFleet, rMember, iNextFighterLaunchBay, takenSpawnPoints);
				if (optionalSpawnCoordinate)
				{
					rMember.flags.Set(FleetMemberFlags::kIsDead, false);
					rMember.coordinate = *optionalSpawnCoordinate;
					if (rMember.eType == ShipType::kFighter)
					{
						bAnyFighterLaunched = true;
					}
				}
				LOG(kNetwork, kVerbose, "ServerFleetManager::TickRespawnTimers Guid: ({},{}) FleetGuid: ({},{}) Member: {}", rGuid.uiHigh, rGuid.uiLow, rFleet.guid.uiHigh, rFleet.guid.uiLow, rMember.globalId.iValue);
			}
			if (bAnyFighterLaunched)
			{
				rFleet.iNextFighterLaunchTick = gpGame->miTickCounter + static_cast<int64_t>(kfShipSpawnInterval * engine::kiTickRate);
			}
		}
	}
}

// A connected owner with no fleet still gets its empty fleet sync.
void ServerFleetManager::SendChangedFleetSyncs()
{
	for (auto& [rClientGuid, rOwner] : mGuidToClientId)
	{
		if (rOwner.iClientId == 0)
		{
			continue;
		}

		auto it = mFleets.find(rClientGuid);
		if (it != mFleets.end())
		{
			::game::SendFleetSync(rOwner.iClientId, it->second, rOwner.uiLastFleetSyncCrc);
		}
		else
		{
			::game::SendFleetSync(rOwner.iClientId, {}, rOwner.uiLastFleetSyncCrc);
		}
	}
}

void ServerFleetManager::ClearPendingRequests()
{
	mPendingCreateFleetRequests.clear();
	mPendingSpawnIntoFleetRequests.clear();
	mPendingDestroyFleetMemberRequests.clear();
	mPendingSquadronOrdersRequests.clear();
}

void ServerFleetManager::OnOwnedEntityTransferred(const engine::ClientGuid& rGuid, engine::GlobalId globalId, engine::GridCoord destination)
{
	auto fleetIt = mFleets.find(rGuid);
	if (fleetIt == mFleets.end())
	{
		return;
	}

	for (Fleet& rFleet : fleetIt->second)
	{
		for (FleetMember& rMember : rFleet.members)
		{
			if (rMember.globalId == globalId)
			{
				rMember.coordinate = destination;
				return;
			}
		}
	}
}

void ServerFleetManager::OnClientConnected(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
	// Heap: insert_or_assign connected-client mapping; the first fleet's creation
	ScopedSuppressAllocationTracking suppress;

	// A fresh entry clears the stored CRC, so the next SendChangedFleetSyncs sends this client its fleet sync.
	mGuidToClientId.insert_or_assign(rClientGuid, ConnectedOwner {.iClientId = iClientId});
	auto fleetIt = mFleets.find(rClientGuid);
	// Playback takes every fleet from the recording. Once playback ends, the load reset's ServerClientManager::ResetState
	// hands a client still without a fleet back to NewClients, which then makes its first fleet.
	if (!gpGame->mbReplaying && (fleetIt == mFleets.end() || fleetIt->second.empty()))
	{
		CreateFleet(rClientGuid);
	}
}

void ServerFleetManager::OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
	// Mark as disconnected — fleet data stays in mFleets
	auto it = mGuidToClientId.find(rClientGuid);
	if (it != mGuidToClientId.end())
	{
		it->second.iClientId = 0;
	}

	// The Process*FleetRequests loops rely on this purge: every request they see names a live client.
	auto IsFromClient = [iClientId](const auto& rRequest)
	{
		return rRequest.iClientId == iClientId;
	};
	std::erase_if(mPendingCreateFleetRequests, IsFromClient);
	std::erase_if(mPendingSpawnIntoFleetRequests, IsFromClient);
	std::erase_if(mPendingDestroyFleetMemberRequests, IsFromClient);
	std::erase_if(mPendingSquadronOrdersRequests, IsFromClient);
}

void ServerFleetManager::OnResetForLoad(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
	// Heap: insert_or_assign connected-client mapping
	ScopedSuppressAllocationTracking suppress;

	// A fresh entry clears the stored CRC: the client cleared its fleets at the load notification, so the next
	// SendChangedFleetSyncs must send.
	mGuidToClientId.insert_or_assign(rClientGuid, ConnectedOwner {.iClientId = iClientId});
}

// Connected and offline owners take the same rule, and the result does not depend on the mCells walk order. Dead
// members queue at the next tick's fleet step. The load may move the tick counter backward, so each fleet's next Fighter
// launch may come at once. After a playback adoption, the start replay fleet record replaces this result on the first
// dispatched tick.
void ServerFleetManager::RebuildFleetMembersForLoad()
{
	for ([[maybe_unused]] auto& [rGuid, rFleets] : mFleets)
	{
		for (Fleet& rFleet : rFleets)
		{
			rFleet.iNextFighterLaunchTick = 0;
			for (FleetMember& rMember : rFleet.members)
			{
				rMember.flags.Set(FleetMemberFlags::kIsDead);
				rMember.respawnTimerSeconds = std::chrono::duration<float>::zero();
			}
		}
	}

	for (const auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		for (const engine::RegistryOwnershipLayer& rLayer : Frame::OwnershipLayers(*rCell.pCurrent))
		{
			for (int64_t i = 0; i < rLayer.iCount; ++i)
			{
				auto fleetIt = mFleets.find(rLayer.pClientGuids[i]);
				if (fleetIt == mFleets.end())
				{
					continue;
				}

				for (Fleet& rFleet : fleetIt->second)
				{
					auto memberIt = std::ranges::find(rFleet.members, rLayer.pGlobalIds[i], &FleetMember::globalId);
					if (memberIt != rFleet.members.end())
					{
						memberIt->flags.Set(FleetMemberFlags::kIsDead, false);
						memberIt->coordinate = rCoordinate;
						break;
					}
				}
			}
		}
	}
}

// A live member's cell stays active while it holds the member's row, so a missing cell means the row is gone.
void ServerFleetManager::DetectFleetMemberDeaths()
{
	for ([[maybe_unused]] auto& [rGuid, rFleets] : mFleets)
	{
		for (Fleet& rFleet : rFleets)
		{
			for (FleetMember& rMember : rFleet.members)
			{
				if (rMember.flags & FleetMemberFlags::kIsDead)
				{
					continue;
				}

				if (gpGame->mCells.contains(rMember.coordinate) && FindShipByGlobalId(*gpGame->mCells.at(rMember.coordinate).pCurrent, rMember.globalId))
				{
					continue;
				}

				rMember.flags.Set(FleetMemberFlags::kIsDead);
				// A Fighter's countdown is zero, so it queues for relaunch from its Battleship at the next tick's fleet step.
				rMember.respawnTimerSeconds = rMember.eType == ShipType::kFighter ? std::chrono::duration<float>::zero() : kRespawnDelay;
			}
		}
	}
}

// Runs between ticks on the main thread, like CreateFleet's fleet RNG draws: the next tick's fleet checksum differs, so
// replay recording captures this change, the fleet RNG state included, and playback, which refuses the request, adopts it.
void ServerFleetManager::UpdateFleetNavigation(const engine::ClientGuid& rGuid, const FleetGuid& rFleetGuid, std::chrono::duration<float> navigationDelaySeconds, FleetBehaviour eBehaviour, engine::GridCoord holdCoordinate)
{
	auto fleetIt = mFleets.find(rGuid);
	if (fleetIt == mFleets.end())
	{
		return;
	}

	auto it = std::ranges::find(fleetIt->second, rFleetGuid, &Fleet::guid);
	if (it == fleetIt->second.end())
	{
		return;
	}

	Fleet& rFleet = *it;
	rFleet.navigationDelaySeconds = navigationDelaySeconds;

	// Entering Stay in Cell holds the flagship's own cell, so the request's hold is ignored there. Fleet navigation
	// never moves a flagship into the origin, so one at the origin holds a random neighbour of it instead.
	if (eBehaviour == FleetBehaviour::kStayInCell && rFleet.eBehaviour != FleetBehaviour::kStayInCell)
	{
		holdCoordinate = std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId)->coordinate;
		if (holdCoordinate == engine::kOriginCoordinate)
		{
			const common::HexDirection& rDirection = engine::kCellDirections.pEntries[common::Random(common::kiHexDirectionCount - 1, mRandomEngine)];
			holdCoordinate = engine::GridCoord {.iQ = rDirection.iDeltaQ, .iR = rDirection.iDeltaR};
		}
	}

	// A behaviour or hold change makes the fleet pick at once instead of after its delay.
	if (eBehaviour != rFleet.eBehaviour || !(holdCoordinate == rFleet.holdCoordinate))
	{
		rFleet.cellChangeTimerSeconds = std::chrono::duration<float>::zero();
	}
	rFleet.eBehaviour = eBehaviour;
	rFleet.holdCoordinate = holdCoordinate;
	LOG(kNetwork, kDebug, "ServerFleetManager::UpdateFleetNavigation Guid: ({},{}) FleetGuid: ({},{}) Delay: {} Behaviour: {} Hold: ({},{})", rGuid.uiHigh, rGuid.uiLow, rFleetGuid.uiHigh, rFleetGuid.uiLow, common::Wb(navigationDelaySeconds.count(), 3), kpcFleetBehaviourNames[std::to_underlying(eBehaviour)], holdCoordinate.iQ, holdCoordinate.iR);
}

void ServerFleetManager::ResetState()
{
	// mRandomEngine intentionally not re-seeded: constructor TimeSeeds once; ReadFleetData restores from save for replay determinism.
	mFleets.clear();
	mGuidToClientId.clear();
}

} // namespace game

#endif // BT_SERVER
