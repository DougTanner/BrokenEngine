#include "Pch.h"

#include "Agent/Commands/ServerSimulationHarnessRigs.h"

#if defined(BT_SERVER)

#include "Agent/Commands/ReplayHarnessRigs.h"
#include "File/Replay.h"
#include "Frame/FlightPlanes.h"
#include "Network/Server/ServerTransferManager.h"
#include "Network/StatusChangeFields.h"

#include "Agent/AgentCommandsServerQueries.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Network/Server/ServerFleetManager.h"
#include "Network/Server/ServerSession.h"
#include "Game.h"

namespace game
{

// iTick is the tick the change is released on, or the first later tick its coordinate can take it.
struct ScheduledStatusChange
{
	int64_t iTick = 0;
	StatusChange change;
};

struct ServerSimulationHarnessRigState
{
	const ServerSession* pSession = nullptr;
	std::unordered_map<engine::GridCoord, std::vector<ScheduledStatusChange>> pendingAgentStatusChanges;
};

static ServerSimulationHarnessRigState sHarnessRig;

static void Bind(const ServerSession& rSession)
{
	if (sHarnessRig.pSession != &rSession)
	{
		sHarnessRig = {};
		sHarnessRig.pSession = &rSession;
	}
}

static bool IsCoordinateActive(engine::GridCoord coordinate)
{
	return std::ranges::contains(gpGame->mActiveCoordinates, coordinate);
}

static void CommandReplayTransferCapture(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	engine::ReplayHarnessRigs::TransferCaptureSnapshot captureSnapshot = engine::CommandReplayTransferCapture(rParameters, rResult);
	nlohmann::json& rEvents = rResult.at("events");
	for (const auto& [i, rEvent] : std::views::enumerate(captureSnapshot.events))
	{
		const ReplayTransferCaptureCounts& rCounts = rEvent.transferCounts;
		nlohmann::json& rEventJson = rEvents.at(static_cast<size_t>(i));
		rEventJson["fighterCount"] = rCounts.iFighterCount;
		rEventJson["omniCount"] = rCounts.iOmniCount;
		rEventJson["battleshipCount"] = rCounts.iBattleshipCount;
		rEventJson["blasterCount"] = rCounts.iBlasterCount;
		rEventJson["missileCount"] = rCounts.iMissileCount;
	}
}

template <typename COLUMN, typename DATA>
static void ParseStatusChangeField(const nlohmann::json& rEntry, DATA& rData)
{
	static constexpr std::string_view kName = COLUMN::kName;
	if (!rEntry.contains(kName))
	{
		return;
	}

	using Member = std::remove_cvref_t<decltype(rData.*COLUMN::kpMember)>;
	[[maybe_unused]] Member& rMember = rData.*COLUMN::kpMember;
	const nlohmann::json& rValue = rEntry.at(std::string(kName));
	if constexpr (std::is_same_v<Member, bool>)
	{
		if (!rValue.is_boolean())
		{
			throw std::runtime_error(std::format("'{}' must be a boolean", kName));
		}
		rMember = rValue.get<bool>();
	}
	else if constexpr (std::is_same_v<Member, int64_t>)
	{
		if (!rValue.is_number_integer())
		{
			throw std::runtime_error(std::format("'{}' must be an integer", kName));
		}
		rMember = rValue.get<int64_t>();
	}
	else if constexpr (std::is_same_v<Member, uint8_t>)
	{
		if (!rValue.is_number_integer())
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,255]", kName));
		}
		int64_t iValue = rValue.get<int64_t>();
		if (!std::in_range<uint8_t>(iValue))
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,255]", kName));
		}
		rMember = static_cast<uint8_t>(iValue);
	}
	else if constexpr (std::is_same_v<Member, float>)
	{
		rMember = engine::AgentFloatValue(rValue, kName);
	}
	else if constexpr (std::is_same_v<Member, std::chrono::duration<float>>)
	{
		rMember = std::chrono::duration<float>(engine::AgentFloatValue(rValue, kName));
	}
	else if constexpr (std::is_same_v<Member, engine::GridCoord>)
	{
		rMember = engine::CoordinateFromParameter(rEntry, kName);
	}
	else if constexpr (std::is_same_v<Member, XMVECTOR>)
	{
		if (!rValue.is_array())
		{
			throw std::runtime_error(std::format("'{}' must be an [x,y] array of finite numbers", kName));
		}
		if (rValue.size() != 2)
		{
			throw std::runtime_error(std::format("'{}' must be an [x,y] array of finite numbers", kName));
		}
		// The caller supplies local XY only; the position sits on the lowest flight plane until a ship spawn seats its
		// target plane's height, and the W lane follows the position/direction invariant.
		static constexpr bool kbIsPosition = kName == "vecPosition";
		rMember = XMVectorSet(engine::AgentFloatValue(rValue.at(0), kName), engine::AgentFloatValue(rValue.at(1), kName), kbIsPosition ? engine::gpFlightPlanes->Lowest().fHeightMeters : 0.0f, kbIsPosition ? 1.0f : 0.0f);
	}
	else if constexpr (std::is_same_v<Member, engine::AlignmentIdentifier>)
	{
		if (rValue == "fleet")
		{
			rMember = gpGame->mFleetAlignment;
		}
		else if (rValue == "random")
		{
			rMember = gpGame->mRandomAlignment;
		}
		else
		{
			throw std::runtime_error(std::format("'{}' must be \"fleet\" or \"random\"", kName));
		}
	}
	else if constexpr (engine::kbIsFlags<Member>)
	{
		using Enum = decltype(rMember.meFlags);
		using Underlying = std::underlying_type_t<Enum>;
		if (!rValue.is_number_integer())
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,{}]", kName, std::numeric_limits<Underlying>::max()));
		}
		int64_t iValue = rValue.get<int64_t>();
		if (!std::in_range<Underlying>(iValue))
		{
			throw std::runtime_error(std::format("'{}' must be an integer in [0,{}]", kName, std::numeric_limits<Underlying>::max()));
		}
		rMember.meFlags = static_cast<Enum>(iValue);
	}
	else if constexpr (std::is_enum_v<Member>)
	{
		EnumFromToken(rValue, kName, rMember);
	}
	else if constexpr (std::is_same_v<Member, engine::GlobalId>)
	{
		// A ship's own global id is minted by the server; any other names an existing ship.
		if constexpr (kName == "globalId")
		{
			throw std::runtime_error(std::format("'{}' is minted by the server", kName));
		}
		else
		{
			if (!rValue.is_number_integer())
			{
				throw std::runtime_error(std::format("'{}' must be an integer", kName));
			}
			rMember.iValue = rValue.get<int64_t>();
		}
	}
	else
	{
		static_assert(false, "inject_payload has no JSON parse for this payload member type");
	}
}

// The keys of an entry are the Fields() member tokens of its payload; every other key except coord, type, and tick is
// rejected, and a member whose key is absent keeps its current value.
template <typename DATA>
static void ParseStatusChangeFields(const nlohmann::json& rEntry, DATA& rData)
{
	[&]<typename... COLUMNS>(engine::FrameColumnList<COLUMNS...>)
	{
		for (const auto& [rKey, rValue] : rEntry.items())
		{
			if (rKey != "coord" && rKey != "type" && rKey != "tick" && ((rKey != COLUMNS::kName) && ...))
			{
				throw std::runtime_error(std::format("inject_payload entry has unknown field '{}' for its type", rKey));
			}
		}
		(ParseStatusChangeField<COLUMNS>(rEntry, rData), ...);
	}(DATA::Fields());
}

template <typename COLUMN>
static void RequireEntryKey(const nlohmann::json& rEntry)
{
	if (!rEntry.contains(COLUMN::kName))
	{
		throw std::runtime_error(std::format("'{}' required", COLUMN::kName));
	}
}

static StatusChangeType StatusChangeTypeFromEntry(const nlohmann::json& rEntry)
{
	if (!rEntry.contains("type"))
	{
		throw std::runtime_error("each entry requires string 'type'");
	}
	if (!rEntry.at("type").is_string())
	{
		throw std::runtime_error("each entry requires string 'type'");
	}
	std::string type = rEntry.at("type").get<std::string>();
	for (int64_t i = 0; i < static_cast<int64_t>(StatusChangeType::kCount); ++i)
	{
		StatusChangeType eType = static_cast<StatusChangeType>(i);
		// A transfer is a cell crossing only the simulation produces.
		if (!IsTransferType(eType) && type == StatusChangeTypeName(eType))
		{
			return eType;
		}
	}
	throw std::runtime_error("'type' must be SpawnFighter|SpawnOmni|SpawnBattleship|SpawnBlaster|SpawnMissile|SpawnExplosion|DestroyShip|UpdateShip|UpdateFleet");
}

static void ValidateInjectedPosition(FXMVECTOR vecPosition)
{
	if (!common::InsideArea(vecPosition, engine::LocalCellArea()))
	{
		throw std::runtime_error("inject_payload position must lie inside the cell");
	}
}

// Seats the spawn defaults, parses the entry over them, and rejects an outside position, which QueueSpawn asserts on,
// and a ship navigation delay the frame read would reject.
template <typename SPAWN_INFO>
static SPAWN_INFO& ParseSpawnEntry(const nlohmann::json& rEntry, StatusChange& rChange, [[maybe_unused]] engine::GridCoord coordinate)
{
	SPAWN_INFO& rData = std::get<SPAWN_INFO>(rChange.data);
	rData.vecPosition = XMVectorSet(0.0f, 0.0f, engine::gpFlightPlanes->Lowest().fHeightMeters, 1.0f);
	if constexpr (requires { rData.vecDirection; })
	{
		rData.vecDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if constexpr (requires { rData.vecVelocity; })
	{
		rData.vecVelocity = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if constexpr (requires { rData.vecStoredDirection; })
	{
		rData.vecStoredDirection = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	if constexpr (requires { rData.alignment; })
	{
		rData.alignment = gpGame->mFleetAlignment;
	}
	if constexpr (requires { rData.fleetWantedCoordinate; })
	{
		rData.fleetWantedCoordinate = coordinate;
	}
	ParseStatusChangeFields(rEntry, rData);
	ValidateInjectedPosition(rData.vecPosition);
	if constexpr (requires { rData.navigationDelay; })
	{
		if (!IsNavigationDelayInRange(rData.navigationDelay.count()))
		{
			throw std::runtime_error(std::format("'{}' must be within [0,60]", FRAME_COLUMN(SPAWN_INFO, navigationDelay)::kName));
		}
	}
	return rData;
}

// The plane is tested before its height is read, because the plane lookup is unchecked and the orders byte can name
// any index up to 255.
template <typename SPAWN_INFO>
static void SeatShipSpawnHeight(SPAWN_INFO& rData, ShipType eType)
{
	int64_t iFlightPlane = GetTargetFlightPlane(rData.orders);
	if (!IsFlightPlaneAllowed(eType, iFlightPlane))
	{
		throw std::runtime_error(std::format("'{}' target flight plane is not allowed for {}", FRAME_COLUMN(SPAWN_INFO, orders)::kName, EnumToken(eType)));
	}
	rData.vecPosition = XMVectorSetZ(rData.vecPosition, engine::gpFlightPlanes->Plane(iFlightPlane).fHeightMeters);
	rData.uiFlightPlane = static_cast<uint8_t>(iFlightPlane);
}

// Validates one entry without touching any queue.
static std::pair<engine::GridCoord, ScheduledStatusChange> BuildInjectedEntry(const nlohmann::json& rEntry)
{
	if (!rEntry.is_object())
	{
		throw std::runtime_error("each inject_payload entry must be an object");
	}
	engine::GridCoord coordinate = engine::CoordinateFromParameter(rEntry);
	StatusChangeType eType = StatusChangeTypeFromEntry(rEntry);

	ScheduledStatusChange scheduled {.iTick = gpGame->miTickCounter + 1, .change = {.eType = eType, .data = DefaultDataForType(eType)}};
	if (rEntry.contains("tick"))
	{
		if (!rEntry.at("tick").is_number_integer())
		{
			throw std::runtime_error("'tick' must be an integer");
		}
		scheduled.iTick = rEntry.at("tick").get<int64_t>();
		if (scheduled.iTick <= gpGame->miTickCounter)
		{
			throw std::runtime_error("'tick' must be greater than the current status.tick");
		}
	}

	if (!IsCoordinateActive(coordinate))
	{
		throw std::runtime_error("status-change entry 'coord' is not active");
	}

	switch (eType)
	{
		case StatusChangeType::kSpawnFighter:
			SeatShipSpawnHeight(ParseSpawnEntry<FightersPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate), ShipType::kFighter);
			break;
		case StatusChangeType::kSpawnOmni:
		{
			OmnisPostRender::SpawnInfo& rData = ParseSpawnEntry<OmnisPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			SeatShipSpawnHeight(rData, ShipType::kOmni);
			// An omni coasts under a transfer lock the entry may set. A velocity that crosses a whole cell in one tick
			// outruns the one-cell transfer delta and asserts at the destination's Spawn, so cap it at the speed
			// navigation never exceeds.
			if (!(XMVectorGetX(XMVector2LengthSq(rData.vecVelocity)) <= kfOmniMaximumSpeed * kfOmniMaximumSpeed))
			{
				throw std::runtime_error(std::format("SpawnOmni '{}' must not exceed the maximum omni speed", FRAME_COLUMN(OmnisPostRender::SpawnInfo, vecVelocity)::kName));
			}
			if (!OmnisPostRender::IsBlasterFireTimeInRange(rData.nextBlasterFireTime.count()))
			{
				throw std::runtime_error(std::format("'{}' is out of range", FRAME_COLUMN(OmnisPostRender::SpawnInfo, nextBlasterFireTime)::kName));
			}
			break;
		}
		case StatusChangeType::kSpawnBattleship:
			SeatShipSpawnHeight(ParseSpawnEntry<BattleshipsPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate), ShipType::kBattleship);
			break;
		case StatusChangeType::kSpawnBlaster:
			ParseSpawnEntry<BlastersPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			if (!IsAdoptableStatusChange(scheduled.change))
			{
				throw std::runtime_error(std::format("'{}' is not a registered Blaster type", FRAME_COLUMN(BlastersPostRender::SpawnInfo, iTypeIndex)::kName));
			}
			break;
		case StatusChangeType::kSpawnMissile:
		{
			MissilesPostRender::SpawnInfo& rData = ParseSpawnEntry<MissilesPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			// Without explicit flags the missile homes on the other alignment.
			if (!rEntry.contains(FRAME_COLUMN(MissilesPostRender::SpawnInfo, flags)::kName))
			{
				rData.flags = (rData.alignment == gpGame->mFleetAlignment) ? MissileFlags::kTargetRandom : MissileFlags::kTargetFleet;
			}
			break;
		}
		case StatusChangeType::kSpawnExplosion:
		{
			const engine::ExplosionsPostRender::SpawnInfo& rData = ParseSpawnEntry<engine::ExplosionsPostRender::SpawnInfo>(rEntry, scheduled.change, coordinate);
			if (!IsAdoptableStatusChange(scheduled.change))
			{
				throw std::runtime_error(std::format("'{}' is not a registered Explosion type", FRAME_COLUMN(engine::ExplosionsPostRender::SpawnInfo, iTypeIndex)::kName));
			}
			// Spawn caps only the trail count's upper end; the explosion read check of a later save or replay rejects a
			// negative count or a time percent outside [0,1].
			if (rData.iTrailCount < 0)
			{
				throw std::runtime_error(std::format("'{}' must not be negative", FRAME_COLUMN(engine::ExplosionsPostRender::SpawnInfo, iTrailCount)::kName));
			}
			if (!(rData.fTimePercent >= 0.0f && rData.fTimePercent <= 1.0f))
			{
				throw std::runtime_error(std::format("'{}' must be within [0,1]", FRAME_COLUMN(engine::ExplosionsPostRender::SpawnInfo, fTimePercent)::kName));
			}
			break;
		}
		case StatusChangeType::kDestroyShip:
		{
			DestroyShipData& rData = std::get<DestroyShipData>(scheduled.change.data);
			RequireEntryKey<FRAME_COLUMN(DestroyShipData, eType)>(rEntry);
			RequireEntryKey<FRAME_COLUMN(DestroyShipData, iUuid)>(rEntry);
			ParseStatusChangeFields(rEntry, rData);
			// A fleet Battleship is permanent, so its removal is refused.
			if (rData.eType == ShipType::kBattleship)
			{
				const Frame& rFrame = QueryFrame(rEntry);
				auto it = rFrame.interpolate.pBattleships->idToIndexMap.find(BattleshipsInterpolate::id_t(engine::Uuid(rData.iUuid)));
				if (it != rFrame.interpolate.pBattleships->idToIndexMap.end() && rFrame.postRender.pBattleships->pController[it->second] == ShipController::kFleet)
				{
					throw std::runtime_error(std::format("DestroyShip '{}' {} is a fleet Battleship and cannot be destroyed", FRAME_COLUMN(DestroyShipData, iUuid)::kName, rData.iUuid));
				}
			}
			break;
		}
		case StatusChangeType::kUpdateShip:
		{
			UpdateShipData& rData = std::get<UpdateShipData>(scheduled.change.data);
			rData.uiPendingWeaponModeTicks = static_cast<uint8_t>(engine::kiTickRate);
			RequireEntryKey<FRAME_COLUMN(UpdateShipData, eType)>(rEntry);
			RequireEntryKey<FRAME_COLUMN(UpdateShipData, iUuid)>(rEntry);
			ParseStatusChangeFields(rEntry, rData);
			// Rejects a delay the wire would reject, so an injected ship never holds an out-of-range delay.
			if (!IsNavigationDelayInRange(rData.navigationDelaySeconds.count()))
			{
				throw std::runtime_error(std::format("'{}' must be within [0,60]", FRAME_COLUMN(UpdateShipData, navigationDelaySeconds)::kName));
			}
			// eType parsed as a token, so only the target flight plane can fail here.
			if (!IsAdoptableStatusChange(scheduled.change))
			{
				throw std::runtime_error(std::format("'{}' is not allowed for {}", FRAME_COLUMN(UpdateShipData, uiTargetFlightPlane)::kName, EnumToken(rData.eType)));
			}
			break;
		}
		default:
		{
			UpdateFleetData& rData = std::get<UpdateFleetData>(scheduled.change.data);
			rData.uiPendingFleetWantedCoordinateTicks = static_cast<uint8_t>(engine::kiTickRate);
			RequireEntryKey<FRAME_COLUMN(UpdateFleetData, eType)>(rEntry);
			RequireEntryKey<FRAME_COLUMN(UpdateFleetData, iUuid)>(rEntry);
			RequireEntryKey<FRAME_COLUMN(UpdateFleetData, fleetWantedCoordinate)>(rEntry);
			ParseStatusChangeFields(rEntry, rData);
			break;
		}
	}
	return {coordinate, std::move(scheduled)};
}

static void CommandInjectPayload(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	for (const auto& [rKey, rValue] : rParameters.items())
	{
		if (rKey != "entries" && rKey != "pauseAfterWriterInput")
		{
			throw std::runtime_error("inject_payload unknown parameter '" + rKey + "'");
		}
	}
	if (!rParameters.contains("entries"))
	{
		throw std::runtime_error("inject_payload requires non-empty array 'entries'");
	}
	if (!rParameters.at("entries").is_array())
	{
		throw std::runtime_error("inject_payload requires non-empty array 'entries'");
	}
	if (rParameters.at("entries").empty())
	{
		throw std::runtime_error("inject_payload requires non-empty array 'entries'");
	}

	bool bPauseAfterWriterInputPresent = rParameters.contains("pauseAfterWriterInput");
	bool bPauseAfterWriterInput = false;
	if (bPauseAfterWriterInputPresent)
	{
		if (!rParameters.at("pauseAfterWriterInput").is_boolean())
		{
			throw std::runtime_error("'pauseAfterWriterInput' must be bool");
		}
		if constexpr (!kbDebugInput)
		{
			throw std::runtime_error("'pauseAfterWriterInput' requires kbDebugInput build");
		}
		bPauseAfterWriterInput = rParameters.at("pauseAfterWriterInput").get<bool>();
	}

	// Replay playback loads recorded mFrameInputs through SyncReplayTick's LoadDifference; live injection is incompatible with simulation and broadcasts.
	if (gpGame->mbReplaying)
	{
		throw std::runtime_error("cannot inject during replay playback");
	}
	const nlohmann::json& rEntries = rParameters.at("entries");

	// Validate the whole batch before any queue mutation or global id is minted.
	std::vector<std::pair<engine::GridCoord, ScheduledStatusChange>> built;
	built.reserve(rEntries.size());
	bool bHasSpawn = false;
	bool bHasTick = false;
	for (const nlohmann::json& rEntry : rEntries)
	{
		const std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry = built.emplace_back(BuildInjectedEntry(rEntry));
		bHasSpawn = bHasSpawn || IsSpawnType(rBuiltEntry.second.change.eType);
		bHasTick = bHasTick || rEntry.contains("tick");
	}
	bool bPendingStart = (gpGame->mGameFlags & engine::GameFlags::kPaused) && (gpGame->mGameFlags & engine::GameFlags::kSaveReplay)
	                  && engine::gpReplay->mReplayWriters.empty();
	if (bPauseAfterWriterInput)
	{
		if (!bHasSpawn)
		{
			throw std::runtime_error("pauseAfterWriterInput requires a spawn entry");
		}
		if (engine::gpReplay->mReplayWriters.empty() && !bPendingStart)
		{
			throw std::runtime_error("pauseAfterWriterInput requires active recording or a paused pending recording start");
		}
		if (engine::ReplayHarnessRigs::IsWriterPauseArmed(*engine::gpReplay))
		{
			throw std::runtime_error("pauseAfterWriterInput is already armed");
		}
		// The writer-input pause assumes the batch enters the next tick.
		if (bHasTick)
		{
			throw std::runtime_error("pauseAfterWriterInput cannot be combined with entry 'tick'");
		}
	}

	// Ids are reserved here and committed to miNextGlobalId only with the queue.
	int64_t iNextGlobalId = gpGame->miNextGlobalId;
	nlohmann::json globalIds = nlohmann::json::array();
	for (std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry : built)
	{
		// A fleet ship spawn needs a global id; a random-spawn ship keeps 0, as the director's do.
		std::visit([&](auto& rData)
		{
			if constexpr (requires { rData.eController; })
			{
				if (rData.eController == ShipController::kFleet)
				{
					int64_t iGlobalId = iNextGlobalId++;
					rData.globalId = engine::GlobalId {.iValue = iGlobalId};
					globalIds.push_back(iGlobalId);
				}
			}
		}, rBuiltEntry.second.change.data);
	}
	if (bPauseAfterWriterInputPresent)
	{
		rResult["pauseAfterWriterInput"] = bPauseAfterWriterInput;
	}

	gpGame->miNextGlobalId = iNextGlobalId;
	for (const std::pair<engine::GridCoord, ScheduledStatusChange>& rBuiltEntry : built)
	{
		const auto& [rCoordinate, rScheduled] = rBuiltEntry;
		// A spawn is applied right after its target tick, with that tick's arrivals; any other entry enters its target
		// tick's input.
		if (IsSpawnType(rScheduled.change.eType))
		{
			gpServerSession->mpTransferManager->QueueSpawn(rCoordinate, rScheduled.iTick, rScheduled.change);
		}
		else
		{
			QueueAgentStatusChange(*gpServerSession, rCoordinate, rScheduled.iTick, rScheduled.change);
		}
	}
	if (bPauseAfterWriterInput)
	{
		engine::ReplayHarnessRigs::ArmPauseAfterNextWriterInput(*engine::gpReplay, bPendingStart);
	}
	rResult["injected"] = std::ssize(built);
	rResult["globalIds"] = std::move(globalIds);
	rResult["deferred"] = static_cast<bool>(gpGame->mGameFlags & engine::GameFlags::kPaused);
}

// Writes only the fleet's per-type plane, which its later spawns of that type take; live members keep their planes.
static void CommandSetFleetFlightPlane(const nlohmann::json& rParameters)
{
	// Fleet state is not tick input, so a write during playback would be missing from the recording.
	if (gpGame->mbReplaying)
	{
		throw std::runtime_error("cannot set fleet flight plane during replay playback");
	}
	for (const auto& [rKey, rValue] : rParameters.items())
	{
		if (rKey != "globalId" && rKey != "type" && rKey != "plane")
		{
			throw std::runtime_error("set_fleet_flight_plane unknown parameter '" + rKey + "'");
		}
	}
	if (!rParameters.contains("globalId") || !rParameters.at("globalId").is_number_integer())
	{
		throw std::runtime_error("set_fleet_flight_plane requires integer 'globalId'");
	}
	ShipType eType {};
	EnumFromToken(rParameters.contains("type") ? rParameters.at("type") : nlohmann::json(), "type", eType);
	if (!rParameters.contains("plane") || !rParameters.at("plane").is_number_integer())
	{
		throw std::runtime_error("set_fleet_flight_plane requires integer 'plane'");
	}
	int64_t iFlightPlane = rParameters.at("plane").get<int64_t>();
	if (!IsFlightPlaneAllowed(eType, iFlightPlane))
	{
		throw std::runtime_error(std::format("'plane' is not allowed for {}", EnumToken(eType)));
	}

	engine::GlobalId globalId {.iValue = rParameters.at("globalId").get<int64_t>()};
	for (auto& [rClientGuid, rFleets] : gpServerSession->mpFleetManager->mFleets)
	{
		for (Fleet& rFleet : rFleets)
		{
			if (std::ranges::contains(rFleet.members, globalId, &FleetMember::globalId))
			{
				rFleet.targetFlightPlanes[std::to_underlying(eType)] = static_cast<uint8_t>(iFlightPlane);
				return;
			}
		}
	}
	throw std::runtime_error("no fleet contains globalId");
}

bool ExecuteServerSimulationHarnessRigCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (command != "replay_transfer_capture" && command != "inject_payload" && command != "set_fleet_flight_plane")
	{
		return false;
	}
	Bind(*gpServerSession);
	if (command == "replay_transfer_capture")
	{
		CommandReplayTransferCapture(rParameters, rResult);
		return true;
	}
	if (command == "inject_payload")
	{
		CommandInjectPayload(rParameters, rResult);
		return true;
	}
	if (command == "set_fleet_flight_plane")
	{
		CommandSetFleetFlightPlane(rParameters);
		return true;
	}
	return false;
}

void QueueAgentStatusChange(const ServerSession& rSession, engine::GridCoord coordinate, int64_t iTick, const StatusChange& rChange)
{
	// Runs at the agent command drain point under AgentCommandServer::Drain's allocation suppression.
	Bind(rSession);
	sHarnessRig.pendingAgentStatusChanges.try_emplace(coordinate).first->second.push_back({.iTick = iTick, .change = rChange});
}

void DrainPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sHarnessRig.pSession != &rSession)
	{
		return;
	}

	int64_t iTick = gpGame->miTickCounter;
	auto IsReleased = [iTick](const ScheduledStatusChange& rScheduled)
	{
		return rScheduled.iTick <= iTick;
	};
	for (auto it = sHarnessRig.pendingAgentStatusChanges.begin(); it != sHarnessRig.pendingAgentStatusChanges.end();)
	{
		const engine::GridCoord& rCoordinate = it->first;
		if (!IsCoordinateActive(rCoordinate))
		{
			++it;
			continue;
		}
		std::vector<ScheduledStatusChange>& rScheduledChanges = it->second;
		if (std::ranges::none_of(rScheduledChanges, IsReleased))
		{
			++it;
			continue;
		}
		std::vector<StatusChange>& rStatusChanges = gpGame->mFrameInputs.try_emplace(rCoordinate).first->second.statusChanges;
		for (const ScheduledStatusChange& rScheduled : rScheduledChanges)
		{
			if (IsReleased(rScheduled))
			{
				rStatusChanges.push_back(rScheduled.change);
			}
		}
		for (auto itStatusChange = rStatusChanges.begin(); itStatusChange != rStatusChanges.end(); ++itStatusChange)
		{
			auto itInsertion = std::upper_bound(rStatusChanges.begin(), itStatusChange, *itStatusChange, [](const StatusChange& rLeft, const StatusChange& rRight)
			{
				return rLeft.eType < rRight.eType;
			});
			std::rotate(itInsertion, itStatusChange, std::next(itStatusChange));
		}
		std::erase_if(rScheduledChanges, IsReleased);
		it = rScheduledChanges.empty() ? sHarnessRig.pendingAgentStatusChanges.erase(it) : std::next(it);
	}
}

void ResetPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sHarnessRig.pSession == &rSession)
	{
		sHarnessRig.pendingAgentStatusChanges.clear();
	}
}

int64_t CountPendingAgentStatusChanges(const ServerSession& rSession)
{
	if (sHarnessRig.pSession != &rSession)
	{
		return 0;
	}

	int64_t iCount = 0;
	for (const auto& [rCoordinate, rChanges] : sHarnessRig.pendingAgentStatusChanges)
	{
		iCount += std::ssize(rChanges);
	}
	return iCount;
}

void DetachServerSimulationHarnessRigs(const ServerSession& rSession)
{
	if (sHarnessRig.pSession == &rSession)
	{
		sHarnessRig = {};
	}
}

void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts)
{
	for (const StatusChange& rTransfer : transfers)
	{
		switch (rTransfer.eType)
		{
			case StatusChangeType::kSpawnFighter:
			case StatusChangeType::kTransferFighter: ++rCounts.iFighterCount; break;
			case StatusChangeType::kSpawnOmni:
			case StatusChangeType::kTransferOmni: ++rCounts.iOmniCount; break;
			case StatusChangeType::kSpawnBattleship:
			case StatusChangeType::kTransferBattleship: ++rCounts.iBattleshipCount; break;
			case StatusChangeType::kSpawnBlaster:
			case StatusChangeType::kTransferBlaster: ++rCounts.iBlasterCount; break;
			case StatusChangeType::kSpawnMissile:
			case StatusChangeType::kTransferMissile: ++rCounts.iMissileCount; break;
			// The capture counts have no explosion field.
			case StatusChangeType::kSpawnExplosion: break;
			default: DEBUG_BREAK(); break;
		}
	}
}

} // namespace game

#endif // BT_SERVER
