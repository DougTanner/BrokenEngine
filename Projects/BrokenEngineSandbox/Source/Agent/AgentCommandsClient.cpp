#include "Agent/AgentCommands.h"

#if defined(BT_CLIENT)

#include "Agent/AgentScene.h"
#include "Game.h"
#include "Network/Client/ClientSession.h"

namespace game
{

namespace
{

nlohmann::json BuildFullStateFixtureCoordState(engine::GridCoord coord)
{
	nlohmann::json result;
	result["coord"] = {coord.x, coord.y};

	auto coordIt = gpGame->mCoordFrames.find(coord);
	if (coordIt == gpGame->mCoordFrames.end())
	{
		result["present"] = false;
		return result;
	}

	const engine::CoordFrames& rFrames = coordIt->second;
	result["present"] = true;
	result["confirmedTick"] = rFrames.iConfirmedTick;
	result["confirmedOffset"] = rFrames.iConfirmedOffset;
	result["highWaterValidatedTick"] = rFrames.iHighWaterValidatedTick;
	result["lastFullStateTick"] = rFrames.iLastFullStateTick;
	result["snapshotHead"] = rFrames.iSnapshotHead;
	result["snapshotCount"] = rFrames.iSnapshotCount;
	result["lastRenderedTick"] = rFrames.iLastRenderedTick;
	result["lastRenderedTime"] = rFrames.fLastRenderedTime;
	result["serverUpdateCount"] = rFrames.serverUpdates.size();
	result["lastReplayConfirmedTick"] = rFrames.iLastReplayConfirmedTick;
	result["lastReplayServerUpdateCount"] = rFrames.iLastReplayServerUpdateCount;
	result["stuckFrameCount"] = rFrames.iStuckFrameCount;

	result["pendingFullStateTick"] = nullptr;
	if (rFrames.pendingFullState.has_value())
	{
		result["pendingFullStateTick"] = rFrames.pendingFullState->iTick;
	}

	result["firstServerUpdateTick"] = nullptr;
	result["lastServerUpdateTick"] = nullptr;
	if (!rFrames.serverUpdates.empty())
	{
		result["firstServerUpdateTick"] = rFrames.serverUpdates.begin()->first;
		result["lastServerUpdateTick"] = rFrames.serverUpdates.rbegin()->first;
	}

	bool bRingValid = rFrames.iSnapshotHead >= 0 && rFrames.iSnapshotHead < engine::kiNetworkBufferSize
		&& rFrames.iSnapshotCount >= 0 && rFrames.iSnapshotCount <= engine::kiNetworkBufferSize;
	int64_t iPreviousTick = -1;
	for (int64_t i = 0; bRingValid && i < rFrames.iSnapshotCount; ++i)
	{
		int64_t iPhysical = engine::SnapshotIndex(rFrames.iSnapshotHead, i);
		const std::unique_ptr<Frame>& pSnapshot = rFrames.snapshots[iPhysical];
		bRingValid = pSnapshot != nullptr && (i == 0 || pSnapshot->interpolate.iTick == iPreviousTick + 1);
		if (pSnapshot != nullptr)
		{
			iPreviousTick = pSnapshot->interpolate.iTick;
		}
	}
	if (rFrames.iConfirmedTick >= 0)
	{
		bRingValid = bRingValid && rFrames.iConfirmedOffset >= 0 && rFrames.iConfirmedOffset < rFrames.iSnapshotCount;
		if (bRingValid)
		{
			int64_t iConfirmedPhysical = engine::SnapshotIndex(rFrames.iSnapshotHead, rFrames.iConfirmedOffset);
			bRingValid = rFrames.snapshots[iConfirmedPhysical] != nullptr
				&& rFrames.snapshots[iConfirmedPhysical]->interpolate.iTick == rFrames.iConfirmedTick;
		}
	}
	result["ringValid"] = bRingValid;
	result["tailTick"] = rFrames.iSnapshotCount > 0 ? iPreviousTick : -1;
	return result;
}

nlohmann::json BuildFullStateFixtureState()
{
	engine::ClientDesyncCore& rDesyncCore = *gpClientSession->mpDesyncCore;
	nlohmann::json result;
	result["clientTick"] = gpGame->TickCounter();
	result["stalled"] = gpClientSession->mpDesyncCore->IsStalled();
	result["desyncTick"] = gpClientSession->mpDesyncCore->GetDesyncTick();
	result["syntheticStall"] = rDesyncCore.IsAgentFullStateFixtureArmed();
	result["armedTick"] = rDesyncCore.GetAgentFullStateFixtureTick();
	result["timeMultiply"] = gpGame->mTimeStep.miTimeMultiply;
	result["timeDivide"] = gpGame->mTimeStep.miTimeDivide;
	result["coordState"] = BuildFullStateFixtureCoordState(rDesyncCore.GetAgentFullStateFixtureCoord());
	return result;
}

// Drives the matching-tick injection case: contiguous updates make the pending full-state tick the
// replay endpoint, so the injected frame must end up as the committed ring head. rbClockForced
// reports whether the client clock was moved before a throw, which decides whether the caller can
// leave the fixture armed for a retry.
void ExerciseFullStateMatchingTick(engine::GridCoord coord, nlohmann::json& rResult, bool& rbClockForced)
{
	auto coordIt = gpGame->mCoordFrames.find(coord);
	if (coordIt == gpGame->mCoordFrames.end() || !coordIt->second.pendingFullState.has_value())
	{
		throw std::runtime_error("client_full_state_fixture requires a received pending full state");
	}

	engine::CoordFrames& rFrames = coordIt->second;
	const int64_t iPendingTick = rFrames.pendingFullState->iTick;
	const int64_t iDeferTargetTick = gpGame->TickCounter();
	if (iPendingTick <= iDeferTargetTick)
	{
		throw std::runtime_error("client_full_state_fixture requires a pending full state ahead of client tick");
	}

	nlohmann::json beforeDefer = BuildFullStateFixtureCoordState(coord);
	engine::ReconcileDesyncInfo deferDesync = gpClientSession->mpReconciler->Run();
	bool bPendingPreserved = rFrames.pendingFullState.has_value() && rFrames.pendingFullState->iTick == iPendingTick;
	nlohmann::json afterDefer = BuildFullStateFixtureCoordState(coord);
	if (!bPendingPreserved)
	{
		throw std::runtime_error("future pending full state was not deferred");
	}

	// A gap before the pending tick would route to direct adoption instead; the caller retries once
	// the missing updates arrive.
	for (int64_t iTick = rFrames.iConfirmedTick + 1; iTick <= iPendingTick; ++iTick)
	{
		if (!rFrames.serverUpdates.contains(iTick))
		{
			throw std::runtime_error("client_full_state_fixture requires contiguous server updates through the pending full state tick");
		}
	}

	const float fPendingTime = rFrames.pendingFullState->pFrame->interpolate.fCurrentTime;
	rbClockForced = true;
	gpGame->SetTickCounter(iPendingTick);
	gpGame->SetCurrentTime(fPendingTime);
	gpGame->mTimeStep.ClearAccumulator();
	engine::ReconcileDesyncInfo injectionDesync = gpClientSession->mpReconciler->Run();

	nlohmann::json afterInjection = BuildFullStateFixtureCoordState(coord);
	bool bPendingCleared = !rFrames.pendingFullState.has_value();
	int64_t iHeadTick = -1;
	if (rFrames.iSnapshotCount > 0 && rFrames.snapshots[rFrames.iSnapshotHead] != nullptr)
	{
		iHeadTick = rFrames.snapshots[rFrames.iSnapshotHead]->interpolate.iTick;
	}

	rResult["pendingTick"] = iPendingTick;
	rResult["deferTargetTick"] = iDeferTargetTick;
	rResult["beforeDefer"] = std::move(beforeDefer);
	rResult["afterDefer"] = std::move(afterDefer);
	rResult["deferPendingPreserved"] = bPendingPreserved;
	rResult["deferDesync"] = deferDesync.bDesync;
	rResult["injectionDesync"] = injectionDesync.bDesync;
	rResult["afterInjection"] = std::move(afterInjection);
	rResult["pendingCleared"] = bPendingCleared;
	rResult["headTick"] = iHeadTick;
	rResult["confirmedTick"] = rFrames.iConfirmedTick;
	rResult["confirmedOffset"] = rFrames.iConfirmedOffset;
}

void CommandClientFullStateFixture(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!engine::PhysicalInputSuppressed())
	{
		throw std::runtime_error("client_full_state_fixture requires an agent-mode client");
	}
	if (!rParams.is_object() || !rParams.contains("action") || !rParams.at("action").is_string() || rParams.size() != 1)
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}

	const std::string action = rParams.at("action").get<std::string>();
	engine::ClientDesyncCore& rDesyncCore = *gpClientSession->mpDesyncCore;
	if (action == "clear")
	{
		rDesyncCore.ClearAgentFullStateFixture();
		rResult = BuildFullStateFixtureState();
		return;
	}

	if (action == "arm_stall")
	{
		if (gpClientSession->mpRuntime->mpClient == nullptr || !(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnectionAccepted) || !(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected) || gpClientSession->mpRuntime->mpClient->mpServerPeer == nullptr)
		{
			throw std::runtime_error("client_full_state_fixture requires an accepted connection");
		}
		if (gpClientSession->mpDesyncCore->IsStalled())
		{
			throw std::runtime_error("client_full_state_fixture is already stalled");
		}

		const engine::GridCoord coord = gpGame->mClientGridCoord;
		bool bActive = std::ranges::any_of(gpClientSession->mpRuntime->mpClient->mCoordSlots, [coord](const engine::ClientCoordSlot& rSlot)
		{
			return rSlot.eState == engine::CoordSubscriptionState::kActive && rSlot.coord == coord;
		});
		auto coordIt = gpGame->mCoordFrames.find(coord);
		if (!bActive || coordIt == gpGame->mCoordFrames.end() || coordIt->second.iConfirmedTick < 0)
		{
			throw std::runtime_error("client_full_state_fixture requires an active confirmed client coord");
		}
		if (coordIt->second.pendingFullState.has_value())
		{
			throw std::runtime_error("client_full_state_fixture requires no pre-existing pending full state");
		}

		rDesyncCore.ArmAgentFullStateFixture(gpGame->TickCounter(), coord);
		rResult = BuildFullStateFixtureState();
		return;
	}

	if (!rDesyncCore.IsAgentFullStateFixtureArmed())
	{
		throw std::runtime_error("client_full_state_fixture is not armed");
	}
	if (action == "inspect")
	{
		rResult = BuildFullStateFixtureState();
		return;
	}
	if (action == "exercise_matching_tick")
	{
		bool bClockForced = false;
		try
		{
			ExerciseFullStateMatchingTick(rDesyncCore.GetAgentFullStateFixtureCoord(), rResult, bClockForced);
		}
		catch (...)
		{
			// A precondition failure leaves the client untouched apart from reconciles it would run
			// anyway, so keep the fixture armed and let the caller retry; 'clear' still releases it.
			if (bClockForced)
			{
				rDesyncCore.ClearAgentFullStateFixture();
			}
			throw;
		}

		rDesyncCore.ClearAgentFullStateFixture();
		rResult["cleared"] = true;
		return;
	}
	if (action != "exercise_gap")
	{
		throw std::runtime_error("client_full_state_fixture 'action' must be arm_stall|inspect|exercise_gap|exercise_matching_tick|clear");
	}

	try
	{
		const engine::GridCoord coord = rDesyncCore.GetAgentFullStateFixtureCoord();
		auto coordIt = gpGame->mCoordFrames.find(coord);
		if (coordIt == gpGame->mCoordFrames.end() || !coordIt->second.pendingFullState.has_value())
		{
			throw std::runtime_error("client_full_state_fixture requires a received pending full state");
		}

		engine::CoordFrames& rFrames = coordIt->second;
		const int64_t iPendingTick = rFrames.pendingFullState->iTick;
		const int64_t iDeferTargetTick = gpGame->TickCounter();
		if (iPendingTick <= iDeferTargetTick)
		{
			throw std::runtime_error("client_full_state_fixture requires a pending full state ahead of client tick");
		}

		nlohmann::json beforeDefer = BuildFullStateFixtureCoordState(coord);
		engine::ReconcileDesyncInfo deferDesync = gpClientSession->mpReconciler->Run();
		bool bPendingPreserved = rFrames.pendingFullState.has_value() && rFrames.pendingFullState->iTick == iPendingTick;
		nlohmann::json afterDefer = BuildFullStateFixtureCoordState(coord);
		if (!bPendingPreserved)
		{
			throw std::runtime_error("future pending full state was not deferred");
		}

		const int64_t iConfirmedBeforeGap = rFrames.iConfirmedTick;
		auto eraseBegin = rFrames.serverUpdates.upper_bound(iConfirmedBeforeGap);
		auto eraseEnd = rFrames.serverUpdates.upper_bound(iPendingTick);
		const int64_t iRemovedUpdateCount = std::distance(eraseBegin, eraseEnd);
		rFrames.serverUpdates.erase(eraseBegin, eraseEnd);
		int64_t iUncappedConsecutiveEndpoint = iConfirmedBeforeGap;
		while (rFrames.serverUpdates.contains(iUncappedConsecutiveEndpoint + 1))
		{
			++iUncappedConsecutiveEndpoint;
		}
		const bool bDirectAdoptionRequired = iPendingTick > iUncappedConsecutiveEndpoint;
		if (!bDirectAdoptionRequired)
		{
			throw std::runtime_error("client_full_state_fixture failed to create an update gap before pending full state");
		}

		const float fPendingTime = rFrames.pendingFullState->pFrame->interpolate.fCurrentTime;
		gpGame->SetTickCounter(iPendingTick);
		gpGame->SetCurrentTime(fPendingTime);
		gpGame->mTimeStep.ClearAccumulator();
		engine::ReconcileDesyncInfo adoptionDesync = gpClientSession->mpReconciler->Run();

		nlohmann::json afterAdoption = BuildFullStateFixtureCoordState(coord);
		bool bObsoleteUpdatesAbsent = rFrames.serverUpdates.empty() || rFrames.serverUpdates.begin()->first > iPendingTick;
		bool bRenderBaseNotOlder = rFrames.iLastRenderedTick < 0 || rFrames.iSnapshotCount <= 0
			|| rFrames.snapshots[rFrames.iSnapshotHead]->interpolate.iTick >= rFrames.iLastRenderedTick;
		bool bPendingCleared = !rFrames.pendingFullState.has_value();
		bool bAdoptedTicksMatch = rFrames.iConfirmedTick == iPendingTick
			&& rFrames.iHighWaterValidatedTick == iPendingTick
			&& rFrames.iLastFullStateTick == iPendingTick;
		bool bRingHeadIsAdopted = rFrames.iSnapshotCount > 0
			&& rFrames.snapshots[rFrames.iSnapshotHead] != nullptr
			&& rFrames.snapshots[rFrames.iSnapshotHead]->interpolate.iTick == iPendingTick;
		bool bDirectAdoptionProven = bDirectAdoptionRequired && !adoptionDesync.bDesync && bPendingCleared
			&& bAdoptedTicksMatch && rFrames.iConfirmedOffset == 0 && bObsoleteUpdatesAbsent && bRingHeadIsAdopted;

		rResult["pendingTick"] = iPendingTick;
		rResult["deferTargetTick"] = iDeferTargetTick;
		rResult["beforeDefer"] = std::move(beforeDefer);
		rResult["afterDefer"] = std::move(afterDefer);
		rResult["deferPendingPreserved"] = bPendingPreserved;
		rResult["deferDesync"] = deferDesync.bDesync;
		rResult["removedUpdateCount"] = iRemovedUpdateCount;
		rResult["uncappedConsecutiveEndpoint"] = iUncappedConsecutiveEndpoint;
		rResult["directAdoptionRequired"] = bDirectAdoptionRequired;
		rResult["adoptionDesync"] = adoptionDesync.bDesync;
		rResult["afterAdoption"] = std::move(afterAdoption);
		rResult["pendingCleared"] = bPendingCleared;
		rResult["adoptedTicksMatch"] = bAdoptedTicksMatch;
		rResult["confirmedOffsetZero"] = rFrames.iConfirmedOffset == 0;
		rResult["obsoleteUpdatesAbsent"] = bObsoleteUpdatesAbsent;
		rResult["ringHeadIsAdopted"] = bRingHeadIsAdopted;
		rResult["directAdoptionProven"] = bDirectAdoptionProven;
		rResult["renderBaseNotOlderThanLastRendered"] = bRenderBaseNotOlder;
	}
	catch (...)
	{
		rDesyncCore.ClearAgentFullStateFixture();
		throw;
	}

	rDesyncCore.ClearAgentFullStateFixture();
	rResult["cleared"] = true;
}

int64_t DesyncProbeCountParameter(const nlohmann::json& rParameters, std::string_view parameterName)
{
	std::string parameterNameString(parameterName);
	if (!rParameters.contains(parameterNameString) || !rParameters.at(parameterNameString).is_number_integer())
	{
		throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
	}

	if (rParameters.at(parameterNameString).is_number_unsigned())
	{
		uint64_t uiCount = rParameters.at(parameterNameString).get<uint64_t>();
		if (uiCount > 8)
		{
			throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
		}
		return static_cast<int64_t>(uiCount);
	}

	int64_t iCount = rParameters.at(parameterNameString).get<int64_t>();
	if (iCount < 0 || iCount > 8)
	{
		throw std::runtime_error("desync_probe '" + parameterNameString + "' must be an integer in [0,8]");
	}
	return iCount;
}

// desync_probe: exercise the real desync packet and recovery paths without manufacturing a simulation mismatch.
// Schema: {"desyncReports"?:int[0,8],"debugFrameRequests"?:int[0,8]} OR {"triggerRecovery":true}.
void CommandDesyncProbe(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	// Heap: validation errors, JSON result, and triggerRecovery's serialized Frame snapshot
	ScopedSuppressAllocationTracking suppress;

	if (!rParameters.is_object())
	{
		throw std::runtime_error("desync_probe params must be an object");
	}

	size_t iKnownParameterCount = static_cast<size_t>(rParameters.contains("desyncReports")) + static_cast<size_t>(rParameters.contains("debugFrameRequests")) + static_cast<size_t>(rParameters.contains("triggerRecovery"));
	if (rParameters.size() != iKnownParameterCount)
	{
		throw std::runtime_error("desync_probe accepts only 'desyncReports', 'debugFrameRequests', and 'triggerRecovery'");
	}

	int64_t iDesyncReportCount = rParameters.contains("desyncReports") ? DesyncProbeCountParameter(rParameters, "desyncReports") : 0;
	int64_t iDebugFrameRequestCount = rParameters.contains("debugFrameRequests") ? DesyncProbeCountParameter(rParameters, "debugFrameRequests") : 0;
	bool bTriggerRecovery = false;
	if (rParameters.contains("triggerRecovery"))
	{
		if (!rParameters.at("triggerRecovery").is_boolean())
		{
			throw std::runtime_error("desync_probe 'triggerRecovery' must be a boolean");
		}
		bTriggerRecovery = rParameters.at("triggerRecovery").get<bool>();
		if (!bTriggerRecovery)
		{
			throw std::runtime_error("desync_probe 'triggerRecovery' must be true when present");
		}
	}
	if (bTriggerRecovery && (rParameters.contains("desyncReports") || rParameters.contains("debugFrameRequests")))
	{
		throw std::runtime_error("desync_probe 'triggerRecovery' is mutually exclusive with packet counts");
	}

	if (gpGame == nullptr || gpClientSession == nullptr || gpClientSession->mpRuntime->mpClient == nullptr || !(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected) || gpClientSession->mpRuntime->mpClient->mpServerPeer == nullptr || gpGame->InMainMenu())
	{
		throw std::runtime_error("desync_probe requires a connected live client/server session");
	}

	auto coordIt = gpGame->mCoordFrames.find(gpGame->mClientGridCoord);
	if (coordIt == gpGame->mCoordFrames.end() || coordIt->second.iSnapshotCount <= 0)
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}
	int64_t iSnapshotIndex = engine::SnapshotIndex(coordIt->second.iSnapshotHead, coordIt->second.iSnapshotCount - 1);
	const std::unique_ptr<Frame>& pCurrentFrame = coordIt->second.snapshots[iSnapshotIndex];
	if (pCurrentFrame == nullptr)
	{
		throw std::runtime_error("desync_probe requires a current client frame");
	}

	const int64_t iTick = pCurrentFrame->interpolate.iTick;
	const engine::GridCoord coord = gpGame->mClientGridCoord;
	const common::crc_t uiExpectedCrc = pCurrentFrame->postRender.sharedCrc;
	const common::crc_t uiActualCrc = uiExpectedCrc ^ static_cast<common::crc_t>(1);

	for (int64_t i = 0; i < iDesyncReportCount; ++i)
	{
		gpClientSession->mpRuntime->mpClient->SendDesyncReport(iTick, coord, uiExpectedCrc, uiActualCrc);
	}
	for (int64_t i = 0; i < iDebugFrameRequestCount; ++i)
	{
		gpClientSession->mpRuntime->mpClient->SendDebugFrameRequest(iTick, coord);
	}

	if (bTriggerRecovery)
	{
		if (gpClientSession->mpDesyncCore->IsStalled())
		{
			throw std::runtime_error("desync_probe cannot trigger recovery while desync debug mode is already stalled");
		}

		std::ostringstream outputStream(std::ios::binary);
		outputStream << *pCurrentFrame;
		std::istringstream inputStream(outputStream.str(), std::ios::binary);
		std::unique_ptr<Frame> pSnapshot = std::make_unique<Frame>();
		inputStream >> *pSnapshot;

		engine::ReconcileDesyncInfo desyncInfo
		{
			.bDesync = true,
			.iDesyncTick = iTick,
			.desyncCoord = coord,
			.desyncExpectedCrc = uiExpectedCrc,
			.desyncActualCrc = uiActualCrc,
			.pDesyncClientFrame = std::move(pSnapshot),
		};
		gpClientSession->mpDesyncCore->OnDesyncDetected(std::move(desyncInfo));
	}

	rResult["tick"] = iTick;
	rResult["coord"] = {coord.x, coord.y};
	rResult["desyncDebugFrames"] = kbDesyncDebugFrames;
	rResult["stalled"] = gpClientSession->mpDesyncCore->IsStalled();
	rResult["desyncReports"] = iDesyncReportCount;
	rResult["debugFrameRequests"] = iDebugFrameRequestCount;
	rResult["triggerRecovery"] = bTriggerRecovery;
}

// Keeps agent-supplied cells far from the int32 extremes that server adjacency deltas and the client 3x3
// neighbour ring compute from them.
constexpr int32_t kiAgentGridCoordLimit = 1'000'000;

int32_t ClientGridCoordValue(const nlohmann::json& rValue)
{
	if (!rValue.is_number_integer())
	{
		throw std::runtime_error("set_client_grid_coord 'coord' must be an array of 2 integers");
	}

	if (rValue.is_number_unsigned())
	{
		uint64_t uiValue = rValue.get<uint64_t>();
		if (uiValue > static_cast<uint64_t>(kiAgentGridCoordLimit))
		{
			throw std::runtime_error("set_client_grid_coord 'coord' values must be within +/-1000000");
		}
		return static_cast<int32_t>(uiValue);
	}

	int64_t iValue = rValue.get<int64_t>();
	if (iValue < -static_cast<int64_t>(kiAgentGridCoordLimit) || iValue > static_cast<int64_t>(kiAgentGridCoordLimit))
	{
		throw std::runtime_error("set_client_grid_coord 'coord' values must be within +/-1000000");
	}
	return static_cast<int32_t>(iValue);
}

// set_client_grid_coord: move the client's grid cell so automation can drive the cross-cell subscribe and
// full-state adoption path. Schema: {"coord":[x,y]}.
void CommandSetClientGridCoord(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	// Heap: validation errors and JSON result
	ScopedSuppressAllocationTracking suppress;

	if (!rParameters.is_object())
	{
		throw std::runtime_error("set_client_grid_coord params must be an object");
	}
	if (rParameters.size() != 1 || !rParameters.contains("coord"))
	{
		throw std::runtime_error("set_client_grid_coord accepts only 'coord'");
	}

	const nlohmann::json& rCoord = rParameters.at("coord");
	if (!rCoord.is_array() || rCoord.size() != 2)
	{
		throw std::runtime_error("set_client_grid_coord 'coord' must be an array of 2 integers");
	}

	const engine::GridCoord coord {ClientGridCoordValue(rCoord.at(0)), ClientGridCoordValue(rCoord.at(1))};

	// Before player assignment the subscription policy falls back to origin, so the requested cell would be dropped.
	if (gpGame == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpClientSession == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpClientSession->mpRuntime->mpClient == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpClientSession->mpRuntime->mpClient->mpServerPeer == nullptr)
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (gpGame->InMainMenu())
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}
	if (!gpGame->ClientPlayerId().IsValid())
	{
		throw std::runtime_error("set_client_grid_coord requires a connected live client/server session with an assigned player");
	}

	// Order is an invariant: the setter clears the visible-neighbour cache keyed by the old cell.
	gpGame->SetClientGridCoord(coord);
	gpClientSession->UpdateDesiredCoords(SubscriptionChangeReason::kPollTick);

	rResult["clientGridCoord"] = {coord.x, coord.y};
}

} // namespace

bool ExecuteAgentCommandClient(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (cmd == "client_full_state_fixture")
	{
		CommandClientFullStateFixture(rParams, rResult);
		return true;
	}
	if (cmd == "describe_scene")
	{
		CommandDescribeScene(rParams, rResult);
		return true;
	}
	if (cmd == "desync_probe")
	{
		CommandDesyncProbe(rParams, rResult);
		return true;
	}
	if (cmd == "set_client_grid_coord")
	{
		CommandSetClientGridCoord(rParams, rResult);
		return true;
	}
	return false;
}

} // namespace game

#endif // defined(BT_CLIENT)
