#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// Engine-generic client agent commands shared by every game project: capture, window state, UI and scene inspection,
// synthetic input, the client GPU profile query, presentation_continuity_probe, and the network, desync,
// grid-coordinate, and audio streaming harness rigs.
// Returns true when command was handled.
// Throws on invalid params; AgentCommandServer::Drain() converts to the failure envelope.
bool ExecuteClientAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);

const char* UiStateName(UiState eState);
nlohmann::json GameFlagNames(GameFlags_t flags);

// The connected client once the server has accepted it; otherwise throws, named by command.
Client& RequireAcceptedClient(std::string_view command);

// Writes the uiState, tweaksVisible, and gameFlags fields that describe_ui and describe_scene share.
void AppendUiStateFields(nlohmann::json& rResult);

} // namespace engine

#endif // defined(BT_CLIENT)
