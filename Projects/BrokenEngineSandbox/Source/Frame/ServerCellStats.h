#pragma once

#if defined(BT_SERVER)

namespace game
{

struct Frame;

// Row labels of ServerCellStats::entityCounts, indexed by ServerEntityCount.
inline constexpr std::array<const char*, 5> kServerEntityCountNames {"Fighters", "Omnis", "Battleships", "Blasters", "Missiles"};
enum class ServerEntityCount : uint8_t { kFighters, kOmnis, kBattleships, kBlasters, kMissiles };

// One cell's monitoring snapshot; the tick/time are the ones the cell last advanced to.
struct ServerCellStats
{
	int64_t iTick = 0;
	std::chrono::duration<float> durationCurrentTime = std::chrono::duration<float>::zero();
	std::array<int64_t, std::size(kServerEntityCountNames)> entityCounts {};
	int64_t iExplosions = 0;
};

[[nodiscard]] ServerCellStats GetServerCellStatistics(const Frame& rFrame);

// Sums the active cells into the game entity counters the profile overlay and the agent profile query read.
void PublishServerEntityCounts();

} // namespace game

#endif // BT_SERVER
