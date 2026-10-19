#include "Pch.h"

#include "Network/Server/ServerTransferManager.h"

#if defined(BT_SERVER)

#include "File/Replay.h"

#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace engine
{

// A destination is "live" if it has a committed owned entity or any client actively subscribes to it.
// Deliberately does not consult mActiveCoordinates: ComputeActiveSet force-adds kOriginCoordinate (0,0)
// every tick as an initial-spawn bootstrap, so membership there doesn't imply anyone is
// watching. This check is used to decide whether non-owned transfers should be dropped instead
// of materializing ghost entities that clients can't see.
bool ServerTransferManager::IsDestinationLive(engine::GridCoord destination) const
{
	auto it = game::gpGame->mCells.find(destination);
	if (it != game::gpGame->mCells.end() && game::Frame::OwnedEntityCount(*it->second.pCurrent) > 0)
	{
		return true;
	}
	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		for (const engine::ClientConnection::SlotState& rSlot : rClient.slots)
		{
			if ((rSlot.subscription.flags & engine::SubscriptionFlags::kActive) && rSlot.subscription.coordinate == destination)
			{
				return true;
			}
		}
	}
	return false;
}

void ServerTransferManager::CollectTransfers(common::ScopedWorkbufferArena& rTransfersArena)
{
	for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoordinates)
	{
		game::Frame& rNextFrame = (*game::gpGame->mCells.at(rCoord).pNext);
		if (rNextFrame.postRender.transferRequests.empty())
		{
			continue;
		}

		for (const game::TransferRequest& rRequest : rNextFrame.postRender.transferRequests)
		{
			// Transfers must stay within one cell in each axis; larger deltas teleport entities.
			if (std::abs(rRequest.iDeltaX) > 1 || std::abs(rRequest.iDeltaY) > 1) [[unlikely]]
			{
				LOG(kDefault, kError, "Transfer delta spans more than one grid cell Tick: {} Source: ({},{}) Delta: ({},{}) Dest: ({},{}) Type: {} Position: {}", rNextFrame.interpolate.iTick, rCoord.iX, rCoord.iY, static_cast<int32_t>(rRequest.iDeltaX), static_cast<int32_t>(rRequest.iDeltaY), rCoord.iX + rRequest.iDeltaX, rCoord.iY + rRequest.iDeltaY, game::StatusChangeTypeName(rRequest.change.eType), common::WbV2(game::SpawnPosition(rRequest.change), 1));
				DEBUG_BREAK();
			}

			// Reject coordinate-edge transfers before queuing or creating a destination, preventing wraparound.
			// The source has already released the entity; an out-of-range destination cannot exist.
			engine::GridCoord destination {};
			if (!engine::TryAddGridCoordinate(rCoord, rRequest.iDeltaX, rRequest.iDeltaY, destination)) [[unlikely]]
			{
				LOG(kDefault, kError, "Transfer destination leaves the coordinate range Tick: {} Source: ({},{}) Delta: ({},{}) Type: {} Position: {}", rNextFrame.interpolate.iTick, rCoord.iX, rCoord.iY, static_cast<int32_t>(rRequest.iDeltaX), static_cast<int32_t>(rRequest.iDeltaY), game::StatusChangeTypeName(rRequest.change.eType), common::WbV2(game::SpawnPosition(rRequest.change), 1));
				continue;
			}

			// Owned-entity arrivals create subscriptions. Other transfers require an owned entity or an active
			// subscription, including subscriptions to empty sibling cells, to avoid creating unseen entities.
			if (!game::IsOwnedEntityTransfer(rRequest.change) && !IsDestinationLive(destination))
			{
				LOG(kNetwork, kVerbose, "Dropping transfer to unsubscribed Frame Tick: {} Source: ({},{}) Dest: ({},{}) Type: {}", rNextFrame.interpolate.iTick, rCoord.iX, rCoord.iY, destination.iX, destination.iY, game::StatusChangeTypeName(rRequest.change.eType));
				continue;
			}

			auto it = game::gpGame->mCells.find(destination);
			if (it == game::gpGame->mCells.end())
			{
				game::gpGame->CreateCellAtCoordinate(destination);
				engine::Cell& rCell = game::gpGame->mCells.at(destination);
				std::swap(rCell.pCurrent, rCell.pNext);
				it = game::gpGame->mCells.find(destination);
			}

			mTransfers.try_emplace(destination).first->second.push_back(rRequest.change);

			if (game::IsOwnedEntityTransfer(rRequest.change) && (game::OwnedSpawnGlobalId(rRequest.change).iValue != 0))
			{
				rTransfersArena.mBuffer.PushBack(ClientTransferInfo
				{
					.globalId = game::OwnedSpawnGlobalId(rRequest.change),
					.destination = destination,
					.clientGuid = game::OwnedSpawnClientGuid(rRequest.change),
				});
			}
		}
	}
}

void ServerTransferManager::DrainPendingSpawns()
{
	int64_t iTick = game::gpGame->miTickCounter;
	auto IsReleased = [iTick](const PendingSpawn& rPending)
	{
		return rPending.iTick <= iTick;
	};
	for (auto it = mPendingSpawns.begin(); it != mPendingSpawns.end();)
	{
		auto& [rCoordinate, rPendingSpawns] = *it;
		if (!std::ranges::contains(game::gpGame->mActiveCoordinates, rCoordinate) || std::ranges::none_of(rPendingSpawns, IsReleased))
		{
			++it;
			continue;
		}

		std::vector<game::StatusChange>& rTransfers = mTransfers.try_emplace(rCoordinate).first->second;
		for (const PendingSpawn& rPending : rPendingSpawns)
		{
			if (IsReleased(rPending))
			{
				rTransfers.push_back(rPending.spawn);
			}
		}
		std::erase_if(rPendingSpawns, IsReleased);
		it = rPendingSpawns.empty() ? mPendingSpawns.erase(it) : std::next(it);
	}
}

void ServerTransferManager::SortTransfersByType()
{
	for (auto& [rCoord, rTransfers] : mTransfers)
	{
		std::ranges::sort(rTransfers, [](const game::StatusChange& rLeft, const game::StatusChange& rRight)
		{
			return rLeft.eType < rRight.eType;
		});
	}
}

void ServerTransferManager::SpawnTransfers(bool bFilterDestinationLiveness)
{
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		game::Frame& rDestinationFrame = *game::gpGame->mCells.at(rCoord).pNext;
		for (const game::StatusChange& rTransfer : rTransfers)
		{
			// Liveness filtering requires non-owned transfers to target live destinations, matching CollectTransfers.
			// A spawn targets an active cell that need not be live. Replay spawning bypasses this check.
			if (bFilterDestinationLiveness && game::IsTransferType(rTransfer.eType) && !game::IsOwnedEntityTransfer(rTransfer) && !IsDestinationLive(rCoord)) [[unlikely]]
			{
				LOG(kDefault, kError, "Non-owned transfer reached Spawn for non-live Frame Tick: {} Dest: ({},{}) Type: {}", rDestinationFrame.interpolate.iTick, rCoord.iX, rCoord.iY, game::StatusChangeTypeName(rTransfer.eType));
				DEBUG_BREAK();
			}

			game::ApplySpawnStatusChange(rDestinationFrame, rTransfer, rCoord);
		}
	}
}

void ServerTransferManager::ApplyPreparedTransfers(const common::ScopedWorkbufferArena& rTransfersArena, bool bFilterDestinationLiveness)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	{
		common::ScopedWorkbufferArena preCrcsArena = rWorkbuffer.Push();

		// Capture pre-transfer CRCs from frame state populated by RunFrameTick before the transfer tail. The
		// destination frame is changed in place by SpawnTransfers, so index these values before that mutation.
		for (const auto& [rCoord, rTransfers] : mTransfers)
		{
			const game::Frame& rDestinationFrame = *game::gpGame->mCells.at(rCoord).pNext;
			preCrcsArena.mBuffer.PushBack(rDestinationFrame.postRender.uiSharedCrc);
		}

		SpawnTransfers(bFilterDestinationLiveness);

		// Recompute CRCs for destination frames after transfers modified them. RunFrameTick computes CRCs before
		// arrived transfers land, so this is required for both live publication and replay publication.
		for (int64_t i = 0; const auto& [rCoord, rTransfers] : mTransfers)
		{
			game::Frame& rDestinationFrame = *game::gpGame->mCells.at(rCoord).pNext;
			common::crc_t uiPreCrc = preCrcsArena.mBuffer.Span<const common::crc_t>()[i++];
			rDestinationFrame.postRender.uiSharedCrc = rDestinationFrame.Crc();

			char pcCrcPre[20] {}, pcCrcPost[20] {};
			common::ToHex(std::span<char, 20>(pcCrcPre), uiPreCrc);
			common::ToHex(std::span<char, 20>(pcCrcPost), rDestinationFrame.postRender.uiSharedCrc);

			char pcOwnedIds[192] {};
			char* pOwnedIdsWrite = pcOwnedIds;
			int64_t iOwnedIdCount = 0;
			for (const game::StatusChange& rTransfer : rTransfers)
			{
				if (!game::IsOwnedEntityTransfer(rTransfer))
				{
					continue;
				}

				if (iOwnedIdCount >= 8)
				{
					continue;
				}

				// The final byte stays the zero terminator.
				pOwnedIdsWrite = std::format_to_n(pOwnedIdsWrite, pcOwnedIds + std::ssize(pcOwnedIds) - 1 - pOwnedIdsWrite, "{}{}", iOwnedIdCount > 0 ? ", " : "", game::OwnedSpawnGlobalId(rTransfer).iValue).out;
				++iOwnedIdCount;
			}

			if (iOwnedIdCount > 0)
			{
				LOG(kNetwork, kVerbose, "ServerTransferManager::SpawnTransfers Dest: ({},{}) TransferCount: {} OwnedCount: {} CrcPre: {} CrcPost: {} OwnedIds: [{}]", rCoord.iX, rCoord.iY, std::ssize(rTransfers), game::Frame::OwnedEntityCount(rDestinationFrame), pcCrcPre, pcCrcPost, pcOwnedIds);
			}
			else
			{
				bool bAnySubscribed = std::ranges::any_of(engine::gpServer->mClients, [&rCoord](const engine::ClientConnection& rClient)
				{
					return rClient.FindSlotForCoordinate(rCoord) >= 0;
				});
				if (bAnySubscribed)
				{
					LOG(kNetwork, kVerbose, "ServerTransferManager::SpawnTransfers Dest: ({},{}) TransferCount: {} OwnedCount: {} CrcPre: {} CrcPost: {}", rCoord.iX, rCoord.iY, std::ssize(rTransfers), game::Frame::OwnedEntityCount(rDestinationFrame), pcCrcPre, pcCrcPost);
				}
			}
		}
	}

	TrackClientTransfers(rTransfersArena.mBuffer.Span<const ClientTransferInfo>());
}

void ServerTransferManager::TrackClientTransfers(std::span<const ClientTransferInfo> clientTransfers)
{
	for (const ClientTransferInfo& rClientTransfer : clientTransfers)
	{
		game::Frame& rDestinationFrame = *game::gpGame->mCells.at(rClientTransfer.destination).pNext;
		for (const engine::RegistryOwnershipLayer& rDestinationLayer : game::Frame::OwnershipLayers(rDestinationFrame))
		{
			// An invalid uuid means the transferred owned entity did not land in this layer, so there is nothing to bind.
			if (!(engine::RegistryUuidByGlobalId(rDestinationLayer, rClientTransfer.globalId).iValue != 0))
			{
				continue;
			}

			// Connected or not, the owner keeps its GUID in the destination; ServerSessionBase::RebuildOwnedEntities reports
			// the cell change to a connected owner.
			if (rClientTransfer.clientGuid.uiHigh != 0 || rClientTransfer.clientGuid.uiLow != 0)
			{
				engine::AssignRegistryClientGuid(rDestinationLayer, rClientTransfer.globalId, rClientTransfer.clientGuid);
				game::gpServerSession->OnOwnedEntityChangedCell(rClientTransfer.clientGuid, rClientTransfer.globalId, rClientTransfer.destination);
			}
			break;
		}
	}
}

void ServerTransferManager::QueueSpawn(engine::GridCoord coordinate, int64_t iTick, const game::StatusChange& rSpawn)
{
	ASSERT(game::IsSpawnType(rSpawn.eType));
	ASSERT(common::InsideArea(game::SpawnPosition(rSpawn), engine::LocalCellArea()));
	mPendingSpawns.try_emplace(coordinate).first->second.push_back({.iTick = iTick, .spawn = rSpawn});
}

int64_t ServerTransferManager::PendingSpawnCount() const
{
	int64_t iCount = 0;
	for ([[maybe_unused]] const auto& [rCoordinate, rPendingSpawns] : mPendingSpawns)
	{
		iCount += std::ssize(rPendingSpawns);
	}
	return iCount;
}

void ServerTransferManager::HarvestTransfers()
{
	// Heap: Transfer spawns into destination frames, which may grow SOA buffers and update idToIndexMaps
	ScopedSuppressAllocationTracking suppress;

	mTransfers.clear();

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena transfersArena = rWorkbuffer.Push();
	CollectTransfers(transfersArena);
	DrainPendingSpawns();

	SortTransfersByType();

	// Replay capture uses each destination's sorted batch and pre-transfer frame; playback applies it at the same point.
	// Capture must precede ApplyPreparedTransfers so the snapshot excludes arriving entities.
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		if (!gpReplay->CaptureAcceptedTransfers(rCoord, rTransfers, *game::gpGame->mCells.at(rCoord).pNext)) [[unlikely]]
		{
			LOG(kDefault, kError, "Replay transfer capture failed; recording invalidated");
		}
	}

	ApplyPreparedTransfers(transfersArena, true);

	// mTransfers still holds the batch just applied; the broadcaster clears it later this tick.
	for (const auto& [rCoordinate, rTransfers] : mTransfers)
	{
		miHarvestedTransferTotal += std::ranges::count_if(rTransfers, [](const game::StatusChange& rTransfer)
		{
			return game::IsTransferType(rTransfer.eType);
		});
	}
}

void ServerTransferManager::PrepareReplayTransfers(engine::GridCoord coord, std::span<const game::StatusChange> recordedTransfers)
{
	std::vector<game::StatusChange>& rTransfers = mTransfers.try_emplace(coord).first->second;
	rTransfers.append_range(recordedTransfers);
}

void ServerTransferManager::ApplyReplayTransfers()
{
	// Heap: replay transfer spawns may grow destination SOA buffers and update idToIndexMaps
	ScopedSuppressAllocationTracking suppress;

	common::ScopedWorkbufferArena clientTransfersArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		for (const game::StatusChange& rTransfer : rTransfers)
		{
			if (!game::IsOwnedEntityTransfer(rTransfer))
			{
				continue;
			}

			engine::GlobalId globalId = game::OwnedSpawnGlobalId(rTransfer);
			if (!(globalId.iValue != 0))
			{
				continue;
			}

			clientTransfersArena.mBuffer.PushBack(ClientTransferInfo
			{
				.globalId = globalId,
				.destination = rCoord,
				.clientGuid = game::OwnedSpawnClientGuid(rTransfer),
			});
		}
	}

	ApplyPreparedTransfers(clientTransfersArena, false);
}

void ServerTransferManager::ResetState()
{
	mTransfers.clear();
	mPendingSpawns.clear();
	miHarvestedTransferTotal = 0;
}

} // namespace engine

#endif // BT_SERVER
