#include "Network/Client/ClientReconciler.h"

#if defined(BT_CLIENT)

#include "Network/Client/ReconcileReplay.h"

#include "Frame/Collections/Ships/ShipQueries.h"
#include "Game.h"

namespace game
{

// The visual-error smoothing in Run and the three position helpers below is game-agnostic apart from this function's
// ship row read: a future project may want it moved into shared engine code, depending on its requirements.
static bool GetClientOwnedEntityPosition(const Frame& rFrame, XMVECTOR& rVecPosition)
{
	std::optional<ShipLocator> optionalClientOwnedEntity = gpGame->ClientOwnedEntity(rFrame);
	if (!optionalClientOwnedEntity.has_value())
	{
		return false;
	}
	rVecPosition = ShipPosition(rFrame.interpolate, *optionalClientOwnedEntity);
	return true;
}

static bool GetClientSnapshotPosition(engine::GridCoord& rFocusedCoordinate, XMVECTOR& rVecPosition, int64_t& riTick)
{
	engine::GlobalId focusedGlobalIdentifier = gpGame->ClientOwnedEntityIdentifier();
	if (focusedGlobalIdentifier.iValue == 0)
	{
		return false;
	}

	rFocusedCoordinate = gpGame->mClientGridCoordinate;
	auto ownedIt = std::ranges::find(gpGame->mClientOwnedEntities, focusedGlobalIdentifier, &engine::OwnedEntity::globalId);
	if (ownedIt != gpGame->mClientOwnedEntities.end())
	{
		rFocusedCoordinate = ownedIt->coord;
	}

	auto it = gpGame->mCells.find(rFocusedCoordinate);
	if (it == gpGame->mCells.end())
	{
		return false;
	}
	if (it->second.iSnapshotCount <= 0)
	{
		return false;
	}
	const Frame& rFrame = gpGame->RenderFrame(rFocusedCoordinate);
	riTick = rFrame.interpolate.iTick;
	return GetClientOwnedEntityPosition(rFrame, rVecPosition);
}

// A full replay can leave the ring without history before its confirmed frame, so the replayed timeline's frame at
// iTick is looked up in the focused entity's coord replay stack rather than the ring.
static bool GetClientReplayedPosition(std::span<const engine::CoordWork> works, engine::GridCoord focusedCoordinate, int64_t iTick, XMVECTOR& rVecPosition)
{
	for (const engine::CoordWork& rWork : works)
	{
		if (rWork.coord != focusedCoordinate || !(rWork.scratch.flags & engine::ReconcileScratchFlags::kReplayed))
		{
			continue;
		}
		const engine::CoordScratch& rScratch = rWork.scratch;
		for (int64_t i = 0; i < rScratch.iReplayStackCount; ++i)
		{
			const Frame& rFrame = *rScratch.ppReplayStack[i];
			if (rFrame.interpolate.iTick == iTick)
			{
				return GetClientOwnedEntityPosition(rFrame, rVecPosition);
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

	ConfirmedClientState confirmedClientState {.clientGlobalIdentifier = gpGame->ClientOwnedEntityIdentifier(), .fPreviousClientArmor = gpGame->mfPreviousClientArmor};

	engine::ReconcileInputs inputs;
	inputs.iTargetTick = gpGame->miTickCounter;
	ASSERT(inputs.iTargetTick >= 0);
	common::LogTickScope logTickScope(inputs.iTargetTick);

	// Capture the client position at the render source tick for visual error smoothing, so the post-replay read
	// compares the same tick and excludes the ticks this dispatch commits. Read before the dispatch, which is the
	// single engine entry point; the value goes unused when no coord is eligible.
	engine::GridCoord preWritebackCoordinate {};
	XMVECTOR vecPreWritebackPosition {};
	int64_t iPreWritebackTick = -1;
	bool bCapturedPrePosition = GetClientSnapshotPosition(preWritebackCoordinate, vecPreWritebackPosition, iPreWritebackTick);

	engine::ReconcileDispatchResult dispatch = mDispatcher.Run(inputs);

	if (dispatch.activeWorks.empty())
	{
		return {};
	}

	if (dispatch.desync.bDesync)
	{
		return std::move(dispatch.desync);
	}

	// Owned-entity transfer migration updates previous client armor.
	bool bAnyWorkReplayed = std::ranges::any_of(dispatch.activeWorks, [](const engine::CoordWork& rWork)
	{
		return static_cast<bool>(rWork.scratch.flags & engine::ReconcileScratchFlags::kReplayed);
	});
	ReconcileUpdateClientState(dispatch.activeWorks, bAnyWorkReplayed, confirmedClientState);

	if (bCapturedPrePosition)
	{
		XMVECTOR vecPostWritebackPosition {};
		if (GetClientReplayedPosition(dispatch.activeWorks, preWritebackCoordinate, iPreWritebackTick, vecPostWritebackPosition))
		{
			XMVECTOR vecError = XMVectorSubtract(vecPreWritebackPosition, vecPostWritebackPosition);
			float fTotal = engine::gpCamera->AddVisualErrorOffset(vecError);
			if (fTotal > engine::CameraBase::kfVisualErrorMaxDistance)
			{
				LOG(kNetwork, kWarning, "Visual error offset reset (exceeded max) Coord: ({},{}) Delta: {} Max: {}", preWritebackCoordinate.iQ, preWritebackCoordinate.iR, common::Wb(XMVectorGetX(XMVector3Length(vecError)), 3), common::Wb(engine::CameraBase::kfVisualErrorMaxDistance, 1));
			}
			else
			{
				float fDelta = XMVectorGetX(XMVector3Length(vecError));
				if (fTotal > 0.1f)
				{
					float fChange = (mfLastLoggedVisualErrorDelta > 0.0f)
						? std::abs(fDelta - mfLastLoggedVisualErrorDelta) / mfLastLoggedVisualErrorDelta
						: 1.0f;
					int64_t iCurrentTick = gpGame->miTickCounter;
					if (fChange > 0.15f && (iCurrentTick - miLastVisualErrorLogTick > 32))
					{
						LOG(kNetwork, kDebug, "Visual error offset Coord: ({},{}) Delta: {} Accumulated: {}", preWritebackCoordinate.iQ, preWritebackCoordinate.iR, common::Wb(fDelta, 3), common::Wb(fTotal, 3));
						mfLastLoggedVisualErrorDelta = fDelta;
						miLastVisualErrorLogTick = iCurrentTick;
					}
				}
			}
		}
	}

	gpGame->mfPreviousClientArmor = confirmedClientState.fPreviousClientArmor;

	return {};
}

void ClientReconciler::Reset()
{
	mDispatcher.Reset();
	mfLastLoggedVisualErrorDelta = 0.0f;
	miLastVisualErrorLogTick = -1'000;
}

} // namespace game

#endif // BT_CLIENT
