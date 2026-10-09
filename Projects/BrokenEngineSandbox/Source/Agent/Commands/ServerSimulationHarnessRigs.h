#pragma once

#if defined(BT_SERVER)

namespace engine
{

class ServerTransferManager;
struct GridCoord;

} // namespace engine

namespace game
{

class ServerSession;
struct StatusChange;

struct ReplayTransferCaptureCounts
{
	int64_t iPlayerCount = 0;
	int64_t iSpaceshipCount = 0;
	int64_t iBlasterCount = 0;
	int64_t iMissileCount = 0;
};

bool ExecuteServerSimulationHarnessRigCommand(std::string_view command, const nlohmann::json& rParameters, nlohmann::json& rResult);
void QueueAgentStatusChange(const ServerSession& rSession, engine::GridCoord coordinate, int64_t iTick, const StatusChange& rChange);
bool QueueReplayTransferHarnessRig(const ServerSession& rSession, engine::GridCoord destination, int64_t iTick, StatusChange transfer);
void DrainPendingAgentStatusChanges(const ServerSession& rSession);
void DrainReplayTransferHarnessRigs(const ServerSession& rSession, engine::ServerTransferManager& rTransferManager);
void ResetPendingAgentStatusChanges(const ServerSession& rSession);
void ResetReplayTransferHarnessRigs(const ServerSession& rSession);
int64_t CountPendingAgentStatusChanges(const ServerSession& rSession);
int64_t CountReplayTransferHarnessRigs(const ServerSession& rSession);
void DetachServerSimulationHarnessRigs(const ServerSession& rSession);
void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts);

} // namespace game

#endif // BT_SERVER
