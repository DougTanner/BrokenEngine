#include "Pch.h"

#if defined(BT_SERVER)

#include "Frame/ServerCellStats.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Profile/ProfileManager.h"
#include "Game.h"

namespace game
{

ServerCellStats GetServerCellStats(const Frame& rFrame)
{
	return ServerCellStats
	{
		.iTick = rFrame.interpolate.iTick,
		.fCurrentTime = rFrame.interpolate.fCurrentTime,
		.iPlayers = rFrame.interpolate.pPlayers->iCount,
		.iSpaceships = rFrame.interpolate.pSpaceships->iCount,
		.iBlasters = rFrame.interpolate.pBlasters->iCount,
		.iMissiles = rFrame.interpolate.pMissiles->iCount,
		.iExplosions = rFrame.interpolate.explosions.iCount,
	};
}

void PublishServerEntityCounts()
{
	if constexpr (!kbProfiling)
	{
		return;
	}

	int64_t iTotalPlayers = 0;
	int64_t iTotalSpaceships = 0;
	int64_t iTotalBlasters = 0;
	int64_t iTotalMissiles = 0;

	for (const engine::GridCoord& rCoord : gpGame->mActiveCoordinates)
	{
		ServerCellStats cellStats = GetServerCellStats((*gpGame->mCoordinateFrames.at(rCoord).pCurrent));
		iTotalPlayers += cellStats.iPlayers;
		iTotalSpaceships += cellStats.iSpaceships;
		iTotalBlasters += cellStats.iBlasters;
		iTotalMissiles += cellStats.iMissiles;
	}

	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterPlayers).iCount = iTotalPlayers;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterSpaceships).iCount = iTotalSpaceships;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterBlasters).iCount = iTotalBlasters;
	}
	if constexpr (kbProfiling)
	{
		gpProfileManager->GetCpuCounter(kCpuCounterMissiles).iCount = iTotalMissiles;
	}
}

} // namespace game

#endif // BT_SERVER
