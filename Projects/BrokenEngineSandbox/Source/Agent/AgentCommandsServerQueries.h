#pragma once

#if defined(BT_SERVER)

namespace game
{

// rParameters is the request "params" object; rResult receives the response "result" object.
// Invalid parameters throw into AgentCommandServer::Drain()'s failure envelope.
// Fault fixtures are in Commands/ServerFaultFixtures.cpp; injection commands are in Commands/ServerSimulationFixtures.cpp;
// frame edits are in Commands/ServerFrameEdit.cpp, which shares the queries' cell-frame lookup.
// AgentCommandsServer.cpp owns simulation control, dispatch, and the coordinate parser shared with injection commands.
engine::GridCoord CoordinateFromParameter(const nlohmann::json& rParameters, std::string_view key = "coord");
Frame& QueryFrame(const nlohmann::json& rParameters);
void CommandQueryFrame(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandQueryPlayers(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandQueryCollection(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_SERVER)
