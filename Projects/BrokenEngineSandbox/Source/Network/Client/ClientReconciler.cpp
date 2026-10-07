#include "Network/Client/ClientReconciler.h"

#include "Audio/StaticVoices.h"
#include "Network/Client/ReconcileReplay.h"

#include "Frame/Collections/Players/Players.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace game
{

#if defined(BT_CLIENT)

static bool GetClientSnapshotPosition(XMVECTOR& rVecPosition)
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
	int64_t iPhysical = engine::SnapshotIndex(it->second.iSnapshotHead, it->second.iSnapshotCount - 1);
	const std::unique_ptr<game::Frame>& rpSnapshot = it->second.snapshots[iPhysical];
	if (rpSnapshot == nullptr)
	{
		return false;
	}
	std::optional<int64_t> oClientPlayerIndex = gpGame->ClientPlayerIndex(*rpSnapshot->postRender.pPlayers);
	if (!oClientPlayerIndex.has_value())
	{
		return false;
	}
	rVecPosition = rpSnapshot->interpolate.pPlayers->pVecPositions[*oClientPlayerIndex];
	return true;
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

	// Capture client pre-writeback position for visual error smoothing. Read before the dispatch,
	// which is the single engine entry point; the value goes unused when no coord is eligible.
	XMVECTOR vecPreWritebackPosition {};
	bool bCapturedPrePosition = GetClientSnapshotPosition(vecPreWritebackPosition);

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
	ReconcileUpdateClientState(std::span<const engine::CoordWork>(mDispatcher.mWorks.data(), static_cast<size_t>(mDispatcher.miActiveCount)), dispatch.bAnyFullReplay, newConfirmedClientState);

	if (bCapturedPrePosition && dispatch.bAnyFullReplay)
	{
		XMVECTOR vecPostWritebackPosition {};
		if (GetClientSnapshotPosition(vecPostWritebackPosition))
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
