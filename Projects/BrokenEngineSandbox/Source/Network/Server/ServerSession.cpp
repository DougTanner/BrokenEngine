#include "Pch.h"

#include "Network/Server/ServerSession.h"

#include "File/Replay.h"
#include "Network/Server/ServerBroadcaster.h"
#include "Network/Server/ServerTransferManager.h"
#include "Network/NetworkCursor.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/ServerCellStats.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/GamePacketType.h"
#include "Network/PlayerEvents.h"
#include "Game.h"

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
	mpRuntime = std::make_unique<engine::ServerSessionRuntime>(*this, engine::kiDefaultPort);
}

ServerSession::~ServerSession()
{
	DetachServerSimulationHarnessRigs(*this);
	mpRuntime.reset();
	gpServerSession = nullptr;
}

void ServerSession::PrepareTick()
{
	// During replay, PrepareActiveSet already configured mActiveCoordinates from the reader map
	if (gpGame->mbReplaying) [[unlikely]]
	{
		return;
	}

	// Recompute active set each tick so new client subscriptions
	// (requested by the client after the previous frame's assignment) are picked up immediately
	// Heap: ComputeActiveSet/EnsureNextFrames may grow mActiveCoordinates and Cell maps
	ScopedSuppressAllocationTracking suppress;

	mpRuntime->ComputeActiveSet();
	gpGame->EnsureNextFrames();

	for (const engine::GridCoord& rCoordinate : gpGame->mActiveCoordinates)
	{
		if (!gpGame->mFrameInputs.contains(rCoordinate))
		{
			gpGame->mFrameInputs.try_emplace(rCoordinate);
		}
	}

	mpBroadcaster->PrepareTickStatusChanges();
}

// Reject a wire-supplied navigation delay outside [0, 60] before it enters server-authoritative sim state:
// the delay resets the navigation timers, and a non-finite one would freeze them or fire them every tick.
static float AdmitNavigationDelay(float fDelay)
{
	if (!PlayersPostRender::IsNavigationDelayInRange(fDelay))
	{
		engine::NetworkMessages::ThrowCorruptStream("ServerSession navigation delay");
	}
	return fDelay;
}

static bool ReadBooleanByte(const uint8_t*& pCursor)
{
	int64_t iValue = engine::ReadUint8(pCursor);
	if (iValue > 1)
	{
		engine::NetworkMessages::ThrowCorruptStream("ServerSession Boolean byte");
	}
	return iValue != 0;
}

void ServerSession::ParseReceivedGamePackets()
{
	for (const engine::ReceivedGamePacket& rPacket : mpRuntime->mpServer->mReceivedGamePackets)
	{
		GamePacketType eType = static_cast<GamePacketType>(rPacket.iPacketType);

		// Contract gate (trust boundary): validate every game-range packet once before dispatch (drop -> count -> escalate).
		// Every client-sendable game contract row has min == max (GamePacketType.h), so this gate settles each admitted
		// payload's size exactly and the cases below add no per-case size check; only residual value checks a row
		// cannot express — navigation-delay range and Boolean bytes — stay, and they throw to drop the packet.
		engine::ClientPacketContract contract = GetGamePacketContract(eType);
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
					const uint8_t* pCursor = rPacket.payload.data();
					engine::GlobalId globalId {};
					globalId.iValue = engine::ReadInt64(pCursor);
					bool bUseMissiles = ReadBooleanByte(pCursor);
					float fNavigationDelay = AdmitNavigationDelay(engine::ReadFloat(pCursor));
					mpBroadcaster->mPendingUpdatePlayerRequests.push_back({.iClientId = rPacket.iClientId, .globalId = globalId, .bUseMissiles = bUseMissiles, .navigationDelaySeconds = std::chrono::duration<float>(fNavigationDelay)});
					break;
				}
				case GamePacketType::kClientCreateFleetRequest:
				{
					mpFleetManager->mPendingCreateFleetRequests.push_back({.iClientId = rPacket.iClientId});
					break;
				}
				case GamePacketType::kClientDeleteFleetRequest:
				{
					// 16B fleetGuid = 16 bytes (type byte already stripped)
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					mpFleetManager->mPendingDeleteFleetRequests.push_back({.iClientId = rPacket.iClientId, .fleetGuid = fleetGuid});
					break;
				}
				case GamePacketType::kClientSpawnIntoFleetRequest:
				{
					// 16B fleetGuid = 16 bytes (type byte already stripped)
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					mpFleetManager->mPendingSpawnIntoFleetRequests.push_back({.iClientId = rPacket.iClientId, .fleetGuid = fleetGuid});
					break;
				}
				case GamePacketType::kClientRespawnInFleetRequest:
				{
					// 16B fleetGuid + 8B member globalId = 24 bytes (type byte already stripped)
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					engine::GlobalId memberGlobalPlayerId {.iValue = engine::ReadInt64(pCursor)};
					mpFleetManager->mPendingRespawnInFleetRequests.push_back({.iClientId = rPacket.iClientId, .fleetGuid = fleetGuid, .memberGlobalPlayerId = memberGlobalPlayerId});
					break;
				}
				case GamePacketType::kClientFleetNavigationDelay:
				{
					// 16B fleetGuid + 4B delay = 20 bytes (type byte already stripped)
					const uint8_t* pCursor = rPacket.payload.data();
					FleetGuid fleetGuid {};
					fleetGuid.uiHigh = engine::ReadUint64(pCursor);
					fleetGuid.uiLow = engine::ReadUint64(pCursor);
					float fDelay = AdmitNavigationDelay(engine::ReadFloat(pCursor));
					const engine::ClientConnection* pClient = engine::gpServer->FindClient(rPacket.iClientId);
					if (pClient != nullptr)
					{
						mpFleetManager->UpdateFleetNavigationDelay(pClient->clientGuid, fleetGuid, std::chrono::duration<float>(fDelay));
					}
					break;
				}
				case GamePacketType::kClientSaveRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientSaveRequest Client: {}", rPacket.iClientId);
					if (!gpGame->mGameSaveLoad.ServerSave(std::filesystem::path("ServerQuicksave.save")))
					{
						LOG(kDefault, kWarning, "ServerSession::kClientSaveRequest ServerSave failed");
					}
					break;
				}
				case GamePacketType::kClientLoadRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientLoadRequest Client: {}", rPacket.iClientId);
					if (!gpGame->mGameSaveLoad.ServerLoad(std::filesystem::path("ServerQuicksave.save")))
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
					if ((game::gpGame->mbReplaying || (game::gpGame->mGameFlags & engine::GameFlags::kLoadReplay)))
					{
						LOG(kDefault, kWarning, "ServerSession::kClientReplayRecordRequest rejected: playback is active or pending Client: {}", rPacket.iClientId);
						break;
					}
					gpGame->mGameFlags.Set(engine::GameFlags::kSaveReplay);
					break;
				}
				case GamePacketType::kClientReplayPlaybackRequest:
				{
					LOG(kDefault, kDebug, "ServerSession::kClientReplayPlaybackRequest Client: {}", rPacket.iClientId);
					if ((!engine::gpReplay->mReplayWriters.empty() || (game::gpGame->mGameFlags & engine::GameFlags::kSaveReplay)))
					{
						LOG(kDefault, kWarning, "ServerSession::kClientReplayPlaybackRequest rejected: active or pending recording Client: {}", rPacket.iClientId);
						break;
					}
					gpGame->mGameFlags.Set(engine::GameFlags::kLoadReplay);
					break;
				}
				case GamePacketType::kClientPauseRequest:
				{
					// 1B paused (type byte already stripped)
					const uint8_t* pCursor = rPacket.payload.data();
					bool bPaused = ReadBooleanByte(pCursor);
					gpGame->mGameFlags.Set(engine::GameFlags::kPaused, bPaused);
					LOG(kDefault, kDebug, "Server paused: {}", bPaused);
					break;
				}
				case GamePacketType::kClientTimespeedRequest:
				{
					// 1B direction (type byte already stripped); 0 = slower, 1 = faster
					const uint8_t* pCursor = rPacket.payload.data();
					StepTimescale(ReadBooleanByte(pCursor));
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
			engine::gpServer->RecordContractViolation(rPacket.iClientId, engine::ContractViolationKind::kCorrupt, "game packet handler threw", rPacket.iPacketType, std::ssize(rPacket.payload) + 1);
		}
	}
}

void ServerSession::BeforeNetworkPoll()
{
	// mfLastDeltaTime still describes the previous update here. A zero-delta update could not consume
	// injected player requests, so retain them; a positive delta keeps their existing one-update lifetime.
	if (gpGame->mfLastDeltaTime > 0.0f)
	{
		mpBroadcaster->mPendingUpdatePlayerRequests.clear();
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
	// spawn queue, and that order is the order spawn status changes enter the frame input, and
	// therefore the simulation and the CRC.
	mpFleetManager->ProcessCreateFleetRequests();
	mpFleetManager->ProcessDeleteFleetRequests();
	mpFleetManager->ProcessSpawnIntoFleetRequests();
	mpFleetManager->ProcessRespawnInFleetRequests();
}

void ServerSession::FinalizeTickClients()
{
	mpClientManager->DetectPlayerDeaths();
	mpFleetManager->DetectDisconnectedPlayerDeaths();

	// Post-swap, so the counters describe the tick that just finished, and once per advancing tick.
	PublishServerEntityCounts();
}

void ServerSession::AddGameRequiredCoordinates()
{
	for (const auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		if (rCell.pCurrent->postRender.pPlayers->iCount > 0)
		{
			if (!std::ranges::contains(gpGame->mActiveCoordinates, rCoordinate))
			{
				gpGame->mActiveCoordinates.push_back(rCoordinate);
			}
		}
	}
}

void ServerSession::SendAssignPlayer(int64_t iClientId, engine::GlobalId globalId, engine::GridCoord coordinate)
{
	engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(GamePacketType::kServerAssignPlayer));
	GameMessages::AssignPlayerMessage message {.iGlobalPlayerIdentifier = globalId.iValue, .coordinate = coordinate};
	engine::NetworkMessages::Write(rWorkbuffer, message);
	ASSERT(rWorkbuffer.Count<uint8_t>() == sizeof(uint8_t) + GameMessages::AssignPlayerMessage::kiSize);
	engine::NetworkManager::SendPacket(pClient->pPeer, engine::NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void ServerSession::SendPlayerState(int64_t iClientId, PlayerStateWireType eWireType, int64_t iGlobalPlayerId, engine::GridCoord coordinate)
{
	engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	const GameMessages::PlayerStateDescriptor& rDescriptor = GameMessages::kpPlayerStateDescriptors[static_cast<size_t>(eWireType)];
	LOG(kNetwork, kInfo, "ServerSession::SendPlayerState State: {} Client: {} GlobalPlayer: {} Grid: ({},{})", rDescriptor.pcName, iClientId, iGlobalPlayerId, coordinate.iX, coordinate.iY);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(GamePacketType::kServerPlayerState));
	GameMessages::PlayerStateMessage message {.uiWireType = static_cast<uint8_t>(eWireType), .iGlobalPlayerIdentifier = iGlobalPlayerId, .coordinate = coordinate};
	engine::NetworkMessages::Write(rWorkbuffer, message);
	ASSERT(rWorkbuffer.Count<uint8_t>() == sizeof(uint8_t) + GameMessages::PlayerStateMessage::kiSize);
	engine::NetworkManager::SendPacket(pClient->pPeer, engine::NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
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

	// Clients update desired subscriptions from player assignments and cell-change notifications.
	for (const SubscriptionUpdate& rUpdate : rPendingUpdates)
	{
		SendAssignPlayer(rUpdate.iClientId, rUpdate.globalPlayerId, rUpdate.newCoordinate);
		SendPlayerState(rUpdate.iClientId, PlayerStateWireType::kChangedCell, rUpdate.globalPlayerId.iValue, rUpdate.newCoordinate);
	}

	rPendingUpdates.clear();
}

void ServerSession::ResetClientsForLoad()
{
	mpRuntime->mpServer->AdvanceLoadGeneration();
	LOG(kDefault, kDebug, "ServerSession::ResetClientsForLoad");
	// Relinking may allocate registry entries and authorizedCoordinates.
	ScopedSuppressAllocationTracking suppress;

	mpRuntime->mpServer->BroadcastLoadNotification();

	mpFleetManager->mNavigation.mPendingFlagshipUpdates.clear();
	std::vector<engine::ClientConnection>& rClients = engine::gpServer->mClients;

	for (engine::ClientConnection& rClient : rClients)
	{
		for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
		{
			if (rClient.slots.at(i).subscription.flags & engine::SubscriptionFlags::kActive)
			{
				rClient.FreeSlot(i);
			}
		}

		mClientPlayers.Clear(rClient.iClientId);
		if (RelinkFromFrames(rClient.iClientId, rClient.clientGuid, RelinkContext::kLoad) == 0)
		{
			LOG(kDefault, kDebug, "ServerSession::ResetClientsForLoad Client: {} no GUID match, will respawn", rClient.iClientId);
		}

		mpFleetManager->OnResetForLoad(rClient.iClientId, rClient.clientGuid);
	}

	mpClientManager->ResetState();
	mpTransferManager->ResetState();
	miHarvestedTransferTotal = 0;
	mpBroadcaster->ResetState();
	// ReadFleetData restores mFleets and OnResetForLoad restores mGuidToClientId; preserve both while clearing pending requests.
	mpFleetManager->ClearPendingRequests();
	// OnResetForLoad queues flagship updates after the initial clear; retain those updates.

	mpRuntime->ResetTransportForLoad();
}

int64_t ServerSession::RelinkFromFrames(int64_t iClientId, const engine::ClientGuid& rGuid, RelinkContext eContext)
{
	if ((rGuid.uiHigh == 0 && rGuid.uiLow == 0))
	{
		return 0;
	}

	std::vector<engine::OwnedEntity> relinkEntries;
	relinkEntries.reserve(gpGame->mCells.size());
	for (const auto& [rCoordinate, rCell] : gpGame->mCells)
	{
		const PlayersPostRender& rPlayers = *rCell.pCurrent->postRender.pPlayers;
		for (int64_t i = 0; i < rPlayers.iCount; ++i)
		{
			if (rPlayers.pClientGuids[i] == rGuid)
			{
				relinkEntries.push_back({.globalId = rPlayers.pGlobalPlayerIds[i], .coord = rCoordinate});
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
			LOG(kNetwork, kVerbose, "ServerClientManager::NewClients Re-linked Client: {} GlobalPlayer: {} Coord: ({},{})", iClientId, rEntry.globalId, rEntry.coord.iX, rEntry.coord.iY);
		}
		else
		{
			LOG(kDefault, kDebug, "ServerSession::ResetClientsForLoad Re-linked Client: {} GlobalPlayer: {} Coord: ({},{})", iClientId, rEntry.globalId, rEntry.coord.iX, rEntry.coord.iY);
		}
	}

	return std::ssize(relinkEntries);
}

void ServerSession::WriteFleetData(std::fstream& rFileStream) const
{
	::game::WriteFleetData(rFileStream, mpFleetManager->mFleets, mpFleetManager->mRandomEngine);
}

#endif // BT_SERVER

} // namespace game
