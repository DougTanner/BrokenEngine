#include "Agent/Commands/ClientFullStateFixture.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"
#include "LaunchOptions.h"

#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

struct FullStateFixtureState
{
	ClientSession* pSession = nullptr;
	int64_t iTick = -1;
	engine::GridCoord coordinate {};
	bool bArmed = false;
};

static FullStateFixtureState sFixture;

static bool IsFixtureStalled(const engine::ClientDesyncCore& rCore)
{
	return sFixture.bArmed && sFixture.pSession != nullptr && sFixture.pSession->mpDesynchronizationCore.get() == &rCore;
}

static void ClearFixture()
{
	ClientSession* pSession = sFixture.pSession;
	sFixture = {};
	sFixture.iTick = -1;
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

static void ResetFixture(engine::ClientDesyncCore& rCore)
{
	if (sFixture.pSession != nullptr && sFixture.pSession->mpDesynchronizationCore.get() == &rCore)
	{
		ClearFixture();
	}
}

static nlohmann::json BuildCoordinateState(engine::GridCoord coordinate)
{
	nlohmann::json result;
	result["coord"] = {coordinate.iX, coordinate.iY};

	auto it = gpGame->mCoordinateFrames.find(coordinate);
	if (it == gpGame->mCoordinateFrames.end())
	{
		result["present"] = false;
		return result;
	}

	const engine::CoordFrames& rFrames = it->second;
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
		auto [firstUpdateIt, lastUpdateIt] = std::ranges::minmax_element(rFrames.serverUpdates, {}, [](const auto& rEntry)
		{
			return rEntry.first;
		});
		result["firstServerUpdateTick"] = firstUpdateIt->first;
		result["lastServerUpdateTick"] = lastUpdateIt->first;
	}

	bool bRingValid = rFrames.iSnapshotHead >= 0 && rFrames.iSnapshotHead < engine::kiNetworkBufferSize && rFrames.iSnapshotCount >= 0
	               && rFrames.iSnapshotCount <= engine::kiNetworkBufferSize;
	int64_t iPreviousTick = -1;
	for (int64_t i = 0; bRingValid && i < rFrames.iSnapshotCount; ++i)
	{
		int64_t iPhysical = engine::SnapshotIndex(rFrames.iSnapshotHead, i);
		const std::unique_ptr<Frame>& rpSnapshot = rFrames.snapshots[iPhysical];
		bRingValid = rpSnapshot != nullptr && (i == 0 || rpSnapshot->interpolate.iTick == iPreviousTick + 1);
		if (rpSnapshot != nullptr)
		{
			iPreviousTick = rpSnapshot->interpolate.iTick;
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

static nlohmann::json BuildState()
{
	nlohmann::json result;
	result["clientTick"] = gpGame->miTickCounter;
	result["stalled"] = gpClientSession->mpDesynchronizationCore->IsStalled();
	result["desyncTick"] = gpClientSession->mpDesynchronizationCore->mDesyncDebugState.iTick;
	result["syntheticStall"] = sFixture.bArmed && sFixture.pSession == gpClientSession;
	result["armedTick"] = sFixture.iTick;
	result["timeMultiply"] = gpGame->mTimeStep.miTimeMultiply;
	result["timeDivide"] = gpGame->mTimeStep.miTimeDivide;
	if (gpClientSession->mpRuntime->mpClient != nullptr)
	{
		result["loadGeneration"] = gpClientSession->mpRuntime->mpClient->muiCommittedLoadGeneration;
	}
	else
	{
		result["loadGeneration"] = nullptr;
	}
	result["coordState"] = BuildCoordinateState(sFixture.coordinate);
	return result;
}

static void ExerciseMatchingTick(engine::GridCoord coordinate, nlohmann::json& rResult, bool& rbClockForced)
{
	auto it = gpGame->mCoordinateFrames.find(coordinate);
	if (it == gpGame->mCoordinateFrames.end())
	{
		throw std::runtime_error("client_full_state_fixture requires a received pending full state");
	}
	if (!it->second.pendingFullState.has_value())
	{
		throw std::runtime_error("client_full_state_fixture requires a received pending full state");
	}

	engine::CoordFrames& rFrames = it->second;
	int64_t iPendingTick = rFrames.pendingFullState->iTick;
	int64_t iDeferTargetTick = gpGame->miTickCounter;
	if (iPendingTick <= iDeferTargetTick)
	{
		throw std::runtime_error("client_full_state_fixture requires a pending full state ahead of client tick");
	}

	nlohmann::json beforeDefer = BuildCoordinateState(coordinate);
	engine::ReconcileDesyncInfo deferDesynchronization = gpClientSession->mpReconciler->Run();
	bool bPendingPreserved = rFrames.pendingFullState.has_value() && rFrames.pendingFullState->iTick == iPendingTick;
	nlohmann::json afterDefer = BuildCoordinateState(coordinate);
	if (!bPendingPreserved)
	{
		throw std::runtime_error("future pending full state was not deferred");
	}

	for (int64_t i = rFrames.iConfirmedTick + 1; i <= iPendingTick; ++i)
	{
		if (!rFrames.serverUpdates.contains(i))
		{
			throw std::runtime_error("client_full_state_fixture requires contiguous server updates through the pending full state tick");
		}
	}

	float fPendingTime = rFrames.pendingFullState->pFrame->interpolate.fCurrentTime;
	rbClockForced = true;
	int64_t iTickCounter = iPendingTick;
	ASSERT(iTickCounter >= 0);
	gpGame->miTickCounter = iTickCounter;
	gpGame->mfCurrentTime = fPendingTime;
	gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
	engine::ReconcileDesyncInfo injectionDesynchronization = gpClientSession->mpReconciler->Run();

	nlohmann::json afterInjection = BuildCoordinateState(coordinate);
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
	rResult["deferDesync"] = deferDesynchronization.bDesync;
	rResult["injectionDesync"] = injectionDesynchronization.bDesync;
	rResult["afterInjection"] = std::move(afterInjection);
	rResult["pendingCleared"] = bPendingCleared;
	rResult["headTick"] = iHeadTick;
	rResult["confirmedTick"] = rFrames.iConfirmedTick;
	rResult["confirmedOffset"] = rFrames.iConfirmedOffset;
}

void CommandClientFullStateFixture(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!engine::PhysicalInputSuppressed())
	{
		throw std::runtime_error("client_full_state_fixture requires an agent-mode client");
	}
	if (!rParameters.is_object())
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}
	if (!rParameters.contains("action"))
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}
	if (!rParameters.at("action").is_string())
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}
	if (rParameters.size() != 1)
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}

	std::string action = rParameters.at("action").get<std::string>();
	if (action == "clear")
	{
		ClearFixture();
		rResult = BuildState();
		return;
	}

	if (action == "arm_stall")
	{
		if (gpClientSession->mpRuntime->mpClient == nullptr)
		{
			throw std::runtime_error("client_full_state_fixture requires an accepted connection");
		}
		if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnectionAccepted))
		{
			throw std::runtime_error("client_full_state_fixture requires an accepted connection");
		}
		if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
		{
			throw std::runtime_error("client_full_state_fixture requires an accepted connection");
		}
		if (gpClientSession->mpRuntime->mpClient->mpServerPeer == nullptr)
		{
			throw std::runtime_error("client_full_state_fixture requires an accepted connection");
		}
		if (gpClientSession->mpDesynchronizationCore->IsStalled())
		{
			throw std::runtime_error("client_full_state_fixture is already stalled");
		}

		engine::GridCoord coordinate = gpGame->mClientGridCoordinate;
		bool bActive = std::ranges::any_of(gpClientSession->mpRuntime->mpClient->mSubscriptions.mCoordinateSlots, [coordinate](const engine::ClientCoordSlot& rSlot)
		{
			return rSlot.eState == engine::CoordSubscriptionState::kActive && rSlot.coordinate == coordinate;
		});
		auto it = gpGame->mCoordinateFrames.find(coordinate);
		if (!bActive)
		{
			throw std::runtime_error("client_full_state_fixture requires an active confirmed client coord");
		}
		if (it == gpGame->mCoordinateFrames.end())
		{
			throw std::runtime_error("client_full_state_fixture requires an active confirmed client coord");
		}
		if (it->second.iConfirmedTick < 0)
		{
			throw std::runtime_error("client_full_state_fixture requires an active confirmed client coord");
		}
		if (it->second.pendingFullState.has_value())
		{
			throw std::runtime_error("client_full_state_fixture requires no pre-existing pending full state");
		}

		sFixture = {.pSession = gpClientSession, .iTick = gpGame->miTickCounter, .coordinate = coordinate, .bArmed = true};
		gpClientSession->mpDesynchronizationCore->mpfnAdditionalStall = &IsFixtureStalled;
		gpClientSession->mpDesynchronizationCore->mpfnResetObserver = &ResetFixture;
		gpClientSession->mpRuntime->mpClient->mStateFlags.Set(engine::Client::ClientStateFlags::kDesynchronizationDebugMode);
		gpClientSession->mpRuntime->mpClient->SendResynchronizationRequest();
		rResult = BuildState();
		return;
	}

	if (!sFixture.bArmed)
	{
		throw std::runtime_error("client_full_state_fixture is not armed");
	}
	if (sFixture.pSession != gpClientSession)
	{
		throw std::runtime_error("client_full_state_fixture is not armed");
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
			ExerciseMatchingTick(sFixture.coordinate, rResult, bClockForced);
		}
		catch (...)
		{
			if (bClockForced)
			{
				ClearFixture();
			}
			throw;
		}

		ClearFixture();
		rResult["cleared"] = true;
		return;
	}
	if (action != "exercise_gap")
	{
		throw std::runtime_error("client_full_state_fixture 'action' must be arm_stall|inspect|exercise_gap|exercise_matching_tick|clear");
	}

	try
	{
		engine::GridCoord coordinate = sFixture.coordinate;
		auto it = gpGame->mCoordinateFrames.find(coordinate);
		if (it == gpGame->mCoordinateFrames.end())
		{
			throw std::runtime_error("client_full_state_fixture requires a received pending full state");
		}
		if (!it->second.pendingFullState.has_value())
		{
			throw std::runtime_error("client_full_state_fixture requires a received pending full state");
		}

		engine::CoordFrames& rFrames = it->second;
		int64_t iPendingTick = rFrames.pendingFullState->iTick;
		int64_t iDeferTargetTick = gpGame->miTickCounter;
		if (iPendingTick <= iDeferTargetTick)
		{
			throw std::runtime_error("client_full_state_fixture requires a pending full state ahead of client tick");
		}

		nlohmann::json beforeDefer = BuildCoordinateState(coordinate);
		engine::ReconcileDesyncInfo deferDesynchronization = gpClientSession->mpReconciler->Run();
		bool bPendingPreserved = rFrames.pendingFullState.has_value() && rFrames.pendingFullState->iTick == iPendingTick;
		nlohmann::json afterDefer = BuildCoordinateState(coordinate);
		if (!bPendingPreserved)
		{
			throw std::runtime_error("future pending full state was not deferred");
		}

		int64_t iConfirmedBeforeGap = rFrames.iConfirmedTick;
		int64_t iRemovedUpdateCount = static_cast<int64_t>(std::erase_if(rFrames.serverUpdates, [iConfirmedBeforeGap, iPendingTick](const auto& rEntry)
		{
			return rEntry.first > iConfirmedBeforeGap && rEntry.first <= iPendingTick;
		}));
		int64_t iUncappedConsecutiveEndpoint = iConfirmedBeforeGap;
		while (rFrames.serverUpdates.contains(iUncappedConsecutiveEndpoint + 1))
		{
			++iUncappedConsecutiveEndpoint;
		}
		bool bDirectAdoptionRequired = iPendingTick > iUncappedConsecutiveEndpoint;
		if (!bDirectAdoptionRequired)
		{
			throw std::runtime_error("client_full_state_fixture failed to create an update gap before pending full state");
		}

		float fPendingTime = rFrames.pendingFullState->pFrame->interpolate.fCurrentTime;
		int64_t iTickCounter = iPendingTick;
		ASSERT(iTickCounter >= 0);
		gpGame->miTickCounter = iTickCounter;
		gpGame->mfCurrentTime = fPendingTime;
		gpGame->mTimeStep.mTickRemainderNanoseconds = 0ns;
		engine::ReconcileDesyncInfo adoptionDesynchronization = gpClientSession->mpReconciler->Run();

		nlohmann::json afterAdoption = BuildCoordinateState(coordinate);
		bool bObsoleteUpdatesAbsent = std::ranges::none_of(rFrames.serverUpdates, [iPendingTick](const auto& rEntry)
		{
			return rEntry.first <= iPendingTick;
		});
		bool bRenderBaseNotOlder = rFrames.iLastRenderedTick < 0 || rFrames.iSnapshotCount <= 0
		                        || rFrames.snapshots[rFrames.iSnapshotHead]->interpolate.iTick >= rFrames.iLastRenderedTick;
		bool bPendingCleared = !rFrames.pendingFullState.has_value();
		bool bAdoptedTicksMatch = rFrames.iConfirmedTick == iPendingTick && rFrames.iHighWaterValidatedTick == iPendingTick
		                       && rFrames.iLastFullStateTick == iPendingTick;
		bool bRingHeadIsAdopted = rFrames.iSnapshotCount > 0 && rFrames.snapshots[rFrames.iSnapshotHead] != nullptr
		                       && rFrames.snapshots[rFrames.iSnapshotHead]->interpolate.iTick == iPendingTick;
		bool bDirectAdoptionProven = bDirectAdoptionRequired && !adoptionDesynchronization.bDesync && bPendingCleared && bAdoptedTicksMatch
		                          && rFrames.iConfirmedOffset == 0 && bObsoleteUpdatesAbsent && bRingHeadIsAdopted;

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
		rResult["confirmedOffsetZero"] = rFrames.iConfirmedOffset == 0;
		rResult["obsoleteUpdatesAbsent"] = bObsoleteUpdatesAbsent;
		rResult["ringHeadIsAdopted"] = bRingHeadIsAdopted;
		rResult["directAdoptionProven"] = bDirectAdoptionProven;
		rResult["renderBaseNotOlderThanLastRendered"] = bRenderBaseNotOlder;
	}
	catch (...)
	{
		ClearFixture();
		throw;
	}

	ClearFixture();
	rResult["cleared"] = true;
}

void DetachClientFullStateFixture(ClientSession& rSession)
{
	if (sFixture.pSession == &rSession)
	{
		ClearFixture();
	}
}

} // namespace game

#endif // BT_CLIENT
