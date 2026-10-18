#include "Pch.h"

#include "Network/Server/ServerSessionBase.h"

#if defined(BT_SERVER)

#include "File/Replay.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerSessionRuntime.h"
#include "Network/Server/ServerTransferManager.h"

#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace engine
{

ServerSessionBase::ServerSessionBase()
{
	mpTransferManager = std::make_unique<ServerTransferManager>();
	mpBroadcaster = std::make_unique<ServerBroadcaster>();
	mpRuntime = std::make_unique<ServerSessionRuntime>(kiDefaultPort);
}

ServerSessionBase::~ServerSessionBase() = default;

void ServerSessionBase::PrepareTick()
{
	// During replay, PrepareActiveSet already configured mActiveCoordinates from the reader map
	if (game::gpGame->mbReplaying) [[unlikely]]
	{
		return;
	}

	// Recompute active set each tick so new client subscriptions
	// (requested by the client after the previous frame's assignment) are picked up immediately
	// Heap: ComputeActiveSet/EnsureNextFrames may grow mActiveCoordinates and Cell maps
	ScopedSuppressAllocationTracking suppress;

	mpRuntime->ComputeActiveSet();
	game::gpGame->EnsureNextFrames();

	for (const GridCoord& rCoordinate : game::gpGame->mActiveCoordinates)
	{
		game::gpGame->mFrameInputs.try_emplace(rCoordinate);
	}

	mpBroadcaster->PrepareTickStatusChanges();
}

void ServerSessionBase::StepTimescale(bool bFaster)
{
	if (bFaster)
	{
		game::gpGame->mTimeStep.IncreaseTimeScale();
	}
	else
	{
		game::gpGame->mTimeStep.DecreaseTimeScale();
	}
	gpServer->BroadcastTimespeedIfChanged();
}

void ServerSessionBase::DispatchDeferredClientPackets()
{
	for (const DeferredClientPacket& rPacket : mpRuntime->mpServer->mDeferredClientPackets)
	{
		// Server::RemoveClient purges a removed client's deferred packets, and no client is removed inside this loop.
		ClientConnection& rClient = *gpServer->FindClient(rPacket.iClientId);
		if (rClient.pPeer->state != ENET_PEER_STATE_CONNECTED)
		{
			continue;
		}

		bool bGamePacket = rPacket.iPacketType >= static_cast<int64_t>(PacketType::kGamePacketStart);
		if (bGamePacket)
		{
			// Contract gate (trust boundary): Server::Receive gates engine types, so each game-range packet is
			// validated once here before dispatch (drop -> count -> escalate).
			ClientPacketContract contract = game::GetGamePacketContract(static_cast<game::NetworkSessionContract::GamePacket>(rPacket.iPacketType));
			if (!gpServer->AdmitGamePacket(rClient, rPacket, contract))
			{
				continue;
			}
		}

		try
		{
			if (bGamePacket)
			{
				game::gpServerSession->HandleGamePacket(rPacket);
			}
			else
			{
				HandleDebugControlPacket(rPacket);
			}
		}
		catch (const std::exception& rException)
		{
			// Trust boundary: an untrusted packet's handler can throw (corrupt count/size from a reader, .at(), file
			// I/O). The drain runs after the engine network poll — a different call stack than Server::Receive — so an
			// uncaught throw would tear down ServerUpdate. Drop the single packet and continue, parity with
			// Server::Receive/Client::Receive.
			LOG(kNetwork, kDebug, "ServerSessionBase::DispatchDeferredClientPackets dropped corrupt packet (type {}) Client: {}: {}", rPacket.iPacketType, rPacket.iClientId, rException.what());
			gpServer->RecordContractViolation(rClient, ContractViolationKind::kCorrupt, "deferred packet handler threw", rPacket.iPacketType, std::ssize(rPacket.payload) + 1);
		}
	}

	gpServer->RemoveDisconnectedClients();
}

// Server::Receive validated the payload, including the pause and timespeed byte, before deferring it.
void ServerSessionBase::HandleDebugControlPacket(const DeferredClientPacket& rPacket)
{
	switch (static_cast<PacketType>(rPacket.iPacketType))
	{
		case PacketType::kClientSaveRequest:
		{
			LOG(kDefault, kDebug, "ServerSessionBase::kClientSaveRequest Client: {}", rPacket.iClientId);
			if (!game::gpGame->mServerSaveLoad.ServerSave(std::filesystem::path("ServerQuicksave.save")))
			{
				LOG(kDefault, kWarning, "ServerSessionBase::kClientSaveRequest ServerSave failed");
			}
			break;
		}
		case PacketType::kClientLoadRequest:
		{
			LOG(kDefault, kDebug, "ServerSessionBase::kClientLoadRequest Client: {}", rPacket.iClientId);
			if (!game::gpGame->mServerSaveLoad.ServerLoad(std::filesystem::path("ServerQuicksave.save")))
			{
				// Corrupt/truncated save: engine::ReadGridSave already left a clean-slate grid, but ServerLoad's success
				// tail (client reset + active-set recompute) never ran. Fall back exactly like ServerReset
				// (fresh frame + reset connected clients for load) rather than ticking a torn grid.
				LOG(kDefault, kError, "ServerSessionBase::kClientLoadRequest ServerLoad failed; resetting to fresh game");
				game::gpGame->mServerSaveLoad.ServerReset();
			}
			break;
		}
		case PacketType::kClientResetRequest:
		{
			LOG(kDefault, kDebug, "ServerSessionBase::kClientResetRequest Client: {}", rPacket.iClientId);
			game::gpGame->mServerSaveLoad.ServerReset();
			break;
		}
		case PacketType::kClientReplayRecordRequest:
		{
			LOG(kDefault, kDebug, "ServerSessionBase::kClientReplayRecordRequest Client: {}", rPacket.iClientId);
			if ((game::gpGame->mbReplaying || (game::gpGame->mGameFlags & GameFlags::kLoadReplay)))
			{
				LOG(kDefault, kWarning, "ServerSessionBase::kClientReplayRecordRequest rejected: playback is active or pending Client: {}", rPacket.iClientId);
				break;
			}
			game::gpGame->mGameFlags.Set(GameFlags::kSaveReplay);
			break;
		}
		case PacketType::kClientReplayPlaybackRequest:
		{
			LOG(kDefault, kDebug, "ServerSessionBase::kClientReplayPlaybackRequest Client: {}", rPacket.iClientId);
			if ((!gpReplay->mReplayWriters.empty() || (game::gpGame->mGameFlags & GameFlags::kSaveReplay)))
			{
				LOG(kDefault, kWarning, "ServerSessionBase::kClientReplayPlaybackRequest rejected: active or pending recording Client: {}", rPacket.iClientId);
				break;
			}
			game::gpGame->mGameFlags.Set(GameFlags::kLoadReplay);
			break;
		}
		case PacketType::kClientPauseRequest:
		{
			bool bPaused = rPacket.payload.at(0) != 0;
			game::gpGame->mGameFlags.Set(GameFlags::kPaused, bPaused);
			LOG(kDefault, kDebug, "Server paused: {}", bPaused);
			break;
		}
		case PacketType::kClientTimespeedRequest:
		{
			StepTimescale(rPacket.payload.at(0) != 0);
			break;
		}
		default:
			break;
	}
}

void ServerSessionBase::RebuildOwnedEntities()
{
	// Heap: registry entries and authorizedCoordinates may grow
	ScopedSuppressAllocationTracking suppress;

	for (const ClientConnection& rClient : mpRuntime->mpServer->mClients)
	{
		if (!rClient.bHandshakeComplete)
		{
			continue;
		}

		common::ScopedWorkbufferArena entitiesArena = common::gpThreadLocal->mWorkbuffer.Push();
		for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
		{
			RegistryOwnershipLayer layer = game::Frame::OwnershipLayer(*rCell.pCurrent);
			for (int64_t i = 0; i < layer.iCount; ++i)
			{
				if (layer.pClientGuids[i] == rClient.clientGuid)
				{
					entitiesArena.mBuffer.PushBack(OwnedEntity {.globalId = layer.pGlobalIds[i], .coord = rCoordinate});
				}
			}
		}

		std::span<OwnedEntity> entities = entitiesArena.mBuffer.Span<OwnedEntity>();
		std::ranges::sort(entities, [](const OwnedEntity& rLeft, const OwnedEntity& rRight)
		{
			return rLeft.globalId.iValue < rRight.globalId.iValue;
		});

		// Every rebuild writes the registry list in this sorted order, so walking it backward is descending by global ID.
		auto ownedIt = mClientOwnedEntities.mOwned.find(rClient.iClientId);
		std::span<const OwnedEntity> previous = ownedIt != mClientOwnedEntities.mOwned.end() ? std::span<const OwnedEntity>(ownedIt->second) : std::span<const OwnedEntity>();

		// Deaths go first, in descending global-ID order, so the earliest-spawned of several entities dying together
		// reaches the game hook last.
		for (int64_t i = std::ssize(previous) - 1; i >= 0; --i)
		{
			const OwnedEntity& rPrevious = previous[i];
			if (std::ranges::contains(entities, rPrevious.globalId, &OwnedEntity::globalId))
			{
				continue;
			}
			SendOwnedEntityState(rClient.iClientId, OwnedEntityState::kDied, rPrevious.globalId, rPrevious.coord);
			game::gpServerSession->OnOwnedEntityDied(rClient.clientGuid, rPrevious.globalId);
		}

		for (const OwnedEntity& rEntity : entities)
		{
			auto previousIt = std::ranges::find(previous, rEntity.globalId, &OwnedEntity::globalId);
			if (previousIt == previous.end())
			{
				SendOwnedEntityState(rClient.iClientId, OwnedEntityState::kSpawned, rEntity.globalId, rEntity.coord);
			}
			else if (previousIt->coord != rEntity.coord)
			{
				SendOwnedEntityState(rClient.iClientId, OwnedEntityState::kChangedCell, rEntity.globalId, rEntity.coord);
			}
		}

		mClientOwnedEntities.Clear(rClient.iClientId);
		mClientOwnedEntities.Reserve(rClient.iClientId, std::ssize(entities));
		for (const OwnedEntity& rEntity : entities)
		{
			mClientOwnedEntities.Add(rClient.iClientId, rEntity.globalId, rEntity.coord);
		}
	}
}

void ServerSessionBase::AddOwnedEntityCoordinates()
{
	for (const auto& [rCoordinate, rCell] : game::gpGame->mCells)
	{
		if (game::Frame::OwnershipLayer(*rCell.pCurrent).iCount > 0 && !std::ranges::contains(game::gpGame->mActiveCoordinates, rCoordinate))
		{
			game::gpGame->mActiveCoordinates.push_back(rCoordinate);
		}
	}
}

void ServerSessionBase::SendOwnedEntityState(int64_t iClientId, OwnedEntityState eState, GlobalId globalId, GridCoord coordinate)
{
	ClientConnection* pClient = gpServer->FindClient(iClientId);

	const OwnedEntityStateDescriptor& rDescriptor = kpOwnedEntityStateDescriptors[static_cast<size_t>(eState)];
	LOG(kNetwork, kInfo, "ServerSessionBase::SendOwnedEntityState State: {} Client: {} GlobalId: {} Grid: ({},{})", rDescriptor.pcName, iClientId, globalId.iValue, coordinate.iX, coordinate.iY);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerOwnedEntityStateMessage message {.uiState = static_cast<uint8_t>(eState), .iGlobalId = globalId.iValue, .coordinate = coordinate};
	NetworkMessages::Write(rWorkbuffer, message);
	ASSERT(rWorkbuffer.Count<uint8_t>() == NetworkMessages::ServerOwnedEntityStateMessage::kiFixedSize);
	NetworkManager::SendPacket(pClient->pPeer, NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void ServerSessionBase::ResetForLoad()
{
	mpRuntime->mpServer->AdvanceLoadGeneration();
	LOG(kDefault, kDebug, "ServerSessionBase::ResetForLoad");
	// Heap: the rebuild may allocate registry entries and authorizedCoordinates
	ScopedSuppressAllocationTracking suppress;

	mpRuntime->mpServer->BroadcastLoadNotification();

	std::vector<ClientConnection>& rClients = mpRuntime->mpServer->mClients;
	for (ClientConnection& rClient : rClients)
	{
		for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
		{
			if (rClient.slots.at(i).subscription.flags & SubscriptionFlags::kActive)
			{
				rClient.FreeSlot(i);
			}
		}

		mClientOwnedEntities.Clear(rClient.iClientId);
	}

	// The cleared registry makes every surviving entity Spawned and reports no death.
	RebuildOwnedEntities();

	for (const ClientConnection& rClient : rClients)
	{
		if (rClient.bHandshakeComplete && mClientOwnedEntities.mOwned.at(rClient.iClientId).empty())
		{
			LOG(kDefault, kDebug, "ServerSessionBase::ResetForLoad Client: {} no GUID match, will respawn", rClient.iClientId);
		}
	}

	game::gpServerSession->OnResetForLoad();

	mpTransferManager->ResetState();
	mpBroadcaster->ResetState();
	mpRuntime->ResetTransportForLoad();
}

} // namespace engine

#endif // BT_SERVER
