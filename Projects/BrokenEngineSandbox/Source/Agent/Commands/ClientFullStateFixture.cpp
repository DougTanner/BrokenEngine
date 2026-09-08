#include "Agent/Commands/ClientFullStateFixture.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "LaunchOptions.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSession.h"
#include "Network/Client/ClientSessionRuntime.h"

namespace game
{

namespace
{

struct FullStateFixtureState
{
	ClientSession* pSession = nullptr;
	int64_t iTick = -1;
	engine::GridCoord coord {};
	bool bArmed = false;
};

FullStateFixtureState sFixture;

bool IsFixtureStalled(const engine::ClientDesyncCore& rCore)
{
	return sFixture.bArmed && sFixture.pSession != nullptr && sFixture.pSession->mpDesyncCore.get() == &rCore;
}

void ClearFixture()
{
	ClientSession* pSession = sFixture.pSession;
	sFixture = {};
	sFixture.iTick = -1;
	if (pSession == nullptr)
	{
		return;
	}
	engine::ClientDesyncCore& rCore = *pSession->mpDesyncCore;
	rCore.mpfnAdditionalStall = nullptr;
	rCore.mpfnResetObserver = nullptr;
	if (pSession->mpRuntime->mpClient != nullptr && rCore.GetDesyncTick() < 0)
	{
		pSession->mpRuntime->mpClient->mStateFlags.Clear(engine::Client::ClientStateFlags::kDesyncDebugMode);
	}
}

void ResetFixture(engine::ClientDesyncCore& rCore)
{
	if (sFixture.pSession != nullptr && sFixture.pSession->mpDesyncCore.get() == &rCore)
	{
		ClearFixture();
	}
}

nlohmann::json BuildCoordState(engine::GridCoord coord)
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

	bool bRingValid = rFrames.iSnapshotHead >= 0 && rFrames.iSnapshotHead < engine::kiNetworkBufferSize && rFrames.iSnapshotCount >= 0
	               && rFrames.iSnapshotCount <= engine::kiNetworkBufferSize;
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

nlohmann::json BuildState()
{
	nlohmann::json result;
	result["clientTick"] = gpGame->TickCounter();
	result["stalled"] = gpClientSession->mpDesyncCore->IsStalled();
	result["desyncTick"] = gpClientSession->mpDesyncCore->GetDesyncTick();
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
	result["coordState"] = BuildCoordState(sFixture.coord);
	return result;
}

void ExerciseMatchingTick(engine::GridCoord coord, nlohmann::json& rResult, bool& rbClockForced)
{
	auto coordIt = gpGame->mCoordFrames.find(coord);
	if (coordIt == gpGame->mCoordFrames.end())
	{
		throw std::runtime_error("client_full_state_fixture requires a received pending full state");
	}
	if (!coordIt->second.pendingFullState.has_value())
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

	nlohmann::json beforeDefer = BuildCoordState(coord);
	engine::ReconcileDesyncInfo deferDesync = gpClientSession->mpReconciler->Run();
	bool bPendingPreserved = rFrames.pendingFullState.has_value() && rFrames.pendingFullState->iTick == iPendingTick;
	nlohmann::json afterDefer = BuildCoordState(coord);
	if (!bPendingPreserved)
	{
		throw std::runtime_error("future pending full state was not deferred");
	}

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

	nlohmann::json afterInjection = BuildCoordState(coord);
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

} // namespace

void CommandClientFullStateFixture(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!engine::PhysicalInputSuppressed())
	{
		throw std::runtime_error("client_full_state_fixture requires an agent-mode client");
	}
	if (!rParams.is_object())
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}
	if (!rParams.contains("action"))
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}
	if (!rParams.at("action").is_string())
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}
	if (rParams.size() != 1)
	{
		throw std::runtime_error("client_full_state_fixture requires only string 'action'");
	}

	const std::string action = rParams.at("action").get<std::string>();
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
		if (!bActive)
		{
			throw std::runtime_error("client_full_state_fixture requires an active confirmed client coord");
		}
		if (coordIt == gpGame->mCoordFrames.end())
		{
			throw std::runtime_error("client_full_state_fixture requires an active confirmed client coord");
		}
		if (coordIt->second.iConfirmedTick < 0)
		{
			throw std::runtime_error("client_full_state_fixture requires an active confirmed client coord");
		}
		if (coordIt->second.pendingFullState.has_value())
		{
			throw std::runtime_error("client_full_state_fixture requires no pre-existing pending full state");
		}

		sFixture = {.pSession = gpClientSession, .iTick = gpGame->TickCounter(), .coord = coord, .bArmed = true};
		gpClientSession->mpDesyncCore->mpfnAdditionalStall = &IsFixtureStalled;
		gpClientSession->mpDesyncCore->mpfnResetObserver = &ResetFixture;
		gpClientSession->mpRuntime->mpClient->mStateFlags.Set(engine::Client::ClientStateFlags::kDesyncDebugMode);
		gpClientSession->mpRuntime->mpClient->SendResyncRequest();
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
			ExerciseMatchingTick(sFixture.coord, rResult, bClockForced);
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
		const engine::GridCoord coord = sFixture.coord;
		auto coordIt = gpGame->mCoordFrames.find(coord);
		if (coordIt == gpGame->mCoordFrames.end())
		{
			throw std::runtime_error("client_full_state_fixture requires a received pending full state");
		}
		if (!coordIt->second.pendingFullState.has_value())
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

		nlohmann::json beforeDefer = BuildCoordState(coord);
		engine::ReconcileDesyncInfo deferDesync = gpClientSession->mpReconciler->Run();
		bool bPendingPreserved = rFrames.pendingFullState.has_value() && rFrames.pendingFullState->iTick == iPendingTick;
		nlohmann::json afterDefer = BuildCoordState(coord);
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

		nlohmann::json afterAdoption = BuildCoordState(coord);
		bool bObsoleteUpdatesAbsent = rFrames.serverUpdates.empty() || rFrames.serverUpdates.begin()->first > iPendingTick;
		bool bRenderBaseNotOlder = rFrames.iLastRenderedTick < 0 || rFrames.iSnapshotCount <= 0
		                        || rFrames.snapshots[rFrames.iSnapshotHead]->interpolate.iTick >= rFrames.iLastRenderedTick;
		bool bPendingCleared = !rFrames.pendingFullState.has_value();
		bool bAdoptedTicksMatch = rFrames.iConfirmedTick == iPendingTick && rFrames.iHighWaterValidatedTick == iPendingTick
		                       && rFrames.iLastFullStateTick == iPendingTick;
		bool bRingHeadIsAdopted = rFrames.iSnapshotCount > 0 && rFrames.snapshots[rFrames.iSnapshotHead] != nullptr
		                       && rFrames.snapshots[rFrames.iSnapshotHead]->interpolate.iTick == iPendingTick;
		bool bDirectAdoptionProven = bDirectAdoptionRequired && !adoptionDesync.bDesync && bPendingCleared && bAdoptedTicksMatch
		                          && rFrames.iConfirmedOffset == 0 && bObsoleteUpdatesAbsent && bRingHeadIsAdopted;

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
