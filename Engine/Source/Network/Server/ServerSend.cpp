#include "Pch.h"

#if defined(BT_SERVER)

#include "Network/Server/Server.h"

#include "Frame/FrameStaticData.h"
#include "Network/NetworkCursor.h"

#include "Game.h"

namespace engine
{

void Server::SendCoordinateFullState(int64_t iClientIdentifier, int64_t iSlot, int64_t iTick, GridCoord coordinate, const game::Frame* pFrame)
{
	ClientConnection* pClient = FindClient(iClientIdentifier);
	if (pClient == nullptr)
	{
		return;
	}

	// Adopting this full state makes iTick the client's ACK floor, which moves that floor backward whenever the
	// client had already received later ticks. Record the tick so Server::ClientAcknowledgementStream admits that one
	// regression; without it the monotonic floor guard rejects every later ack for this slot, the ticks the
	// client discarded on adoption are never resent, and the client self-disconnects on its frozen floor.
	// Pre-adoption ACKs keep raising the server's floor until the reliable full state arrives.
	ClientConnection::SlotState& rSlot = pClient->slots.at(iSlot);
	LOG(kNetwork, kDebug, "Server::SendCoordFullState Client: {} Frame: {} Slot: {} Coord: ({},{}) AckFloor: {}", iClientIdentifier, iTick, iSlot, coordinate.iX, coordinate.iY, rSlot.ack.iAcknowledgmentFloor);
	rSlot.iPendingFullStateTick = iTick;

	// Serialize frame into the reusable scratch (server main thread only - single-writer contract)
	ScopedSuppressAllocationTracking suppress;
	// Heap: scratch grows until steady state
	mSendScratch.clear();
	mFrameStreamBuffer.mpTarget = &mSendScratch;
	game::NetworkSessionContract::WriteFrame(mFrameStream, *pFrame);

	// LZ4 compress
	int iCompressedSize = CompressToBuffer(std::span<const char>(mSendScratch.data(), mSendScratch.size()));

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ServerCoordFullStateMessage message
	{
		.uiLoadGeneration = muiLoadGeneration,
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = rSlot.ack.uiEpoch,
		.iTick = iTick,
		.coord = coordinate,
		.iUncompressedSize = static_cast<int32_t>(mSendScratch.size()),
		.compressedPayload = {.puiData = mCompressionBuffer.data(), .iSize = static_cast<int32_t>(iCompressedSize)},
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(pClient->pPeer, NetworkManager::CoordinateSlotReliable(iSlot), rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::SendCoordinateStaticData(int64_t iClientIdentifier, int64_t iSlot, GridCoord coordinate, const FrameStaticData& rStaticData)
{
	ClientConnection* pClient = FindClient(iClientIdentifier);
	if (pClient == nullptr)
	{
		return;
	}

	LOG(kNetwork, kDebug, "Server::SendCoordStaticData Client: {} Slot: {} Coord: ({},{})", iClientIdentifier, iSlot, coordinate.iX, coordinate.iY);

	// Serialize static data into the reusable scratch (server main thread only - single-writer contract)
	ScopedSuppressAllocationTracking suppress;
	// Heap: scratch grows until steady state
	mSendScratch.clear();
	mFrameStreamBuffer.mpTarget = &mSendScratch;
	rStaticData.Write(mFrameStream, /*bIncludeNavigationData=*/true);

	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ServerCoordStaticDataMessage message
	{
		.uiLoadGeneration = muiLoadGeneration,
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = pClient->slots.at(iSlot).ack.uiEpoch,
		.coord = coordinate,
		.staticData = {.puiData = reinterpret_cast<const uint8_t*>(mSendScratch.data()), .iSize = static_cast<int32_t>(mSendScratch.size())},
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(pClient->pPeer, NetworkManager::CoordinateSlotReliable(iSlot), rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::SendConnectionResponse(ENetPeer* pPeer, bool bAccepted, const char* pcMessage, const ClientGuid* pGloballyUniqueIdentifier)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

	NetworkMessages::ServerConnectionResponseMessage message
	{
		.uiLoadGeneration = muiLoadGeneration,
		.uiAccepted = bAccepted ? 1ui32 : 0ui32,
		.uiDebugInput = kbDebugInput ? 1ui32 : 0ui32,
		.guid = (pGloballyUniqueIdentifier != nullptr) ? *pGloballyUniqueIdentifier : ClientGuid {},
		.bHasGuid = bAccepted && pGloballyUniqueIdentifier != nullptr,
		.rejectionMessage = (!bAccepted && pcMessage != nullptr) ? pcMessage : "",
	};
	NetworkMessages::Write(rWorkbuffer, message);

	NetworkManager::SendPacket(pPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::SendSubscribeAccept(const ClientConnection& rClient, int64_t iSlot, GridCoord coordinate)
{
	uint16_t uiEpoch = (iSlot < std::ssize(rClient.slots)) ? rClient.slots.at(iSlot).ack.uiEpoch : 0;
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerSubscribeAcceptMessage message
	{
		.uiLoadGeneration = muiLoadGeneration,
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = uiEpoch,
		.coord = coordinate,
	};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(rClient.pPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::WriteBufferedFramePacket(common::Workbuffer& rWorkbuffer, PacketType eType, int64_t iSlot, uint16_t uiEpoch, const PerCoordBufferedFrame& rBuffered, int64_t iTimestampNanoseconds)
{
	NetworkMessages::CoordUpdateFields fields
	{
		.uiLoadGeneration = muiLoadGeneration,
		.uiSlotIndex = static_cast<uint8_t>(iSlot),
		.uiEpoch = uiEpoch,
		.iTick = rBuffered.iTick,
		.iEchoedTimestampNanoseconds = iTimestampNanoseconds,
		.uiSharedCrc = rBuffered.uiSharedCrc,
		.compressedPayload = {.puiData = rBuffered.compressedData.data(), .iSize = static_cast<int32_t>(rBuffered.compressedData.size())},
	};
	if (eType == PacketType::kServerCoordinateUpdate)
	{
		NetworkMessages::ServerCoordUpdateMessage message {};
		static_cast<NetworkMessages::CoordUpdateFields&>(message) = fields;
		NetworkMessages::Write(rWorkbuffer, message);
	}
	else if (eType == PacketType::kServerCoordinateResend)
	{
		NetworkMessages::ServerCoordResendMessage message {};
		static_cast<NetworkMessages::CoordUpdateFields&>(message) = fields;
		NetworkMessages::Write(rWorkbuffer, message);
	}
}

void Server::SendUpdate(ClientConnection& rClient, int64_t iTick)
{
	// Heap: workbuffer Push (per-slot scope) and ENet packet creation in SendPacket
	ScopedSuppressAllocationTracking suppress;

	for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
	{
		ClientConnection::SlotState& rSlot = rClient.slots.at(i);
		if (!(rSlot.subscription.flags & SubscriptionFlags::kActive))
		{
			continue;
		}

		GridCoord coordinate = rSlot.subscription.coordinate;

		const PerCoordBufferedFrame* pBuffered = FindBufferedFrame(coordinate, iTick);
		if (pBuffered == nullptr)
		{
			continue;
		}

		bool bRestartedStream = mPerCoordinateBufferedFrames.at(coordinate).front().iTick == iTick && rSlot.ack.iAcknowledgmentFloor >= 0
		                     && iTick > rSlot.ack.iAcknowledgmentFloor + 1;
		if (bRestartedStream)
		{
			rSlot.bHoldUpdatesUntilFullStateAck = true;
			if (rSlot.iPendingFullStateTick != iTick)
			{
				SendCoordinateFullState(rClient.iClientId, i, iTick, coordinate, &(*game::gpGame->mCoordinateFrames.at(coordinate).pCurrent));
			}
		}
		if (rSlot.bHoldUpdatesUntilFullStateAck)
		{
			continue;
		}

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

		WriteBufferedFramePacket(rWorkbuffer, PacketType::kServerCoordinateUpdate, i, rSlot.ack.uiEpoch, *pBuffered, rClient.iClientTimestampNanoseconds);

		NetworkManager::SendPacket(rClient.pPeer, NetworkManager::CoordinateSlotUnreliable(i), rWorkbuffer, 0);
		if (!(rSlot.subscription.flags & SubscriptionFlags::kFirstUpdateLogged))
		{
			LOG(kNetwork, kVerbose, "Server::SendUpdate Client: {} Slot: {} Coord: ({},{}) Tick: {}", rClient.iClientId, i, coordinate.iX, coordinate.iY, iTick);
			rSlot.subscription.flags.Set(SubscriptionFlags::kFirstUpdateLogged);
		}
	}
}

void Server::SendResends(ClientConnection& rClient, int64_t iTick)
{
	// Heap: workbuffer Push and ENet packet creation per resend frame
	ScopedSuppressAllocationTracking suppress;

	// Iterate per-slot: each active subscription has its own ACK state and coord ring buffer
	for (int64_t i = 0; i < std::ssize(rClient.slots); ++i)
	{
		ClientConnection::SlotState& rSlot = rClient.slots.at(i);
		GridCoord coordinate = rSlot.subscription.coordinate;

		if (!(rSlot.subscription.flags & SubscriptionFlags::kActive))
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}
		if (rSlot.bHoldUpdatesUntilFullStateAck)
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}

		const AckState& rAcknowledgmentState = rSlot.ack;
		if (rAcknowledgmentState.iAcknowledgmentFloor < 0)
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}

		int64_t iAcknowledgmentGap = iTick - rAcknowledgmentState.iAcknowledgmentFloor;
		if (iAcknowledgmentGap > 64)
		{
			LOG(kNetwork, kVerbose, "Server::SendResends Client: {} Slot: {} Coord: ({},{}) AckFloor: {} CurrentTick: {} Gap: {}", rClient.iClientId, i, coordinate.iX, coordinate.iY, rAcknowledgmentState.iAcknowledgmentFloor, iTick, iAcknowledgmentGap);
		}

		if (rAcknowledgmentState.uiReceivedBitfieldLow == 0 && rAcknowledgmentState.uiReceivedBitfieldHigh == 0)
		{
			UpdateResendLogState(rClient, i, 0, coordinate);
			continue;
		}
		int64_t iScanLimit = (rAcknowledgmentState.uiReceivedBitfieldHigh != 0)
			? std::max(static_cast<int64_t>(std::bit_width(rAcknowledgmentState.uiReceivedBitfieldLow)), 64 + static_cast<int64_t>(std::bit_width(rAcknowledgmentState.uiReceivedBitfieldHigh)))
			: static_cast<int64_t>(std::bit_width(rAcknowledgmentState.uiReceivedBitfieldLow));

		int64_t iSlotResendCount = 0;
		for (int64_t j = 0; j < iScanLimit && iSlotResendCount < kiMaximumResendFrames; ++j)
		{
			bool bReceived = (j < 64) ? (rAcknowledgmentState.uiReceivedBitfieldLow & (1ui64 << j)) != 0 : (rAcknowledgmentState.uiReceivedBitfieldHigh & (1ui64 << (j - 64))) != 0;
			if (bReceived)
			{
				continue;
			}

			int64_t iMissingFrame = rAcknowledgmentState.iAcknowledgmentFloor + 1 + j;
			if (iMissingFrame >= iTick)
			{
				break;
			}

			const PerCoordBufferedFrame* pBuffered = FindBufferedFrame(coordinate, iMissingFrame);
			if (pBuffered == nullptr)
			{
				LOG(kNetwork, kVerbose, "Server::SendResends Evicted frame Client: {} Slot: {} Coord: ({},{}) MissingTick: {} LatestBuffered: {}", rClient.iClientId, i, coordinate.iX, coordinate.iY, iMissingFrame, miLatestBufferedTick);
				continue;
			}

			common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
			common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();

			WriteBufferedFramePacket(rWorkbuffer, PacketType::kServerCoordinateResend, i, rSlot.ack.uiEpoch, *pBuffered, rClient.iClientTimestampNanoseconds);

			NetworkManager::SendPacket(rClient.pPeer, NetworkManager::CoordinateSlotUnreliable(i), rWorkbuffer, 0);

			++iSlotResendCount;
		}

		UpdateResendLogState(rClient, i, iSlotResendCount, coordinate);
	}
}

void Server::UpdateResendLogState(ClientConnection& rClient, int64_t iSlot, int64_t iSlotResendCount, GridCoord coordinate)
{
	static constexpr int64_t kiResendLogCooldownTicks = 64;

	bool bWasResending = rClient.slots.at(iSlot).iPreviousResendCount > 0;
	bool bIsResending = iSlotResendCount > 0;

	if (rClient.slots.at(iSlot).iResendLogCooldown > 0)
	{
		--rClient.slots.at(iSlot).iResendLogCooldown;
	}

	if (bWasResending != bIsResending && rClient.slots.at(iSlot).iResendLogCooldown <= 0)
	{
		LOG(kNetwork, kVerbose, "Server::SendResends Client: {} Slot: {} Coord: ({},{}) Count: {}", rClient.iClientId, iSlot, coordinate.iX, coordinate.iY, iSlotResendCount);
		rClient.slots.at(iSlot).iResendLogCooldown = kiResendLogCooldownTicks;
	}
	rClient.slots.at(iSlot).iPreviousResendCount = iSlotResendCount;
}

void Server::BroadcastLoadNotification()
{
	LOG(kDefault, kDebug, "Server::BroadcastLoadNotification");

	for (const ClientConnection& rClient : mClients)
	{
		if (!rClient.bHandshakeComplete)
		{
			continue;
		}

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		NetworkMessages::ServerLoadNotificationMessage message {.uiLoadGeneration = muiLoadGeneration};
		NetworkMessages::Write(rWorkbuffer, message);
		NetworkManager::SendPacket(rClient.pPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
	}
}

void Server::SendTimespeedUpdate(ENetPeer* pPeer, int64_t iMultiply, int64_t iDivide)
{
	common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
	common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
	NetworkMessages::ServerTimespeedUpdateMessage message {.iMultiply = iMultiply, .iDivide = iDivide};
	NetworkMessages::Write(rWorkbuffer, message);
	NetworkManager::SendPacket(pPeer, NetworkManager::kuiChannelReliable, rWorkbuffer, ENET_PACKET_FLAG_RELIABLE);
}

void Server::BroadcastTimespeedIfChanged()
{
	if (!game::gpGame->mTimeStep.mbTimeScaleChanged) [[likely]]
	{
		return;
	}
	game::gpGame->mTimeStep.mbTimeScaleChanged = false;

	int64_t iMultiply = game::gpGame->mTimeStep.miTimeMultiply;
	int64_t iDivide = game::gpGame->mTimeStep.miTimeDivide;
	LOG(kNetwork, kDebug, "Server::BroadcastTimespeedIfChanged Multiply: {} Divide: {}", iMultiply, iDivide);

	for (const ClientConnection& rClient : mClients)
	{
		if (!rClient.bHandshakeComplete)
		{
			continue;
		}

		SendTimespeedUpdate(rClient.pPeer, iMultiply, iDivide);
	}
}

} // namespace engine

#endif // BT_SERVER
