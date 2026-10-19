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
		Fleet& rNewFleet = mFleets.try_emplace(guid).first->second.emplace_back();
		rNewFleet.guid.uiHigh = common::RandomNext(mRandomEngine);
		rNewFleet.guid.uiLow = common::RandomNext(mRandomEngine);
		mGuidToClientId.insert_or_assign(guid, rRequest.iClientId);
		LOG(kNetwork, kDebug, "ServerFleetManager::ProcessCreateFleetRequests Client: {} FleetCount: {} FleetGuid: ({},{})", rRequest.iClientId, std::ssize(mFleets.at(guid)), rNewFleet.guid.uiHigh, rNewFleet.guid.uiLow);
		SendFleetSyncToClient(rRequest.iClientId, guid);
	}

	// Drain on process: AfterNetworkPoll runs twice per update, and a retained request would create a
	// second fleet on the tick-boundary poll.
	mPendingCreateFleetRequests.clear();
}

void ServerFleetManager::ProcessDeleteFleetRequests()
{
	for (const PendingDeleteFleetRequest& rRequest : mPendingDeleteFleetRequests)
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
		if (!rFleet.members.empty())
		{
			continue;
		}

		it->second.erase(it->second.begin() + iFleetIndex);
		LOG(kNetwork, kDebug, "ServerFleetManager::ProcessDeleteFleetRequests Client: {} FleetGuid: ({},{}) FleetCount: {}", rRequest.iClientId, rRequest.fleetGuid.uiHigh, rRequest.fleetGuid.uiLow, std::ssize(it->second));
		SendFleetSyncToClient(rRequest.iClientId, guid);
	}

	mPendingDeleteFleetRequests.clear();
}

void ServerFleetManager::ProcessSpawnIntoFleetRequests()
{
	// Heap: QueueSpawnForClient appends to spawn queue
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

		const Fleet& rFleet = it->second.at(static_cast<size_t>(iFleetIndex));
		if (std::ssize(rFleet.members) >= kiMaximumFleetMembers)
		{
			LOG(kNetwork, kWarning, "ServerFleetManager::ProcessSpawnIntoFleetRequests Client: {} FleetGuid: ({},{}) at member cap {}, ignoring spawn", rRequest.iClientId, rRequest.fleetGuid.uiHigh, rRequest.fleetGuid.uiLow, kiMaximumFleetMembers);
			continue;
		}

		gpServerSession->mpClientManager->QueueSpawnForClient(rRequest.iClientId, guid, rRequest.fleetGuid, {}, rRequest.eType);
		LOG(kNetwork, kDebug, "ServerFleetManager::ProcessSpawnIntoFleetRequests Client: {} FleetGuid: ({},{}) Type: {}", rRequest.iClientId, rRequest.fleetGuid.uiHigh, rRequest.fleetGuid.uiLow, ShipTypeName(rRequest.eType));
	}

	mPendingSpawnIntoFleetRequests.clear();
}

void ServerFleetManager::ProcessRespawnInFleetRequests()
{
	// Heap: QueueSpawnForClient appends to spawn queue on respawn
	ScopedSuppressAllocationTracking suppress;

	for (const PendingRespawnInFleetRequest& rRequest : mPendingRespawnInFleetRequests)
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

		const Fleet& rFleet = it->second.at(static_cast<size_t>(iFleetIndex));
		auto memberIt = std::ranges::find(rFleet.members, rRequest.memberGlobalId, &FleetMember::globalId);
		if (!(rRequest.memberGlobalId.iValue != 0) || memberIt == rFleet.members.end() || !(memberIt->flags & FleetMemberFlags::kIsDead))
		{
			continue;
		}

		gpServerSession->mpClientManager->QueueSpawnForClient(rRequest.iClientId, guid, rRequest.fleetGuid, rRequest.memberGlobalId, memberIt->eType);
		LOG(kNetwork, kDebug, "ServerFleetManager::ProcessRespawnInFleetRequests Client: {} FleetGuid: ({},{}) Member: {}", rRequest.iClientId, rRequest.fleetGuid.uiHigh, rRequest.fleetGuid.uiLow, rRequest.memberGlobalId.iValue);
	}

	mPendingRespawnInFleetRequests.clear();
}

void ServerFleetManager::TickFleetTimers()
{
	// Heap: pending flagship updates vector grows when flagship picks new direction; QueueSpawnForClient appends to spawn queue
	ScopedSuppressAllocationTracking suppress;

	mNavigation.TickFleetTimers(mFleets, mRandomEngine);
	TickRespawnTimers();
}

void ServerFleetManager::TickRespawnTimers()
{
	for (auto& [rGuid, rFleets] : mFleets)
	{
		// A disconnected owner's countdowns hold; reconnect re-arms them through RefreshFleetMembers.
		int64_t iClientId = FindClientIdForGuid(rGuid);
		if (iClientId == 0)
		{
			continue;
		}

		for (Fleet& rFleet : rFleets)
		{
			for (FleetMember& rMember : rFleet.members)
			{
				if (!(rMember.flags & FleetMemberFlags::kIsDead))
				{
					continue;
				}

				rMember.respawnTimerSeconds -= std::chrono::duration<float>(gpGame->mfLastDeltaTime);
				if (rMember.respawnTimerSeconds.count() > 0.0f)
				{
					continue;
				}

				gpServerSession->mpClientManager->QueueSpawnForClient(iClientId, rGuid, rFleet.guid, rMember.globalId, rMember.eType);
				LOG(kNetwork, kDebug, "ServerFleetManager::TickRespawnTimers Client: {} FleetGuid: ({},{}) Member: {}", iClientId, rFleet.guid.uiHigh, rFleet.guid.uiLow, rMember.globalId.iValue);
			}
		}
	}
}

void ServerFleetManager::ProcessFlagshipUpdates()
{
	// Heap: per-member statusChanges vector grows on flagship update broadcast
	ScopedSuppressAllocationTracking suppress;

	mNavigation.ProcessFlagshipUpdates(mFleets);
}

void ServerFleetManager::SendFleetSyncToClient(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
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
	mPendingDeleteFleetRequests.clear();
	mPendingSpawnIntoFleetRequests.clear();
	mPendingRespawnInFleetRequests.clear();
}

void ServerFleetManager::OnOwnedEntityDeath(const engine::ClientGuid& rGuid, engine::GlobalId globalId)
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
			if (rMember.globalId == globalId && !(rMember.flags & FleetMemberFlags::kIsDead))
			{
				rMember.flags.Set(FleetMemberFlags::kIsDead);
				rMember.respawnTimerSeconds = kRespawnDelay;

				if (globalId == rFleet.flagshipGlobalId)
				{
					mNavigation.ShiftFlagshipAfterDeath(rGuid, rFleet);
				}

				int64_t iClientId = FindClientIdForGuid(rGuid);
				if (iClientId != 0)
				{
					SendFleetSyncToClient(iClientId, rGuid);
				}
				return;
			}
		}
	}
}

void ServerFleetManager::OnOwnedEntitySpawned(int64_t iClientId, const engine::ClientGuid& rClientGuid, const ClientSpawnInformation& rSpawnInfo, engine::GlobalId globalId)
{
	if ((rSpawnInfo.fleetGuid.uiHigh == 0 && rSpawnInfo.fleetGuid.uiLow == 0))
	{
		return;
	}

	// Heap: try_emplace fleet entry, fleet members vector grows, pending flagship update
	ScopedSuppressAllocationTracking suppress;

	std::vector<Fleet>& rFleets = mFleets.try_emplace(rClientGuid).first->second;
	int64_t iFleetIndex = FindFleetIndexByGuid(rFleets, rSpawnInfo.fleetGuid);
	if (iFleetIndex < 0)
	{
		return;
	}

	Fleet& rFleet = rFleets.at(static_cast<size_t>(iFleetIndex));
	if ((rSpawnInfo.memberGlobalId.iValue != 0))
	{
		// Respawn: revive the dead member in place; it keeps its global ID and list position
		auto memberIt = std::ranges::find(rFleet.members, rSpawnInfo.memberGlobalId, &FleetMember::globalId);
		if (memberIt == rFleet.members.end())
		{
			return;
		}
		memberIt->flags.Set(FleetMemberFlags::kIsDead, false);
		memberIt->coordinate = engine::kOriginCoordinate;
	}
	else
	{
		// The member cap bounds client-driven growth; rejection must precede flagship mutation to keep the flagship in the roster.
		if (std::ssize(rFleet.members) >= kiMaximumFleetMembers)
		{
			LOG(kNetwork, kWarning, "ServerFleetManager::OnOwnedEntitySpawned Client: {} FleetGuid: ({},{}) at member cap {}, ignoring spawn", iClientId, rFleet.guid.uiHigh, rFleet.guid.uiLow, kiMaximumFleetMembers);
			return;
		}
		rFleet.members.push_back(FleetMember {.globalId = globalId, .eType = rSpawnInfo.eType, .coordinate = engine::kOriginCoordinate});
	}

	auto flagshipIt = std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId);
	bool bHasAliveFlagship = (rFleet.flagshipGlobalId.iValue != 0) && flagshipIt != rFleet.members.end()
	                      && !(flagshipIt->flags & FleetMemberFlags::kIsDead) && rFleet.flagshipGlobalId != globalId;
	if (!bHasAliveFlagship)
	{
		rFleet.flagshipGlobalId = globalId;
		rFleet.wantedCoordinate = engine::kOriginCoordinate;
		rFleet.cellChangeTimerSeconds = std::chrono::duration<float>(common::Random(rFleet.navigationDelaySeconds.count(), mRandomEngine));
		mNavigation.mPendingFlagshipUpdates.push_back({.clientGuid = rClientGuid, .fleetGuid = rFleet.guid, .newWantedCoordinate = rFleet.wantedCoordinate});
	}

	// After the flagship assignment: clients trust the sync, so it must never carry a nonempty fleet without a flagship.
	SendFleetSyncToClient(iClientId, rClientGuid);
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
	// Heap: insert_or_assign connected-client mapping
	ScopedSuppressAllocationTracking suppress;

	mGuidToClientId.insert_or_assign(rClientGuid, iClientId);
	auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(iClientId);
	std::span<const engine::OwnedEntity> ownedEntities = ownedIt != gpServerSession->mClientOwnedEntities.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();

	auto fleetIt = mFleets.find(rClientGuid);
	if (fleetIt != mFleets.end())
	{
		for (Fleet& rFleet : fleetIt->second)
		{
			RefreshFleetMembers(rFleet, ownedEntities);
			auto flagshipIt = std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId);
			if ((rFleet.flagshipGlobalId.iValue != 0) && flagshipIt != rFleet.members.end() && (flagshipIt->flags & FleetMemberFlags::kIsDead))
			{
				mNavigation.ShiftFlagshipAfterDeath(rClientGuid, rFleet);
			}
		}
	}
	SendFleetSyncToClient(iClientId, rClientGuid);
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
	std::erase_if(mPendingDeleteFleetRequests, IsFromClient);
	std::erase_if(mPendingSpawnIntoFleetRequests, IsFromClient);
	std::erase_if(mPendingRespawnInFleetRequests, IsFromClient);
}

void ServerFleetManager::OnResetForLoad(int64_t iClientId, const engine::ClientGuid& rClientGuid)
{
	// Heap: pending flagship updates after load
	ScopedSuppressAllocationTracking suppress;

	mGuidToClientId.insert_or_assign(rClientGuid, iClientId);

	auto fleetIt = mFleets.find(rClientGuid);
	if (fleetIt != mFleets.end())
	{
		for (Fleet& rFleet : fleetIt->second)
		{
			ResetFleetForLoad(rFleet, rClientGuid);
		}
	}
	SendFleetSyncToClient(iClientId, rClientGuid);
}

void ServerFleetManager::RefreshFleetMembers(Fleet& rFleet, std::span<const engine::OwnedEntity> ownedEntities)
{
	for (FleetMember& rMember : rFleet.members)
	{
		rMember.flags.Set(FleetMemberFlags::kIsDead);
		rMember.respawnTimerSeconds = kRespawnDelay;
		for (const engine::OwnedEntity& rOwnedEntity : ownedEntities)
		{
			if (rOwnedEntity.globalId == rMember.globalId)
			{
				rMember.flags.Set(FleetMemberFlags::kIsDead, false);
				rMember.coordinate = rOwnedEntity.coord;
				break;
			}
		}
	}
}

void ServerFleetManager::ResetFleetForLoad(Fleet& rFleet, const engine::ClientGuid& rClientGuid)
{
	auto ownedIt = gpServerSession->mClientOwnedEntities.mOwned.find(FindClientIdForGuid(rClientGuid));
	std::span<const engine::OwnedEntity> ownedEntities = ownedIt != gpServerSession->mClientOwnedEntities.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();
	RefreshFleetMembers(rFleet, ownedEntities);

	auto flagshipIt = std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId);
	if (!(rFleet.flagshipGlobalId.iValue != 0) || flagshipIt == rFleet.members.end())
	{
		return;
	}

	if (flagshipIt->flags & FleetMemberFlags::kIsDead)
	{
		mNavigation.ShiftFlagshipAfterDeath(rClientGuid, rFleet);
	}
	else
	{
		rFleet.wantedCoordinate = flagshipIt->coordinate;
		rFleet.cellChangeTimerSeconds = rFleet.navigationDelaySeconds;
		mNavigation.mPendingFlagshipUpdates.push_back({.clientGuid = rClientGuid, .fleetGuid = rFleet.guid, .newWantedCoordinate = rFleet.wantedCoordinate});
	}
}

ServerFleetManager::FleetLookupResult ServerFleetManager::LookupFleetWantedCoord(const engine::ClientGuid& rClientGuid, const FleetGuid& rFleetGuid, engine::GlobalId memberGlobalId)
{
	auto fleetIt = mFleets.find(rClientGuid);
	if (fleetIt == mFleets.end())
	{
		return {};
	}

	int64_t iFleetIndex = FindFleetIndexByGuid(fleetIt->second, rFleetGuid);
	if (iFleetIndex < 0)
	{
		return {};
	}

	const Fleet& rFleet = fleetIt->second.at(static_cast<size_t>(iFleetIndex));
	bool bIsFlagship = rFleet.members.empty();
	if ((memberGlobalId.iValue != 0))
	{
		// A respawn is still spawnable only while its member is dead; the broadcaster drops it otherwise, so a live
		// global ID is never minted twice.
		auto memberIt = std::ranges::find(rFleet.members, memberGlobalId, &FleetMember::globalId);
		if (memberIt == rFleet.members.end() || !(memberIt->flags & FleetMemberFlags::kIsDead))
		{
			return {};
		}
		bIsFlagship = memberGlobalId == rFleet.flagshipGlobalId;
	}

	FleetLookupFlags_t flags {FleetLookupFlags::kFound};
	flags.Set(FleetLookupFlags::kIsFlagship, bIsFlagship);
	return {.flags = flags, .flagshipGlobalId = rFleet.flagshipGlobalId, .fleetWantedCoord = rFleet.wantedCoordinate, .iPendingFleetWantedCoordTicks = rFleet.uiPendingFleetWantedCoordinateTicks};
}

void ServerFleetManager::DetectDisconnectedOwnedEntityDeaths()
{
	// Heap: ShiftFlagshipAfterDeath may push pending-flagship-update entries
	ScopedSuppressAllocationTracking suppress;

	for (auto& [rGuid, rFleets] : mFleets)
	{
		// Skip connected clients (handled by ServerSessionBase::RebuildOwnedEntities)
		int64_t iClientId = FindClientIdForGuid(rGuid);
		if (iClientId != 0)
		{
			continue;
		}

		for (Fleet& rFleet : rFleets)
		{
			for (FleetMember& rMember : rFleet.members)
			{
				if (rMember.flags & FleetMemberFlags::kIsDead)
				{
					continue;
				}

				if (!gpGame->mCells.contains(rMember.coordinate))
				{
					continue;
				}

				if (!FindShipByGlobalId(*gpGame->mCells.at(rMember.coordinate).pCurrent, rMember.globalId))
				{
					rMember.flags.Set(FleetMemberFlags::kIsDead);
					if (rMember.globalId == rFleet.flagshipGlobalId)
					{
						mNavigation.ShiftFlagshipAfterDeath(rGuid, rFleet);
					}
				}
			}
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
	int64_t iClientId = FindClientIdForGuid(rGuid);
	if (iClientId != 0)
	{
		SendFleetSyncToClient(iClientId, rGuid);
	}
}

void ServerFleetManager::ResetState()
{
	// mRandomEngine intentionally not re-seeded: constructor TimeSeeds once; ReadFleetData restores from save for replay determinism.
	mPendingCreateFleetRequests.clear();
	mPendingDeleteFleetRequests.clear();
	mPendingSpawnIntoFleetRequests.clear();
	mPendingRespawnInFleetRequests.clear();
	mFleets.clear();
	mGuidToClientId.clear();
}

#endif // BT_SERVER

} // namespace game
