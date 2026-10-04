#pragma once

namespace game
{

// rParameters is the request "params" object; rResult receives the response "result" object.
// Handler exceptions propagate to engine::AgentCommandServer::Drain(), which formats the failure envelope.
// Engine calling game:: is sanctioned; nlohmann::json arrives through the game Pch's ExternalHeaders BT_ENGINE gate.
void ExecuteAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);

#if defined(BT_CLIENT)
// One agent-supplied grid coordinate value, named by command in the failure message. Accepts the full
// signed-int32 identity domain and throws for an integer outside it or a non-integer.
int32_t ClientGridCoordinateValue(const nlohmann::json& rValue, std::string_view command);

// Defined in AgentCommandsClient.cpp, which is compiled only into the client; returns true when handled.
// Handler exceptions propagate to engine::AgentCommandServer::Drain().
bool ExecuteAgentCommandClient(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);

#endif

#if defined(BT_SERVER)
// Defined in AgentCommandsServer.cpp, which is compiled only into the server; returns true when handled.
// Handler exceptions propagate to engine::AgentCommandServer::Drain().
bool ExecuteAgentCommandServer(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);
#endif

} // namespace game
