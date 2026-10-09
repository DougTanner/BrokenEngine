#include "Network/Client/ClientReconciler.h"

#include "Audio/StaticVoices.h"
#include "Network/Client/ReconcileReplay.h"

#include "Frame/Collections/Players/Players.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace game
{

#if defined(BT_CLIENT)

static bool GetClientPlayerPosition(const Frame& rFrame, XMVECTOR& rVecPosition)
{
	std::optional<int64_t> oClientPlayerIndex = gpGame->ClientPlayerIndex(*rFrame.postRender.pPlayers);
	if (!oClientPlayerIndex.has_value())
	{
		return false;
	}
	rVecPosition = rFrame.interpolate.pPlayers->pVecPositions[*oClientPlayerIndex];
	return true;
}

static bool GetClientSnapshotPosition(XMVECTOR& rVecPosition, int64_t& riTick)
{
	auto it = gpGame->mCells.find(gpGame->mClientGridCoordinate);
	if (it == gpGame->mCells.end())
	{
		return false;
	}
	if (it->second.iSnapshotCount <= 0)
	{
		return false;
	}
	const Frame& rFrame = gpGame->RenderFrame(gpGame->mClientGridCoordinate);
	riTick = rFrame.interpolate.iTick;
	return GetClientPlayerPosition(rFrame, rVecPosition);
}

// A full replay can leave the ring without history before its confirmed frame, so the replayed timeline's frame at
// iTick is looked up in the client coord's replay stack rather than the ring.
static bool GetClientReplayedPosition(std::span<const engine::CoordWork> works, int64_t iTick, XMVECTOR& rVecPosition)
{
	for (const engine::CoordWork& rWork : works)
	{
		if (rWork.coord != gpGame->mClientGridCoordinate)
		{
			continue;
		}
		const engine::CoordScratch& rScratch = rWork.scratch;
		for (int64_t i = 0; i < rScratch.iReplayStackCount; ++i)
		{
			const Frame& rFrame = *rScratch.replayStack.at(static_cast<size_t>(i));
			if (rFrame.interpolate.iTick == iTick)
			{
				return GetClientPlayerPosition(rFrame, rVecPosition);
			}
		}
		return false;
	}
	return false;
}

engine::ReconcileDesyncInfo ClientReconciler::Run()
{
	// Heap: Frame allocation during replay, map operations on serverUpdates, and scratch resize
	ScopedSuppressAllocationTracking suppress;

	// Re-sync client identity from main thread
	mConfirmedClientState.clientGridCoordinate = gpGame->mClientGridCoordinate;
	mConfirmedClientState.clientGlobalPlayerIdentifier = gpGame->ClientPlayerIdentifier();
	mConfirmedClientState.fPreviousClientArmor = gpGame->mfPreviousClientArmor;

	engine::ReconcileInputs inputs;
	inputs.iTargetTick = gpGame->miTickCounter;
	ASSERT(inputs.iTargetTick >= 0);
	common::LogTickScope logTickScope(inputs.iTargetTick);

	// Capture the client position at the render source tick for visual error smoothing, so the post-replay read
	// compares the same tick and excludes the ticks this dispatch commits. Read before the dispatch, which is the
	// single engine entry point; the value goes unused when no coord is eligible.
	XMVECTOR vecPreWritebackPosition {};
	int64_t iPreWritebackTick = -1;
	bool bCapturedPrePosition = GetClientSnapshotPosition(vecPreWritebackPosition, iPreWritebackTick);

	engine::ReconcileDispatchResult dispatch = mDispatcher.Run(inputs);

	if (dispatch.iActiveCount == 0)
	{
		return {};
	}

	if (dispatch.pDesyncWork != nullptr)
	{
		engine::CoordWork* pDesynchronizationWork = dispatch.pDesyncWork;
		char acExpected[20] {}, acActual[20] {};
		common::ToHex(std::span<char, 20>(acExpected), pDesynchronizationWork->scratch.desyncExpectedCrc);
		common::ToHex(std::span<char, 20>(acActual), pDesynchronizationWork->scratch.desyncActualCrc);
		LOG(kNetwork, kError, "CONFIRMED DESYNC after full rollback/replay Coord: ({},{}) DesyncTick: {} ExpectedCrc: {} ActualCrc: {} ReplayTicks: {} NewConfirmed: {}", pDesynchronizationWork->coord.iX, pDesynchronizationWork->coord.iY, pDesynchronizationWork->scratch.iDesyncTick, acExpected, acActual, pDesynchronizationWork->scratch.iReplayStackCount, pDesynchronizationWork->scratch.iNewConfirmedTick);

		engine::ReconcileDesyncInfo desynchronizationInformation;
		desynchronizationInformation.bDesync = true;
		desynchronizationInformation.iDesyncTick = pDesynchronizationWork->scratch.iDesyncTick;
		desynchronizationInformation.desyncCoord = pDesynchronizationWork->coord;
		desynchronizationInformation.desyncExpectedCrc = pDesynchronizationWork->scratch.desyncExpectedCrc;
		desynchronizationInformation.desyncActualCrc = pDesynchronizationWork->scratch.desyncActualCrc;
		desynchronizationInformation.pDesyncClientFrame = std::move(pDesynchronizationWork->scratch.pDesyncClientFrame);
		return desynchronizationInformation;
	}

	// Player-transfer migration updates previous client armor.
	ConfirmedClientState newConfirmedClientState = mConfirmedClientState;
	std::span<const engine::CoordWork> activeWorks(mDispatcher.mWorks.data(), static_cast<size_t>(mDispatcher.miActiveCount));
	ReconcileUpdateClientState(activeWorks, dispatch.bAnyFullReplay, newConfirmedClientState);

	if (bCapturedPrePosition && dispatch.bAnyFullReplay)
	{
		XMVECTOR vecPostWritebackPosition {};
		if (GetClientReplayedPosition(activeWorks, iPreWritebackTick, vecPostWritebackPosition))
		{
			XMVECTOR vecError = XMVectorSubtract(vecPreWritebackPosition, vecPostWritebackPosition);
			XMVECTOR vecTotal = XMVectorAdd(gpGame->mVecVisualErrorOffset, vecError);
			float fTotal = XMVectorGetX(XMVector3Length(vecTotal));
			if (fTotal > Game::kfVisualErrorMaxDistance)
			{
				gpGame->mVecVisualErrorOffset = {};
				LOG(kNetwork, kWarning, "Visual error offset reset (exceeded max) Coord: ({},{}) Delta: {} Max: {}", gpGame->mClientGridCoordinate.iX, gpGame->mClientGridCoordinate.iY, common::Wb(XMVectorGetX(XMVector3Length(vecError)), 3), common::Wb(Game::kfVisualErrorMaxDistance, 1));
			}
			else
			{
				gpGame->mVecVisualErrorOffset = vecTotal;
				float fDelta = XMVectorGetX(XMVector3Length(vecError));
				if (fTotal > 0.1f)
				{
					float fChange = (mfLastLoggedVisualErrorDelta > 0.0f)
						? std::abs(fDelta - mfLastLoggedVisualErrorDelta) / mfLastLoggedVisualErrorDelta
						: 1.0f;
					int64_t iCurrentTick = gpGame->miTickCounter;
					if (fChange > 0.15f && (iCurrentTick - miLastVisualErrorLogTick > 32))
					{
						LOG(kNetwork, kDebug, "Visual error offset Coord: ({},{}) Delta: {} Accumulated: {}", gpGame->mClientGridCoordinate.iX, gpGame->mClientGridCoordinate.iY, common::Wb(fDelta, 3), common::Wb(fTotal, 3));
						mfLastLoggedVisualErrorDelta = fDelta;
						miLastVisualErrorLogTick = iCurrentTick;
					}
				}
			}
		}
	}

	mConfirmedClientState = newConfirmedClientState;
	gpGame->mfPreviousClientArmor = newConfirmedClientState.fPreviousClientArmor;

	gpProfileManager->mCrcValidatedTicksPerSecond.Set(dispatch.profiling.iCrcValidatedFrameTicks);
	gpProfileManager->mAssumedTicksPerSecond.Set(dispatch.profiling.iAssumedFrameTicks);
	gpProfileManager->mCrcFastPathEventsPerSecond.Set(dispatch.profiling.iCrcFastPathEvents);
	gpProfileManager->mStatusChangeReplayTicksPerSecond.Set(dispatch.profiling.iStatusChangeReplayTicks);
	gpProfileManager->mKnockOnReplayTicksPerSecond.Set(dispatch.profiling.iKnockOnReplayTicks);

	// Large single-frame re-sim bursts are frame-time spike candidates
	int64_t iResimulationTicks = dispatch.profiling.iStatusChangeReplayTicks + dispatch.profiling.iKnockOnReplayTicks;
	if (iResimulationTicks >= 8)
	{
		LOG(kNetwork, kVerbose, "Replay burst ReSimTicks: {} StatusChange: {} KnockOn: {} Assumed: {} CrcValidated: {} Coords: {}", iResimulationTicks, dispatch.profiling.iStatusChangeReplayTicks, dispatch.profiling.iKnockOnReplayTicks, dispatch.profiling.iAssumedFrameTicks, dispatch.profiling.iCrcValidatedFrameTicks, dispatch.iActiveCount);
	}

	if (dispatch.bAnyFullReplay && engine::gpAudioManager != nullptr)
	{
		engine::gpAudioManager->mpStaticVoices->mbSkipNextInvalidation = true;
	}

	return {};
}

void ClientReconciler::Reset()
{
	mConfirmedClientState = {};
	mDispatcher.Reset();
	mfLastLoggedVisualErrorDelta = 0.0f;
	miLastVisualErrorLogTick = -1'000;
}

#endif // BT_CLIENT

} // namespace game
