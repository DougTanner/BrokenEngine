#pragma once

#if defined(BT_SERVER)

namespace engine
{

namespace ReplayHarnessRigs
{

struct TransferCaptureSnapshot;

} // namespace ReplayHarnessRigs

// Engine-generic server agent commands shared by every game project: simulation control, save/load/reset, the server
// CPU profile query, create_cell, and the replay commands.
// Returns true when command was handled.
// Throws on invalid params; AgentCommandServer::Drain() converts to the failure envelope.
bool ExecuteServerAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);

// The engine part of status, load, and replay_transfer_capture, for a game handler that takes one of those commands
// over and appends its own fields. CommandReplayTransferCapture returns the snapshot its events were written from.
void CommandStatus(const nlohmann::json& rParameters, nlohmann::json& rResult);
void CommandLoad(const nlohmann::json& rParameters, nlohmann::json& rResult);
ReplayHarnessRigs::TransferCaptureSnapshot CommandReplayTransferCapture(const nlohmann::json& rParameters, nlohmann::json& rResult);

// No replay records the command's change, so a recording or playback would diverge from the live frame.
void RefuseDuringReplay(std::string_view command);

} // namespace engine

#endif // defined(BT_SERVER)
