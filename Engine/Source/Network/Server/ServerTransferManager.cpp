#include "Pch.h"

#include "Network/Server/ServerTransferManager.h"

#if defined(BT_SERVER)

#include "File/Replay.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"
#include "SpawnTransfer.h"

namespace engine
{

static engine::ClientGuid TransferDataClientGuid(const game::TransferData& rData)
{
	return {.uiHigh = rData.uiClientGuidHigh, .uiLow = rData.uiClientGuidLow};
}

// A destination is "live" if it has a committed player or any client actively subscribes to it.
// Deliberately does not consult mActiveCoordinates: ComputeActiveSet force-adds kOriginCoordinate (0,0)
// every tick as an initial-fleet-spawn bootstrap, so membership there doesn't imply anyone is
// watching. This check is used to decide whether non-Player transfers should be dropped instead
// of materializing ghost entities that clients can't see.
bool ServerTransferManager::IsDestinationLive(engine::GridCoord destination) const
{
	auto it = game::gpGame->mCells.find(destination);
	if (it != game::gpGame->mCells.end() && it->second.pCurrent != nullptr
	 && (game::Frame::OwnershipLayer(*it->second.pCurrent)).iCount > 0)
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
				LOG(kDefault, kError, "Transfer delta spans more than one grid cell Tick: {} Source: ({},{}) Delta: ({},{}) Dest: ({},{}) Type: {} Position: {} Velocity: {}", rNextFrame.interpolate.iTick, rCoord.iX, rCoord.iY, static_cast<int32_t>(rRequest.iDeltaX), static_cast<int32_t>(rRequest.iDeltaY), rCoord.iX + rRequest.iDeltaX, rCoord.iY + rRequest.iDeltaY, game::StatusChangeTypeName(rRequest.eType), common::WbV2(rRequest.data.vecPosition, 1), common::WbV2(rRequest.data.vecVelocity, 1));
				DEBUG_BREAK();
			}

			// Reject coordinate-edge transfers before queuing or creating a destination, preventing wraparound.
			// The source has already released the entity; an out-of-range destination cannot exist.
			engine::GridCoord destination {};
			if (!engine::TryAddGridCoordinate(rCoord, rRequest.iDeltaX, rRequest.iDeltaY, destination)) [[unlikely]]
			{
				LOG(kDefault, kError, "Transfer destination leaves the coordinate range Tick: {} Source: ({},{}) Delta: ({},{}) Type: {} Position: {}", rNextFrame.interpolate.iTick, rCoord.iX, rCoord.iY, static_cast<int32_t>(rRequest.iDeltaX), static_cast<int32_t>(rRequest.iDeltaY), game::StatusChangeTypeName(rRequest.eType), common::WbV2(rRequest.data.vecPosition, 1));
				continue;
			}

			// Player arrivals create subscriptions. Other transfers require a player or an active subscription,
			// including subscriptions to empty sibling cells, to avoid creating unseen entities.
			if (rRequest.eType != game::StatusChangeType::kTransferPlayer && !IsDestinationLive(destination))
			{
				LOG(kNetwork, kVerbose, "Dropping transfer to unsubscribed Frame Tick: {} Source: ({},{}) Dest: ({},{}) Type: {}", rNextFrame.interpolate.iTick, rCoord.iX, rCoord.iY, destination.iX, destination.iY, game::StatusChangeTypeName(rRequest.eType));
				continue;
			}

			auto it = game::gpGame->mCells.find(destination);
			if (it == game::gpGame->mCells.end() || it->second.pNext == nullptr)
			{
				game::gpGame->CreateCellAtCoordinate(destination);
				engine::Cell& rCell = game::gpGame->mCells.at(destination);
				rCell.pNext = std::make_unique<game::Frame>();
				std::swap(rCell.pCurrent, rCell.pNext);
				it = game::gpGame->mCells.find(destination);
			}

			mTransfers.try_emplace(destination).first->second.push_back(
			{
				.eType = rRequest.eType,
				.data = game::StatusChangeData(rRequest.data),
			});

			if (rRequest.eType == game::StatusChangeType::kTransferPlayer && (rRequest.data.globalPlayerId.iValue != 0))
			{
				rTransfersArena.mBuffer.PushBack(ClientTransferInfo
				{
					.globalPlayerId = rRequest.data.globalPlayerId,
					.destination = destination,
					.clientGuid = TransferDataClientGuid(rRequest.data),
				});
			}
		}
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
			// Liveness filtering requires non-player transfers to target live destinations, matching CollectTransfers.
			// Replay spawning bypasses this check.
			if (bFilterDestinationLiveness && rTransfer.eType != game::StatusChangeType::kTransferPlayer && !IsDestinationLive(rCoord)) [[unlikely]]
			{
				LOG(kDefault, kError, "Non-Player transfer reached Spawn for non-live Frame Tick: {} Dest: ({},{}) Type: {}", rDestinationFrame.interpolate.iTick, rCoord.iX, rCoord.iY, game::StatusChangeTypeName(rTransfer.eType));
				DEBUG_BREAK();
			}

			game::TransferData data = std::get<game::TransferData>(rTransfer.data);
			game::SpawnTransfer(rDestinationFrame, rTransfer.eType, data, game::gpGame->mPlayerAlignment);
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

			char acCrcPre[20] {}, acCrcPost[20] {};
			common::ToHex(std::span<char, 20>(acCrcPre), uiPreCrc);
			common::ToHex(std::span<char, 20>(acCrcPost), rDestinationFrame.postRender.uiSharedCrc);

			char acPlayerIds[192] {};
			char* pPlayerIdsWrite = acPlayerIds;
			int64_t iPlayerIdCount = 0;
			for (const game::StatusChange& rTransfer : rTransfers)
			{
				if (rTransfer.eType != game::StatusChangeType::kTransferPlayer)
				{
					continue;
				}

				if (iPlayerIdCount >= 8)
				{
					continue;
				}

				// The final byte stays the zero terminator.
				pPlayerIdsWrite = std::format_to_n(pPlayerIdsWrite, acPlayerIds + std::ssize(acPlayerIds) - 1 - pPlayerIdsWrite, "{}{}", iPlayerIdCount > 0 ? ", " : "", std::get<game::TransferData>(rTransfer.data).globalPlayerId.iValue).out;
				++iPlayerIdCount;
			}

			if (iPlayerIdCount > 0)
			{
				LOG(kNetwork, kVerbose, "ServerTransferManager::SpawnTransfers Dest: ({},{}) TransferCount: {} PlayerCount: {} BlasterCount: {} SpaceshipCount: {} MissileCount: {} CrcPre: {} CrcPost: {} PlayerIds: [{}]", rCoord.iX, rCoord.iY, std::ssize(rTransfers), (game::Frame::OwnershipLayer(rDestinationFrame)).iCount, rDestinationFrame.postRender.pBlasters->iCount, rDestinationFrame.postRender.pSpaceships->iCount, rDestinationFrame.postRender.pMissiles->iCount, acCrcPre, acCrcPost, acPlayerIds);
			}
			else
			{
				bool bAnySubscribed = std::ranges::any_of(engine::gpServer->mClients, [&rCoord](const engine::ClientConnection& rClient)
				{
					return rClient.FindSlotForCoordinate(rCoord) >= 0;
				});
				if (bAnySubscribed)
				{
					LOG(kNetwork, kVerbose, "ServerTransferManager::SpawnTransfers Dest: ({},{}) TransferCount: {} PlayerCount: {} BlasterCount: {} SpaceshipCount: {} MissileCount: {} CrcPre: {} CrcPost: {}", rCoord.iX, rCoord.iY, std::ssize(rTransfers), (game::Frame::OwnershipLayer(rDestinationFrame)).iCount, rDestinationFrame.postRender.pBlasters->iCount, rDestinationFrame.postRender.pSpaceships->iCount, rDestinationFrame.postRender.pMissiles->iCount, acCrcPre, acCrcPost);
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
		engine::RegistryOwnershipLayer destinationLayer = game::Frame::OwnershipLayer(rDestinationFrame);

		// An invalid uuid means the transferred player never landed in the destination, so there is nothing to bind.
		if (!(engine::RegistryUuidByGlobalId(destinationLayer, rClientTransfer.globalPlayerId).iValue != 0))
		{
			continue;
		}

		bool bFoundClient = false;
		std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;
		for (const engine::ClientConnection& rClient : rClients)
		{
			auto ownedIt = game::gpServerSession->mClientPlayers.mOwned.find(rClient.iClientId);
			std::span<const engine::OwnedEntity> ownedPlayers = ownedIt != game::gpServerSession->mClientPlayers.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();
			for (const engine::OwnedEntity& rOwnedPlayer : ownedPlayers)
			{
				if (rOwnedPlayer.globalId == rClientTransfer.globalPlayerId)
				{
					game::gpServerSession->mClientPlayers.UpdateCoord(rClient.iClientId, rClientTransfer.globalPlayerId, rClientTransfer.destination);
					game::gpServerSession->mPendingSubscriptionUpdates.push_back(
					{
						.iClientId = rClient.iClientId,
						.newCoordinate = rClientTransfer.destination,
						.globalPlayerId = rClientTransfer.globalPlayerId,
					});

					engine::AssignRegistryClientGuid(destinationLayer, rClientTransfer.globalPlayerId, rClient.clientGuid);

					game::gpServerSession->mpFleetManager->OnPlayerTransferred(rClient.clientGuid, rClientTransfer.globalPlayerId, rClientTransfer.destination);

					bFoundClient = true;
					break;
				}
			}
			if (bFoundClient)
			{
				break;
			}
		}

		// Orphaned players (client disconnected): preserve GUID and update fleet
		if (!bFoundClient && (rClientTransfer.clientGuid.uiHigh != 0 || rClientTransfer.clientGuid.uiLow != 0))
		{
			engine::AssignRegistryClientGuid(destinationLayer, rClientTransfer.globalPlayerId, rClientTransfer.clientGuid);
			game::gpServerSession->mpFleetManager->OnPlayerTransferred(rClientTransfer.clientGuid, rClientTransfer.globalPlayerId, rClientTransfer.destination);
		}
	}
}

void ServerTransferManager::HarvestTransfers()
{
	// Heap: Transfer spawns into destination frames, which may grow SOA buffers and update idToIndexMaps
	ScopedSuppressAllocationTracking suppress;

	mTransfers.clear();

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena transfersArena = rWorkbuffer.Push();
	CollectTransfers(transfersArena);
	game::DrainReplayTransferHarnessRigs(*game::gpServerSession, *this);

	SortTransfersByType();

	// Replay capture uses each destination's sorted batch and pre-transfer frame; playback applies it at the same point.
	// Capture must precede ApplyPreparedTransfers so the snapshot excludes arriving entities.
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		if (gpReplay->CaptureAcceptedTransfers(rCoord, rTransfers, *game::gpGame->mCells.at(rCoord).pNext)) [[unlikely]]
		{
			LOG(kDefault, kError, "Replay transfer capture failed; recording invalidated");
		}
	}

	ApplyPreparedTransfers(transfersArena, true);
}

void ServerTransferManager::PrepareReplayTransfers(engine::GridCoord coord, std::span<const game::StatusChange> recordedTransfers)
{
	std::vector<game::StatusChange>& rTransfers = mTransfers.try_emplace(coord).first->second;
	rTransfers.append_range(recordedTransfers);

	auto it = game::gpGame->mCells.find(coord);
	if (it == game::gpGame->mCells.end() || it->second.pNext == nullptr)
	{
		game::gpGame->CreateCellAtCoordinate(coord);
		engine::Cell& rCell = game::gpGame->mCells.at(coord);
		rCell.pNext = std::make_unique<game::Frame>();
		std::swap(rCell.pCurrent, rCell.pNext);
	}
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
			if (rTransfer.eType != game::StatusChangeType::kTransferPlayer)
			{
				continue;
			}

			const game::TransferData& rData = std::get<game::TransferData>(rTransfer.data);
			if (!(rData.globalPlayerId.iValue != 0))
			{
				continue;
			}

			clientTransfersArena.mBuffer.PushBack(ClientTransferInfo
			{
				.globalPlayerId = rData.globalPlayerId,
				.destination = rCoord,
				.clientGuid = TransferDataClientGuid(rData),
			});
		}
	}

	ApplyPreparedTransfers(clientTransfersArena, false);
}

void ServerTransferManager::ResetState()
{
	mTransfers.clear();
	game::ResetReplayTransferHarnessRigs(*game::gpServerSession);
	game::gpServerSession->mPendingSubscriptionUpdates.clear();
}

} // namespace engine

#endif // BT_SERVER
