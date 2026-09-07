#include "Agent/AgentCommandsServerFaultFixtures.h"

#if defined(BT_SERVER)

#include "Game.h"
#include "Network/Server/ServerClientManager.h"

namespace game
{

void CommandGamePacketFaultFixture(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.is_object())
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}
	if (rParams.size() != 1)
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}
	if (!rParams.contains("case"))
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}
	if (!rParams.at("case").is_string())
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}

	const std::string caseName = rParams.at("case").get<std::string>();
	if (caseName != "server_only" && caseName != "undersized" && caseName != "oversized" && caseName != "over_cap")
	{
		throw std::runtime_error("game_packet_fault_fixture 'case' must be server_only|undersized|oversized|over_cap");
	}

	int64_t iClientId = 0;
	int64_t iHandshakenClientCount = 0;
	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		if (rClient.bHandshakeComplete)
		{
			iClientId = rClient.iClientId;
			++iHandshakenClientCount;
		}
	}
	if (iHandshakenClientCount != 1)
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly one handshaken client");
	}

	GamePacketType eType = GamePacketType::kClientUpdatePlayerRequest;
	int64_t iPayloadSize = 0;
	int64_t iFullSize = 1;
	int64_t iEntryCount = 1;
	if (caseName == "server_only")
	{
		eType = GamePacketType::kServerAssignPlayer;
	}
	else if (caseName == "undersized")
	{
		iPayloadSize = 12;
		iFullSize = 13;
	}
	else if (caseName == "oversized")
	{
		iPayloadSize = 14;
		iFullSize = 15;
	}
	else
	{
		iPayloadSize = 13;
		iEntryCount = 9;
		iFullSize = 14;
	}

	// AfterNetworkPoll has consumed real disconnects from the first poll. The agent drain admits only one command
	// before the next poll, so clear the retained range before the fixture parser can append its own notification.
	engine::gpServer->mPendingDisconnects.clear();
	engine::gpServer->mReceivedGamePackets.clear();
	engine::gpServer->mReceivedGamePackets.reserve(static_cast<size_t>(iEntryCount));
	if (caseName == "over_cap")
	{
		// Start the fixed burst at zero for its raw packet type; all other client counters remain unchanged.
		engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientId);
		pClient->tickTypeCounts[static_cast<uint8_t>(GamePacketType::kClientUpdatePlayerRequest)] = 0;
	}
	if (caseName == "server_only")
	{
		engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientId, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(0, 0)});
	}
	else if (caseName == "undersized")
	{
		engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientId, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(12, 0)});
	}
	else if (caseName == "oversized")
	{
		engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientId, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(14, 0)});
	}
	else
	{
		for (int64_t i = 0; i < 9; ++i)
		{
			engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientId, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(13, 0)});
		}
	}

	gpServerSession->ParseReceivedGamePackets();
	// Consume the fixture's game-layer disconnect bookkeeping before the next poll clears it.
	gpServerSession->mpClientManager->Disconnects();

	rResult["clientId"] = iClientId;
	rResult["case"] = caseName;
	rResult["type"] = static_cast<uint8_t>(eType);
	rResult["payloadSize"] = iPayloadSize;
	rResult["fullSize"] = iFullSize;
	rResult["entryCount"] = iEntryCount;
}

// engine_packet_fault_fixture: dispatch one malformed engine packet through the real Server::Receive path, so the
// admission gates, the dispatch catch, and RecordContractViolation all run. Ungated, mirroring
// game_packet_fault_fixture, because it only drops a packet. Both cases use kClientAckStream: it is the one
// client-sendable engine row whose [min,max] size range admits a packet the reader can still reject, so the failure
// lands in decode rather than at the exact-size admission gate.
void CommandEnginePacketFaultFixture(const nlohmann::json& rParams, nlohmann::json& rResult)
{
	if (!rParams.is_object() || rParams.size() != 1 || !rParams.contains("case") || !rParams.at("case").is_string())
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly {\"case\":\"truncated|size_mismatch\"}");
	}

	const std::string caseName = rParams.at("case").get<std::string>();
	if (caseName != "truncated" && caseName != "size_mismatch")
	{
		throw std::runtime_error("engine_packet_fault_fixture 'case' must be truncated|size_mismatch");
	}

	int64_t iClientId = 0;
	ENetPeer* pPeer = nullptr;
	int64_t iHandshakenClientCount = 0;
	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		if (rClient.bHandshakeComplete)
		{
			iClientId = rClient.iClientId;
			pPeer = rClient.pPeer;
			++iHandshakenClientCount;
		}
	}
	if (iHandshakenClientCount != 1 || pPeer == nullptr)
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly one handshaken client");
	}

	// One declared ack entry in both cases. "truncated" carries only 10 of the 37 bytes that entry needs, so the
	// shared reader throws and the dispatch catch records "corrupt payload"; "size_mismatch" carries a readable
	// entry plus one trailing byte, so the reader succeeds and the handler's exact-size cross-check records
	// "ackstream size" locally. The two are mutually exclusive, so one packet is counted exactly once.
	const int64_t iSize = caseName == "truncated" ? 10 : 38;
	std::vector<uint8_t> packet(static_cast<size_t>(iSize), 0);
	packet.at(0) = static_cast<uint8_t>(engine::PacketType::kClientAckStream);
	packet.at(1) = 1;

	// RecordContractViolation may remove the client, so nothing below touches the connection again.
	engine::gpServer->Receive(packet, pPeer);

	rResult["clientId"] = iClientId;
	rResult["case"] = caseName;
	rResult["type"] = packet.at(0);
	rResult["size"] = iSize;
}

void CommandServerPreHandshakeAckFixture([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("server_pre_handshake_ack_fixture requires kbDebugInput build");
	}
	else
	{
		if (!rParams.is_object())
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires exactly {}");
		}
		if (!rParams.empty())
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires exactly {}");
		}

		engine::ClientConnection* pClient = nullptr;
		int64_t iHandshakenClientCount = 0;
		for (engine::ClientConnection& rClient : engine::gpServer->mClients)
		{
			if (rClient.bHandshakeComplete)
			{
				pClient = &rClient;
				++iHandshakenClientCount;
			}
		}
		if (iHandshakenClientCount != 1)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires exactly one handshaken client");
		}
		if (pClient == nullptr)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires exactly one handshaken client");
		}
		if (pClient->pPeer == nullptr)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires exactly one handshaken client");
		}
		if (pClient->iTickPacketCount > engine::kiMaxClientPacketsPerTick - 1)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires packet-count headroom");
		}
		if (pClient->iTickByteCount > engine::kiMaxClientInboundBytesPerTick - engine::NetworkMessages::ClientAckStreamMessage::kiFixedSize)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires byte-count headroom");
		}

		const int64_t iClientId = pClient->iClientId;
		ENetPeer* const pPeer = pClient->pPeer;
		const bool bHandshakeComplete = pClient->bHandshakeComplete;
		const int64_t iContractViolations = pClient->iContractViolations;
		const int64_t iPacketCount = pClient->iTickPacketCount;
		const int64_t iByteCount = pClient->iTickByteCount;
		const uint8_t uiPacketType = static_cast<uint8_t>(engine::PacketType::kClientAckStream);
		const uint16_t uiTypeCount = pClient->tickTypeCounts[uiPacketType];
		const int64_t iClientTimestampNs = pClient->iClientTimestampNs;
		const int64_t iConsecutiveZeroAdvanceAcks = pClient->iConsecutiveZeroAdvanceAcks;
		const bool bFloorStalled = pClient->bFloorStalled;
		const int64_t iPeakConsecutiveStallAcks = pClient->iPeakConsecutiveStallAcks;
		std::vector<engine::AckState> ackStates;
		ackStates.reserve(pClient->slots.size());
		for (const engine::ClientConnection::SlotState& rSlot : pClient->slots)
		{
			ackStates.push_back(rSlot.ack);
		}

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		engine::NetworkMessages::ClientAckStreamMessage message {};
		engine::NetworkMessages::Write(rWorkbuffer, message);
		const std::span<const uint8_t> packetData(reinterpret_cast<const uint8_t*>(rWorkbuffer.View().data()), rWorkbuffer.View().size());
		const int64_t iPacketSize = static_cast<int64_t>(packetData.size());
		if (iPacketSize != engine::NetworkMessages::ClientAckStreamMessage::kiFixedSize)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture failed to serialize the fixed ACK layout");
		}

		{
			pClient->bHandshakeComplete = false;
			common::ScopedLambda restoreHandshake([iClientId]()
			{
				if (engine::ClientConnection* pRestoreClient = engine::gpServer->FindClient(iClientId); pRestoreClient != nullptr)
				{
					pRestoreClient->bHandshakeComplete = true;
				}
			});
			engine::gpServer->Receive(packetData, pPeer);
		}

		pClient = engine::gpServer->FindClient(iClientId);
		if (pClient == nullptr)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture client identity changed during receive");
		}
		if (pClient->pPeer != pPeer)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture client identity changed during receive");
		}

		bool bAckSlotsUnchanged = pClient->slots.size() == ackStates.size();
		for (int64_t i = 0; bAckSlotsUnchanged && i < std::ssize(pClient->slots); ++i)
		{
			const engine::AckState& rBefore = ackStates.at(i);
			const engine::AckState& rAfter = pClient->slots.at(i).ack;
			bAckSlotsUnchanged = rAfter.iAckFloor == rBefore.iAckFloor &&
				rAfter.uiReceivedBitfieldLow == rBefore.uiReceivedBitfieldLow &&
				rAfter.uiReceivedBitfieldHigh == rBefore.uiReceivedBitfieldHigh &&
				rAfter.uiEpoch == rBefore.uiEpoch;
		}

		const bool bAdmissionAdvanced = pClient->iTickPacketCount == iPacketCount + 1 &&
			pClient->iTickByteCount == iByteCount + engine::NetworkMessages::ClientAckStreamMessage::kiFixedSize;
		const bool bHandshakeRestored = bHandshakeComplete && pClient->bHandshakeComplete;
		const bool bTypeCountUnchanged = pClient->tickTypeCounts[uiPacketType] == uiTypeCount;
		const bool bAckStallUnchanged = pClient->iConsecutiveZeroAdvanceAcks == iConsecutiveZeroAdvanceAcks &&
			pClient->bFloorStalled == bFloorStalled &&
			pClient->iPeakConsecutiveStallAcks == iPeakConsecutiveStallAcks;
		const bool bTimestampUnchanged = pClient->iClientTimestampNs == iClientTimestampNs;
		const bool bContractViolationsUnchanged = pClient->iContractViolations == iContractViolations;
		if (!bAdmissionAdvanced)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}
		if (!bHandshakeRestored)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}
		if (!bTypeCountUnchanged)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}
		if (!bAckSlotsUnchanged)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}
		if (!bAckStallUnchanged)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}
		if (!bTimestampUnchanged)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}
		if (!bContractViolationsUnchanged)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}

		rResult["clientId"] = iClientId;
		rResult["type"] = uiPacketType;
		rResult["size"] = iPacketSize;
		rResult["packetCountBefore"] = iPacketCount;
		rResult["packetCountAfter"] = pClient->iTickPacketCount;
		rResult["byteCountBefore"] = iByteCount;
		rResult["byteCountAfter"] = pClient->iTickByteCount;
		rResult["handshakeRestored"] = bHandshakeRestored;
		rResult["clientPreserved"] = true;
		rResult["peerPreserved"] = true;
		rResult["typeCountUnchanged"] = bTypeCountUnchanged;
		rResult["ackSlotsUnchanged"] = bAckSlotsUnchanged;
		rResult["ackStallUnchanged"] = bAckStallUnchanged;
		rResult["timestampUnchanged"] = bTimestampUnchanged;
		rResult["contractViolationsUnchanged"] = bContractViolationsUnchanged;
	}
}

} // namespace game

#endif // defined(BT_SERVER)
