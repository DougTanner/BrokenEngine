#pragma once

#if defined(BT_SERVER)

namespace game
{

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
void DrainPendingAgentStatusChanges();
void ResetPendingAgentStatusChanges();
int64_t CountPendingAgentStatusChanges();
void CountCapturedReplayTransfers(std::span<const StatusChange> transfers, ReplayTransferCaptureCounts& rCounts);

} // namespace game

#endif // BT_SERVER
