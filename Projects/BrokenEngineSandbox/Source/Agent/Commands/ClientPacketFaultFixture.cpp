#include "Agent/Commands/ClientPacketFaultFixture.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSession.h"
#include "Network/Client/ClientSessionRuntime.h"
#include "Network/GamePacketType.h"
#include "Network/NetworkMessages.h"

namespace game
{

namespace
{

ClientSession* spSession = nullptr;
std::vector<uint8_t> sArmedPacketFault;

} // namespace

void CommandClientPacketFaultFixture([[maybe_unused]] const nlohmann::json& rParams, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_packet_fault_fixture requires kbDebugInput build");
	}
	else
	{
		// Heap: validation errors, the armed packet buffer, and the JSON result
		ScopedSuppressAllocationTracking suppress;

		if (!rParams.is_object())
		{
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope|status_change|game_packet\"}");
		}
		if (rParams.size() != 1)
		{
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope|status_change|game_packet\"}");
		}
		if (!rParams.contains("case"))
		{
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope|status_change|game_packet\"}");
		}
		if (!rParams.at("case").is_string())
		{
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope|status_change|game_packet\"}");
		}
		const std::string caseName = rParams.at("case").get<std::string>();
		if (caseName != "engine_envelope" && caseName != "status_change" && caseName != "game_packet")
		{
			throw std::runtime_error("client_packet_fault_fixture 'case' must be engine_envelope|status_change|game_packet");
		}
		if (!sArmedPacketFault.empty())
		{
			throw std::runtime_error("client_packet_fault_fixture is already armed");
		}
		if (gpGame == nullptr)
		{
			throw std::runtime_error("client_packet_fault_fixture requires a connected client");
		}
		if (gpClientSession == nullptr)
		{
			throw std::runtime_error("client_packet_fault_fixture requires a connected client");
		}
		if (gpClientSession->mpRuntime->mpClient == nullptr)
		{
			throw std::runtime_error("client_packet_fault_fixture requires a connected client");
		}
		if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
		{
			throw std::runtime_error("client_packet_fault_fixture requires a connected client");
		}
		engine::Client& rClient = *gpClientSession->mpRuntime->mpClient;

		std::vector<uint8_t> packet;
		if (caseName == "engine_envelope")
		{
			packet = {static_cast<uint8_t>(engine::PacketType::kServerCoordFullState), 0, 0};
		}
		else if (caseName == "status_change")
		{
			int64_t iSlot = -1;
			for (int64_t i = 0; i < std::ssize(rClient.mCoordSlots); ++i)
			{
				if (rClient.mCoordSlots.at(i).eState == engine::CoordSubscriptionState::kActive)
				{
					iSlot = i;
					break;
				}
			}
			if (iSlot < 0)
			{
				throw std::runtime_error("client_packet_fault_fixture 'status_change' requires an active coord slot");
			}

			uint8_t payloadBytes[8] {};
			constexpr int32_t kiOutOfRangeUncompressedSize = std::numeric_limits<int32_t>::max();
			std::memcpy(payloadBytes, &kiOutOfRangeUncompressedSize, sizeof(int32_t));

			engine::NetworkMessages::ServerCoordUpdateMessage message {};
			message.uiLoadGeneration = rClient.muiCommittedLoadGeneration;
			message.uiSlotIndex = static_cast<uint8_t>(iSlot);
			message.uiEpoch = rClient.mCoordSlots.at(iSlot).ackState.uiEpoch;
			message.iTick = gpGame->TickCounter();
			message.iEchoedTimestampNs = 0;
			message.compressedPayload = {.pData = payloadBytes, .iSize = static_cast<int32_t>(sizeof(payloadBytes))};

			common::Workbuffer& rWorkbuffer = common::gpThreadLocal->mWorkbuffer;
			common::ScopedWorkbufferArena scopedWorkbufferArena = rWorkbuffer.Push();
			engine::NetworkMessages::Write(rWorkbuffer, message);
			std::string_view built = rWorkbuffer.View();
			const uint8_t* pBuilt = reinterpret_cast<const uint8_t*>(built.data());
			packet.assign(pBuilt, pBuilt + built.size());
		}
		else
		{
			packet = {static_cast<uint8_t>(GamePacketType::kServerAssignPlayer), 0};
		}

		rResult["case"] = caseName;
		rResult["type"] = packet.front();
		rResult["size"] = std::ssize(packet);
		rResult["armed"] = true;
		spSession = gpClientSession;
		sArmedPacketFault = std::move(packet);
	}
}

void InjectArmedClientPacketFault()
{
	if (sArmedPacketFault.empty()) [[likely]]
	{
		return;
	}

	// Heap: the armed buffer moves out and is released here, so the disarm survives a fatal dispatch
	ScopedSuppressAllocationTracking suppress;
	std::vector<uint8_t> packet = std::move(sArmedPacketFault);
	sArmedPacketFault.clear();
	ClientSession* pSession = spSession;
	spSession = nullptr;

	if (pSession == nullptr)
	{
		return;
	}
	if (gpClientSession != pSession)
	{
		return;
	}
	if (pSession->mpRuntime->mpClient == nullptr)
	{
		return;
	}
	const bool bGamePacket = packet.front() >= static_cast<uint8_t>(engine::PacketType::kGamePacketStart);
	pSession->mpRuntime->mpClient->Receive(packet);
	if (bGamePacket)
	{
		pSession->ProcessReceivedGamePackets();
	}
}

void ResetClientPacketFaultFixture(ClientSession& rSession)
{
	if (spSession == &rSession)
	{
		sArmedPacketFault.clear();
		spSession = nullptr;
	}
}

void DetachClientPacketFaultFixture(ClientSession& rSession)
{
	ResetClientPacketFaultFixture(rSession);
}

} // namespace game

#endif // BT_CLIENT
