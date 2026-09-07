#include "Pch.h"

#include "Network/Server/ServerBroadcaster.h"

#if defined(BT_SERVER)

#include "Game.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Network/Server/ServerTransferManager.h"

namespace engine
{

void ServerBroadcaster::BuildFrameInputs()
{
	// Heap: unordered_map clear/insert, vector resize for statusChanges
	ScopedSuppressAllocationTracking suppress;

	game::gpGame->mFrameInputs.clear();
	mBroadcastStatusChanges.clear();

	// Initialize FrameInputs for all active coordinates
	for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoords)
	{
		game::gpGame->mFrameInputs.try_emplace(rCoord);
	}

	const bool bAdvancing = game::gpGame->mfLastDeltaTime > 0.0f;
	if (bAdvancing)
	{
		std::erase_if(game::gpServerSession->mpClientManager->mClientsWaitingForSpawn, [&](const game::ClientSpawnInfo& rClientSpawnInformation)
		{
			if (rClientSpawnInformation.fleetGuid.IsEmpty())
			{
				return false;
			}

			game::ServerFleetManager::FleetLookupResult result = game::gpServerSession->mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.iMemberIndex);
			if (result.flags & game::ServerFleetManager::FleetLookupFlags::kFound)
			{
				return false;
			}

			LOG(kNetwork, kWarning, "ServerBroadcaster::BuildFrameInputs Dropping queued spawn Client: {} FleetGuid: ({},{})", rClientSpawnInformation.iClientId, rClientSpawnInformation.fleetGuid.uiHigh, rClientSpawnInformation.fleetGuid.uiLow);
			return true;
		});

		// Add spawn StatusChanges for clients waiting for initial spawn
		for (const game::ClientSpawnInfo& rClientSpawnInformation : game::gpServerSession->mpClientManager->mClientsWaitingForSpawn)
		{
			int64_t iGlobalId = game::gpGame->GenerateGlobalId();

			bool bIsFlagship = false;
			engine::GridCoord spawnFleetWantedCoord {};
			uint8_t uiSpawnPendingFleetTicks = 0;
			if (!rClientSpawnInformation.fleetGuid.IsEmpty())
			{
				game::ServerFleetManager::FleetLookupResult result = game::gpServerSession->mpFleetManager->LookupFleetWantedCoord(rClientSpawnInformation.clientGuid, rClientSpawnInformation.fleetGuid, rClientSpawnInformation.iMemberIndex);
				bIsFlagship = result.flags & game::ServerFleetManager::FleetLookupFlags::kIsFlagship;
				spawnFleetWantedCoord = result.fleetWantedCoord;
				uiSpawnPendingFleetTicks = result.uiPendingFleetWantedCoordTicks;
			}

			game::StatusChange spawnChange {.eType = game::StatusChangeType::kSpawnPlayer, .data = game::SpawnPlayerData{.iGlobalId = iGlobalId, .bIsFlagship = bIsFlagship, .fleetWantedCoord = spawnFleetWantedCoord, .uiPendingFleetWantedCoordTicks = uiSpawnPendingFleetTicks}};
			game::gpGame->mFrameInputs.try_emplace(engine::kOriginCoord).first->second.statusChanges.push_back(spawnChange);
			LOG(kNetwork, kVerbose, "ServerBroadcaster::BuildFrameInputs::kSpawnPlayer Client: {} GlobalId: {} Coord: ({},{}) Flagship: {}", rClientSpawnInformation.iClientId, iGlobalId, engine::kOriginCoord.x, engine::kOriginCoord.y, bIsFlagship);
		}

		// Inject weapon mode toggle StatusChanges
		ProcessUpdatePlayerRequests();

		// Tick fleet timers and inject fleet coord updates only when this update advances. Subtracting
		// a zero delta does not make TickFleetTimers inert: an already-expired timer can still fire,
		// consume random state, and queue an update that no frame tick could consume. Deferring the
		// timer and all queued work here applies them in order on the first advancing update.
		game::gpServerSession->mpFleetManager->TickFleetTimers();
		game::gpServerSession->mpFleetManager->ProcessFlagshipUpdates();

		// Drain agent-injected StatusChanges into mFrameInputs so they ride the same broadcast / CRC / replay channel
		// as real spawns. Entries not consumable this update stay in the map (deferred) and apply on the first update
		// that can take them, matching the paused deferral. Whole-map defers: the update won't tick (mfLastDeltaTime == 0:
		// paused / zero-accumulated ticks — the per-tick consumer loop won't run and the next BuildFrameInputs wipes
		// mFrameInputs); replay playback (LoadDifference overwrites mFrameInputs from the recorded stream); or a client
		// sits in mClientsWaitingForSpawn (an agent spawn landing the same tick would corrupt the spawn-assignment-by-
		// snapshot-diff zip). Per-coord defers below: a coord the tick loop won't simulate (inactive, or no committed
		// pCurrent frame yet).
		if (game::gpGame->mfLastDeltaTime > 0.0f && !game::gpGame->mbReplaying && game::gpServerSession->mpClientManager->mClientsWaitingForSpawn.empty())
		{
			for (auto it = game::gpServerSession->mPendingAgentStatusChanges.begin(); it != game::gpServerSession->mPendingAgentStatusChanges.end();)
			{
				const engine::GridCoord& rCoord = it->first;
				auto framesIt = game::gpGame->mCoordFrames.find(rCoord);
				bool bActive = std::find(game::gpGame->mActiveCoords.begin(), game::gpGame->mActiveCoords.end(), rCoord) != game::gpGame->mActiveCoords.end();
				if (!bActive || framesIt == game::gpGame->mCoordFrames.end() || framesIt->second.pCurrent == nullptr)
				{
					++it;
					continue;
				}
				std::vector<game::StatusChange>& rStatusChanges = game::gpGame->mFrameInputs.try_emplace(rCoord).first->second.statusChanges;
				rStatusChanges.insert(rStatusChanges.end(), it->second.begin(), it->second.end());
				// Keep pending entries in status-change codec order before this tick consumes them.
				std::stable_sort(rStatusChanges.begin(), rStatusChanges.end(), [](const game::StatusChange& rLeft, const game::StatusChange& rRight)
				{
					return rLeft.eType < rRight.eType;
				});
				it = game::gpServerSession->mPendingAgentStatusChanges.erase(it);
			}
		}

		// Save StatusChanges for broadcasting (transfers handled separately in HarvestTransfers)
		for (const auto& [rCoord, rFrameInput] : game::gpGame->mFrameInputs)
		{
			if (!rFrameInput.statusChanges.empty())
			{
				mBroadcastStatusChanges.insert_or_assign(rCoord, rFrameInput.statusChanges);
			}
		}

		// Take snapshot of player IDs at spawn coordinates for FinalizeNewClients
		if (!game::gpServerSession->mpClientManager->mClientsWaitingForSpawn.empty() && game::gpGame->mCoordFrames.contains(engine::kOriginCoord))
		{
			game::gpServerSession->mpClientManager->RefreshPreSpawnSnapshot();
		}
		else
		{
			game::gpServerSession->mpClientManager->mPreSpawnPlayerIds.clear();
		}
	}
}

void ServerBroadcaster::BuildTickPublication(int64_t iTick, engine::ServerSessionRuntime& rRuntime, [[maybe_unused]] common::ScopedWorkbufferArena& rPublicationArena)
{
	const std::unordered_map<engine::GridCoord, std::vector<game::StatusChange>>& rTransfers = game::gpServerSession->mpTransferManager->mTransfers;

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	const bool bReplaying = game::gpGame->mbReplaying;
	auto ForEachPublicationCoord = [&](auto&& rCallback)
	{
		if (!bReplaying)
		{
			for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoords)
			{
				rCallback(rCoord);
			}
			return;
		}

		for (auto it = game::gpGame->mActiveCoords.begin(); it != game::gpGame->mActiveCoords.end(); ++it)
		{
			if (std::find(game::gpGame->mActiveCoords.begin(), it, *it) == it)
			{
				rCallback(*it);
			}
		}
		for ([[maybe_unused]] const auto& [rCoord, rStatusChanges] : mBroadcastStatusChanges)
		{
			if (!std::ranges::contains(game::gpGame->mActiveCoords, rCoord))
			{
				rCallback(rCoord);
			}
		}
		for ([[maybe_unused]] const auto& [rCoord, rStatusChanges] : rTransfers)
		{
			if (!std::ranges::contains(game::gpGame->mActiveCoords, rCoord) && !mBroadcastStatusChanges.contains(rCoord))
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
	int64_t iGridUpdatesOffset = common::RoundUp(iPublicationCoordsBytes, static_cast<int64_t>(16));
	int64_t iGridUpdatesBytes = iPublicationCoordCount * static_cast<int64_t>(sizeof(std::pair<engine::GridCoord, engine::GridUpdateData>));
	int64_t iStatusChangesOffset = common::RoundUp(iGridUpdatesOffset + iGridUpdatesBytes, static_cast<int64_t>(16));
	int64_t iStatusChangesBytes = iStatusChangeCount * static_cast<int64_t>(sizeof(game::StatusChange));
	int64_t iFullFramesOffset = common::RoundUp(iStatusChangesOffset + iStatusChangesBytes, static_cast<int64_t>(16));
	int64_t iFullFramesBytes = kbDesyncDebugFrames ? iPublicationCoordCount * static_cast<int64_t>(sizeof(std::pair<engine::GridCoord, const game::Frame*>)) : 0;
	int64_t iPublicationBytes = kbDesyncDebugFrames ? iFullFramesOffset + iFullFramesBytes : iStatusChangesOffset + iStatusChangesBytes;
	int64_t iPublicationHighWaterBytes = common::RoundUp(iPublicationBytes, static_cast<int64_t>(16)) + engine::kiMaxCompressStatusChangeWorkbufferBytes;
	// Grow before any publication pointer exists, then retain that capacity for nested status compression.
	{
		[[maybe_unused]] auto highWaterAllocation = rWorkbuffer.PushBuffer<std::byte*>(iPublicationHighWaterBytes);
	}

	// One allocation keeps every publication view stable until PublishTick finishes consuming it.
	auto publicationAllocation = rWorkbuffer.PushBuffer<std::byte*>(iPublicationBytes);
	std::byte* pPublicationBytes = publicationAllocation;
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
		updateData.sharedCrc = game::gpGame->CurrentFrame(rCoord).postRender.sharedCrc;
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
			LOG(kNetwork, kVerbose, "ServerBroadcaster::BuildTickPublication Coord: ({},{}) Tick: {} StatusChanges: {}", rCoord.x, rCoord.y, iTick, iRunCount);
		}
		pGridUpdates[iGridUpdateIndex++] = {rCoord, updateData};
	}

	if constexpr (kbDesyncDebugFrames)
	{
		std::pair<engine::GridCoord, const game::Frame*>* pFullFrames = reinterpret_cast<std::pair<engine::GridCoord, const game::Frame*>*>(pPublicationBytes + iFullFramesOffset);
		int64_t iFullFrameCount = 0;
		for (const engine::GridCoord& rCoord : publicationCoords)
		{
			pFullFrames[iFullFrameCount++] = {rCoord, &game::gpGame->CurrentFrame(rCoord)};
		}
		rRuntime.PublishTick(iTick, pGridUpdates, iGridUpdateIndex, pFullFrames, iFullFrameCount);
	}
	else
	{
		rRuntime.PublishTick(iTick, pGridUpdates, iGridUpdateIndex, nullptr, 0);
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
		std::span<const engine::OwnedEntity> ownedPlayers = game::gpServerSession->mClientPlayers.Owned(pClient->iClientId);
		if (ownedPlayers.empty())
		{
			continue;
		}

		// Find the coord for this global player ID in the client's owned list
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

		auto frameInputIt = game::gpGame->mFrameInputs.find(updateCoord);
		if (frameInputIt == game::gpGame->mFrameInputs.end())
		{
			continue;
		}

		// Find the frame-local player ID for this global player ID
		if (!game::gpGame->mCoordFrames.contains(updateCoord))
		{
			continue;
		}
		const int64_t iPlayerUuid = engine::RegistryUuidByGlobalId(game::Frame::OwnershipLayer(game::gpGame->CurrentFrame(updateCoord)), rRequest.globalId).Value();
		if (iPlayerUuid == 0)
		{
			continue;
		}

		uint8_t uiPendingWeaponModeTicks = static_cast<uint8_t>(engine::kiTickRate);
		game::StatusChange updateChange {.eType = game::StatusChangeType::kUpdatePlayer, .data = game::UpdatePlayerData{.iPlayerUuid = iPlayerUuid, .bUseMissiles = rRequest.bUseMissiles, .fNavigationDelay = rRequest.fNavigationDelay, .uiPendingWeaponModeTicks = uiPendingWeaponModeTicks}};
		frameInputIt->second.statusChanges.push_back(updateChange);

		LOG(kNetwork, kDebug, "ServerBroadcaster::ProcessUpdatePlayerRequests Client: {} GlobalPlayer: {} PlayerUuid: {} Coord: ({},{}) Missiles: {} NavDelay: {}", rRequest.iClientId, rRequest.globalId, iPlayerUuid, updateCoord.x, updateCoord.y, rRequest.bUseMissiles, common::Wb(rRequest.fNavigationDelay, 3));
	}
}

void ServerBroadcaster::QueueUpdatePlayerRequest(const PendingUpdatePlayerRequest& rRequest)
{
	mPendingUpdatePlayerRequests.push_back(rRequest);
}

void ServerBroadcaster::QueueAgentStatusChange(engine::GridCoord coord, const game::StatusChange& rChange)
{
	// Runs at the agent command drain point (top of ServerUpdate) under AgentCommandServer::Drain's blanket
	// allocation suppression — mirrors QueueUpdatePlayerRequest (no independent inner guard).
	game::gpServerSession->mPendingAgentStatusChanges.try_emplace(coord).first->second.push_back(rChange);
}

void ServerBroadcaster::ClearPendingRequests()
{
	mPendingUpdatePlayerRequests.clear();
}

void ServerBroadcaster::ResetState()
{
	mBroadcastStatusChanges.clear();
	mPendingUpdatePlayerRequests.clear();
	// Drop any paused-deferred agent injection so a globalId minted against the pre-load game can't leak a stale
	// StatusChange into freshly-loaded frames. Not cleared in ClearPendingRequests (runs before the agent Drain).
	game::gpServerSession->mPendingAgentStatusChanges.clear();
}

} // namespace engine

#endif // BT_SERVER
