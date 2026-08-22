#include "Pch.h"

#if defined(BT_SERVER)

#include "Frame/ServerCellStats.h"

#include "Frame/Collections/Blasters/Blasters.h"
#include "Frame/Collections/Missiles/Missiles.h"
#include "Frame/Collections/Players/Players.h"
#include "Frame/Collections/Spaceships/Spaceships.h"
#include "Game.h"
#include "Profile/ProfileManager.h"

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

	for (const engine::GridCoord& rCoord : gpGame->mActiveCoords)
	{
		ServerCellStats cellStats = GetServerCellStats(gpGame->CurrentFrame(rCoord));
		iTotalPlayers += cellStats.iPlayers;
		iTotalSpaceships += cellStats.iSpaceships;
		iTotalBlasters += cellStats.iBlasters;
		iTotalMissiles += cellStats.iMissiles;
	}

	gpProfileManager->SetCount(kCpuCounterPlayers, iTotalPlayers);
	gpProfileManager->SetCount(kCpuCounterSpaceships, iTotalSpaceships);
	gpProfileManager->SetCount(kCpuCounterBlasters, iTotalBlasters);
	gpProfileManager->SetCount(kCpuCounterMissiles, iTotalMissiles);
}

} // namespace game

#endif // BT_SERVER
