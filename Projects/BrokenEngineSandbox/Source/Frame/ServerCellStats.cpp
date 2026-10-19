#include "Pch.h"

#if defined(BT_SERVER)

#include "Frame/ServerCellStats.h"

#include "Frame/Collections/Battleships/Battleships.h"
#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Fighters/Fighters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Omnis/Omnis.h"
#include "Profile/ProfileRows.h"
#include "Game.h"

namespace game
{

ServerCellStats GetServerCellStatistics(const Frame& rFrame)
{
	ServerCellStats cellStatistics
	{
		.iTick = rFrame.interpolate.iTick,
		.durationCurrentTime = std::chrono::duration<float>(common::NanosecondsToFloatSeconds<float>(engine::TickToSimulationTime(rFrame.interpolate.iTick).time_since_epoch())),
		.iExplosions = rFrame.interpolate.explosions.iCount,
	};
	cellStatistics.entityCounts[std::to_underlying(ServerEntityCount::kFighters)] = rFrame.interpolate.pFighters->iCount;
	cellStatistics.entityCounts[std::to_underlying(ServerEntityCount::kOmnis)] = rFrame.interpolate.pOmnis->iCount;
	cellStatistics.entityCounts[std::to_underlying(ServerEntityCount::kBattleships)] = rFrame.interpolate.pBattleships->iCount;
	cellStatistics.entityCounts[std::to_underlying(ServerEntityCount::kBlasters)] = rFrame.interpolate.pBlasters->iCount;
	cellStatistics.entityCounts[std::to_underlying(ServerEntityCount::kMissiles)] = rFrame.interpolate.pMissiles->iCount;
	return cellStatistics;
}

void PublishServerEntityCounts()
{
	if constexpr (!kbProfiling)
	{
		return;
	}

	std::array<int64_t, std::size(kServerEntityCountNames)> totalEntityCounts {};

	for (const engine::GridCoord& rCoordinate : gpGame->mActiveCoordinates)
	{
		ServerCellStats cellStatistics = GetServerCellStatistics((*gpGame->mCells.at(rCoordinate).pCurrent));
		for (int64_t i = 0; i < std::ssize(totalEntityCounts); ++i)
		{
			totalEntityCounts[i] += cellStatistics.entityCounts[i];
		}
	}

	gpProfileManager->GetCpuCounter(kCpuCounterFighters).iCount = totalEntityCounts[std::to_underlying(ServerEntityCount::kFighters)];
	gpProfileManager->GetCpuCounter(kCpuCounterOmnis).iCount = totalEntityCounts[std::to_underlying(ServerEntityCount::kOmnis)];
	gpProfileManager->GetCpuCounter(kCpuCounterBattleships).iCount = totalEntityCounts[std::to_underlying(ServerEntityCount::kBattleships)];
	gpProfileManager->GetCpuCounter(kCpuCounterBlasters).iCount = totalEntityCounts[std::to_underlying(ServerEntityCount::kBlasters)];
	gpProfileManager->GetCpuCounter(kCpuCounterMissiles).iCount = totalEntityCounts[std::to_underlying(ServerEntityCount::kMissiles)];
}

} // namespace game

#endif // BT_SERVER
