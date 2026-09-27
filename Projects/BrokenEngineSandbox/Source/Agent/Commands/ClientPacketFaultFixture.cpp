#include "Agent/Commands/ClientPacketFaultFixture.h"

#if defined(BT_CLIENT)

#include "Game.h"
#include "Network/Client/Client.h"
#include "Network/Client/ClientSession.h"
#include "Network/Client/ClientSessionRuntime.h"
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
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope\"}");
		}
		if (rParams.size() != 1)
		{
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope\"}");
		}
		if (!rParams.contains("case"))
		{
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope\"}");
		}
		if (!rParams.at("case").is_string())
		{
			throw std::runtime_error("client_packet_fault_fixture requires exactly {\"case\":\"engine_envelope\"}");
		}
		std::string caseName = rParams.at("case").get<std::string>();
		if (caseName != "engine_envelope")
		{
			throw std::runtime_error("client_packet_fault_fixture 'case' must be engine_envelope");
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

		std::vector<uint8_t> packet = {static_cast<uint8_t>(engine::PacketType::kServerCoordFullState), 0, 0};

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
	pSession->mpRuntime->mpClient->Receive(packet);
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
