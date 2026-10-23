#pragma once

namespace game
{

enum GameCpuCounters : int64_t
{
	kGameCpuCounterFirst = engine::kEngineCpuCounterCount,
	kGameCpuCounterCount = kGameCpuCounterFirst,
};

enum GameCpuTimers : int64_t
{
	kCpuTimerFrameUpdate = engine::kEngineCpuTimerCount,
	kGameCpuTimerCount,
};

inline constexpr std::array<std::string_view, 0> kGameCpuCounterNames {};
inline constexpr std::array<std::string_view, 1> kGameCpuTimerNames {"Viewer frame"};
static_assert(kGameCpuCounterNames.size() == static_cast<size_t>(kGameCpuCounterCount) - static_cast<size_t>(engine::kEngineCpuCounterCount));
static_assert(kGameCpuTimerNames.size() == static_cast<size_t>(kGameCpuTimerCount) - static_cast<size_t>(engine::kEngineCpuTimerCount));

} // namespace game
