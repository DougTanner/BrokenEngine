#pragma once

#if defined(BT_SERVER)

namespace engine
{

struct GridCoord;

} // namespace engine

namespace game
{

class ServerSession;
struct StatusChange;

struct ReplayTransferCaptureCounts
{
	int64_t iFighterCount = 0;
	int64_t iOmniCount = 0;
	int64_t iBattleshipCount = 0;
	int64_t iBlasterCount = 0;
	int64_t iMissileCount = 0;
};

bool ExecuteServerSimulationHarnessRigCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);
void QueueAgentStatusChange(const ServerSession& rSession, engine::GridCoord coordinate, int64_t iTick, const StatusChange& rChange);
void DrainPendingAgentStatusChanges(const ServerSession& rSession);
void ResetPendingAgentStatusChanges(const ServerSession& rSession);
int64_t CountPendingAgentStatusChanges(const ServerSession& rSession);
void DetachServerSimulationHarnessRigs(const ServerSession& rSession);
void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts);

} // namespace game

#endif // BT_SERVER
