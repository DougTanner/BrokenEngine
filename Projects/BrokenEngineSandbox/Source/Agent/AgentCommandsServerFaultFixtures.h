#pragma once

#if defined(BT_SERVER)

namespace game
{

// Packet fault fixture commands; frame-read queries remain in AgentCommandsServerQueries.cpp, while the dispatcher
// and other server handlers remain in AgentCommandsServer.cpp.
void CommandGamePacketFaultFixture(const nlohmann::json& rParams, nlohmann::json& rResult);
void CommandEnginePacketFaultFixture(const nlohmann::json& rParams, nlohmann::json& rResult);
void CommandServerPreHandshakeAckFixture(const nlohmann::json& rParams, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_SERVER)
