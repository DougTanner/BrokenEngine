#include "Pch.h"

#include "Agent/Commands/ClientNetworkFixtures.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/NetworkMessages.h"

namespace engine::ClientNetworkFixtures
{

struct Binding
{
	Client* pClient = nullptr;
	std::weak_ptr<StaleUpdateState> pStaleUpdate;
	std::weak_ptr<CancelledSubscriptionState> pCancelledSubscription;
	SubscribeAcceptResult* pSubscribeAcceptResult = nullptr;
};

static Binding sBinding;

static void Bind(Client& rClient)
{
	if (sBinding.pClient != &rClient)
	{
		sBinding = {};
		sBinding.pClient = &rClient;
	}
}

SubscribeAcceptResult ReceiveSubscribeAccept(Client& rClient, uint8_t uiSlotIndex, uint16_t uiEpoch, GridCoord coord)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerSubscribeAcceptMessage message {.uiLoadGeneration = rClient.muiCommittedLoadGeneration, .uiSlotIndex = uiSlotIndex, .uiEpoch = uiEpoch, .coord = coord};
	NetworkMessages::Write(rWorkbuffer, message);

	Bind(rClient);
	SubscribeAcceptResult result {};
	sBinding.pSubscribeAcceptResult = &result;
	common::ScopedLambda clearFixture([]()
	{
		sBinding.pSubscribeAcceptResult = nullptr;
	});
	rClient.Receive(std::span(reinterpret_cast<const uint8_t*>(rWorkbuffer.View().data()), rWorkbuffer.View().size()));
	return result;
}

void ArmStaleUpdate(Client& rClient, const std::shared_ptr<StaleUpdateState>& pState)
{
	Bind(rClient);
	if (!sBinding.pStaleUpdate.expired())
	{
		throw std::runtime_error("client_stale_update_fixture is already active");
	}
	sBinding.pStaleUpdate = pState;
}

void ArmCancelledSubscription(Client& rClient, const std::shared_ptr<CancelledSubscriptionState>& pState)
{
	Bind(rClient);
	if (!sBinding.pCancelledSubscription.expired())
	{
		throw std::runtime_error("client_cancelled_subscription_fixture is already active");
	}
	sBinding.pCancelledSubscription = pState;
}

void CaptureStaleUpdate(const Client& rClient, std::span<const uint8_t> packetData, uint8_t uiSlotIndex, uint16_t uiEpoch, int64_t iTick)
{
	if (sBinding.pClient != &rClient)
	{
		return;
	}
	if (std::shared_ptr<StaleUpdateState> pState = sBinding.pStaleUpdate.lock(); pState != nullptr
	 && !(pState->flags & StaleUpdateFlags::kCaptured))
	{
		// Heap: the fixture owns one exact packet copy and releases it before recursive delivery.
		pState->packet.assign(packetData.begin(), packetData.end());
		pState->iCapturedBytes = std::ssize(packetData);
		pState->iCapturedAtPoll = pState->iCapturePolls;
		pState->uiSlotIndex = uiSlotIndex;
		pState->uiEpoch = uiEpoch;
		pState->iTick = iTick;
		pState->coord = rClient.mSubscriptions.mCoordinateSlots.at(uiSlotIndex).coordinate;
		pState->flags.Set(StaleUpdateFlags::kCaptured);
	}
}

std::shared_ptr<StaleUpdateState> PollBeforeDrain(Client& rClient, QueryCoordUpdateState pfnQueryCoordUpdateState)
{
	if (sBinding.pClient != &rClient)
	{
		return nullptr;
	}
	std::shared_ptr<StaleUpdateState> pState = sBinding.pStaleUpdate.lock();
	if (pState == nullptr)
	{
		return nullptr;
	}
	if (++pState->iCapturePolls > kiNetworkBufferSize)
	{
		pState->packet.clear();
		pState->flags.Set(StaleUpdateFlags::kBoundExpired);
		sBinding.pStaleUpdate.reset();
		return nullptr;
	}
	if (!(pState->flags & StaleUpdateFlags::kCaptured))
	{
		return nullptr;
	}
	if (pState->iCapturePolls <= pState->iCapturedAtPoll + 1)
	{
		return nullptr;
	}
	if (pState->uiSlotIndex >= std::ssize(rClient.mSubscriptions.mCoordinateSlots))
	{
		return nullptr;
	}

	ClientCoordSlot& rSlot = rClient.mSubscriptions.mCoordinateSlots.at(pState->uiSlotIndex);
	CoordUpdateState coordState = pfnQueryCoordUpdateState(pState->coord, pState->iTick);
	if (rSlot.eState != CoordSubscriptionState::kActive)
	{
		return nullptr;
	}
	if (rSlot.coordinate != pState->coord)
	{
		return nullptr;
	}
	if (rSlot.acknowledgementState.uiEpoch != pState->uiEpoch)
	{
		return nullptr;
	}
	if (rSlot.acknowledgementState.iAcknowledgmentFloor < pState->iTick)
	{
		return nullptr;
	}
	if (!(coordState.flags & CoordUpdateFlags::kPresent))
	{
		return nullptr;
	}
	if (coordState.iConfirmedTick < pState->iTick)
	{
		return nullptr;
	}

	pState->iAckFloorBefore = rSlot.acknowledgementState.iAcknowledgmentFloor;
	pState->iConfirmedBefore = coordState.iConfirmedTick;
	std::vector<uint8_t> packet = std::move(pState->packet);
	pState->packet.clear();
	sBinding.pStaleUpdate.reset();
	rClient.Receive(packet);
	pState->iAckFloorAfter = rSlot.acknowledgementState.iAcknowledgmentFloor;
	return pState;
}

void PollAfterDrain(const Client& rClient, const std::shared_ptr<StaleUpdateState>& pState, QueryCoordUpdateState pfnQueryCoordUpdateState)
{
	if (pState == nullptr)
	{
		return;
	}
	CoordUpdateState coordState = pfnQueryCoordUpdateState(pState->coord, pState->iTick);
	pState->iConfirmedAfter = coordState.iConfirmedTick;
	if (coordState.flags & CoordUpdateFlags::kUpdateRetained)
	{
		pState->flags.Set(StaleUpdateFlags::kRetainedAfterDrain);
	}
	if (rClient.mStateFlags & Client::ClientStateFlags::kConnected)
	{
		pState->flags.Set(StaleUpdateFlags::kConnectedAfterDrain);
	}
	pState->flags.Set(StaleUpdateFlags::kComplete);
}

bool ObserveSubscribeAcceptCleanup(const Client& rClient, uint8_t uiSerializedSlot, int64_t iSerializedBytes)
{
	if (sBinding.pClient != &rClient)
	{
		return false;
	}
	if (sBinding.pSubscribeAcceptResult == nullptr)
	{
		return false;
	}
	sBinding.pSubscribeAcceptResult->uiSerializedSlot = uiSerializedSlot;
	sBinding.pSubscribeAcceptResult->iSerializedBytes = iSerializedBytes;
	sBinding.pSubscribeAcceptResult->bSendSuppressed = true;
	return true;
}

void ObserveUnsubscribeAck(const Client& rClient, uint8_t uiSlotIndex)
{
	if (sBinding.pClient != &rClient)
	{
		return;
	}
	if (std::shared_ptr<CancelledSubscriptionState> pState = sBinding.pCancelledSubscription.lock(); pState != nullptr && pState->iSlot == uiSlotIndex)
	{
		pState->eOutcome = CancelledSubscriptionOutcome::kAcked;
	}
}

void Reset(const Client& rClient)
{
	if (sBinding.pClient != &rClient)
	{
		return;
	}
	if (std::shared_ptr<StaleUpdateState> pState = sBinding.pStaleUpdate.lock(); pState != nullptr)
	{
		pState->packet.clear();
		pState->flags.Set(StaleUpdateFlags::kReset);
		sBinding.pStaleUpdate.reset();
	}
	if (std::shared_ptr<CancelledSubscriptionState> pState = sBinding.pCancelledSubscription.lock(); pState != nullptr)
	{
		pState->eOutcome = CancelledSubscriptionOutcome::kReset;
		sBinding.pCancelledSubscription.reset();
	}
}

void Detach()
{
	if (std::shared_ptr<StaleUpdateState> pState = sBinding.pStaleUpdate.lock(); pState != nullptr)
	{
		pState->packet.clear();
	}
	sBinding = {};
}

} // namespace engine::ClientNetworkFixtures

#endif // BT_CLIENT
