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
	return std::ranges::any_of(engine::gpServer->mClients, [destination](const engine::ClientConnection& rClient)
	{
		return rClient.FindSlotForCoordinate(destination) >= 0;
	});
}

void ServerTransferManager::CollectTransfers()
{
	for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoordinates)
	{
		game::Frame& rNextFrame = (*game::gpGame->mCells.at(rCoord).pNext);
		for (const game::TransferRequest& rRequest : rNextFrame.postRender.transferRequests)
		{
			// Reject coordinate-edge transfers before queuing or creating a destination, preventing wraparound.
			// The source has already released the entity; an out-of-range destination cannot exist.
			const common::HexDirection& rDirection = engine::kCellDirections.pEntries[rRequest.iDirection];
			engine::GridCoord destination {};
			if (!engine::TryAddGridCoordinate(rCoord, rDirection.iDeltaQ, rDirection.iDeltaR, destination)) [[unlikely]]
			{
				LOG(kDefault, kError, "Transfer destination leaves the coordinate range Tick: {} Source: ({},{}) Direction: {} Type: {} Position: {}", rNextFrame.interpolate.iTick, rCoord.iQ, rCoord.iR, static_cast<int64_t>(rRequest.iDirection), game::StatusChangeTypeName(rRequest.change.eType), common::WbV2(game::SpawnPosition(rRequest.change), 1));
				continue;
			}

			// Owned-entity arrivals create subscriptions. Other transfers require an owned entity or an active
			// subscription, including subscriptions to empty sibling cells, to avoid creating unseen entities.
			if (!game::IsOwnedEntityTransfer(rRequest.change) && !IsDestinationLive(destination))
			{
				LOG(kNetwork, kVerbose, "Dropping transfer to unsubscribed Frame Tick: {} Source: ({},{}) Dest: ({},{}) Type: {}", rNextFrame.interpolate.iTick, rCoord.iQ, rCoord.iR, destination.iQ, destination.iR, game::StatusChangeTypeName(rRequest.change.eType));
				continue;
			}

			if (!game::gpGame->mCells.contains(destination))
			{
				game::gpGame->CreateCellAtCoordinate(destination);
				engine::Cell& rCell = game::gpGame->mCells.at(destination);
				std::swap(rCell.pCurrent, rCell.pNext);
			}

			mTransfers.try_emplace(destination).first->second.push_back(rRequest.change);
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

void ServerTransferManager::SpawnTransfers()
{
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		game::Frame& rDestinationFrame = *game::gpGame->mCells.at(rCoord).pNext;
		for (const game::StatusChange& rTransfer : rTransfers)
		{
			game::ApplySpawnStatusChange(rDestinationFrame, rTransfer);

			if (!game::IsOwnedEntityTransfer(rTransfer))
			{
				continue;
			}

			engine::GlobalId globalId = game::OwnedSpawnGlobalId(rTransfer);
			engine::ClientGuid clientGuid = game::OwnedSpawnClientGuid(rTransfer);
			if (globalId.iValue != 0 && (clientGuid.uiHigh != 0 || clientGuid.uiLow != 0))
			{
				game::gpServerSession->OnOwnedEntityChangedCell(clientGuid, globalId, rCoord);
			}
		}
	}
}

void ServerTransferManager::ApplyPreparedTransfers()
{
	SpawnTransfers();

	// Recompute CRCs for destination frames after transfers modified them. RunFrameTick computes CRCs before
	// arrived transfers land, so this is required for both live publication and replay publication.
	for ([[maybe_unused]] const auto& [rCoord, rTransfers] : mTransfers)
	{
		game::Frame& rDestinationFrame = *game::gpGame->mCells.at(rCoord).pNext;
		rDestinationFrame.postRender.uiSharedCrc = rDestinationFrame.Crc();
	}
}

void ServerTransferManager::QueueSpawn(engine::GridCoord coordinate, int64_t iTick, const game::StatusChange& rSpawn)
{
	ASSERT(game::IsSpawnType(rSpawn.eType));
	ASSERT(engine::IsInsideCell(game::SpawnPosition(rSpawn)));
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

	CollectTransfers();
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

	ApplyPreparedTransfers();

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

	ApplyPreparedTransfers();
}

void ServerTransferManager::ResetState()
{
	mTransfers.clear();
	mPendingSpawns.clear();
	miHarvestedTransferTotal = 0;
}

} // namespace engine

#endif // BT_SERVER
