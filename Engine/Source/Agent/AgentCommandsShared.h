#pragma once

#include "Frame/GridCoord.h"

namespace engine
{

// Engine-generic agent commands shared by every game project: ping, quit, get_logs, set_log_level,
// crash_report_fixture, cell_coordinate_probe.
// Returns true when command was handled. iGameTick is the game's current tick (-1 before game creation), reported by ping.
// Throws on invalid params; AgentCommandServer::Drain() converts to the failure envelope.
bool ExecuteSharedAgentCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult, int64_t iGameTick);

// The one agent position shape, shared by every command that reports one: centered cell-local meters under a
// "local" key, and — only where the rows of a report span cells — the owning cell beside it under "coord".
// Absolute world meters are never reported; a consumer that wants them derives them from the pair.
nlohmann::json AgentCoordinateJson(GridCoord coordinate);
nlohmann::json AgentLocalPositionJson(FXMVECTOR vecLocalPosition);

// Read one grid-coordinate element; name prefixes the thrown message. The whole signed-int32 domain
// is a legal cell identity, so the only rejected values are non-integers and integers no GridCoord can hold.
int64_t AgentGridCoordinateValue(const nlohmann::json& rValue, std::string_view name);

} // namespace engine
