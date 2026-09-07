#pragma once

namespace game
{

// Dispatches a single agent command by name. rParams is the request "params" object, rResult the response
// "result" object to populate. Tries the engine shared commands first (engine::ExecuteSharedAgentCommand —
// ping/quit/get_logs/set_log_level/crash_report_fixture), then falls through to the side-specific BT_CLIENT/BT_SERVER handlers.
// Throws (std::runtime_error / nlohmann type/parse errors) on any failure — unknown command, missing/mistyped
// params, unknown category or level, invalid regex — which the engine AgentCommandServer::Drain() catches and
// formats into the failure envelope. Engine calling game:: is the sanctioned direction. nlohmann::json arrives
// via the game Pch (ExternalHeaders BT_ENGINE gate).
void ExecuteAgentCommand(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult);

#if defined(BT_CLIENT)
// Client-only command dispatch (network fixtures, full-state fixture, scene query, desync probe, and grid-cell move). ExecuteAgentCommand
// falls through to this under BT_CLIENT after engine::ExecuteClientAgentCommand and before the unknown-command throw;
// returns true if handled. Defined in the client-vcxproj-only AgentCommandsClient.cpp. Throws on bad params (trust
// boundary), caught by AgentCommandServer::Drain().
bool ExecuteAgentCommandClient(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult);

// Delivers the malformed packet client_packet_fault_fixture armed, then disarms; a no-op when nothing is armed.
// Must be called from the client main loop outside AgentCommandServer::Drain, whose catch would otherwise swallow
// the corrupt-stream ASSERT the delivery is meant to raise. A game-range packet is parsed here too, because the
// engine only queues it and the next client poll clears that queue first. Defined in AgentCommandsClient.cpp.
void InjectArmedClientPacketFault();
#endif

#if defined(BT_SERVER)
// Server-only command dispatch (status / pause / timescale / save / load / reset / replay_record / replay_play /
// replay_drop_retained_end_frame / CPU query_profile).
// ExecuteAgentCommand falls through to this under BT_SERVER before the unknown-command throw; returns true if
// handled. Defined in the server-vcxproj-only AgentCommandsServer.cpp. Throws on bad params (trust boundary), caught
// by AgentCommandServer::Drain().
bool ExecuteAgentCommandServer(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult);
#endif

} // namespace game
