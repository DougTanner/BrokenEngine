#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// Engine-generic client agent commands shared by every game project: capture, window state, UI inspection,
// synthetic input, the client GPU profile query, and presentation_continuity_probe.
// Returns true when command was handled.
// Throws on invalid params; AgentCommandServer::Drain() converts to the failure envelope.
bool ExecuteClientAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);

const char* UiStateName(UiState eState);
nlohmann::json GameFlagNames(GameFlags_t flags);

} // namespace engine

#endif // defined(BT_CLIENT)
