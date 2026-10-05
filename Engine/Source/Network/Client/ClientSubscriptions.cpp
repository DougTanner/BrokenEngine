#include "Pch.h"

#include "Network/Client/ClientSubscriptions.h"

#if defined(BT_CLIENT)

namespace engine
{

constexpr std::chrono::seconds kSubscriptionTransitionTimeout = 5s;

void SubscribeRequests::Add(GridCoord coordinate)
{
	// Heap: subscribe request list grows on subscribe (SynchronizeSubscriptions suppresses tracking)
	mRecords.push_back({.coordinate = coordinate, .startTime = std::chrono::steady_clock::now()});
}

void SubscribeRequests::Cancel(GridCoord coordinate)
{
	for (SubscribeRequest& rRecord : mRecords)
	{
		if (rRecord.coordinate == coordinate)
		{
			rRecord.flags.Set(SubscribeRequestFlags::kCancelled);
		}
	}
	LOG(kNetwork, kVerbose, "SubscribeRequests::Cancel Coord: ({},{}) Records: {}", coordinate.iX, coordinate.iY, std::ssize(mRecords));
}

void SubscribeRequests::WarnTimedOut(std::chrono::steady_clock::time_point now)
{
	for (SubscribeRequest& rRecord : mRecords)
	{
		if (rRecord.flags & SubscribeRequestFlags::kCancelled)
		{
			continue;
		}

		if (rRecord.flags & SubscribeRequestFlags::kTimeoutWarned)
		{
			continue;
		}

		if (now - rRecord.startTime < kSubscriptionTransitionTimeout)
		{
			continue;
		}

		rRecord.flags.Set(SubscribeRequestFlags::kTimeoutWarned);
		LOG(kNetwork, kWarning, "SubscribeRequests::WarnTimedOut unanswered subscribe Coord: ({},{})", rRecord.coordinate.iX, rRecord.coordinate.iY);
	}
}

bool SubscribeRequests::TakeAnswer(GridCoord coordinate)
{
	auto it = std::ranges::find(mRecords, coordinate, &SubscribeRequest::coordinate);
	if (it == mRecords.end())
	{
		return false;
	}
	bool bLive = !(it->flags & SubscribeRequestFlags::kCancelled);
	mRecords.erase(it);
	return bLive;
}

bool SubscribeRequests::IsLive(GridCoord coordinate) const
{
	return std::ranges::any_of(mRecords, [coordinate](const SubscribeRequest& rRecord)
	{
		return rRecord.coordinate == coordinate && !(rRecord.flags & SubscribeRequestFlags::kCancelled);
	});
}

void ClientSubscriptions::FreeSlot(int64_t iSlot)
{
	ClientCoordSlot& rSlot = mCoordinateSlots.at(iSlot);
	// Reset slot state but retain the epoch of the subscription being cleared, so a packet still in
	// flight from that subscription can be recognized as stale before the slot is admitted again
	int64_t iEpoch = rSlot.acknowledgementState.uiEpoch;
	rSlot = {};
	rSlot.acknowledgementState.uiEpoch = static_cast<uint16_t>(iEpoch);
}

void ClientSubscriptions::Reset()
{
	for (int64_t i = 0; i < std::ssize(mCoordinateSlots); ++i)
	{
		FreeSlot(i);
	}
	mSubscribeRequests.mRecords.clear();
}

void ClientSubscriptions::RecoverTimedOutSubscriptions()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	mSubscribeRequests.WarnTimedOut(now);
	// kWaitingFullState has no timeout: the server keeps an accepted subscription queued until the coordinate has a frame with
	// navigation data built, which during replay waits for the coordinate's recorded activation
	for (int64_t i = 0; i < std::ssize(mCoordinateSlots); ++i)
	{
		const ClientCoordSlot& rSlot = mCoordinateSlots.at(i);
		if (rSlot.eState == CoordSubscriptionState::kUnsubscribing && now - rSlot.transitionStartTime >= kSubscriptionTransitionTimeout)
		{
			FreeSlot(i);
		}
	}
}

bool ClientSubscriptions::IsStaleRetainedEpoch(int64_t iSlot, int64_t iEpoch, GridCoord coordinate) const
{
	// Cleared slots retain their last epoch, so a packet from a retired subscription (load reset,
	// unsubscribe, cancel) is recognized before it can be admitted onto the reused slot. Wrap-aware:
	// the server only ever increments the epoch, so genuine traffic is strictly newer.
	int64_t iRetainedEpoch = mCoordinateSlots.at(iSlot).acknowledgementState.uiEpoch;
	if (static_cast<int16_t>(iEpoch - iRetainedEpoch) > 0)
	{
		return false;
	}

	LOG(kNetwork, kDebug, "Client::IsStaleRetainedEpoch dropped stale packet Slot: {} Coord: ({},{}) Epoch: {} RetainedEpoch: {}", iSlot, coordinate.iX, coordinate.iY, iEpoch, iRetainedEpoch);
	return true;
}

ClientSubscriptions::FullStateFlags_t ClientSubscriptions::ClassifyFullState(int64_t iSlotIndex, int64_t iEpoch, GridCoord coordinate)
{
	const ClientCoordSlot& rSlot = mCoordinateSlots.at(static_cast<size_t>(iSlotIndex));

	// Full state arrived before SubscribeAccept (different ENet channels)
	if (rSlot.eState == CoordSubscriptionState::kUnsubscribed)
	{
		if (mSubscribeRequests.IsLive(coordinate))
		{
			if (IsStaleRetainedEpoch(iSlotIndex, iEpoch, coordinate))
			{
				return {};
			}
			return { FullStateFlags::kAdoptCoordinate, FullStateFlags::kCommit };
		}
		return FullStateFlags::kRejectAsGhost;
	}

	if (rSlot.eState == CoordSubscriptionState::kWaitingFullState)
	{
		// Stale full-state from a previous subscription to a different coordinate
		if (rSlot.coordinate != coordinate)
		{
			return FullStateFlags::kRejectAsGhost;
		}
		// Epoch guard: SubscribeAccept set the epoch; stale full-state on the same coordinate/slot is dropped
		if (iEpoch != rSlot.acknowledgementState.uiEpoch)
		{
			return {};
		}
		return FullStateFlags::kCommit;
	}

	// The server may reallocate a slot before its unsubscribe ACK arrives, and this per-slot lane can overtake that ACK
	if (rSlot.eState == CoordSubscriptionState::kUnsubscribing && mSubscribeRequests.IsLive(coordinate) && !IsStaleRetainedEpoch(iSlotIndex, iEpoch, coordinate))
	{
		return { FullStateFlags::kAdoptCoordinate, FullStateFlags::kCommit };
	}

	// kActive: resync full-state re-commit when coordinate+epoch match (desync recovery).
	// A genuine resend re-activates the slot at the resend tick via ServerCoordinateFullState's
	// commit block; a stale/ghost full state (wrong coordinate or superseded epoch) still falls
	// through to the reject below.
	if (rSlot.eState == CoordSubscriptionState::kActive && rSlot.coordinate == coordinate && iEpoch == rSlot.acknowledgementState.uiEpoch)
	{
		return FullStateFlags::kCommit;
	}

	// kActive (mismatched), or kUnsubscribing without a newer live subscribe request — silent reject
	return {};
}

ClientSubscriptions::CoordUpdateFlags_t ClientSubscriptions::ClassifyCoordinateUpdate(int64_t iSlotIndex, int64_t iEpoch)
{
	const ClientCoordSlot& rSlot = mCoordinateSlots.at(static_cast<size_t>(iSlotIndex));

	if (rSlot.eState == CoordSubscriptionState::kWaitingFullState && iEpoch == rSlot.acknowledgementState.uiEpoch)
	{
		// Pre-full-state buffering: accept but do not advance the per-slot tick counter
		return CoordUpdateFlags::kCommit;
	}
	if (rSlot.eState == CoordSubscriptionState::kActive && iEpoch == rSlot.acknowledgementState.uiEpoch)
	{
		return { CoordUpdateFlags::kCommit, CoordUpdateFlags::kTrackTick };
	}
	return {};
}

ClientSubscriptions::SubscribeAcceptFlags_t ClientSubscriptions::ClassifySubscribeAccept(int64_t iSlotIndex, int64_t iEpoch, GridCoord coordinate)
{
	const ClientCoordSlot& rSlot = mCoordinateSlots.at(static_cast<size_t>(iSlotIndex));

	// Re-subscription whose stale predecessor data already activated the slot — heal in place
	if (rSlot.eState == CoordSubscriptionState::kActive && rSlot.coordinate == coordinate)
	{
		return SubscribeAcceptFlags::kHealEpoch;
	}

	// State mismatch (and not active-coordinate-match) — ghost reject
	if (rSlot.eState != CoordSubscriptionState::kUnsubscribed)
	{
		return SubscribeAcceptFlags::kRejectGhost;
	}

	// The accept of a subscription already retired on this slot (by a ghost full state or a cancelled
	// commit) is dropped: that path already sent its unsubscribe, which frees the server slot.
	if (IsStaleRetainedEpoch(iSlotIndex, iEpoch, coordinate))
	{
		return {};
	}

	return SubscribeAcceptFlags::kCommitInitialization;
}

} // namespace engine

#endif // BT_CLIENT
