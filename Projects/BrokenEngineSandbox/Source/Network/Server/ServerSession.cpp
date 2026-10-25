#include "Pch.h"

#include "Network/Server/ServerSession.h"

#include "Network/NetworkCursor.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"
#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Frame/Collections/Ships/ShipQueries.h"
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

static bool ReadBooleanByte(const uint8_t*& pCursor)
{
	int64_t iValue = engine::ReadUint8(pCursor);
	if (iValue > 1)
	{
		engine::NetworkMessages::ThrowCorruptStream("ServerSession Boolean byte");
	}
	return iValue != 0;
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
// size check; only residual value checks a row cannot express — navigation-delay range, Boolean bytes, and ship type
// bytes — stay, and they throw to drop the packet.
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
		case GamePacketType::kClientUpdateOwnedEntityRequest:
		{
			// 8B global ID + 1B bUseMissiles + 4B fNavigationDelay = 13 bytes (type byte already stripped)
			const uint8_t* pCursor = rPacket.payload.data();
			engine::GlobalId globalId {};
			globalId.iValue = engine::ReadInt64(pCursor);
			bool bUseMissiles = ReadBooleanByte(pCursor);
			float fNavigationDelay = AdmitNavigationDelay(engine::ReadFloat(pCursor));
			mPendingUpdateOwnedEntityRequests.push_back({.iClientId = rPacket.iClientId, .globalId = globalId, .bUseMissiles = bUseMissiles, .navigationDelaySeconds = std::chrono::duration<float>(fNavigationDelay)});
			break;
		}
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

void ServerSession::BeforeNetworkPoll()
{
	// mfLastDeltaTime still describes the previous update here. A zero-delta update could not consume
	// injected owned-entity requests, so retain them; a positive delta keeps their existing one-update lifetime.
	if (gpGame->mfLastDeltaTime > 0.0f)
	{
		mPendingUpdateOwnedEntityRequests.clear();
	}
	// Fleet request queues drain inside their own Process*Requests, so there is nothing left to clear here.
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

void ServerSession::BuildAdvancingFrameInputs()
{
	// Inject weapon mode toggle StatusChanges
	ProcessUpdateOwnedEntityRequests();
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

void ServerSession::ProcessUpdateOwnedEntityRequests()
{
	// Heap: pending owned-entity update requests may grow frame status changes
	ScopedSuppressAllocationTracking suppress;

	for (const PendingUpdateOwnedEntityRequest& rRequest : mPendingUpdateOwnedEntityRequests)
	{
		engine::ClientConnection* pClient = engine::gpServer->FindClient(rRequest.iClientId);
		if (pClient == nullptr)
		{
			continue;
		}
		auto ownedIt = mClientOwnedEntities.mOwned.find(pClient->iClientId);
		std::span<const engine::OwnedEntity> ownedEntities = ownedIt != mClientOwnedEntities.mOwned.end() ? std::span<const engine::OwnedEntity>(ownedIt->second) : std::span<const engine::OwnedEntity>();

		engine::GridCoord updateCoord {};
		bool bFound = false;
		for (const engine::OwnedEntity& rOwnedEntity : ownedEntities)
		{
			if (rOwnedEntity.globalId == rRequest.globalId)
			{
				updateCoord = rOwnedEntity.coord;
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

		ShipUuid shipUuid = ShipUuidByGlobalId(*game::gpGame->mCells.at(updateCoord).pCurrent, rRequest.globalId);
		if (shipUuid.iUuid == 0)
		{
			continue;
		}

		int64_t iPendingWeaponModeTicks = engine::kiTickRate;
		game::StatusChange updateChange {.eType = game::StatusChangeType::kUpdateShip, .data = game::UpdateShipData {.eType = shipUuid.eType, .iUuid = shipUuid.iUuid, .bUseMissiles = rRequest.bUseMissiles, .navigationDelaySeconds = rRequest.navigationDelaySeconds, .uiPendingWeaponModeTicks = static_cast<uint8_t>(iPendingWeaponModeTicks)}};
		it->second.statusChanges.push_back(updateChange);

		LOG(kNetwork, kDebug, "ServerSession::ProcessUpdateOwnedEntityRequests Client: {} GlobalId: {} Type: {} Uuid: {} Coord: ({},{}) Missiles: {} NavDelay: {}", rRequest.iClientId, rRequest.globalId, ShipTypeName(shipUuid.eType), shipUuid.iUuid, updateCoord.iQ, updateCoord.iR, rRequest.bUseMissiles, common::Wb(rRequest.navigationDelaySeconds.count(), 3));
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
	mPendingUpdateOwnedEntityRequests.clear();
	// ReadFleetData restores mFleets and OnResetForLoad restores mGuidToClientId; preserve both while clearing pending requests.
	mpFleetManager->ClearPendingRequests();
}

void ServerSession::WriteFleetData(std::fstream& rFileStream) const
{
	::game::WriteFleetData(rFileStream, mpFleetManager->mFleets, mpFleetManager->mRandomEngine);
}

#endif // BT_SERVER

} // namespace game
