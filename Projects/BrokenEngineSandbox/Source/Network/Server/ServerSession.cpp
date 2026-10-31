#include "Pch.h"

#include "Network/Server/ServerSession.h"

#include "Network/NetworkCursor.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/ServerCellStats.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerFleetSerialization.h"
#include "Network/GamePacketType.h"
#include "Game.h"

namespace game
{

#if defined(BT_SERVER)

ServerSession::ServerSession()
: common::Singleton<ServerSession>(gpServerSession, this)
{
	mpFleetManager = std::make_unique<ServerFleetManager>();
	mpClientManager = std::make_unique<ServerClientManager>();
}

ServerSession::~ServerSession()
{
	DetachServerSimulationHarnessRigs(*this);
	mpRuntime.reset();
}

// Reject a wire-supplied navigation delay outside [0, 60] before it enters server-authoritative sim state:
// the delay resets the navigation timers, and a non-finite one would freeze them or fire them every tick.
static float AdmitNavigationDelay(float fDelay)
{
	if (!IsNavigationDelayInRange(fDelay))
	{
		engine::NetworkMessages::ThrowCorruptStream("ServerSession navigation delay");
	}
	return fDelay;
}

static ShipType ReadShipTypeByte(const uint8_t*& pCursor)
{
	uint8_t uiValue = engine::ReadUint8(pCursor);
	if (uiValue >= std::to_underlying(ShipType::kCount))
	{
		engine::NetworkMessages::ThrowCorruptStream("ServerSession ship type byte");
	}
	return static_cast<ShipType>(uiValue);
}

// The engine drain admits each packet through its contract row first. Every client-sendable game contract row has
// min == max (GamePacketType.h), so admission settles each payload's size exactly and the cases below add no per-case
// size check; only residual value checks a row cannot express — navigation-delay range and ship type bytes — stay, and
// they throw to drop the packet.
void ServerSession::HandleGamePacket(const engine::DeferredClientPacket& rPacket)
{
	// Playback takes every simulation change from the recording. This is a timing refusal, not corruption, so it records
	// no contract violation.
	if (gpGame->mbReplaying)
	{
		LOG(kNetwork, kWarning, "ServerSession::HandleGamePacket type {} refused during replay playback Client: {}", rPacket.iPacketType, rPacket.iClientId);
		return;
	}

	switch (static_cast<GamePacketType>(rPacket.iPacketType))
	{
		case GamePacketType::kClientCreateFleetRequest:
		{
			mpFleetManager->mPendingCreateFleetRequests.push_back({.iClientId = rPacket.iClientId});
			break;
		}
		case GamePacketType::kClientSpawnIntoFleetRequest:
		{
			// 16B fleetGuid + 1B ShipType = 17 bytes (type byte already stripped)
			const uint8_t* pCursor = rPacket.payload.data();
			FleetGuid fleetGuid {};
			fleetGuid.uiHigh = engine::ReadUint64(pCursor);
			fleetGuid.uiLow = engine::ReadUint64(pCursor);
			ShipType eType = ReadShipTypeByte(pCursor);
			// A fleet's one Battleship is made with the fleet.
			if (eType == ShipType::kBattleship)
			{
				engine::NetworkMessages::ThrowCorruptStream("ServerSession spawn-into Battleship type");
			}
			mpFleetManager->mPendingSpawnIntoFleetRequests.push_back({.iClientId = rPacket.iClientId, .fleetGuid = fleetGuid, .eType = eType});
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
			mpFleetManager->UpdateFleetNavigationDelay(pClient->clientGuid, fleetGuid, std::chrono::duration<float>(fDelay));
			break;
		}
		default:
			break;
	}
}

// Empty: fleet request queues drain inside their own Process*Requests, so no queue expires unprocessed.
void ServerSession::BeforeNetworkPoll()
{
}

void ServerSession::AfterNetworkPoll()
{
	mpClientManager->NewClients();
	mpFleetManager->ProcessCreateFleetRequests();
	mpFleetManager->ProcessSpawnIntoFleetRequests();
}

void ServerSession::FinalizeTickClients()
{
	mpFleetManager->DetectFleetMemberDeaths();

	// Post-swap, so the counters describe the tick that just finished, and once per advancing tick.
	PublishServerEntityCounts();
}

// Empty: no queued client request becomes a once-per-update StatusChange.
void ServerSession::BuildAdvancingFrameInputs()
{
}

void ServerSession::TickGameTimers(bool bRecording)
{
	// Every fleet change outside the fleet step and the tick lands between the previous tick's end-of-tick checksum and
	// here, so a changed checksum means the recording needs this tick's fleet state.
	ReplayFleetLog& rReplayLog = mpFleetManager->mReplayLog;
	bool bOutsideChange = bRecording && common::Crc(ReplayFleetRecordBytes()) != rReplayLog.uiLastChecksum;

	mpFleetManager->TickFleetTimers();

	// Each spawn is queued for this tick's post-tick batch; playback queues none.
	mpFleetManager->TickRespawnTimers();

	if (bOutsideChange)
	{
		rReplayLog.recordedRecords.push_back({.iTick = gpGame->miTickCounter, .bytes = ReplayFleetRecordBytes()});
	}
}

void ServerSession::OnOwnedEntityChangedCell(const engine::ClientGuid& rClientGuid, engine::GlobalId globalId, engine::GridCoord destination)
{
	mpFleetManager->OnOwnedEntityTransferred(rClientGuid, globalId, destination);
}

// Empty: DetectFleetMemberDeaths handles every fleet member death, for connected and offline owners alike.
void ServerSession::OnOwnedEntityDied([[maybe_unused]] const engine::ClientGuid& rClientGuid, [[maybe_unused]] engine::GlobalId globalId)
{
}

void ServerSession::OnResetForLoad()
{
	// Before the per-client fleet syncs, so each carries the rebuilt member flags.
	mpFleetManager->RebuildFleetMembersForLoad();

	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		mpFleetManager->OnResetForLoad(rClient.iClientId, rClient.clientGuid);
	}

	mpClientManager->ResetState();
	// ReadFleetData restores mFleets and OnResetForLoad restores mGuidToClientId; preserve both while clearing pending requests.
	mpFleetManager->ClearPendingRequests();
}

void ServerSession::WriteFleetData(std::fstream& rFileStream) const
{
	::game::WriteFleetData(rFileStream, mpFleetManager->mFleets, mpFleetManager->mRandomEngine);
}

#endif // BT_SERVER

} // namespace game
