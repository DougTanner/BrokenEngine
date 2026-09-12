#pragma once

#include "Frame/GridCoord.h"

namespace engine
{

// Engine-generic agent commands shared by every game project: ping, quit, get_logs, set_log_level,
// crash_report_fixture, cell_coordinate_probe.
// Returns true when cmd was handled. iGameTick is the game's current tick (-1 before game creation), reported by ping.
// Throws on invalid params (external trust boundary); AgentCommandServer::Drain() converts to the failure envelope.
bool ExecuteSharedAgentCommand(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult, int64_t iGameTick);

// The one agent position shape, shared by every command that reports one: centered cell-local meters under a
// "local" key, and — only where the rows of a report span cells — the owning cell beside it under "coord".
// Absolute world meters are never reported; a consumer that wants them derives them from the pair.
nlohmann::json AgentCoordJson(GridCoord coord);
nlohmann::json AgentLocalPositionJson(FXMVECTOR vecLocalPosition);

} // namespace engine
