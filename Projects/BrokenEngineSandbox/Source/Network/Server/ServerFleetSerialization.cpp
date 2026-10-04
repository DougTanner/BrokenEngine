#include "Pch.h"

#include "Network/Server/ServerFleetSerialization.h"

#include "Frame/Collections/Players/Players.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Network/GamePacketType.h"

namespace game
{

#if defined(BT_SERVER)

void SendFleetSync(int64_t iClientId, const std::vector<Fleet>& rFleets)
{
	engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
	if (pClient == nullptr)
	{
		return;
	}

	LOG(kNetwork, kVerbose, "SendFleetSync Client: {} Fleets: {}", iClientId, std::ssize(rFleets));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(GamePacketType::kServerFleetSync));
	GameMessages::FleetSyncMessage::WritePayload(rWorkbuffer, rFleets);

	engine::NetworkManager::SendPacket(pClient->pPeer, engine::NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

static void WriteFleet(std::fstream& rFileStream, const Fleet& rFleet)
{
	common::Write(rFileStream, rFleet.guid.uiHigh);
	common::Write(rFileStream, rFleet.guid.uiLow);
	int64_t iMemberCount = std::ssize(rFleet.members);
	common::Write(rFileStream, iMemberCount);
	common::Write(rFileStream, rFleet.flagshipGlobalPlayerId.iValue);
	common::Write(rFileStream, rFleet.wantedCoordinate.iX);
	common::Write(rFileStream, rFleet.wantedCoordinate.iY);
	common::Write(rFileStream, rFleet.uiPendingFleetWantedCoordinateTicks);
	common::Write(rFileStream, rFleet.navigationDelaySeconds.count());
	common::Write(rFileStream, rFleet.frameChangeTimerSeconds.count());
	for (const FleetMember& rMember : rFleet.members)
	{
		common::Write(rFileStream, rMember.globalPlayerId.iValue);
		rMember.flags.Write(rFileStream);
		common::Write(rFileStream, rMember.coordinate.iX);
		common::Write(rFileStream, rMember.coordinate.iY);
	}
}

static void ReadFleet(std::fstream& rFileStream, Fleet& rFleet)
{
	common::Read(rFileStream, rFleet.guid.uiHigh);
	common::Read(rFileStream, rFleet.guid.uiLow);
	if ((rFleet.guid.uiHigh == 0 && rFleet.guid.uiLow == 0))
	{
		throw std::ios_base::failure("Fleet FleetGuid");
	}
	int64_t iMemberCount = 0;
	common::Read(rFileStream, iMemberCount);
	common::Read(rFileStream, rFleet.flagshipGlobalPlayerId.iValue);
	int32_t iWantedX = 0;
	int32_t iWantedY = 0;
	common::Read(rFileStream, iWantedX);
	common::Read(rFileStream, iWantedY);
	rFleet.wantedCoordinate = engine::GridCoord {.iX = iWantedX, .iY = iWantedY};
	common::Read(rFileStream, rFleet.uiPendingFleetWantedCoordinateTicks);
	float fNavigationDelay = 0.0f;
	common::Read(rFileStream, fNavigationDelay);
	rFleet.navigationDelaySeconds = std::chrono::duration<float>(fNavigationDelay);
	// Trust boundary (save / replay file): the wire admits only [0, 60], so reject any other fleet delay. It
	// resets frameChangeTimerSeconds, whose count > 0.0f test gates the fire: +Inf freezes fleet nav forever and NaN fires
	// every tick; it also feeds a common::Random bound.
	if (!PlayersPostRender::IsNavigationDelayInRange(rFleet.navigationDelaySeconds.count()))
	{
		throw std::ios_base::failure("Fleet navigation delay");
	}
	float fFrameChangeTimer = 0.0f;
	common::Read(rFileStream, fFrameChangeTimer);
	rFleet.frameChangeTimerSeconds = std::chrono::duration<float>(fFrameChangeTimer);
	// Trust boundary (save / replay file): finite-check only — frameChangeTimerSeconds legitimately goes/stays
	// negative in cardinal mode (FleetNavigationController fires without resetting), so a range test would
	// reject valid data. A saved +Inf would freeze fleet nav forever.
	if (!std::isfinite(rFleet.frameChangeTimerSeconds.count()))
	{
		throw std::ios_base::failure("Fleet frame change timer");
	}
	// Trust boundary (save / replay file): bound the member count against the cap and stream before resize.
	common::ValidateDeserializedCountCapacity(iMemberCount, kiMaximumFleetMembers, sizeof(int64_t) + sizeof(uint8_t) + 2 * sizeof(int32_t), rFileStream, "ReadFleet members");
	rFleet.members.resize(static_cast<size_t>(iMemberCount));
	for (FleetMember& rMember : rFleet.members)
	{
		int64_t iGlobalPlayerId = 0;
		common::Read(rFileStream, iGlobalPlayerId);
		FleetMemberFlags_t flags {};
		flags.Read(rFileStream);
		// Trust boundary (save / replay file): kIsDead is the only member flag.
		FleetMemberFlags_t unknownFlags = flags;
		unknownFlags.Set(FleetMemberFlags::kIsDead, false);
		if (std::to_underlying(unknownFlags.meFlags) != 0)
		{
			throw std::ios_base::failure("Fleet member flags");
		}
		int32_t iCoordX = 0;
		int32_t iCoordY = 0;
		common::Read(rFileStream, iCoordX);
		common::Read(rFileStream, iCoordY);
		rMember = FleetMember {.globalPlayerId = engine::GlobalId {.iValue = iGlobalPlayerId}, .flags = flags, .coordinate = engine::GridCoord {.iX = iCoordX, .iY = iCoordY}};
	}
	// Trust boundary (save / replay file): members and the flagship are looked up by global ID, so each member ID
	// must be valid and unique within its fleet, and the flagship must name a member ({} only for an empty fleet).
	for (int64_t k = 0; k < iMemberCount; ++k)
	{
		engine::GlobalId memberGlobalPlayerId = rFleet.members.at(static_cast<size_t>(k)).globalPlayerId;
		if (!(memberGlobalPlayerId.iValue != 0) || std::ranges::contains(rFleet.members.begin(), rFleet.members.begin() + k, memberGlobalPlayerId, &FleetMember::globalPlayerId))
		{
			throw std::ios_base::failure("Fleet member global ID");
		}
	}
	bool bFlagshipValid = rFleet.members.empty() ? !(rFleet.flagshipGlobalPlayerId.iValue != 0) : std::ranges::contains(rFleet.members, rFleet.flagshipGlobalPlayerId, &FleetMember::globalPlayerId);
	if (!bFlagshipValid)
	{
		throw std::ios_base::failure("Fleet flagship global ID");
	}
}

void WriteFleetData(std::fstream& rFileStream, const std::unordered_map<engine::ClientGuid, std::vector<Fleet>, engine::ClientGuidHash>& rFleets, const common::RandomEngine& rRandom)
{
	// Heap: sorted fleet-owner scratch vector for deterministic save output
	ScopedSuppressAllocationTracking suppress;
	std::vector<engine::ClientGuid> fleetOwnerGuids;
	fleetOwnerGuids.reserve(rFleets.size());
	for (const auto& [rGuid, rFleetVector] : rFleets)
	{
		fleetOwnerGuids.push_back(rGuid);
	}
	std::ranges::sort(fleetOwnerGuids, [](const engine::ClientGuid& rLeft, const engine::ClientGuid& rRight)
	{
		return rLeft.uiHigh != rRight.uiHigh ? rLeft.uiHigh < rRight.uiHigh : rLeft.uiLow < rRight.uiLow;
	});

	int64_t iFleetOwnerCount = std::ssize(rFleets);
	common::Write(rFileStream, iFleetOwnerCount);

	for (const engine::ClientGuid& rGuid : fleetOwnerGuids)
	{
		const std::vector<Fleet>& rFleetVector = rFleets.at(rGuid);
		common::Write(rFileStream, rGuid.uiHigh);
		common::Write(rFileStream, rGuid.uiLow);
		int64_t iFleetCount = std::ssize(rFleetVector);
		common::Write(rFileStream, iFleetCount);
		for (const Fleet& rFleet : rFleetVector)
		{
			WriteFleet(rFileStream, rFleet);
		}
	}

	common::Write(rFileStream, rRandom.State());
}

void ReadFleetData(std::fstream& rFileStream, std::unordered_map<engine::ClientGuid, std::vector<Fleet>, engine::ClientGuidHash>& rFleets, std::unordered_map<engine::ClientGuid, int64_t, engine::ClientGuidHash>& rGuidToClientId, common::RandomEngine& rRandom)
{
	rFleets.clear();
	rGuidToClientId.clear();

	int64_t iFleetOwnerCount = 0;
	common::Read(rFileStream, iFleetOwnerCount);
	// Trust boundary (save / replay file): bound the owner count against the stream (each owner
	// serializes at least its 16-byte GUID plus an int64 fleet count).
	common::ValidateDeserializedCount(iFleetOwnerCount, 2 * sizeof(uint64_t) + sizeof(int64_t), rFileStream, "ReadFleetData owners");
	for (int64_t i = 0; i < iFleetOwnerCount; ++i)
	{
		uint64_t uiGuidHigh = 0;
		uint64_t uiGuidLow = 0;
		common::Read(rFileStream, uiGuidHigh);
		common::Read(rFileStream, uiGuidLow);
		engine::ClientGuid guid {.uiHigh = uiGuidHigh, .uiLow = uiGuidLow};
		if ((guid.uiHigh == 0 && guid.uiLow == 0))
		{
			throw std::ios_base::failure("Fleet owner ClientGuid");
		}
		int64_t iFleetCount = 0;
		common::Read(rFileStream, iFleetCount);
		// Trust boundary (save / replay file): bound the fleet count against the cap before constructing the vector
		// (each fleet serializes at least its 16-byte GUID).
		common::ValidateDeserializedCountCapacity(iFleetCount, kiMaximumFleetsPerClient, 2 * sizeof(uint64_t), rFileStream, "ReadFleetData fleets");
		std::vector<Fleet> fleets(static_cast<size_t>(iFleetCount));
		for (Fleet& rFleet : fleets)
		{
			ReadFleet(rFileStream, rFleet);
		}
		if (!rFleets.try_emplace(guid, std::move(fleets)).second)
		{
			throw std::ios_base::failure("duplicate Fleet owner ClientGuid");
		}
		// All loaded fleets start as disconnected
		rGuidToClientId.insert_or_assign(guid, static_cast<int64_t>(0));
	}

	uint64_t uiRandomState = 0;
	common::Read(rFileStream, uiRandomState);
	rRandom.SetSerializedState(uiRandomState);
}

void WriteSaveState(std::fstream& rFileStream)
{
	gpServerSession->WriteFleetData(rFileStream);
}

void ReadSaveState(std::fstream& rFileStream, SaveStagedState& rStagedState)
{
	ReadFleetData(rFileStream, rStagedState.fleets, rStagedState.guidToClientId, rStagedState.randomEngine);
}

void AdoptSaveState(SaveStagedState&& rStagedState)
{
	ServerFleetManager& rFleetManager = *gpServerSession->mpFleetManager;
	rFleetManager.mFleets = std::move(rStagedState.fleets);
	rFleetManager.mGuidToClientId = std::move(rStagedState.guidToClientId);
	rFleetManager.mRandomEngine = std::move(rStagedState.randomEngine);
}

void ResetSaveState()
{
	gpServerSession->mpFleetManager->ResetState();
}

#endif // BT_SERVER

} // namespace game
