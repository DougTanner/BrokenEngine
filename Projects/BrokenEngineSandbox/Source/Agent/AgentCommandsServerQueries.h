#pragma once

#if defined(BT_SERVER)

namespace game
{

// rParameters is the request "params" object; rResult receives the response "result" object.
// Invalid parameters throw into AgentCommandServer::Drain()'s failure envelope.
// Injection commands are in Commands/ServerSimulationHarnessRigs.cpp;
// frame edits and reads are in Commands/ServerFrameEdit.cpp, which shares the queries' cell-frame lookup.
// AgentCommandsServer.cpp owns the status and load take-overs and the server command dispatch.
Frame& QueryFrame(const nlohmann::json& rParameters);
void CommandQueryFrame(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandQueryOmnis(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandQueryCollection(const nlohmann::json& rParameters, nlohmann::json& rResult);

} // namespace game

#endif // defined(BT_SERVER)
