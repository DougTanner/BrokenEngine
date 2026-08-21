#pragma once

#if defined(BT_CLIENT)

namespace engine
{

// Engine-generic client agent commands shared by every game project: capture, window state, UI inspection,
// synthetic input, and the client GPU profile query.
// Returns true when cmd was handled. rGame and rProfileManager supply the live state the handlers report; the
// game dispatcher owns the globals they come from.
// Throws on invalid params (external trust boundary); AgentCommandServer::Drain() converts to the failure envelope.
bool ExecuteClientAgentCommand(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult, const GameBase& rGame, ProfileManagerBase& rProfileManager);

const char* UiStateName(UiState eState);
nlohmann::json GameFlagNames(GameFlags_t flags);

} // namespace engine

#endif // defined(BT_CLIENT)
