#include "Pch.h"

#include "Network/Client/Client.h"

#if defined(BT_CLIENT)

#include "File/PackChunks.h"

namespace engine
{

bool Client::SendAcknowledgement()
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return false;
	}

	// ACKs are sent at most once per simulation-tick interval (1/kiTickRate seconds), independently of render rate.
	// They continue while simulation is paused or stalled so server resends and RTT remain active.
	static constexpr std::chrono::nanoseconds kAckInterval(1'000'000'000 / kiTickRate);
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	if (now - mLastAcknowledgementSendTime < kAckInterval)
	{
		return false;
	}
	mLastAcknowledgementSendTime = now;

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	// Per-slot ACK state for proactive re-sends
	NetworkMessages::AckStreamEntry pEntries[NetworkManager::kiMaximumEnetCoordinateSlots] {};
	uint8_t uiAckSlotCount = 0;
	for (int64_t i = 0; i < std::ssize(mCoordinateSlots); ++i)
	{
		if (mCoordinateSlots.at(i).eState == CoordSubscriptionState::kActive)
		{
			pEntries[uiAckSlotCount] =
			{
				.uiSlotIndex = static_cast<uint8_t>(i),
				.uiEpoch = mCoordinateSlots.at(i).acknowledgementState.uiEpoch,
				.iAckFloor = mCoordinateSlots.at(i).acknowledgementState.iAcknowledgmentFloor,
				.uiReceivedBitfieldLow = mCoordinateSlots.at(i).acknowledgementState.uiReceivedBitfieldLow,
				.uiReceivedBitfieldHigh = mCoordinateSlots.at(i).acknowledgementState.uiReceivedBitfieldHigh,
			};
			++uiAckSlotCount;
		}
	}

	// Pipeline RTT: embed client timestamp for server to echo back
	int64_t iTimestampNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	NetworkMessages::ClientAckStreamMessage message
	{
		.uiSlotCount = uiAckSlotCount,
		.pEntries = pEntries,
		.iEntryCapacity = NetworkManager::kiMaximumEnetCoordinateSlots,
		.iTimestampNanoseconds = iTimestampNanoseconds,
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelUnreliable, rWorkbuffer, 0);
	return true;
}

void Client::SendDesynchronizationReport(int64_t iTick, GridCoord coordinate, common::crc_t uiExpectedCrc, common::crc_t uiActualCrc)
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	char pcExpected[20] {};
	char pcActual[20] {};
	LOG(kNetwork, kError, "Client::SendDesyncReport Frame: {} Grid: ({},{}) Expected: {} Actual: {}", iTick, coordinate.iX, coordinate.iY, common::ToHex(std::span(pcExpected), uiExpectedCrc), common::ToHex(std::span(pcActual), uiActualCrc));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientDesyncReportMessage message
	{
		.iTick = iTick,
		.coord = coordinate,
		.uiExpectedCrc = static_cast<uint64_t>(uiExpectedCrc),
		.uiActualCrc = static_cast<uint64_t>(uiActualCrc),
	};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Client::SendDebugFrameRequest(int64_t iTick, GridCoord coordinate)
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	LOG(kNetwork, kError, "Client::SendDebugFrameRequest Frame: {} Grid: ({},{})", iTick, coordinate.iX, coordinate.iY);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientDebugFrameRequestMessage message {.iTick = iTick, .coord = coordinate};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

bool Client::SendSubscribe(GridCoord coordinate)
{
	if (!(mStateFlags & ClientStateFlags::kConnected))
	{
		return false;
	}
	if (!(mStateFlags & ClientStateFlags::kConnectionAccepted))
	{
		return false;
	}
	if (mpServerPeer == nullptr)
	{
		return false;
	}

	// The server picks the slot; the client needs a free slot for each unanswered subscribe request, cancelled ones
	// included, so the unanswered count stays within the slot count and under the server's per-update subscribe cap
	int64_t iFreeSlots = std::ranges::count_if(mCoordinateSlots, [](const ClientCoordSlot& rSlot)
	{
		return rSlot.eState == CoordSubscriptionState::kUnsubscribed || rSlot.eState == CoordSubscriptionState::kUnsubscribing;
	});
	if (iFreeSlots <= std::ssize(mSubscribeRequests.mRecords))
	{
		return false;
	}
	mSubscribeRequests.Add(coordinate);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientSubscribeMessage message {.uiLoadGeneration = muiCommittedLoadGeneration, .coord = coordinate};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);

	return true;
}

void Client::SendUnsubscribe(int64_t iSlot)
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	ClientCoordSlot& rSlot = mCoordinateSlots.at(iSlot);
	rSlot.eState = CoordSubscriptionState::kUnsubscribing;
	rSlot.transitionStartTime = std::chrono::steady_clock::now();
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientUnsubscribeMessage message
	{
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = rSlot.acknowledgementState.uiEpoch,
	};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Client::SendResynchronizationRequest()
{
	if (!(mStateFlags & ClientStateFlags::kConnected) || mpServerPeer == nullptr)
	{
		return;
	}

	LOG(kNetwork, kError, "Client::SendResyncRequest");

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ClientResyncRequestMessage message {};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Client::SendHello()
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ClientHelloMessage message
	{
		.uiProtocolVersion = kuiProtocolVersion,
		.iFrameVersion = game::NetworkSessionContract::GetFrameVersion(),
		.uiPackIntegrityToken = gpFileManager->mpPackChunks->mPackIntegrityToken,
		.buildConfiguration = kpcBuildConfigName,
		.guid = mClientGuid,
		.bHasGuid = true,
	};
	NetworkMessages::Write(rWorkbuffer, message);

	miHelloSendTimeNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	NetworkManager::SendPacket(mpServerPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

} // namespace engine

#endif // BT_CLIENT
