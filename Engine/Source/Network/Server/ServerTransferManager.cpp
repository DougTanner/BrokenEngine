#include "Pch.h"

#include "Network/Server/ServerTransferManager.h"

#if defined(BT_SERVER)

#include "File/Replay.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Game.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "SpawnTransfer.h"

namespace engine
{

static engine::ClientGuid TransferDataClientGuid(const game::TransferData& rData)
{
	return {rData.uiClientGuidHigh, rData.uiClientGuidLow};
}

// A destination is "live" if it has a committed player or any client actively subscribes to it.
// Deliberately does not consult mActiveCoords: ComputeActiveSet force-adds kOriginCoord (0,0)
// every tick as an initial-fleet-spawn bootstrap, so membership there doesn't imply anyone is
// watching. This check is used to decide whether non-Player transfers should be dropped instead
// of materializing ghost entities that clients can't see.
static bool IsDestinationLive(engine::GridCoord destination)
{
	auto destinationIt = game::gpGame->mCoordFrames.find(destination);
	if (destinationIt != game::gpGame->mCoordFrames.end() && destinationIt->second.pCurrent != nullptr &&
		engine::CountRegistryRows(game::Frame::OwnershipLayer(*destinationIt->second.pCurrent)) > 0)
	{
		return true;
	}
	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
		{
			if ((rClient.slots.at(i).subscription.flags & engine::SubscriptionFlags::kActive) && rClient.slots.at(i).subscription.coord == destination)
			{
				return true;
			}
		}
	}
	return false;
}

void ServerTransferManager::CollectTransfers(common::ScopedWorkbufferArena& rTransfersArena)
{
	for (const engine::GridCoord& rCoord : game::gpGame->mActiveCoords)
	{
		game::Frame& rNextFrame = game::gpGame->NextFrame(rCoord);
		if (rNextFrame.postRender.transferRequests.empty())
		{
			continue;
		}

		for (const game::TransferRequest& rRequest : rNextFrame.postRender.transferRequests)
		{
			// Transfer destinations must be adjacent (±1 in each axis — Chebyshev distance ≤ 1);
			// anything larger would teleport the entity. Log carries Source + Delta + Dest so the
			// landing coord reads off directly without mental arithmetic.
			if (std::abs(rRequest.iDeltaX) > 1 || std::abs(rRequest.iDeltaY) > 1) [[unlikely]]
			{
				LOG(kDefault, kError, "Transfer delta spans more than one grid cell Tick: {} Source: ({},{}) Delta: ({},{}) Dest: ({},{}) Type: {} Position: {} Velocity: {}", rNextFrame.interpolate.iTick, rCoord.x, rCoord.y, static_cast<int32_t>(rRequest.iDeltaX), static_cast<int32_t>(rRequest.iDeltaY), rCoord.x + rRequest.iDeltaX, rCoord.y + rRequest.iDeltaY, game::StatusChangeTypeName(rRequest.eType), common::WbV2(rRequest.data.vecPosition, 1), common::WbV2(rRequest.data.vecVelocity, 1));
				DEBUG_BREAK();
			}

			engine::GridCoord destination {rCoord.x + rRequest.iDeltaX, rCoord.y + rRequest.iDeltaY};

			// Drop non-Player transfers (spaceships, blasters, missiles) whose destination is not
			// live. kTransferPlayer is always allowed — the player's arrival IS the subscription.
			// Sibling-subscribed empty cells (client watching but no player present) remain live
			// and receive transfers normally so clients don't see entities vanish at cell
			// boundaries.
			if (rRequest.eType != game::StatusChangeType::kTransferPlayer && !IsDestinationLive(destination))
			{
				LOG(kNetwork, kVerbose, "Dropping transfer to unsubscribed Frame Tick: {} Source: ({},{}) Dest: ({},{}) Type: {}", rNextFrame.interpolate.iTick, rCoord.x, rCoord.y, destination.x, destination.y, game::StatusChangeTypeName(rRequest.eType));
				continue;
			}

			auto it = game::gpGame->mCoordFrames.find(destination);
			if (it == game::gpGame->mCoordFrames.end() || it->second.pNext == nullptr)
			{
				game::gpGame->CreateFrameAtCoord(destination);
				engine::CoordFrames& rFrames = game::gpGame->mCoordFrames.at(destination);
				rFrames.pNext = std::make_unique<game::Frame>();
				std::swap(rFrames.pCurrent, rFrames.pNext);
				it = game::gpGame->mCoordFrames.find(destination);
			}

			mTransfers.try_emplace(destination).first->second.push_back({
				.eType = rRequest.eType,
				.data = game::StatusChangeData{rRequest.data},
			});

			if (rRequest.eType == game::StatusChangeType::kTransferPlayer && rRequest.data.globalPlayerId.IsValid())
			{
				rTransfersArena.PushBack(ClientTransferInfo{
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
	for (auto& [rCoord, rTransfers] : mTransfers)
	{
		game::Frame& rDestFrame = *game::gpGame->mCoordFrames.at(rCoord).pNext;
		for (const game::StatusChange& rTransfer : rTransfers)
		{
			// Invariant: only kTransferPlayer may spawn into a non-live destination. CollectTransfers
			// drops all other types whose destination fails IsDestinationLive, so reaching here with
			// a non-Player transfer means CollectTransfers and SpawnTransfers disagree on liveness
			// (e.g. a subscription was dropped or a player destroyed between the two phases) and
			// entities would pile up in a cell no client can observe.
			if (bFilterDestinationLiveness && rTransfer.eType != game::StatusChangeType::kTransferPlayer && !IsDestinationLive(rCoord)) [[unlikely]]
			{
				LOG(kDefault, kError, "Non-Player transfer reached Spawn for non-live Frame Tick: {} Dest: ({},{}) Type: {}", rDestFrame.interpolate.iTick, rCoord.x, rCoord.y, game::StatusChangeTypeName(rTransfer.eType));
				DEBUG_BREAK();
			}

			game::TransferData data = std::get<game::TransferData>(rTransfer.data);
			game::SpawnTransfer(rDestFrame, rTransfer.eType, data, game::gpGame->PlayerAlignment());
		}
	}
}

void ServerTransferManager::ApplyPreparedTransfers(std::span<const ClientTransferInfo> clientTransfers, bool bFilterDestinationLiveness)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;

	// Capture pre-transfer CRCs from frame state populated by RunFrameTick before the transfer tail. The
	// destination frame is changed in place by SpawnTransfers, so index these values before that mutation.
	auto pPreCrcs = rWorkbuffer.PushBuffer<common::crc_t*>(std::ssize(mTransfers) * static_cast<int64_t>(sizeof(common::crc_t)));
	int64_t iPreCrcIndex = 0;
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		const game::Frame& rDestFrame = *game::gpGame->mCoordFrames.at(rCoord).pNext;
		pPreCrcs[iPreCrcIndex++] = rDestFrame.postRender.sharedCrc;
	}

	SpawnTransfers(bFilterDestinationLiveness);

	// Recompute CRCs for destination frames after transfers modified them. RunFrameTick computes CRCs before
	// arrived transfers land, so this is required for both live publication and replay publication.
	iPreCrcIndex = 0;
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		game::Frame& rDestFrame = *game::gpGame->mCoordFrames.at(rCoord).pNext;
		rDestFrame.postRender.sharedCrc = rDestFrame.Crcs();

		char acCrcPre[20] {}, acCrcPost[20] {};
		common::ToHex(std::span<char, 20>(acCrcPre), pPreCrcs[iPreCrcIndex++]);
		common::ToHex(std::span<char, 20>(acCrcPost), rDestFrame.postRender.sharedCrc);

		char acPlayerIds[192] {};
		int64_t iPlayerIdCount = 0;
		size_t iPosition = 0;
		for (const game::StatusChange& rTransfer : rTransfers)
		{
			if (rTransfer.eType != game::StatusChangeType::kTransferPlayer || iPlayerIdCount >= 8)
			{
				continue;
			}

			static constexpr size_t kiReserve = 24; // ", " + max 20-digit int64
			if (iPosition + kiReserve > sizeof(acPlayerIds))
			{
				break;
			}

			if (iPlayerIdCount > 0)
			{
				acPlayerIds[iPosition++] = ',';
				acPlayerIds[iPosition++] = ' ';
			}
			int iWritten = std::snprintf(acPlayerIds + iPosition, sizeof(acPlayerIds) - iPosition, "%lld", std::get<game::TransferData>(rTransfer.data).globalPlayerId.iValue);
			if (iWritten <= 0)
			{
				break;
			}
			iPosition += static_cast<size_t>(iWritten);
			++iPlayerIdCount;
		}

		if (iPlayerIdCount > 0)
		{
			LOG(kNetwork, kVerbose, "ServerTransferManager::SpawnTransfers Dest: ({},{}) TransferCount: {} PlayerCount: {} BlasterCount: {} SpaceshipCount: {} MissileCount: {} CrcPre: {} CrcPost: {} PlayerIds: [{}]", rCoord.x, rCoord.y, rTransfers.size(), engine::CountRegistryRows(game::Frame::OwnershipLayer(rDestFrame)), rDestFrame.postRender.pBlasters->iCount, rDestFrame.postRender.pSpaceships->iCount, rDestFrame.postRender.pMissiles->iCount, acCrcPre, acCrcPost, acPlayerIds);
		}
		else
		{
			bool bAnySubscribed = false;
			for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
			{
				if (rClient.FindSlotForCoord(rCoord) >= 0)
				{
					bAnySubscribed = true;
					break;
				}
			}
			if (bAnySubscribed)
			{
				LOG(kNetwork, kVerbose, "ServerTransferManager::SpawnTransfers Dest: ({},{}) TransferCount: {} PlayerCount: {} BlasterCount: {} SpaceshipCount: {} MissileCount: {} CrcPre: {} CrcPost: {}", rCoord.x, rCoord.y, rTransfers.size(), engine::CountRegistryRows(game::Frame::OwnershipLayer(rDestFrame)), rDestFrame.postRender.pBlasters->iCount, rDestFrame.postRender.pSpaceships->iCount, rDestFrame.postRender.pMissiles->iCount, acCrcPre, acCrcPost);
			}
		}
	}

	TrackClientTransfers(clientTransfers);
}

void ServerTransferManager::TrackClientTransfers(std::span<const ClientTransferInfo> clientTransfers)
{
	for (const ClientTransferInfo& rClientTransfer : clientTransfers)
	{
		game::Frame& rDestFrame = *game::gpGame->mCoordFrames.at(rClientTransfer.destination).pNext;
		const engine::RegistryOwnershipLayer destinationLayer = game::Frame::OwnershipLayer(rDestFrame);

		// An invalid uuid means the transferred player never landed in the destination, so there is nothing to bind.
		if (!engine::RegistryUuidByGlobalId(destinationLayer, rClientTransfer.globalPlayerId).IsValid())
		{
			continue;
		}

		bool bFoundClient = false;
		std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;
		for (engine::ClientConnection& rClient : rClients)
		{
			// Find the client that owns this global ID
			for (const engine::OwnedEntity& rOwnedPlayer : game::gpServerSession->mClientPlayers.Owned(rClient.iClientId))
			{
				if (rOwnedPlayer.globalId == rClientTransfer.globalPlayerId)
				{
					game::gpServerSession->mClientPlayers.UpdateCoord(rClient.iClientId, rClientTransfer.globalPlayerId, rClientTransfer.destination);
					game::gpServerSession->mPendingSubscriptionUpdates.push_back({
						.iClientId = rClient.iClientId,
						.newCoord = rClientTransfer.destination,
						.globalPlayerId = rClientTransfer.globalPlayerId,
					});

					// Copy client GUID to the new player entity in the destination frame
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
		if (!bFoundClient && !rClientTransfer.clientGuid.IsEmpty())
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
	std::span<const ClientTransferInfo> clientTransfers = transfersArena.Span<const ClientTransferInfo>();
	for (auto& [rCoord, rTransfers] : game::gpServerSession->mReplayTransferFixtures)
	{
		auto it = game::gpGame->mCoordFrames.find(rCoord);
		if (it == game::gpGame->mCoordFrames.end() || it->second.pNext == nullptr)
		{
			game::gpGame->CreateFrameAtCoord(rCoord);
			engine::CoordFrames& rFrames = game::gpGame->mCoordFrames.at(rCoord);
			rFrames.pNext = std::make_unique<game::Frame>();
			std::swap(rFrames.pCurrent, rFrames.pNext);
		}

		auto& rDestinationTransfers = mTransfers.try_emplace(rCoord).first->second;
		rDestinationTransfers.insert(rDestinationTransfers.end(), std::make_move_iterator(rTransfers.begin()), std::make_move_iterator(rTransfers.end()));
	}
	game::gpServerSession->mReplayTransferFixtures.clear();

	SortTransfersByType();

	// Replay records the sorted batch here, once per destination and against the pre-transfer frame, because
	// playback re-applies it at this same point; recording after ApplyPreparedTransfers would capture a frame
	// that already contains the transferred entities.
	for (const auto& [rCoord, rTransfers] : mTransfers)
	{
		gpReplay->CaptureHarvestedTransfers(rCoord, rTransfers, *game::gpGame->mCoordFrames.at(rCoord).pNext);
	}

	ApplyPreparedTransfers(clientTransfers, true);
}

void ServerTransferManager::PrepareReplayTransfers(engine::GridCoord coord, std::span<const game::StatusChange> recordedTransfers)
{
	std::vector<game::StatusChange>& rTransfers = mTransfers.try_emplace(coord).first->second;
	rTransfers.insert(rTransfers.end(), recordedTransfers.begin(), recordedTransfers.end());

	auto it = game::gpGame->mCoordFrames.find(coord);
	if (it == game::gpGame->mCoordFrames.end() || it->second.pNext == nullptr)
	{
		game::gpGame->CreateFrameAtCoord(coord);
		engine::CoordFrames& rFrames = game::gpGame->mCoordFrames.at(coord);
		rFrames.pNext = std::make_unique<game::Frame>();
		std::swap(rFrames.pCurrent, rFrames.pNext);
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
			if (rTransfer.eType == game::StatusChangeType::kTransferPlayer)
			{
				const game::TransferData& rData = std::get<game::TransferData>(rTransfer.data);
				if (rData.globalPlayerId.IsValid())
				{
					clientTransfersArena.PushBack(ClientTransferInfo {
						.globalPlayerId = rData.globalPlayerId,
						.destination = rCoord,
						.clientGuid = TransferDataClientGuid(rData),
					});
				}
			}
		}
	}

	ApplyPreparedTransfers(clientTransfersArena.Span<const ClientTransferInfo>(), false);
}

void ServerTransferManager::ResetState()
{
	mTransfers.clear();
	game::gpServerSession->mReplayTransferFixtures.clear();
	game::gpServerSession->mPendingSubscriptionUpdates.clear();
}

bool ServerTransferManager::QueueReplayTransferFixture(engine::GridCoord destination, game::StatusChange transfer)
{
	if constexpr (!kbDebugInput)
	{
		return false;
	}

	if (!game::IsTransferType(transfer.eType) || !std::holds_alternative<game::TransferData>(transfer.data) ||
		(transfer.eType != game::StatusChangeType::kTransferPlayer && !IsDestinationLive(destination)))
	{
		return false;
	}

	game::gpServerSession->mReplayTransferFixtures.try_emplace(destination).first->second.push_back(std::move(transfer));
	return true;
}

bool ServerTransferManager::HasPendingSubscriptionUpdate(int64_t iClientId) const
{
	return std::ranges::contains(game::gpServerSession->mPendingSubscriptionUpdates, iClientId, &game::SubscriptionUpdate::iClientId);
}

} // namespace engine

#endif // BT_SERVER
