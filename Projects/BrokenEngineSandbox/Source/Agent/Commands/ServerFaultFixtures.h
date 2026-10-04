#pragma once

#if defined(BT_SERVER)

namespace game
{

// Packet fault fixture commands; frame-read queries remain in AgentCommandsServerQueries.cpp, while the dispatcher
// and other server handlers remain in AgentCommandsServer.cpp.
void CommandGamePacketFaultFixture(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandEnginePacketFaultFixture(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandServerPreHandshakeAcknowledgmentFixture(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_SERVER)
