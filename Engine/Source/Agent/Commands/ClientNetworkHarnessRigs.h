#pragma once

#if defined(BT_CLIENT)

#include "Frame/GridCoord.h"

namespace engine
{

class Client;

namespace ClientNetworkHarnessRigs
{

struct SubscribeAcceptResult
{
	int64_t iSerializedSlot = 0;
	int64_t iSerializedBytes = 0;
	bool bSendSuppressed = false;
};

enum class StaleUpdateFlags : uint8_t
{
	kCaptured            = 1 << 0,
	kRetainedAfterDrain  = 1 << 1,
	kConnectedAfterDrain = 1 << 2,
	kComplete            = 1 << 3,
	kReset               = 1 << 4,
	kBoundExpired        = 1 << 5,
};

struct StaleUpdateState
{
	std::vector<uint8_t> packet;
	GridCoord coord {};
	int64_t iSlotIndex = 0;
	int64_t iEpoch = 0;
	int64_t iTick = -1;
	int64_t iCapturedBytes = 0;
	int64_t iAckFloorBefore = -1;
	int64_t iAckFloorAfter = -1;
	int64_t iConfirmedBefore = -1;
	int64_t iConfirmedAfter = -1;
	int64_t iCapturePolls = 0;
	int64_t iCapturedAtPoll = -1;
	common::Flags<StaleUpdateFlags> flags;
};

enum class CancelledSubscriptionOutcome : uint8_t
{
	kPending,
	kAcked,
	kReset,
};

struct CancelledSubscriptionState
{
	int64_t iSlot = -1;
	CancelledSubscriptionOutcome eOutcome = CancelledSubscriptionOutcome::kPending;
};

enum class CoordUpdateFlags : uint8_t
{
	kPresent        = 1 << 0,
	kUpdateRetained = 1 << 1,
};

struct CoordUpdateState
{
	int64_t iConfirmedTick = -1;
	common::Flags<CoordUpdateFlags> flags;
};

using QueryCoordUpdateState = CoordUpdateState (*)(GridCoord coord, int64_t iTick);

SubscribeAcceptResult ReceiveSubscribeAccept(Client& rClient, int64_t iSlotIndex, int64_t iEpoch, GridCoord coord);
void ArmStaleUpdate(Client& rClient, const std::shared_ptr<StaleUpdateState>& pState);
void ArmCancelledSubscription(Client& rClient, const std::shared_ptr<CancelledSubscriptionState>& pState);
void CaptureStaleUpdate(const Client& rClient, std::span<const uint8_t> packetData, int64_t iSlotIndex, int64_t iEpoch, int64_t iTick);
std::shared_ptr<StaleUpdateState> PollBeforeDrain(Client& rClient, QueryCoordUpdateState pfnQueryCoordUpdateState);
void PollAfterDrain(const Client& rClient, const std::shared_ptr<StaleUpdateState>& pState, QueryCoordUpdateState pfnQueryCoordUpdateState);
bool ObserveSubscribeAcceptCleanup(const Client& rClient, int64_t iSerializedSlot, int64_t iSerializedBytes);
void ObserveUnsubscribeAck(const Client& rClient, int64_t iSlotIndex);
void Reset(const Client& rClient);
void Detach();

} // namespace ClientNetworkHarnessRigs

} // namespace engine

#endif // BT_CLIENT
