#pragma once

#if defined(BT_SERVER)

namespace game
{

// Packet fault harness rig commands; frame-read queries remain in AgentCommandsServerQueries.cpp, while the dispatcher
// and other server handlers remain in AgentCommandsServer.cpp.
void CommandGamePacketFaultHarnessRig(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandEnginePacketFaultHarnessRig(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandServerPreHandshakeAcknowledgmentHarnessRig(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_SERVER)
