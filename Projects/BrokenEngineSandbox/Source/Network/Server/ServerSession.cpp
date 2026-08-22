#include "Pch.h"

#include "Network/Server/ServerSession.h"

#include "Network/NetworkCursor.h"

#include "File/Replay.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/ServerCellStats.h"
#include "Game.h"
#include "Network/GamePacketType.h"
#include "Network/PlayerEvents.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/Server/ServerTransferManager.h"

namespace game
{

#if defined(BT_SERVER)

ServerSession::ServerSession()
{
	ASSERT(gpServerSession == nullptr);
	gpServerSession = this;
	mpFleetManager = std::make_unique<ServerFleetManager>();
	mpTransferManager = std::make_unique<engine::ServerTransferManager>();
	mpBroadcaster = std::make_unique<engine::ServerBroadcaster>();
	mpClientManager = std::make_unique<ServerClientManager>();
	mpRuntime = std::make_unique<engine::ServerSessionRuntime>(*this, engine::kuiDefaultPort);
}

ServerSession::~ServerSession()
{
	mpRuntime.reset();
	gpServerSession = nullptr;
}

void ServerSession::PrepareTick()
{
	// During replay, PrepareActiveSet already configured mActiveCoords from the reader map
	if (gpGame->mbReplaying) [[unlikely]]
	{
		return;
	}

	// Recompute active set each tick so new client subscriptions
	// (set by FinalizeNewClients on the previous frame) are picked up immediately
	// Heap: ComputeActiveSet/EnsureNextFrames may grow mActiveCoords and CoordFrames maps
	ScopedSuppressAllocationTracking suppress;

	mpRuntime->ComputeActiveSet();
	gpGame->EnsureNextFrames();

	// Add empty frame inputs for any newly active coords
	for (const engine::GridCoord& rCoord : gpGame->mActiveCoords)
	{
		if (!gpGame->mFrameInputs.contains(rCoord))
		{
			gpGame->mFrameInputs.try_emplace(rCoord);
		}
	}
}

// Clamp a wire-supplied navigation delay before it enters server-authoritative sim state.
// Range [0.0f, 60.0f] matches the UI slider (HudScreen.cpp); NaN/Inf substitute the Fleet::fNavigationDelay default (60.0f)
// so a hostile non-finite value can't freeze fleet navigation (every fFrameChangeTimer <= 0 comparison against NaN is false).
static float ValidateNavigationDelay(float fDelay)
{
	return std::isfinite(fDelay) ? std::clamp(fDelay, 0.0f, 60.0f) : 60.0f;
}

void ServerSession::ParseReceivedGamePackets()
{
	for (const engine::ReceivedGamePacket& rPacket : mpRuntime->mpServer->mReceivedGamePackets)
	{
		GamePacketType eType = static_cast<GamePacketType>(rPacket.uiPacketType);

		// Contract gate (trust boundary): validate every game-range packet once before dispatch (drop -> count -> escalate).
		// The per-case size checks and ValidateNavigationDelay clamps below remain as backstops.
		const engine::ClientPacketContract contract = NetworkSessionContract::GetClientPacketContract(eType);
		if (!engine::gpServer->AdmitGamePacket(rPacket, contract))
		{
			continue;
		}

		try
		{
			switch (eType)
			{
				case GamePacketType::kClientUpdatePlayerRequest:
				{
					// 8B global player ID + 1B bUseMissiles + 4B fNavigationDelay = 13 bytes (type byte already stripped)
					if (rPacket.payload.size() < 13)
					{
						break;
					}
					const uint8_t* pCursor = rPacket.payload.data();
					engine::global_id_t globalId {};
					globalId.iValue = engine::ReadInt64(pCursor);
					bool bUseMissiles = engine::ReadUint8(pCursor) != 0;
					float fNavigationDelay = ValidateNavigationDelay(engine::ReadFloat(pCursor));
					mpBroadcaster->QueueUpdatePlayerRequest({rPacket.iClientId, globalId, bUseMissiles, fNavigationDelay});
					break;
				}
				case GamePacketType::kClientCreateFleetRequest:
				{
					mpFleetManager->QueueCreateRequest({rPacket.iClientId});
					break;
				}
				case GamePacketType::kClientDeleteFleetRequest:
				{
					// 16B fleetGuid = 16 bytes (type byte already stripped)
					if (rPacket.payload.size() < 16)
					{
						break;
					}
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					mpFleetManager->QueueDeleteRequest({rPacket.iClientId, fleetGuid});
					break;
				}
				case GamePacketType::kClientSpawnIntoFleetRequest:
				{
					// 16B fleetGuid = 16 bytes (type byte already stripped)
					if (rPacket.payload.size() < 16)
					{
						break;
					}
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					mpFleetManager->QueueSpawnIntoRequest({rPacket.iClientId, fleetGuid});
					break;
				}
				case GamePacketType::kClientRespawnInFleetRequest:
				{
					// 16B fleetGuid + 8B memberIndex = 24 bytes (type byte already stripped)
					if (rPacket.payload.size() < 24)
					{
						break;
					}
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					int64_t iMemberIndex = engine::ReadInt64(pCursor);
					mpFleetManager->QueueRespawnRequest({rPacket.iClientId, fleetGuid, iMemberIndex});
					break;
				}
				case GamePacketType::kClientFleetNavigationDelay:
				{
					// 16B fleetGuid + 4B delay = 20 bytes (type byte already stripped)
					if (rPacket.payload.size() < 20)
					{
						break;
					}
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					float fDelay = ValidateNavigationDelay(engine::ReadFloat(pCursor));
					const engine::ClientConnection* pClient = engine::gpServer->FindClient(rPacket.iClientId);
					if (pClient != nullptr)
					{
						mpFleetManager->UpdateFleetNavigationDelay(pClient->clientGuid, fleetGuid, fDelay);
					}
					break;
				}
				case GamePacketType::kClientSaveRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientSaveRequest Client: {}", rPacket.iClientId);
					if (!gpGame->mGameSaveLoad.ServerSave())
					{
						LOG(kDefault, kWarning, "ServerSession::kClientSaveRequest ServerSave failed");
					}
					break;
				}
				case GamePacketType::kClientLoadRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientLoadRequest Client: {}", rPacket.iClientId);
					if (!gpGame->mGameSaveLoad.ServerLoad())
					{
						// Corrupt/truncated save: engine::ReadGridSave already left a clean-slate grid, but ServerLoad's success
						// tail (client reset + active-set recompute) never ran. Fall back exactly like ServerReset
						// (fresh frame + reset connected clients for load) rather than ticking a torn grid.
						LOG(kDefault, kError, "ServerSession::kClientLoadRequest ServerLoad failed; resetting to fresh game");
						gpGame->mGameSaveLoad.ServerReset();
					}
					break;
				}
				case GamePacketType::kClientResetRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientResetRequest Client: {}", rPacket.iClientId);
					gpGame->mGameSaveLoad.ServerReset();
					break;
				}
				case GamePacketType::kClientReplayRecordRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientReplayRecordRequest Client: {}", rPacket.iClientId);
					gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay);
					break;
				}
				case GamePacketType::kClientReplayPlaybackRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientReplayPlaybackRequest Client: {}", rPacket.iClientId);
					gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay);
					break;
				}
				case GamePacketType::kClientPauseRequest:
				{
					// 1B paused (type byte already stripped)
					if (rPacket.payload.size() < 1)
					{
						break;
					}
					const uint8_t* pCursor = rPacket.payload.data();
					bool bPaused = engine::ReadUint8(pCursor) != 0;
					gpGame->mGameFlags.Set(engine::GameFlags::kPaused, bPaused);
					LOG(kDefault, kDebug, "Server paused: {}", bPaused);
					break;
				}
				case GamePacketType::kClientTimespeedRequest:
				{
					// 1B direction (type byte already stripped); 0 = slower, 1 = faster
					if (rPacket.payload.size() < 1)
					{
						break;
					}
					const uint8_t* pCursor = rPacket.payload.data();
					uint8_t uiDirection = engine::ReadUint8(pCursor);
					StepTimescale(uiDirection != 0);
					break;
				}
				default:
					break;
			}
		}
		catch (const std::exception& rException)
		{
			// Trust boundary: an untrusted game packet's handler can throw (corrupt count/size from a reader,
			// .at(), file I/O). ParseReceivedGamePackets runs after the engine network poll — a different call stack than
			// engine Server::Receive — so an uncaught throw would tear down ServerUpdate. Drop the single
			// packet and continue, parity with Server::Receive/Client::Receive.
			LOG(kNetwork, kDebug, "ServerSession::ParseReceivedGamePackets dropped corrupt packet (type {}) Client: {}: {}", static_cast<uint8_t>(eType), rPacket.iClientId, rException.what());
			engine::gpServer->RecordGamePacketHandlerThrow(rPacket);
		}
	}
}

void ServerSession::BeforeNetworkPoll()
{
	// mfLastDeltaTime still describes the previous update here. A zero-delta update could not consume
	// injected player requests, so retain them; a positive delta keeps their existing one-update lifetime.
	if (gpGame->mfLastDeltaTime > 0.0f)
	{
		mpBroadcaster->ClearPendingRequests();
	}
	// Fleet request queues drain inside their own Process*Requests, so there is nothing left to clear here.
}

void ServerSession::AfterNetworkPoll()
{
	ParseReceivedGamePackets();
	mpClientManager->Disconnects();
	mpClientManager->NewClients();
	// Ordering contract: Create runs first so requests naming a fleet created in the same poll can
	// resolve its guid. SpawnInto before Respawn also matters: both append to the client manager's
	// spawn queue, and spawn assignment pairs new player IDs with waiting clients in request order.
	mpFleetManager->ProcessCreateFleetRequests();
	mpFleetManager->ProcessDeleteFleetRequests();
	mpFleetManager->ProcessSpawnIntoFleetRequests();
	mpFleetManager->ProcessRespawnInFleetRequests();
}

void ServerSession::FinalizeTickClients()
{
	if (!gpGame->mbReplaying)
	{
		mpClientManager->FinalizeNewClients();
	}
	mpClientManager->DetectPlayerDeaths();
	mpFleetManager->DetectDisconnectedPlayerDeaths();

	// Post-swap, so the counters describe the tick that just finished, and once per advancing tick.
	PublishServerEntityCounts();
}

void ServerSession::AddGameRequiredCoords()
{
	for (const auto& [rCoord, rFrames] : gpGame->mCoordFrames)
	{
		if (rFrames.pCurrent->postRender.pPlayers->iCount > 0)
		{
			if (!std::ranges::contains(gpGame->mActiveCoords, rCoord))
			{
				gpGame->mActiveCoords.push_back(rCoord);
			}
		}
	}
}

void ServerSession::OnFrameRetiring(engine::GridCoord coord, std::unique_ptr<game::Frame>& rpFrame)
{
	engine::gpReplay->RetainReplayEndFrame(coord, rpFrame);
}

void ServerSession::SendAssignPlayer(int64_t iClientId, engine::global_id_t globalId, engine::GridCoord coord)
{
	engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(GamePacketType::kServerAssignPlayer));
	GameMessages::AssignPlayerMessage message {.iGlobalPlayerId = globalId.iValue, .coord = coord};
	engine::NetworkMessages::Write(rWorkbuffer, message);
	ASSERT(rWorkbuffer.Count<uint8_t>() == sizeof(uint8_t) + GameMessages::AssignPlayerMessage::kiSize);
	engine::NetworkManager::SendPacket(pClient->pPeer, engine::NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void ServerSession::SendPlayerState(int64_t iClientId, PlayerStateWireType eWireType, int64_t iGlobalPlayerId, engine::GridCoord coord)
{
	engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	const GameMessages::PlayerStateDescriptor& rDescriptor = GameMessages::GetPlayerStateDescriptor(eWireType);
	LOG(kNetwork, kInfo, "ServerSession::SendPlayerState State: {} Client: {} GlobalPlayer: {} Grid: ({},{})", rDescriptor.pcName, iClientId, iGlobalPlayerId, coord.x, coord.y);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(GamePacketType::kServerPlayerState));
	GameMessages::PlayerStateMessage message {.uiWireType = static_cast<uint8_t>(eWireType), .iGlobalPlayerId = iGlobalPlayerId, .coord = coord};
	engine::NetworkMessages::Write(rWorkbuffer, message);
	ASSERT(rWorkbuffer.Count<uint8_t>() == sizeof(uint8_t) + GameMessages::PlayerStateMessage::kiSize);
	engine::NetworkManager::SendPacket(pClient->pPeer, engine::NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void ServerSession::StepTimescale(bool bFaster)
{
	if (bFaster)
	{
		gpGame->mTimeStep.IncreaseTimeScale();
	}
	else
	{
		gpGame->mTimeStep.DecreaseTimeScale();
	}
	engine::gpServer->BroadcastTimespeedIfChanged();
}

void ServerSession::SubscriptionUpdates()
{
	mpRuntime->SendNewSubscriptionFullStates();

	std::vector<SubscriptionUpdate>& rPendingUpdates = mPendingSubscriptionUpdates;
	if (rPendingUpdates.empty())
	{
		return;
	}

	// Client handles subscriptions — server just sends player assignment with global ID
	for (const SubscriptionUpdate& rUpdate : rPendingUpdates)
	{
		SendAssignPlayer(rUpdate.iClientId, rUpdate.globalPlayerId, rUpdate.newCoord);
		SendPlayerState(rUpdate.iClientId, PlayerStateWireType::kChangedFrame, rUpdate.globalPlayerId.iValue, rUpdate.newCoord);
	}

	rPendingUpdates.clear();
}

void ServerSession::ResetClientsForLoad()
{
	LOG(kDefault, kDebug, "ServerSession::ResetClientsForLoad");
	// Heap: re-link rebuilds registry entries and authorizedCoords; pending state cleared across managers
	ScopedSuppressAllocationTracking suppress;

	mpRuntime->mpServer->BroadcastLoadNotification();

	mpFleetManager->mNavigation.ClearPendingFlagshipUpdates();
	std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;

	// Try to re-link each client to their players by GUID
	for (engine::ClientConnection& rClient : rClients)
	{
		// Free all subscription slots
		for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
		{
			if (rClient.slots.at(i).subscription.flags & engine::SubscriptionFlags::kActive)
			{
				rClient.FreeSlot(i);
			}
		}

		// Clear owned players and rebuild from loaded frames.
		mClientPlayers.Clear(rClient.iClientId);
		if (RelinkFromFrames(rClient.iClientId, rClient.clientGuid, RelinkContext::kLoad) == 0)
		{
			LOG(kDefault, kDebug, "ServerSession::ResetClientsForLoad Client: {} no GUID match, will respawn", rClient.iClientId);
		}

		mpFleetManager->OnResetForLoad(rClient.iClientId, rClient.clientGuid);
	}

	// Clear all pending state across managers
	mpClientManager->ResetState();
	mpTransferManager->ResetState();
	mpBroadcaster->ResetState();
	// Fleet manager: only drop pending request queues. mFleets / mGuidToClientId
	// were just authoritatively restored by ReadFleetData + per-client OnResetForLoad above;
	// a full ResetState() here would annihilate that restoration.
	mpFleetManager->ClearPendingRequests();
	// Pending flagship updates were cleared at the start of this function via mNavigation.ClearPendingFlagshipUpdates(); fleet restoration above re-queued entries — do NOT clear again here.

	mpRuntime->ResetTransportForLoad();
}

int64_t ServerSession::RelinkFromFrames(int64_t iClientId, const engine::ClientGuid& rGuid, RelinkContext eContext)
{
	if (rGuid.IsEmpty())
	{
		return 0;
	}

	std::vector<engine::OwnedEntity> relinkEntries;
	relinkEntries.reserve(gpGame->mCoordFrames.size());
	for (const auto& [rCoord, rFrames] : gpGame->mCoordFrames)
	{
		const PlayersPostRender& rPlayers = *rFrames.pCurrent->postRender.pPlayers;
		for (int64_t i = 0; i < rPlayers.iCount; ++i)
		{
			if (rPlayers.pClientGuids[i] == rGuid)
			{
				relinkEntries.push_back({.globalId = rPlayers.pGlobalPlayerIds[i], .coord = rCoord});
			}
		}
	}

	std::ranges::sort(relinkEntries, [](const engine::OwnedEntity& rLeft, const engine::OwnedEntity& rRight)
	{
		return rLeft.globalId.iValue < rRight.globalId.iValue;
	});

	mClientPlayers.Reserve(iClientId, std::ssize(relinkEntries));

	for (const engine::OwnedEntity& rEntry : relinkEntries)
	{
		mClientPlayers.Add(iClientId, rEntry.globalId, rEntry.coord);
		SendAssignPlayer(iClientId, rEntry.globalId, rEntry.coord);
		SendPlayerState(iClientId, PlayerStateWireType::kSpawned, rEntry.globalId.iValue, rEntry.coord);
		if (eContext == RelinkContext::kConnect)
		{
			LOG(kNetwork, kVerbose, "ServerClientManager::NewClients Re-linked Client: {} GlobalPlayer: {} Coord: ({},{})", iClientId, rEntry.globalId, rEntry.coord.x, rEntry.coord.y);
		}
		else
		{
			LOG(kDefault, kDebug, "ServerSession::ResetClientsForLoad Re-linked Client: {} GlobalPlayer: {} Coord: ({},{})", iClientId, rEntry.globalId, rEntry.coord.x, rEntry.coord.y);
		}
	}

	return static_cast<int64_t>(relinkEntries.size());
}

void ServerSession::WriteFleetData(std::fstream& rFileStream) const
{
	::game::WriteFleetData(rFileStream, mpFleetManager->mFleets, mpFleetManager->mRandomEngine);
}

#endif // BT_SERVER

} // namespace game
