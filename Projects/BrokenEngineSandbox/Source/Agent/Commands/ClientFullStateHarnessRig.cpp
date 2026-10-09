#include "Agent/Commands/ClientFullStateHarnessRig.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"
#include "LaunchOptions.h"

#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

struct FullStateHarnessRigState
{
	ClientSession* pSession = nullptr;
	int64_t iTick = -1;
	engine::GridCoord coordinate {};
	bool bArmed = false;
};

static FullStateHarnessRigState sHarnessRig;

static bool IsHarnessRigStalled(const engine::ClientDesyncCore& rCore)
{
	return sHarnessRig.bArmed && sHarnessRig.pSession != nullptr && sHarnessRig.pSession->mpDesynchronizationCore.get() == &rCore;
}

static void ClearHarnessRig()
{
	ClientSession* pSession = sHarnessRig.pSession;
	sHarnessRig = {};
	sHarnessRig.iTick = -1;
	if (pSession == nullptr)
	{
		return;
	}
	engine::ClientDesyncCore& rCore = *pSession->mpDesynchronizationCore;
	rCore.mpfnAdditionalStall = nullptr;
	rCore.mpfnResetObserver = nullptr;
	if (pSession->mpRuntime->mpClient != nullptr && rCore.mDesyncDebugState.iTick < 0)
	{
		pSession->mpRuntime->mpClient->mStateFlags.Set(engine::Client::ClientStateFlags::kDesynchronizationDebugMode, false);
	}
}

static void ResetHarnessRig(engine::ClientDesyncCore& rCore)
{
	if (sHarnessRig.pSession != nullptr && sHarnessRig.pSession->mpDesynchronizationCore.get() == &rCore)
	{
		ClearHarnessRig();
	}
}

static nlohmann::json BuildCoordinateState(engine::GridCoord coordinate)
{
	nlohmann::json result;
	result["coord"] = {coordinate.iX, coordinate.iY};

	auto it = gpGame->mCells.find(coordinate);
	if (it == gpGame->mCells.end())
	{
		result["present"] = false;
		return result;
	}

	const engine::Cell& rCell = it->second;
	result["present"] = true;
	result["confirmedTick"] = rCell.iConfirmedTick;
	result["confirmedOffset"] = rCell.iConfirmedOffset;
	result["highWaterValidatedTick"] = rCell.iHighWaterValidatedTick;
	result["lastFullStateTick"] = rCell.iLastFullStateTick;
	result["snapshotHead"] = rCell.iSnapshotHead;
	result["snapshotCount"] = rCell.iSnapshotCount;
	result["lastRenderedTick"] = rCell.iLastRenderedTick;
	result["lastRenderedTime"] = rCell.fLastRenderedTime;
	result["serverUpdateCount"] = rCell.serverUpdates.size();
	result["lastReplayConfirmedTick"] = rCell.iLastReplayConfirmedTick;
	result["lastReplayServerUpdateCount"] = rCell.iLastReplayServerUpdateCount;
	result["stuckFrameCount"] = rCell.iStuckFrameCount;

	result["pendingFullStateTick"] = nullptr;
	if (rCell.pendingFullState.has_value())
	{
		result["pendingFullStateTick"] = rCell.pendingFullState->iTick;
	}

	result["firstServerUpdateTick"] = nullptr;
	result["lastServerUpdateTick"] = nullptr;
	if (!rCell.serverUpdates.empty())
	{
		auto [firstUpdateIt, lastUpdateIt] = std::ranges::minmax_element(rCell.serverUpdates, {}, [](const auto& rEntry)
		{
			return rEntry.first;
		});
		result["firstServerUpdateTick"] = firstUpdateIt->first;
		result["lastServerUpdateTick"] = lastUpdateIt->first;
	}

	bool bRingValid = rCell.iSnapshotHead >= 0 && rCell.iSnapshotHead < engine::kiNetworkBufferSize && rCell.iSnapshotCount >= 0
	               && rCell.iSnapshotCount <= engine::kiNetworkBufferSize;
	int64_t iPreviousTick = -1;
	for (int64_t i = 0; bRingValid && i < rCell.iSnapshotCount; ++i)
	{
		int64_t iPhysical = engine::SnapshotIndex(rCell.iSnapshotHead, i);
		const std::unique_ptr<Frame>& rpSnapshot = rCell.snapshots[iPhysical];
		bRingValid = rpSnapshot != nullptr && (i == 0 || rpSnapshot->interpolate.iTick == iPreviousTick + 1);
		if (rpSnapshot != nullptr)
		{
			iPreviousTick = rpSnapshot->interpolate.iTick;
		}
	}
	if (rCell.iConfirmedTick >= 0)
	{
		bRingValid = bRingValid && rCell.iConfirmedOffset >= 0 && rCell.iConfirmedOffset < rCell.iSnapshotCount;
		if (bRingValid)
		{
			int64_t iConfirmedPhysical = engine::SnapshotIndex(rCell.iSnapshotHead, rCell.iConfirmedOffset);
			bRingValid = rCell.snapshots[iConfirmedPhysical] != nullptr
			          && rCell.snapshots[iConfirmedPhysical]->interpolate.iTick == rCell.iConfirmedTick;
		}
	}
	result["ringValid"] = bRingValid;
	result["tailTick"] = rCell.iSnapshotCount > 0 ? iPreviousTick : -1;
	return result;
}

static nlohmann::json BuildState()
{
	nlohmann::json result;
	result["clientTick"] = gpGame->miTickCounter;
	result["stalled"] = gpClientSession->mpDesynchronizationCore->IsStalled();
	result["desyncTick"] = gpClientSession->mpDesynchronizationCore->mDesyncDebugState.iTick;
	result["syntheticStall"] = sHarnessRig.bArmed && sHarnessRig.pSession == gpClientSession;
	result["armedTick"] = sHarnessRig.iTick;
	result["timeMultiply"] = gpGame->mTimeStep.miTimeMultiply;
	result["timeDivide"] = gpGame->mTimeStep.miTimeDivide;
	if (gpClientSession->mpRuntime->mpClient != nullptr)
	{
		result["loadGeneration"] = gpClientSession->mpRuntime->mpClient->miCommittedLoadGeneration;
	}
	else
	{
		result["loadGeneration"] = nullptr;
	}
	result["coordState"] = BuildCoordinateState(sHarnessRig.coordinate);
	return result;
}

static void ExerciseMatchingTick(engine::GridCoord coordinate, nlohmann::json& rResult, bool& rbClockForced)
{
	auto it = gpGame->mCells.find(coordinate);
	if (it == gpGame->mCells.end())
	{
		throw std::runtime_error("client_full_state_harness_rig requires a received pending full state");
	}
	if (!it->second.pendingFullState.has_value())
	{
		throw std::runtime_error("client_full_state_harness_rig requires a received pending full state");
	}

	engine::Cell& rCell = it->second;
	int64_t iPendingTick = rCell.pendingFullState->iTick;
	int64_t iDeferTargetTick = gpGame->miTickCounter;
	if (iPendingTick <= iDeferTargetTick)
	{
		throw std::runtime_error("client_full_state_harness_rig requires a pending full state ahead of client tick");
	}

	nlohmann::json beforeDefer = BuildCoordinateState(coordinate);
	engine::ReconcileDesyncInfo deferDesynchronization = gpClientSession->mpReconciler->Run();
	bool bPendingPreserved = rCell.pendingFullState.has_value() && rCell.pendingFullState->iTick == iPendingTick;
	nlohmann::json afterDefer = BuildCoordinateState(coordinate);
	if (!bPendingPreserved)
	{
		throw std::runtime_error("future pending full state was not deferred");
	}

	for (int64_t i = rCell.iConfirmedTick + 1; i <= iPendingTick; ++i)
	{
		if (!rCell.serverUpdates.contains(i))
		{
			throw std::runtime_error("client_full_state_harness_rig requires contiguous server updates through the pending full state tick");
		}
	}

	float fPendingTime = rCell.pendingFullState->pFrame->interpolate.fCurrentTime;
	rbClockForced = true;
	int64_t iTickCounter = iPendingTick;
	ASSERT(iTickCounter >= 0);
	gpGame->miTickCounter = iTickCounter;
	gpGame->mfCurrentTime = fPendingTime;
	gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
	engine::ReconcileDesyncInfo injectionDesynchronization = gpClientSession->mpReconciler->Run();

	nlohmann::json afterInjection = BuildCoordinateState(coordinate);
	bool bPendingCleared = !rCell.pendingFullState.has_value();
	int64_t iHeadTick = -1;
	if (rCell.iSnapshotCount > 0 && rCell.snapshots[rCell.iSnapshotHead] != nullptr)
	{
		iHeadTick = rCell.snapshots[rCell.iSnapshotHead]->interpolate.iTick;
	}

	rResult["pendingTick"] = iPendingTick;
	rResult["deferTargetTick"] = iDeferTargetTick;
	rResult["beforeDefer"] = std::move(beforeDefer);
	rResult["afterDefer"] = std::move(afterDefer);
	rResult["deferPendingPreserved"] = bPendingPreserved;
	rResult["deferDesync"] = deferDesynchronization.bDesync;
	rResult["injectionDesync"] = injectionDesynchronization.bDesync;
	rResult["afterInjection"] = std::move(afterInjection);
	rResult["pendingCleared"] = bPendingCleared;
	rResult["headTick"] = iHeadTick;
	rResult["confirmedTick"] = rCell.iConfirmedTick;
	rResult["confirmedOffset"] = rCell.iConfirmedOffset;
}

void CommandClientFullStateHarnessRig(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!engine::PhysicalInputSuppressed())
	{
		throw std::runtime_error("client_full_state_harness_rig requires an agent-mode client");
	}
	if (!rParameters.is_object())
	{
		throw std::runtime_error("client_full_state_harness_rig requires only string 'action'");
	}
	if (!rParameters.contains("action"))
	{
		throw std::runtime_error("client_full_state_harness_rig requires only string 'action'");
	}
	if (!rParameters.at("action").is_string())
	{
		throw std::runtime_error("client_full_state_harness_rig requires only string 'action'");
	}
	if (rParameters.size() != 1)
	{
		throw std::runtime_error("client_full_state_harness_rig requires only string 'action'");
	}

	std::string action = rParameters.at("action").get<std::string>();
	if (action == "clear")
	{
		ClearHarnessRig();
		rResult = BuildState();
		return;
	}

	if (action == "arm_stall")
	{
		if (gpClientSession->mpRuntime->mpClient == nullptr)
		{
			throw std::runtime_error("client_full_state_harness_rig requires an accepted connection");
		}
		if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnectionAccepted))
		{
			throw std::runtime_error("client_full_state_harness_rig requires an accepted connection");
		}
		if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
		{
			throw std::runtime_error("client_full_state_harness_rig requires an accepted connection");
		}
		if (gpClientSession->mpRuntime->mpClient->mpServerPeer == nullptr)
		{
			throw std::runtime_error("client_full_state_harness_rig requires an accepted connection");
		}
		if (gpClientSession->mpDesynchronizationCore->IsStalled())
		{
			throw std::runtime_error("client_full_state_harness_rig is already stalled");
		}

		engine::GridCoord coordinate = gpGame->mClientGridCoordinate;
		bool bActive = std::ranges::any_of(gpClientSession->mpRuntime->mpClient->mSubscriptions.mCoordinateSlots, [coordinate](const engine::ClientCoordSlot& rSlot)
		{
			return rSlot.eState == engine::CoordSubscriptionState::kActive && rSlot.coordinate == coordinate;
		});
		auto it = gpGame->mCells.find(coordinate);
		if (!bActive)
		{
			throw std::runtime_error("client_full_state_harness_rig requires an active confirmed client coord");
		}
		if (it == gpGame->mCells.end())
		{
			throw std::runtime_error("client_full_state_harness_rig requires an active confirmed client coord");
		}
		if (it->second.iConfirmedTick < 0)
		{
			throw std::runtime_error("client_full_state_harness_rig requires an active confirmed client coord");
		}
		if (it->second.pendingFullState.has_value())
		{
			throw std::runtime_error("client_full_state_harness_rig requires no pre-existing pending full state");
		}

		sHarnessRig = {.pSession = gpClientSession, .iTick = gpGame->miTickCounter, .coordinate = coordinate, .bArmed = true};
		gpClientSession->mpDesynchronizationCore->mpfnAdditionalStall = &IsHarnessRigStalled;
		gpClientSession->mpDesynchronizationCore->mpfnResetObserver = &ResetHarnessRig;
		gpClientSession->mpRuntime->mpClient->mStateFlags.Set(engine::Client::ClientStateFlags::kDesynchronizationDebugMode);
		gpClientSession->mpRuntime->mpClient->SendResynchronizationRequest();
		rResult = BuildState();
		return;
	}

	if (!sHarnessRig.bArmed)
	{
		throw std::runtime_error("client_full_state_harness_rig is not armed");
	}
	if (sHarnessRig.pSession != gpClientSession)
	{
		throw std::runtime_error("client_full_state_harness_rig is not armed");
	}
	if (action == "inspect")
	{
		rResult = BuildState();
		return;
	}
	if (action == "exercise_matching_tick")
	{
		bool bClockForced = false;
		try
		{
			ExerciseMatchingTick(sHarnessRig.coordinate, rResult, bClockForced);
		}
		catch (...)
		{
			if (bClockForced)
			{
				ClearHarnessRig();
			}
			throw;
		}

		ClearHarnessRig();
		rResult["cleared"] = true;
		return;
	}
	if (action != "exercise_gap")
	{
		throw std::runtime_error("client_full_state_harness_rig 'action' must be arm_stall|inspect|exercise_gap|exercise_matching_tick|clear");
	}

	try
	{
		engine::GridCoord coordinate = sHarnessRig.coordinate;
		auto it = gpGame->mCells.find(coordinate);
		if (it == gpGame->mCells.end())
		{
			throw std::runtime_error("client_full_state_harness_rig requires a received pending full state");
		}
		if (!it->second.pendingFullState.has_value())
		{
			throw std::runtime_error("client_full_state_harness_rig requires a received pending full state");
		}

		engine::Cell& rCell = it->second;
		int64_t iPendingTick = rCell.pendingFullState->iTick;
		int64_t iDeferTargetTick = gpGame->miTickCounter;
		if (iPendingTick <= iDeferTargetTick)
		{
			throw std::runtime_error("client_full_state_harness_rig requires a pending full state ahead of client tick");
		}

		nlohmann::json beforeDefer = BuildCoordinateState(coordinate);
		engine::ReconcileDesyncInfo deferDesynchronization = gpClientSession->mpReconciler->Run();
		bool bPendingPreserved = rCell.pendingFullState.has_value() && rCell.pendingFullState->iTick == iPendingTick;
		nlohmann::json afterDefer = BuildCoordinateState(coordinate);
		if (!bPendingPreserved)
		{
			throw std::runtime_error("future pending full state was not deferred");
		}

		int64_t iConfirmedBeforeGap = rCell.iConfirmedTick;
		int64_t iRemovedUpdateCount = static_cast<int64_t>(std::erase_if(rCell.serverUpdates, [iConfirmedBeforeGap, iPendingTick](const auto& rEntry)
		{
			return rEntry.first > iConfirmedBeforeGap && rEntry.first <= iPendingTick;
		}));
		int64_t iUncappedConsecutiveEndpoint = iConfirmedBeforeGap;
		while (rCell.serverUpdates.contains(iUncappedConsecutiveEndpoint + 1))
		{
			++iUncappedConsecutiveEndpoint;
		}
		bool bDirectAdoptionRequired = iPendingTick > iUncappedConsecutiveEndpoint;
		if (!bDirectAdoptionRequired)
		{
			throw std::runtime_error("client_full_state_harness_rig failed to create an update gap before pending full state");
		}

		float fPendingTime = rCell.pendingFullState->pFrame->interpolate.fCurrentTime;
		int64_t iTickCounter = iPendingTick;
		ASSERT(iTickCounter >= 0);
		gpGame->miTickCounter = iTickCounter;
		gpGame->mfCurrentTime = fPendingTime;
		gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
		engine::ReconcileDesyncInfo adoptionDesynchronization = gpClientSession->mpReconciler->Run();

		nlohmann::json afterAdoption = BuildCoordinateState(coordinate);
		bool bObsoleteUpdatesAbsent = std::ranges::none_of(rCell.serverUpdates, [iPendingTick](const auto& rEntry)
		{
			return rEntry.first <= iPendingTick;
		});
		bool bRenderBaseNotOlder = rCell.iLastRenderedTick < 0 || rCell.iSnapshotCount <= 0
		                        || rCell.snapshots[rCell.iSnapshotHead]->interpolate.iTick >= rCell.iLastRenderedTick;
		bool bPendingCleared = !rCell.pendingFullState.has_value();
		bool bAdoptedTicksMatch = rCell.iConfirmedTick == iPendingTick && rCell.iHighWaterValidatedTick == iPendingTick
		                       && rCell.iLastFullStateTick == iPendingTick;
		bool bRingHeadIsAdopted = rCell.iSnapshotCount > 0 && rCell.snapshots[rCell.iSnapshotHead] != nullptr
		                       && rCell.snapshots[rCell.iSnapshotHead]->interpolate.iTick == iPendingTick;
		bool bDirectAdoptionProven = bDirectAdoptionRequired && !adoptionDesynchronization.bDesync && bPendingCleared && bAdoptedTicksMatch
		                          && rCell.iConfirmedOffset == 0 && bObsoleteUpdatesAbsent && bRingHeadIsAdopted;

		rResult["pendingTick"] = iPendingTick;
		rResult["deferTargetTick"] = iDeferTargetTick;
		rResult["beforeDefer"] = std::move(beforeDefer);
		rResult["afterDefer"] = std::move(afterDefer);
		rResult["deferPendingPreserved"] = bPendingPreserved;
		rResult["deferDesync"] = deferDesynchronization.bDesync;
		rResult["removedUpdateCount"] = iRemovedUpdateCount;
		rResult["uncappedConsecutiveEndpoint"] = iUncappedConsecutiveEndpoint;
		rResult["directAdoptionRequired"] = bDirectAdoptionRequired;
		rResult["adoptionDesync"] = adoptionDesynchronization.bDesync;
		rResult["afterAdoption"] = std::move(afterAdoption);
		rResult["pendingCleared"] = bPendingCleared;
		rResult["adoptedTicksMatch"] = bAdoptedTicksMatch;
		rResult["confirmedOffsetZero"] = rCell.iConfirmedOffset == 0;
		rResult["obsoleteUpdatesAbsent"] = bObsoleteUpdatesAbsent;
		rResult["ringHeadIsAdopted"] = bRingHeadIsAdopted;
		rResult["directAdoptionProven"] = bDirectAdoptionProven;
		rResult["renderBaseNotOlderThanLastRendered"] = bRenderBaseNotOlder;
	}
	catch (...)
	{
		ClearHarnessRig();
		throw;
	}

	ClearHarnessRig();
	rResult["cleared"] = true;
}

void DetachClientFullStateHarnessRig(ClientSession& rSession)
{
	if (sHarnessRig.pSession == &rSession)
	{
		ClearHarnessRig();
	}
}

} // namespace game

#endif // BT_CLIENT
