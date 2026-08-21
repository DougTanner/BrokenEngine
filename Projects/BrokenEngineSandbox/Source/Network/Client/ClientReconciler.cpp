#include "Game.h"

#include "Network/Client/ClientReconciler.h"
#include "Network/Client/ReconcileReplay.h"
#include "Frame/Collections/Players/Players.h"
#include "Profile/ProfileManager.h"

namespace game
{

#if defined(BT_CLIENT)

static bool GetClientSnapshotPosition(XMVECTOR& rOut)
{
	auto coordIt = gpGame->mCoordFrames.find(gpGame->mClientGridCoord);
	if (coordIt == gpGame->mCoordFrames.end() || coordIt->second.iSnapshotCount <= 0)
	{
		return false;
	}
	int64_t iPhysical = engine::SnapshotIndex(coordIt->second.iSnapshotHead, coordIt->second.iSnapshotCount - 1);
	const std::unique_ptr<game::Frame>& pSnapshot = coordIt->second.snapshots[iPhysical];
	if (pSnapshot == nullptr)
	{
		return false;
	}
	std::optional<int64_t> oClientPlayerIndex = gpGame->ClientPlayerIndex(*pSnapshot->postRender.pPlayers);
	if (!oClientPlayerIndex.has_value())
	{
		return false;
	}
	rOut = pSnapshot->interpolate.pPlayers->pVecPositions[*oClientPlayerIndex];
	return true;
}

engine::ReconcileDesyncInfo ClientReconciler::Run()
{
	// Heap: Frame allocation during replay, map operations on serverUpdates, and scratch resize
	ScopedSuppressAllocationTracking suppress;

	// Re-sync client identity from main thread
	mConfirmedClientState.clientGridCoord = gpGame->mClientGridCoord;
	mConfirmedClientState.clientGlobalPlayerId = gpGame->ClientPlayerId();
	mConfirmedClientState.fPreviousClientArmor = gpGame->PreviousClientArmor();

	engine::ReconcileInputs inputs;
	inputs.iTargetTick = gpGame->TickCounter();
	ASSERT(inputs.iTargetTick >= 0);
	common::LogTickScope logTickScope(inputs.iTargetTick);

	// Capture client pre-writeback position for visual error smoothing. Read before the dispatch,
	// which is the single engine entry point; the value goes unused when no coord is eligible.
	XMVECTOR vecPreWritebackPosition {};
	bool bCapturedPrePosition = GetClientSnapshotPosition(vecPreWritebackPosition);

	engine::ReconcileDispatchResult dispatch = mDispatcher.Run(*gpGame, inputs);

	if (dispatch.iActiveCount == 0)
	{
		return {};
	}

	if (dispatch.pDesyncWork != nullptr)
	{
		engine::CoordWork* pDesyncWork = dispatch.pDesyncWork;
		char acExpected[20] {}, acActual[20] {};
		common::ToHex(std::span<char, 20>(acExpected), pDesyncWork->scratch.desyncExpectedCrc);
		common::ToHex(std::span<char, 20>(acActual), pDesyncWork->scratch.desyncActualCrc);
		LOG(kNetwork, kError, "CONFIRMED DESYNC after full rollback/replay Coord: ({},{}) DesyncTick: {} ExpectedCrc: {} ActualCrc: {} ReplayTicks: {} NewConfirmed: {}", pDesyncWork->coord.x, pDesyncWork->coord.y, pDesyncWork->scratch.iDesyncTick, acExpected, acActual, pDesyncWork->scratch.iReplayStackCount, pDesyncWork->scratch.iNewConfirmedTick);

		engine::ReconcileDesyncInfo desyncInfo;
		desyncInfo.bDesync = true;
		desyncInfo.iDesyncTick = pDesyncWork->scratch.iDesyncTick;
		desyncInfo.desyncCoord = pDesyncWork->coord;
		desyncInfo.desyncExpectedCrc = pDesyncWork->scratch.desyncExpectedCrc;
		desyncInfo.desyncActualCrc = pDesyncWork->scratch.desyncActualCrc;
		desyncInfo.pDesyncClientFrame = std::move(pDesyncWork->scratch.pDesyncClientFrame);
		return desyncInfo;
	}

	// Compute new confirmed client state (client coord time advance + transfer migration)
	ConfirmedClientState newConfirmedClientState = mConfirmedClientState;
	ReconcileUpdateClientState(mDispatcher.ActiveWorks(), dispatch.bAnyFullReplay, newConfirmedClientState);

	// Visual error offset: pre/post client position delta accumulated into gpGame
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
				LOG(kNetwork, kWarning, "Visual error offset reset (exceeded max) Coord: ({},{}) Delta: {} Max: {}", gpGame->mClientGridCoord.x, gpGame->mClientGridCoord.y, common::Wb(XMVectorGetX(XMVector3Length(vecError)), 3), common::Wb(Game::kfVisualErrorMaxDistance, 1));
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
					int64_t iCurrentTick = gpGame->TickCounter();
					if (fChange > 0.15f && (iCurrentTick - miLastVisualErrorLogTick > 32))
					{
						LOG(kNetwork, kDebug, "Visual error offset Coord: ({},{}) Delta: {} Accumulated: {}", gpGame->mClientGridCoord.x, gpGame->mClientGridCoord.y, common::Wb(fDelta, 3), common::Wb(fTotal, 3));
						mfLastLoggedVisualErrorDelta = fDelta;
						miLastVisualErrorLogTick = iCurrentTick;
					}
				}
			}
		}
	}

	mConfirmedClientState = newConfirmedClientState;
	gpGame->SetPreviousClientArmor(newConfirmedClientState.fPreviousClientArmor);

	gpProfileManager->SetReconcileCounters(dispatch.profiling.iCrcValidatedFrameTicks, dispatch.profiling.iAssumedFrameTicks, dispatch.profiling.iCrcFastPathEvents, dispatch.profiling.iStatusChangeReplayTicks, dispatch.profiling.iKnockOnReplayTicks);

	// Large single-frame re-sim bursts are frame-time spike candidates
	int64_t iReSimTicks = dispatch.profiling.iStatusChangeReplayTicks + dispatch.profiling.iKnockOnReplayTicks;
	if (iReSimTicks >= 8)
	{
		LOG(kNetwork, kVerbose, "Replay burst ReSimTicks: {} StatusChange: {} KnockOn: {} Assumed: {} CrcValidated: {} Coords: {}", iReSimTicks, dispatch.profiling.iStatusChangeReplayTicks, dispatch.profiling.iKnockOnReplayTicks, dispatch.profiling.iAssumedFrameTicks, dispatch.profiling.iCrcValidatedFrameTicks, dispatch.iActiveCount);
	}

	if (dispatch.bAnyFullReplay && engine::gpAudioManager != nullptr)
	{
		engine::gpAudioManager->SkipNextStaticVoiceInvalidation();
	}

	return {};
}

void ClientReconciler::Reset()
{
	mConfirmedClientState = {};
	mDispatcher.Reset();
	mfLastLoggedVisualErrorDelta = 0.0f;
	miLastVisualErrorLogTick = -1000;
}

#endif // BT_CLIENT

} // namespace game
