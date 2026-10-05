#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"

namespace engine
{

enum class CoordSubscriptionState : uint8_t
{
	kUnsubscribed,
	kWaitingFullState,  // Accept received, waiting for full state
	kActive,            // Receiving delta updates
	kUnsubscribing,     // kClientUnsubscribe sent, waiting for ack
};

struct ClientCoordSlot
{
	GridCoord coordinate {};
	CoordSubscriptionState eState = CoordSubscriptionState::kUnsubscribed;
	AckState acknowledgementState;
	std::chrono::steady_clock::time_point transitionStartTime {};
};

enum class SubscribeRequestFlags : uint8_t
{
	kCancelled     = 1 << 0,
	kTimeoutWarned = 1 << 1,
};

// One kClientSubscribe sent and not yet answered by an accept or reject
struct SubscribeRequest
{
	GridCoord coordinate {};
	std::chrono::steady_clock::time_point startTime {};
	common::Flags<SubscribeRequestFlags> flags;
};

// Subscribe requests in send order. Per coordinate: zero or more cancelled records, then at most one live record.
// The server answers every subscribe exactly once on the reliable channel, which is FIFO, so each accept or
// reject consumes the coordinate's oldest record. Full state and static data only consult records.
class SubscribeRequests
{
public:
	void Add(GridCoord coordinate);
	void Cancel(GridCoord coordinate);
	// Warns once per live record left unanswered past the timeout; the record stays live until its answer
	void WarnTimedOut(std::chrono::steady_clock::time_point now);
	// Removes the coordinate's oldest record; true only if that record was live
	bool TakeAnswer(GridCoord coordinate);
	bool IsLive(GridCoord coordinate) const;

	std::vector<SubscribeRequest> mRecords;
};

class ClientSubscriptions
{
public:
	void FreeSlot(int64_t iSlot);
	void Reset();
	void RecoverTimedOutSubscriptions();
	bool IsStaleRetainedEpoch(int64_t iSlot, int64_t iEpoch, GridCoord coordinate) const;

	enum class FullStateFlags : uint8_t
	{
		kAdoptCoordinate    = 1 << 0, // Full state arrived before SubscribeAccept for a live request; adopt the coordinate
		kRejectAsGhost = 1 << 1, // No live request or coordinate mismatch; send epoch-qualified unsubscribe + log
		kCommit        = 1 << 2, // Caller proceeds to push fullState + activate slot
	};
	using FullStateFlags_t = common::Flags<FullStateFlags>;
	FullStateFlags_t ClassifyFullState(int64_t iSlotIndex, int64_t iEpoch, GridCoord coordinate);

	enum class CoordUpdateFlags : uint8_t
	{
		kCommit    = 1 << 0, // Caller proceeds to decompress + push update
		kTrackTick = 1 << 1, // Call TrackReceivedTick (set only on kActive; skipped on kWaitingFullState)
	};
	using CoordUpdateFlags_t = common::Flags<CoordUpdateFlags>;
	CoordUpdateFlags_t ClassifyCoordinateUpdate(int64_t iSlotIndex, int64_t iEpoch);

	enum class SubscribeAcceptFlags : uint8_t
	{
		kHealEpoch   = 1 << 0, // Active slot, same coordinate: update epoch (late accept after re-subscribe)
		kCommitInitialization  = 1 << 1, // Initialize slot to kWaitingFullState
		kRejectGhost = 1 << 2, // State mismatch: send unsubscribe + logs
	};
	using SubscribeAcceptFlags_t = common::Flags<SubscribeAcceptFlags>;
	SubscribeAcceptFlags_t ClassifySubscribeAccept(int64_t iSlotIndex, int64_t iEpoch, GridCoord coordinate);

	std::vector<ClientCoordSlot> mCoordinateSlots;
	SubscribeRequests mSubscribeRequests;
};

} // namespace engine

#endif // BT_CLIENT
