#include "Agent/Commands/ClientPacketFaultHarnessRig.h"

#if defined(BT_CLIENT)

#include "Network/Client/Client.h"
#include "Network/Client/ClientSessionRuntime.h"
#include "Network/NetworkMessages.h"

#include "Network/Client/ClientSession.h"
#include "Game.h"

namespace game
{

static ClientSession* spSession = nullptr;
static std::vector<uint8_t> sArmedPacketFault;

void CommandClientPacketFaultHarnessRig([[maybe_unused]] const nlohmann::json& rParameters, [[maybe_unused]] nlohmann::json& rResult)
{
	if constexpr (!kbDebugInput)
	{
		throw std::runtime_error("client_packet_fault_harness_rig requires kbDebugInput build");
	}
	else
	{
		// Heap: validation errors, the armed packet buffer, and the JSON result
		ScopedSuppressAllocationTracking suppress;

		if (!rParameters.is_object())
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires exactly {\"case\":\"engine_envelope\"}");
		}
		if (rParameters.size() != 1)
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires exactly {\"case\":\"engine_envelope\"}");
		}
		if (!rParameters.contains("case"))
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires exactly {\"case\":\"engine_envelope\"}");
		}
		if (!rParameters.at("case").is_string())
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires exactly {\"case\":\"engine_envelope\"}");
		}
		std::string caseName = rParameters.at("case").get<std::string>();
		if (caseName != "engine_envelope")
		{
			throw std::runtime_error("client_packet_fault_harness_rig 'case' must be engine_envelope");
		}
		if (!sArmedPacketFault.empty())
		{
			throw std::runtime_error("client_packet_fault_harness_rig is already armed");
		}
		if (gpGame == nullptr)
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires a connected client");
		}
		if (gpClientSession == nullptr)
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires a connected client");
		}
		if (gpClientSession->mpRuntime->mpClient == nullptr)
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires a connected client");
		}
		if (!(gpClientSession->mpRuntime->mpClient->mStateFlags & engine::Client::ClientStateFlags::kConnected))
		{
			throw std::runtime_error("client_packet_fault_harness_rig requires a connected client");
		}

		std::vector<uint8_t> packet = {static_cast<uint8_t>(engine::PacketType::kServerCoordinateFullState), 0, 0};

		rResult["case"] = std::move(caseName);
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

void ResetClientPacketFaultHarnessRig(const ClientSession& rSession)
{
	if (spSession == &rSession)
	{
		sArmedPacketFault.clear();
		spSession = nullptr;
	}
}

} // namespace game

#endif // BT_CLIENT
