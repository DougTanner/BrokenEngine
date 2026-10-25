#include "Pch.h"

#include "Network/Server/ServerFleetManager.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

// 32 ticks at engine::kiTickRate; exact in float, so the countdown expires on a tick boundary.
constexpr std::chrono::duration<float> kRespawnDelay = 1s;

ServerFleetManager::ServerFleetManager()
{
	mRandomEngine.TimeSeed();
}

int64_t ServerFleetManager::FindClientIdForGuid(const engine::ClientGuid& rGuid) const
{
	auto it = mGuidToClientId.find(rGuid);
	if (it != mGuidToClientId.end())
	{
		return it->second;
	}
	return 0;
}

int64_t ServerFleetManager::FindFleetIndexByGuid(const std::vector<Fleet>& rFleets, const FleetGuid& rFleetGuid) const
{
	for (int64_t i = 0; i < std::ssize(rFleets); ++i)
	{
		if (rFleets.at(static_cast<size_t>(i)).guid == rFleetGuid)
		{
			return i;
		}
	}
	return -1;
}

// The fleet's Battleship is its first member and its flagship; it and every later member are born dead with an expired
// countdown, so TickRespawnTimers queues each first spawn.
void ServerFleetManager::CreateFleet(const engine::ClientGuid& rClientGuid)
{
	Fleet& rNewFleet = mFleets.try_emplace(rClientGuid).first->second.emplace_back();
	rNewFleet.guid.uiHigh = common::RandomNext(mRandomEngine);
	rNewFleet.guid.uiLow = common::RandomNext(mRandomEngine);
	engine::GlobalId battleshipGlobalId {.iValue = gpGame->miNextGlobalId++};
	rNewFleet.members.push_back(FleetMember {.globalId = battleshipGlobalId, .eType = ShipType::kBattleship, .flags = FleetMemberFlags_t {FleetMemberFlags::kIsDead}, .coordinate = engine::kOriginCoordinate});
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
		mGuidToClientId.insert_or_assign(guid, rRequest.iClientId);
		SendFleetSyncToClient(guid);
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

		int64_t iFleetIndex = FindFleetIndexByGuid(it->second, rRequest.fleetGuid);
		if (iFleetIndex < 0)
		{
			continue;
		}

		Fleet& rFleet = it->second.at(static_cast<size_t>(iFleetIndex));
		if (std::ssize(rFleet.members) >= kiMaximumFleetMembers)
		{
			LOG(kNetwork, kWarning, "ServerFleetManager::ProcessSpawnIntoFleetRequests Client: {} FleetGuid: ({},{}) at member cap {}, ignoring spawn", rRequest.iClientId, rRequest.fleetGuid.uiHigh, rRequest.fleetGuid.uiLow, kiMaximumFleetMembers);
			continue;
		}

		engine::GlobalId memberGlobalId {.iValue = gpGame->miNextGlobalId++};
		rFleet.members.push_back(FleetMember {.globalId = memberGlobalId, .eType = rRequest.eType, .flags = FleetMemberFlags_t {FleetMemberFlags::kIsDead}, .coordinate = engine::kOriginCoordinate});
		LOG(kNetwork, kDebug, "ServerFleetManager::ProcessSpawnIntoFleetRequests Client: {} FleetGuid: ({},{}) Type: {} Member: {}", rRequest.iClientId, rRequest.fleetGuid.uiHigh, rRequest.fleetGuid.uiLow, ShipTypeName(rRequest.eType), memberGlobalId.iValue);
		SendFleetSyncToClient(guid);
	}

	mPendingSpawnIntoFleetRequests.clear();
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
	// Heap: transfer-manager spawn-queue push_back, launch-tick map insert
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

				gpServerSession->mpClientManager->SpawnFleetMember(rGuid, rFleet, rMember, takenSpawnPoints);
				LOG(kNetwork, kVerbose, "ServerFleetManager::TickRespawnTimers Guid: ({},{}) FleetGuid: ({},{}) Member: {}", rGuid.uiHigh, rGuid.uiLow, rFleet.guid.uiHigh, rFleet.guid.uiLow, rMember.globalId.iValue);
			}
		}
	}
}

void ServerFleetManager::SendFleetSyncToClient(const engine::ClientGuid& rClientGuid)
{
	int64_t iClientId = FindClientIdForGuid(rClientGuid);
	if (iClientId == 0)
	{
		return;
	}

	auto it = mFleets.find(rClientGuid);
	if (it != mFleets.end())
	{
		::game::SendFleetSync(iClientId, it->second);
	}
	else
	{
		::game::SendFleetSync(iClientId, {});
	}
}

void ServerFleetManager::ClearPendingRequests()
{
	mPendingCreateFleetRequests.clear();
	mPendingSpawnIntoFleetRequests.clear();
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

	mGuidToClientId.insert_or_assign(rClientGuid, iClientId);
	auto fleetIt = mFleets.find(rClientGuid);
	// Playback takes every fleet from the recording. Once playback ends, the load reset's ServerClientManager::ResetState
	// hands a client still without a fleet back to NewClients, which then makes its first fleet.
	if (!gpGame->mbReplaying && (fleetIt == mFleets.end() || fleetIt->second.empty()))
	{
		CreateFleet(rClientGuid);
	}
	SendFleetSyncToClient(rClientGuid);
}

void ServerFleetManager::OnClientDisconnected(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
	// Mark as disconnected — fleet data stays in mFleets
	auto it = mGuidToClientId.find(rClientGuid);
	if (it != mGuidToClientId.end())
	{
		it->second = 0;
	}

	// The Process*FleetRequests loops rely on this purge: every request they see names a live client.
	auto IsFromClient = [iClientId](const auto& rRequest)
	{
		return rRequest.iClientId == iClientId;
	};
	std::erase_if(mPendingCreateFleetRequests, IsFromClient);
	std::erase_if(mPendingSpawnIntoFleetRequests, IsFromClient);
}

void ServerFleetManager::OnResetForLoad(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
	// Heap: insert_or_assign connected-client mapping
	ScopedSuppressAllocationTracking suppress;

	mGuidToClientId.insert_or_assign(rClientGuid, iClientId);
	SendFleetSyncToClient(rClientGuid);
}

// Connected and offline owners take the same rule, and the result does not depend on the mCells walk order. Dead
// members queue at the next tick's fleet step. After a playback adoption, the start replay fleet record replaces this
// result on the first dispatched tick.
void ServerFleetManager::RebuildFleetMembersForLoad()
{
	for ([[maybe_unused]] auto& [rGuid, rFleets] : mFleets)
	{
		for (Fleet& rFleet : rFleets)
		{
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
	for (auto& [rGuid, rFleets] : mFleets)
	{
		bool bAnyDied = false;
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
				bAnyDied = true;
			}
		}

		if (bAnyDied)
		{
			SendFleetSyncToClient(rGuid);
		}
	}
}

void ServerFleetManager::UpdateFleetNavigationDelay(const engine::ClientGuid& rGuid, const FleetGuid& rFleetGuid, std::chrono::duration<float> navigationDelaySeconds)
{
	auto fleetIt = mFleets.find(rGuid);
	if (fleetIt == mFleets.end())
	{
		return;
	}

	int64_t iFleetIndex = FindFleetIndexByGuid(fleetIt->second, rFleetGuid);
	if (iFleetIndex < 0)
	{
		return;
	}

	fleetIt->second.at(static_cast<size_t>(iFleetIndex)).navigationDelaySeconds = navigationDelaySeconds;
	LOG(kNetwork, kDebug, "ServerFleetManager::UpdateFleetNavigationDelay Guid: ({},{}) FleetGuid: ({},{}) Delay: {}", rGuid.uiHigh, rGuid.uiLow, rFleetGuid.uiHigh, rFleetGuid.uiLow, common::Wb(navigationDelaySeconds.count(), 3));

	// Resync fleet to client so UI updates
	SendFleetSyncToClient(rGuid);
}

void ServerFleetManager::ResetState()
{
	// mRandomEngine intentionally not re-seeded: constructor TimeSeeds once; ReadFleetData restores from save for replay determinism.
	mPendingCreateFleetRequests.clear();
	mPendingSpawnIntoFleetRequests.clear();
	mFleets.clear();
	mGuidToClientId.clear();
}

#endif // BT_SERVER

} // namespace game
