#include "Pch.h"

#include "Network/Server/ServerFleetSerialization.h"

#include "Frame/Collections/Ships/ShipBehaviours.h"
#include "Network/Server/ServerClientManager.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Network/GamePacketType.h"

namespace game
{

#if defined(BT_SERVER)

void SendFleetSync(int64_t iClientId, const std::vector<Fleet>& rFleets)
{
	engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);

	LOG(kNetwork, kVerbose, "SendFleetSync Client: {} Fleets: {}", iClientId, std::ssize(rFleets));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	rWorkbuffer.PushBack<uint8_t>(static_cast<uint8_t>(GamePacketType::kServerFleetSync));
	GameMessages::FleetSyncMessage::WritePayload(rWorkbuffer, rFleets);

	engine::NetworkManager::SendPacket(pClient->pPeer, engine::NetworkManager::kiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

static void WriteFleet(std::ostream& rStream, const Fleet& rFleet)
{
	common::Write(rStream, rFleet.guid.uiHigh);
	common::Write(rStream, rFleet.guid.uiLow);
	int64_t iMemberCount = std::ssize(rFleet.members);
	common::Write(rStream, iMemberCount);
	common::Write(rStream, rFleet.flagshipGlobalId.iValue);
	common::Write(rStream, rFleet.wantedCoordinate.iQ);
	common::Write(rStream, rFleet.wantedCoordinate.iR);
	common::Write(rStream, rFleet.uiPendingFleetWantedCoordinateTicks);
	common::Write(rStream, rFleet.navigationDelaySeconds.count());
	common::Write(rStream, rFleet.cellChangeTimerSeconds.count());
	for (uint8_t uiTargetFlightPlane : rFleet.targetFlightPlanes)
	{
		common::Write(rStream, uiTargetFlightPlane);
	}
	for (const FleetMember& rMember : rFleet.members)
	{
		common::Write(rStream, rMember.globalId.iValue);
		common::Write(rStream, std::to_underlying(rMember.eType));
		rMember.flags.Write(rStream);
		common::Write(rStream, rMember.coordinate.iQ);
		common::Write(rStream, rMember.coordinate.iR);
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
	common::Read(rFileStream, rFleet.flagshipGlobalId.iValue);
	int32_t iWantedQ = 0;
	int32_t iWantedR = 0;
	common::Read(rFileStream, iWantedQ);
	common::Read(rFileStream, iWantedR);
	rFleet.wantedCoordinate = engine::GridCoord {.iQ = iWantedQ, .iR = iWantedR};
	common::Read(rFileStream, rFleet.uiPendingFleetWantedCoordinateTicks);
	float fNavigationDelay = 0.0f;
	common::Read(rFileStream, fNavigationDelay);
	rFleet.navigationDelaySeconds = std::chrono::duration<float>(fNavigationDelay);
	// Trust boundary (save / replay file): the wire admits only [0, 60], so reject any other fleet delay. It
	// resets cellChangeTimerSeconds, whose count > 0.0f test gates the fire: +Inf freezes fleet nav forever and NaN fires
	// every tick; it also feeds a common::Random bound.
	if (!IsNavigationDelayInRange(rFleet.navigationDelaySeconds.count()))
	{
		throw std::ios_base::failure("Fleet navigation delay");
	}
	float fCellChangeTimer = 0.0f;
	common::Read(rFileStream, fCellChangeTimer);
	rFleet.cellChangeTimerSeconds = std::chrono::duration<float>(fCellChangeTimer);
	// Trust boundary (save / replay file): finite-check only — cellChangeTimerSeconds legitimately goes/stays
	// negative in cross-cell mode (FleetNavigationController fires without resetting), so a range test would
	// reject valid data. A saved +Inf would freeze fleet nav forever.
	if (!std::isfinite(rFleet.cellChangeTimerSeconds.count()))
	{
		throw std::ios_base::failure("Fleet cell change timer");
	}
	// Trust boundary (save / replay file): a spawn seats its Z at the plane's height through an unchecked table lookup.
	// Not std::views::enumerate: MSVC then makes its iterator's friend iter_move ambiguous with std::ranges::iter_move in the std::ranges::sort below (C2872).
	for (int64_t i = 0; uint8_t& ruiTargetFlightPlane : rFleet.targetFlightPlanes)
	{
		common::Read(rFileStream, ruiTargetFlightPlane);
		if (!IsFlightPlaneAllowed(static_cast<ShipType>(i), ruiTargetFlightPlane))
		{
			throw std::ios_base::failure("Fleet target flight plane");
		}
		++i;
	}
	// Trust boundary (save / replay file): bound the member count against the cap and stream before resize.
	common::ValidateDeserializedCountCapacity(iMemberCount, kiMaximumFleetMembers, sizeof(int64_t) + 2 * sizeof(uint8_t) + 2 * sizeof(int32_t), rFileStream, "ReadFleet members");
	rFleet.members.resize(static_cast<size_t>(iMemberCount));
	for (FleetMember& rMember : rFleet.members)
	{
		int64_t iGlobalId = 0;
		common::Read(rFileStream, iGlobalId);
		// Trust boundary (save / replay file): a respawn spawns the member's type, so it must name a ship collection.
		uint8_t uiType = 0;
		common::Read(rFileStream, uiType);
		if (uiType >= std::to_underlying(ShipType::kCount))
		{
			throw std::ios_base::failure("Fleet member ship type");
		}
		FleetMemberFlags_t flags {};
		flags.Read(rFileStream);
		// Trust boundary (save / replay file): kIsDead is the only member flag.
		FleetMemberFlags_t unknownFlags = flags;
		unknownFlags.Set(FleetMemberFlags::kIsDead, false);
		if (std::to_underlying(unknownFlags.meFlags) != 0)
		{
			throw std::ios_base::failure("Fleet member flags");
		}
		int32_t iCoordQ = 0;
		int32_t iCoordR = 0;
		common::Read(rFileStream, iCoordQ);
		common::Read(rFileStream, iCoordR);
		rMember = FleetMember {.globalId = engine::GlobalId {.iValue = iGlobalId}, .eType = static_cast<ShipType>(uiType), .flags = flags, .coordinate = engine::GridCoord {.iQ = iCoordQ, .iR = iCoordR}};
	}
	// Trust boundary (save / replay file): the fleet must hold exactly one Battleship, named by the flagship.
	auto flagshipIt = std::ranges::find(rFleet.members, rFleet.flagshipGlobalId, &FleetMember::globalId);
	if (std::ranges::count(rFleet.members, ShipType::kBattleship, &FleetMember::eType) != 1 || flagshipIt == rFleet.members.end() || flagshipIt->eType != ShipType::kBattleship)
	{
		throw std::ios_base::failure("Fleet flagship Battleship");
	}
}

void WriteFleetData(std::ostream& rStream, const std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, const common::RandomEngine& rRandom)
{
	int64_t iFleetOwnerCount = std::ssize(rFleets);
	common::Write(rStream, iFleetOwnerCount);

	common::ScopedWorkbufferArena ownersArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const engine::ClientGuid& rGuid : SortedGuidKeys(rFleets, ownersArena))
	{
		const std::vector<Fleet>& rFleetVector = rFleets.at(rGuid);
		common::Write(rStream, rGuid.uiHigh);
		common::Write(rStream, rGuid.uiLow);
		int64_t iFleetCount = std::ssize(rFleetVector);
		common::Write(rStream, iFleetCount);
		for (const Fleet& rFleet : rFleetVector)
		{
			WriteFleet(rStream, rFleet);
		}
	}

	common::Write(rStream, rRandom.State());
}

void ReadFleetData(std::fstream& rFileStream, int64_t iNextGlobalId, std::unordered_map<engine::ClientGuid, std::vector<Fleet>>& rFleets, std::unordered_map<engine::ClientGuid, int64_t>& rGuidToClientId, common::RandomEngine& rRandom)
{
	rFleets.clear();
	rGuidToClientId.clear();

	int64_t iFleetOwnerCount = 0;
	common::Read(rFileStream, iFleetOwnerCount);
	// Trust boundary (save / replay file): bound the owner count against the stream (each owner
	// serializes at least its 16-byte GUID plus an int64 fleet count).
	common::ValidateDeserializedCount(iFleetOwnerCount, 2 * sizeof(uint64_t) + sizeof(int64_t), rFileStream, "ReadFleetData owners");
	// Trust boundary (save / replay file): members and the flagship are looked up by global ID, and the load revives
	// only the first member matching a row, so each member ID must be valid and unique across every owner's fleets;
	// it must also lie below the saved next-global-ID counter, or a later mint would reuse it.
	// Each fleet GUID must also be unique across every owner: fleet lookup takes the first match and the Fighter
	// launch timer is keyed by fleet GUID alone.
	std::unordered_set<int64_t> memberGlobalIds;
	std::unordered_set<FleetGuid> fleetGuids;
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
			if (!fleetGuids.insert(rFleet.guid).second)
			{
				throw std::ios_base::failure("duplicate Fleet FleetGuid");
			}
			for (const FleetMember& rMember : rFleet.members)
			{
				if (rMember.globalId.iValue <= 0 || rMember.globalId.iValue >= iNextGlobalId || !memberGlobalIds.insert(rMember.globalId.iValue).second)
				{
					throw std::ios_base::failure("Fleet member global ID");
				}
			}
		}
		if (!rFleets.try_emplace(guid, std::move(fleets)).second)
		{
			throw std::ios_base::failure("duplicate Fleet owner ClientGuid");
		}
		// All loaded fleets start as disconnected
		rGuidToClientId.insert_or_assign(guid, 0i64);
	}

	uint64_t uiRandomState = 0;
	common::Read(rFileStream, uiRandomState);
	rRandom.SetSerializedState(uiRandomState);
}

std::string ReplayFleetRecordBytes()
{
	const ServerFleetManager& rFleetManager = *gpServerSession->mpFleetManager;
	std::ostringstream recordStream(std::ios::binary);
	WriteFleetData(recordStream, rFleetManager.mFleets, rFleetManager.mRandomEngine);

	common::ScopedWorkbufferArena ownersArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const engine::ClientGuid& rGuid : SortedGuidKeys(rFleetManager.mFleets, ownersArena))
	{
		for (const Fleet& rFleet : rFleetManager.mFleets.at(rGuid))
		{
			for (const FleetMember& rMember : rFleet.members)
			{
				common::Write(recordStream, rMember.respawnTimerSeconds.count());
			}
		}
	}

	const std::unordered_map<FleetGuid, int64_t>& rNextFighterLaunchTicks = gpServerSession->mpClientManager->mNextFighterLaunchTicks;
	int64_t iLaunchTickCount = std::ssize(rNextFighterLaunchTicks);
	common::Write(recordStream, iLaunchTickCount);
	common::ScopedWorkbufferArena launchFleetsArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const FleetGuid& rFleetGuid : SortedGuidKeys(rNextFighterLaunchTicks, launchFleetsArena))
	{
		common::Write(recordStream, rFleetGuid.uiHigh);
		common::Write(recordStream, rFleetGuid.uiLow);
		common::Write(recordStream, rNextFighterLaunchTicks.at(rFleetGuid));
	}

	return std::move(recordStream).str();
}

void ReadReplayFleetRecord(std::fstream& rFileStream, int64_t iNextGlobalId, ReplayFleetRecord& rRecord)
{
	// A record carries no owner client IDs, so ReadFleetData's all-disconnected map is dropped.
	std::unordered_map<engine::ClientGuid, int64_t> guidToClientId;
	ReadFleetData(rFileStream, iNextGlobalId, rRecord.fleets, guidToClientId, rRecord.randomEngine);

	common::ScopedWorkbufferArena ownersArena = common::gpThreadLocal->mWorkbuffer.Push();
	for (const engine::ClientGuid& rGuid : SortedGuidKeys(rRecord.fleets, ownersArena))
	{
		for (Fleet& rFleet : rRecord.fleets.at(rGuid))
		{
			for (FleetMember& rMember : rFleet.members)
			{
				float fRespawnTimer = 0.0f;
				common::Read(rFileStream, fRespawnTimer);
				rMember.respawnTimerSeconds = std::chrono::duration<float>(fRespawnTimer);
			}
		}
	}

	int64_t iLaunchTickCount = 0;
	common::Read(rFileStream, iLaunchTickCount);
	for (int64_t i = 0; i < iLaunchTickCount; ++i)
	{
		FleetGuid fleetGuid {};
		common::Read(rFileStream, fleetGuid.uiHigh);
		common::Read(rFileStream, fleetGuid.uiLow);
		int64_t iLaunchTick = 0;
		common::Read(rFileStream, iLaunchTick);
		rRecord.nextFighterLaunchTicks.insert_or_assign(fleetGuid, iLaunchTick);
	}
}

void WriteSaveState(std::fstream& rFileStream)
{
	gpServerSession->WriteFleetData(rFileStream);
}

void ReadSaveState(std::fstream& rFileStream, int64_t iNextGlobalId, SaveStagedState& rStagedState)
{
	ReadFleetData(rFileStream, iNextGlobalId, rStagedState.fleets, rStagedState.guidToClientId, rStagedState.randomEngine);
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
