#pragma once

#if defined(BT_SERVER)

namespace engine
{

class ServerTransferManager;
struct GridCoord;

}

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

bool ExecuteServerSimulationFixtureCommand(std::string_view cmd, const nlohmann::json& rParams, nlohmann::json& rResult);
void QueueAgentStatusChange(ServerSession& rSession, engine::GridCoord coord, const StatusChange& rChange);
bool QueueReplayTransferFixture(ServerSession& rSession, engine::GridCoord destination, StatusChange transfer);
void DrainPendingAgentStatusChanges(ServerSession& rSession);
void DrainReplayTransferFixtures(ServerSession& rSession, engine::ServerTransferManager& rTransferManager);
void ResetPendingAgentStatusChanges(ServerSession& rSession);
void ResetReplayTransferFixtures(ServerSession& rSession);
void DetachServerSimulationFixtures(ServerSession& rSession);
void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts);

} // namespace game

#endif // BT_SERVER
