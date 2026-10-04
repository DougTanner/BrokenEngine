#include "Agent/Commands/ServerFaultFixtures.h"

#if defined(BT_SERVER)

#include "Network/Server/ServerClientManager.h"
#include "Game.h"

namespace game
{

void CommandGamePacketFaultFixture(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.is_object())
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}
	if (rParameters.size() != 1)
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}
	if (!rParameters.contains("case"))
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}
	if (!rParameters.at("case").is_string())
	{
		throw std::runtime_error("game_packet_fault_fixture requires exactly {\"case\":\"server_only|undersized|oversized|over_cap\"}");
	}

	std::string caseName = rParameters.at("case").get<std::string>();
	if (caseName != "server_only" && caseName != "undersized" && caseName != "oversized" && caseName != "over_cap")
	{
		throw std::runtime_error("game_packet_fault_fixture 'case' must be server_only|undersized|oversized|over_cap");
	}

	int64_t iClientIdentifier = 0;
	int64_t iHandshakenClientCount = 0;
	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		if (rClient.bHandshakeComplete)
		{
			iClientIdentifier = rClient.iClientId;
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
		engine::ClientConnection* pClient = engine::gpServer->FindClient(iClientIdentifier);
		pClient->uiTickTypeCounts[static_cast<uint8_t>(GamePacketType::kClientUpdatePlayerRequest)] = 0;
	}
	if (caseName == "server_only")
	{
		engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientIdentifier, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(0, 0)});
	}
	else if (caseName == "undersized")
	{
		engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientIdentifier, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(12, 0)});
	}
	else if (caseName == "oversized")
	{
		engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientIdentifier, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(14, 0)});
	}
	else
	{
		for (int64_t i = 0; i < 9; ++i)
		{
			engine::gpServer->mReceivedGamePackets.push_back({.iClientId = iClientIdentifier, .uiPacketType = static_cast<uint8_t>(eType), .payload = std::vector<uint8_t>(13, 0)});
		}
	}

	gpServerSession->ParseReceivedGamePackets();
	// Consume the fixture's game-layer disconnect bookkeeping before the next poll clears it.
	gpServerSession->mpClientManager->Disconnects();

	rResult["clientId"] = iClientIdentifier;
	rResult["case"] = caseName;
	rResult["type"] = static_cast<uint8_t>(eType);
	rResult["payloadSize"] = iPayloadSize;
	rResult["fullSize"] = iFullSize;
	rResult["entryCount"] = iEntryCount;
}

// This fixture and CommandGamePacketFaultFixture run without kbDebugInput. kClientAcknowledgmentStream's admitted
// size range permits reader rejection. Server::Receive records "truncated" through its dispatch catch and
// "size_mismatch" through the handler's size check; RecordContractViolation can disconnect the client.
void CommandEnginePacketFaultFixture(const nlohmann::json& rParameters, nlohmann::json& rResult)
{
	if (!rParameters.is_object())
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly {\"case\":\"truncated|size_mismatch\"}");
	}
	if (rParameters.size() != 1)
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly {\"case\":\"truncated|size_mismatch\"}");
	}
	if (!rParameters.contains("case"))
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly {\"case\":\"truncated|size_mismatch\"}");
	}
	if (!rParameters.at("case").is_string())
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly {\"case\":\"truncated|size_mismatch\"}");
	}

	std::string caseName = rParameters.at("case").get<std::string>();
	if (caseName != "truncated" && caseName != "size_mismatch")
	{
		throw std::runtime_error("engine_packet_fault_fixture 'case' must be truncated|size_mismatch");
	}

	int64_t iClientIdentifier = 0;
	ENetPeer* pPeer = nullptr;
	int64_t iHandshakenClientCount = 0;
	for (const engine::ClientConnection& rClient : engine::gpServer->mClients)
	{
		if (rClient.bHandshakeComplete)
		{
			iClientIdentifier = rClient.iClientId;
			pPeer = rClient.pPeer;
			++iHandshakenClientCount;
		}
	}
	if (iHandshakenClientCount != 1)
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly one handshaken client");
	}
	if (pPeer == nullptr)
	{
		throw std::runtime_error("engine_packet_fault_fixture requires exactly one handshaken client");
	}

	// One declared ack entry in both cases. "truncated" carries only 10 of the 37 bytes that entry needs, so the
	// shared reader throws and the dispatch catch records "corrupt payload"; "size_mismatch" carries a readable
	// entry plus one trailing byte, so the reader succeeds and the handler's exact-size cross-check records
	// "ackstream size" locally. The two are mutually exclusive, so one packet is counted exactly once.
	int64_t iSize = caseName == "truncated" ? 10 : 38;
	std::vector<uint8_t> packet(static_cast<size_t>(iSize), 0);
	packet.at(0) = static_cast<uint8_t>(engine::PacketType::kClientAcknowledgmentStream);
	packet.at(1) = 1;

	// RecordContractViolation may remove the client, so nothing below touches the connection again.
	engine::gpServer->Receive(packet, pPeer);

	rResult["clientId"] = iClientIdentifier;
	rResult["case"] = caseName;
	rResult["type"] = packet.at(0);
	rResult["size"] = iSize;
}

void CommandServerPreHandshakeAcknowledgmentFixture([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("server_pre_handshake_ack_fixture requires kbDebugInput build");
	}
	else
	{
		if (!rParameters.is_object())
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires exactly {}");
		}
		if (!rParameters.empty())
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
		if (pClient->iTickPacketCount > engine::kiMaximumClientPacketsPerTick - 1)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires packet-count headroom");
		}
		if (pClient->iTickByteCount > engine::kiMaximumClientInboundBytesPerTick - engine::NetworkMessages::ClientAckStreamMessage::kiFixedSize)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture requires byte-count headroom");
		}

		int64_t iClientIdentifier = pClient->iClientId;
		ENetPeer* pPeer = pClient->pPeer;
		bool bHandshakeComplete = pClient->bHandshakeComplete;
		int64_t iCorruptViolations = pClient->iCorruptViolations;
		int64_t iRateViolations = pClient->iRateViolations;
		int64_t iPacketCount = pClient->iTickPacketCount;
		int64_t iByteCount = pClient->iTickByteCount;
		static constexpr uint8_t kuiPacketType = static_cast<uint8_t>(engine::PacketType::kClientAcknowledgmentStream);
		uint16_t uiTypeCount = pClient->uiTickTypeCounts[kuiPacketType];
		int64_t iClientTimestampNanoseconds = pClient->iClientTimestampNanoseconds;
		int64_t iConsecutiveZeroAdvanceAcknowledgments = pClient->iConsecutiveZeroAdvanceAcks;
		bool bFloorStalled = pClient->bFloorStalled;
		int64_t iPeakConsecutiveStallAcknowledgments = pClient->iPeakConsecutiveStallAcks;
		std::vector<engine::AckState> acknowledgmentStates;
		acknowledgmentStates.reserve(pClient->slots.size());
		for (const engine::ClientConnection::SlotState& rSlot : pClient->slots)
		{
			acknowledgmentStates.push_back(rSlot.ack);
		}

		common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
		common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
		engine::NetworkMessages::ClientAckStreamMessage message {};
		engine::NetworkMessages::Write(rWorkbuffer, message);
		std::span<const uint8_t> packetData(reinterpret_cast<const uint8_t*>(rWorkbuffer.View().data()), rWorkbuffer.View().size());
		int64_t iPacketSize = static_cast<int64_t>(packetData.size());
		if (iPacketSize != engine::NetworkMessages::ClientAckStreamMessage::kiFixedSize)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture failed to serialize the fixed ACK layout");
		}

		{
			pClient->bHandshakeComplete = false;
			common::ScopedLambda restoreHandshake([iClientIdentifier]()
			{
				if (engine::ClientConnection* pRestoreClient = engine::gpServer->FindClient(iClientIdentifier); pRestoreClient != nullptr)
				{
					pRestoreClient->bHandshakeComplete = true;
				}
			});
			engine::gpServer->Receive(packetData, pPeer);
		}

		pClient = engine::gpServer->FindClient(iClientIdentifier);
		if (pClient == nullptr)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture client identity changed during receive");
		}
		if (pClient->pPeer != pPeer)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture client identity changed during receive");
		}

		bool bAcknowledgmentSlotsUnchanged = pClient->slots.size() == acknowledgmentStates.size();
		for (int64_t i = 0; bAcknowledgmentSlotsUnchanged && i < std::ssize(pClient->slots); ++i)
		{
			const engine::AckState& rBefore = acknowledgmentStates.at(i);
			const engine::AckState& rAfter = pClient->slots.at(i).ack;
			bAcknowledgmentSlotsUnchanged = rAfter.iAcknowledgmentFloor == rBefore.iAcknowledgmentFloor && rAfter.uiReceivedBitfieldLow == rBefore.uiReceivedBitfieldLow && rAfter.uiReceivedBitfieldHigh == rBefore.uiReceivedBitfieldHigh && rAfter.uiEpoch == rBefore.uiEpoch;
		}

		bool bAdmissionAdvanced = pClient->iTickPacketCount == iPacketCount + 1
		                       && pClient->iTickByteCount == iByteCount + engine::NetworkMessages::ClientAckStreamMessage::kiFixedSize;
		bool bHandshakeRestored = bHandshakeComplete && pClient->bHandshakeComplete;
		bool bTypeCountUnchanged = pClient->uiTickTypeCounts[kuiPacketType] == uiTypeCount;
		bool bAcknowledgmentStallUnchanged = pClient->iConsecutiveZeroAdvanceAcks == iConsecutiveZeroAdvanceAcknowledgments
		                                  && pClient->bFloorStalled == bFloorStalled
		                                  && pClient->iPeakConsecutiveStallAcks == iPeakConsecutiveStallAcknowledgments;
		bool bTimestampUnchanged = pClient->iClientTimestampNanoseconds == iClientTimestampNanoseconds;
		bool bContractViolationsUnchanged = pClient->iCorruptViolations == iCorruptViolations && pClient->iRateViolations == iRateViolations;
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
		if (!bAcknowledgmentSlotsUnchanged)
		{
			throw std::runtime_error("server_pre_handshake_ack_fixture observed unexpected state mutation");
		}
		if (!bAcknowledgmentStallUnchanged)
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

		rResult["clientId"] = iClientIdentifier;
		rResult["type"] = kuiPacketType;
		rResult["size"] = iPacketSize;
		rResult["packetCountBefore"] = iPacketCount;
		rResult["packetCountAfter"] = pClient->iTickPacketCount;
		rResult["byteCountBefore"] = iByteCount;
		rResult["byteCountAfter"] = pClient->iTickByteCount;
		rResult["handshakeRestored"] = bHandshakeRestored;
		rResult["clientPreserved"] = true;
		rResult["peerPreserved"] = true;
		rResult["typeCountUnchanged"] = bTypeCountUnchanged;
		rResult["ackSlotsUnchanged"] = bAcknowledgmentSlotsUnchanged;
		rResult["ackStallUnchanged"] = bAcknowledgmentStallUnchanged;
		rResult["timestampUnchanged"] = bTimestampUnchanged;
		rResult["contractViolationsUnchanged"] = bContractViolationsUnchanged;
	}
}

} // namespace game

#endif // defined(BT_SERVER)
