#include "Pch.h"

#include "Network/Server/ServerBroadcaster.h"

#if defined(BT_SERVER)

#include "Network/Server/ServerTransferManager.h"

#include "Agent/Commands/ServerSimulationFixtures.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace engine
{

void ServerBroadcaster::BuildFrameInputs()
{
	// Heap: unordered_map clear/insert, vector resize for statusChanges
	ScopedSuppressAllocationTracking suppress;

	game::gpGame->mFrameInputs.clear();
	mBroadcastStatusChanges.clear();

	for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoordinates)
	{
		game::gpGame->mFrameInputs.try_emplace(rCoord);
	}

	bool bAdvancing = game::gpGame->mfLastDeltaTime > 0.0f;
	if (bAdvancing)
	{
		std::erase_if(game::gpServerSession->mpClientManager->mClientsWaitingForSpawn, [&](const game::ClientSpawnInformation& rClientSpawnInformation)
		{
			if ((rClientSpawnInformation.fleetGuid.uiHigh == 0 && rClientSpawnInformation.fleetGuid.uiLow == 0))
			{
				return false;
			}

			game::ServerFleetManager::FleetLookupResult result = game::gpServerSession->mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.memberGlobalPlayerId);
			if (result.flags & game::ServerFleetManager::FleetLookupFlags::kFound)
			{
				return false;
			}

			LOG(kNetwork, kWarning, "ServerBroadcaster::BuildFrameInputs Dropping queued spawn Client: {} FleetGuid: ({},{})", rClientSpawnInformation.iClientId, rClientSpawnInformation.fleetGuid.uiHigh, rClientSpawnInformation.fleetGuid.uiLow);
			return true;
		});

		// Inject weapon mode toggle StatusChanges
		ProcessUpdatePlayerRequests();

		// Tick fleet timers and inject fleet coord updates only when this update advances. Subtracting
		// a zero delta does not make TickFleetTimers inert: an already-expired timer can still fire,
		// consume random state, and queue an update that no frame tick could consume. Deferring the
		// timer and all queued work here applies them in order on the first advancing update.
		game::gpServerSession->mpFleetManager->TickFleetTimers();
		game::gpServerSession->mpFleetManager->ProcessFlagshipUpdates();

		// After ProcessFlagshipUpdates so the flagship update this queues is consumed on the following update and its
		// fleet-RNG draw keeps its position, and before the first tick's PrepareTickStatusChanges drain so a drain that
		// releases agent entries for the origin coordinate sorts this spawn in with them.
		game::gpServerSession->mpClientManager->SpawnWaitingClients();
	}
}

void ServerBroadcaster::PrepareTickStatusChanges()
{
	// Heap: unordered_map insert, vector growth for statusChanges
	ScopedSuppressAllocationTracking suppress;

	// Runs once per normal-play tick, so an agent-injected StatusChange can enter any tick of a multi-tick update and
	// rides the same broadcast / CRC / replay channel as real spawns. An entry stays queued until its scheduled tick
	// and until its coord is one the tick will simulate (active, with a committed pCurrent frame).
	game::DrainPendingAgentStatusChanges(*game::gpServerSession);

	// Save this tick's StatusChanges for broadcasting (transfers handled separately in HarvestTransfers)
	for (const auto& [rCoord, rFrameInput] : game::gpGame->mFrameInputs)
	{
		if (!rFrameInput.statusChanges.empty())
		{
			mBroadcastStatusChanges.insert_or_assign(rCoord, rFrameInput.statusChanges);
		}
	}
}

void ServerBroadcaster::BuildTickPublication(int64_t iTick, engine::ServerSessionRuntime& rRuntime, [[maybe_unused]] const common::ScopedWorkbufferArena& rPublicationArena)
{
	const std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>>& rTransfers = game::gpServerSession->mpTransferManager->mTransfers;
	const std::vector<engine::GridCoord>& rActiveCoordinates = game::gpGame->mActiveCoordinates;

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	bool bReplaying = game::gpGame->mbReplaying;
	auto ForEachPublicationCoord = [&](const auto& rCallback)
	{
		if (!bReplaying)
		{
			for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoordinates)
			{
				rCallback(rCoord);
			}
			return;
		}

		for (auto it = rActiveCoordinates.begin(); it != rActiveCoordinates.end(); ++it)
		{
			if (std::find(rActiveCoordinates.begin(), it, *it) == it)
			{
				rCallback(*it);
			}
		}
		for ([[maybe_unused]] const auto& [rCoord, rStatusChanges] : mBroadcastStatusChanges)
		{
			if (!std::ranges::contains(game::gpGame->mActiveCoordinates, rCoord))
			{
				rCallback(rCoord);
			}
		}
		for ([[maybe_unused]] const auto& [rCoord, rStatusChanges] : rTransfers)
		{
			if (!std::ranges::contains(game::gpGame->mActiveCoordinates, rCoord) && !mBroadcastStatusChanges.contains(rCoord))
			{
				rCallback(rCoord);
			}
		}
	};

	int64_t iPublicationCoordCount = 0;
	int64_t iStatusChangeCount = 0;
	ForEachPublicationCoord([&](const engine::GridCoord& rCoord)
	{
		++iPublicationCoordCount;
		if (auto it = mBroadcastStatusChanges.find(rCoord); it != mBroadcastStatusChanges.end())
		{
			iStatusChangeCount += std::ssize(it->second);
		}
		if (auto it = rTransfers.find(rCoord); it != rTransfers.end())
		{
			iStatusChangeCount += std::ssize(it->second);
		}
	});

	int64_t iPublicationCoordsBytes = iPublicationCoordCount * static_cast<int64_t>(sizeof(engine::GridCoord));
	int64_t iGridUpdatesOffset = common::RoundUp(iPublicationCoordsBytes, 16i64);
	int64_t iGridUpdatesBytes = iPublicationCoordCount * static_cast<int64_t>(sizeof(std::pair<engine::GridCoord, engine::GridUpdateData>));
	int64_t iStatusChangesOffset = common::RoundUp(iGridUpdatesOffset + iGridUpdatesBytes, 16i64);
	int64_t iStatusChangesBytes = iStatusChangeCount * static_cast<int64_t>(sizeof(game::StatusChange));
	int64_t iFullFramesOffset = common::RoundUp(iStatusChangesOffset + iStatusChangesBytes, 16i64);
	int64_t iFullFramesBytes = kbDesynchronizationDebugFrames ? iPublicationCoordCount * static_cast<int64_t>(sizeof(std::pair<engine::GridCoord, const game::Frame*>)) : 0;
	int64_t iPublicationBytes = kbDesynchronizationDebugFrames ? iFullFramesOffset + iFullFramesBytes : iStatusChangesOffset + iStatusChangesBytes;
	int64_t iPublicationHighWaterBytes = common::RoundUp(iPublicationBytes, 16i64) + engine::kiMaxCompressStatusChangeWorkbufferBytes;
	// Grow before any publication pointer exists, then retain that capacity for nested status compression.
	{
		[[maybe_unused]] auto highWaterAllocation = rWorkbuffer.PushBuffer<std::byte*>(iPublicationHighWaterBytes);
	}

	// One allocation keeps every publication view stable until PublishTick finishes consuming it.
	auto publicationAllocation = rWorkbuffer.PushBuffer<std::byte*>(iPublicationBytes);
	std::byte* pPublicationBytes = publicationAllocation.mpData;
	engine::GridCoord* pPublicationCoords = reinterpret_cast<engine::GridCoord*>(pPublicationBytes);
	std::pair<engine::GridCoord, engine::GridUpdateData>* pGridUpdates = reinterpret_cast<std::pair<engine::GridCoord, engine::GridUpdateData>*>(pPublicationBytes + iGridUpdatesOffset);
	game::StatusChange* pStatusChanges = reinterpret_cast<game::StatusChange*>(pPublicationBytes + iStatusChangesOffset);

	int64_t iPublicationCoordIndex = 0;
	ForEachPublicationCoord([&](const engine::GridCoord& rCoord)
	{
		pPublicationCoords[iPublicationCoordIndex++] = rCoord;
	});
	std::span<const engine::GridCoord> publicationCoords(pPublicationCoords, static_cast<size_t>(iPublicationCoordCount));

	int64_t iStatusChangeIndex = 0;
	int64_t iGridUpdateIndex = 0;
	for (const engine::GridCoord& rCoord : publicationCoords)
	{
		engine::GridUpdateData updateData {};
		updateData.uiSharedCrc = (*game::gpGame->mCoordinateFrames.at(rCoord).pCurrent).postRender.uiSharedCrc;
		int64_t iRunStart = iStatusChangeIndex;
		if (auto it = mBroadcastStatusChanges.find(rCoord); it != mBroadcastStatusChanges.end())
		{
			for (const game::StatusChange& rChange : it->second)
			{
				std::memcpy(pStatusChanges + iStatusChangeIndex++, &rChange, sizeof(game::StatusChange));
			}
		}
		if (auto it = rTransfers.find(rCoord); it != rTransfers.end())
		{
			for (const game::StatusChange& rChange : it->second)
			{
				std::memcpy(pStatusChanges + iStatusChangeIndex++, &rChange, sizeof(game::StatusChange));
			}
		}
		int64_t iRunCount = iStatusChangeIndex - iRunStart;
		if (iRunCount > 0)
		{
			updateData.statusChanges = {pStatusChanges + iRunStart, static_cast<size_t>(iRunCount)};
			LOG(kNetwork, kVerbose, "ServerBroadcaster::BuildTickPublication Coord: ({},{}) Tick: {} StatusChanges: {}", rCoord.iX, rCoord.iY, iTick, iRunCount);
		}
		pGridUpdates[iGridUpdateIndex++] = {rCoord, updateData};
	}

	if constexpr (kbDesynchronizationDebugFrames)
	{
		std::pair<engine::GridCoord, const game::Frame*>* pFullFrames = reinterpret_cast<std::pair<engine::GridCoord, const game::Frame*>*>(pPublicationBytes + iFullFramesOffset);
		int64_t iFullFrameCount = 0;
		for (const engine::GridCoord& rCoord : publicationCoords)
		{
			pFullFrames[iFullFrameCount++] = {rCoord, &(*game::gpGame->mCoordinateFrames.at(rCoord).pCurrent)};
		}
		rRuntime.PublishTick(iTick, {pGridUpdates, static_cast<size_t>(iGridUpdateIndex)}, {pFullFrames, static_cast<size_t>(iFullFrameCount)});
	}
	else
	{
		rRuntime.PublishTick(iTick, {pGridUpdates, static_cast<size_t>(iGridUpdateIndex)}, {});
	}

	// Keep replay/live transfer state alive through PublishTick so the publication can consume it, then retire
	// this tick's transfer batch before the next replay peek or live harvest builds a new one.
	game::gpServerSession->mpTransferManager->mTransfers.clear();
}

void ServerBroadcaster::ProcessUpdatePlayerRequests()
{
	// Heap: pending player-update requests may grow frame status changes
	ScopedSuppressAllocationTracking suppress;

	for (const PendingUpdatePlayerRequest& rRequest : mPendingUpdatePlayerRequests)
	{
		engine::ClientConnection* pClient = engine::gpServer->FindClient(rRequest.iClientId);
		if (pClient == nullptr)
		{
			continue;
		}
		auto ownedIt = game::gpServerSession->mClientPlayers.mOwned.find(pClient->iClientId);
		std::span<const engine::OwnedEntity> ownedPlayers = ownedIt != game::gpServerSession->mClientPlayers.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();
		if (ownedPlayers.empty())
		{
			continue;
		}

		engine::GridCoord updateCoord {};
		bool bFound = false;
		for (const engine::OwnedEntity& rOwnedPlayer : ownedPlayers)
		{
			if (rOwnedPlayer.globalId == rRequest.globalId)
			{
				updateCoord = rOwnedPlayer.coord;
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			continue;
		}

		auto it = game::gpGame->mFrameInputs.find(updateCoord);
		if (it == game::gpGame->mFrameInputs.end())
		{
			continue;
		}

		if (!game::gpGame->mCoordinateFrames.contains(updateCoord))
		{
			continue;
		}
		int64_t iPlayerUuid = engine::RegistryUuidByGlobalId(game::Frame::OwnershipLayer((*game::gpGame->mCoordinateFrames.at(updateCoord).pCurrent)), rRequest.globalId).iValue;
		if (iPlayerUuid == 0)
		{
			continue;
		}

		int64_t iPendingWeaponModeTicks = engine::kiTickRate;
		game::StatusChange updateChange {.eType = game::StatusChangeType::kUpdatePlayer, .data = game::UpdatePlayerData{.iPlayerUuid = iPlayerUuid, .bUseMissiles = rRequest.bUseMissiles, .navigationDelaySeconds = rRequest.navigationDelaySeconds, .uiPendingWeaponModeTicks = static_cast<uint8_t>(iPendingWeaponModeTicks)}};
		it->second.statusChanges.push_back(updateChange);

		LOG(kNetwork, kDebug, "ServerBroadcaster::ProcessUpdatePlayerRequests Client: {} GlobalPlayer: {} PlayerUuid: {} Coord: ({},{}) Missiles: {} NavDelay: {}", rRequest.iClientId, rRequest.globalId, iPlayerUuid, updateCoord.iX, updateCoord.iY, rRequest.bUseMissiles, common::Wb(rRequest.navigationDelaySeconds.count(), 3));
	}
}

void ServerBroadcaster::ResetState()
{
	mBroadcastStatusChanges.clear();
	mPendingUpdatePlayerRequests.clear();
	game::ResetPendingAgentStatusChanges(*game::gpServerSession);
}

} // namespace engine

#endif // BT_SERVER
