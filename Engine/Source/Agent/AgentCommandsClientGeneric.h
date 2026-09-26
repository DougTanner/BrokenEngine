#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// Engine-generic client agent commands shared by every game project: capture, window state, UI inspection,
// synthetic input, the client GPU profile query, and presentation_continuity_probe.
// Returns true when cmd was handled.
// Throws on invalid params (external trust boundary); AgentCommandServer::Drain() converts to the failure envelope.
bool ExecuteClientAgentCommand(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult);

const char* UiStateName(UiState eState);
nlohmann::json GameFlagNames(GameFlags_t flags);

} // namespace engine

#endif // defined(BT_CLIENT)
