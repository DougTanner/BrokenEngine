#pragma once

#if defined(BT_SERVER)

namespace game
{

struct Frame;

// One cell's monitoring snapshot; the tick/time are the ones the cell last advanced to.
struct ServerCellStats
{
	int64_t iTick = 0;
	std::chrono::duration<float> durationCurrentTime = std::chrono::duration<float>::zero();
	int64_t iPlayers = 0;
	int64_t iSpaceships = 0;
	int64_t iBlasters = 0;
	int64_t iMissiles = 0;
	int64_t iExplosions = 0;
};

[[nodiscard]] ServerCellStats GetServerCellStatistics(const Frame& rFrame);

// Sums the active cells into the game entity counters the profile overlay and the agent profile query read.
void PublishServerEntityCounts();

} // namespace game

#endif // BT_SERVER
