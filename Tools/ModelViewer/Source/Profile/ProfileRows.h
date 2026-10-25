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

inline constexpr std::string_view kpGameCpuCounterNames[]
{
	"",
};
// C++ has no zero-length array; the placeholder is never read because the viewer defines no game counters.
static_assert(std::size(kpGameCpuCounterNames) == static_cast<size_t>(kGameCpuCounterCount) - static_cast<size_t>(engine::kEngineCpuCounterCount) + 1);

inline constexpr std::string_view kpGameCpuTimerNames[]
{
	"Viewer frame",
};
static_assert(std::size(kpGameCpuTimerNames) == static_cast<size_t>(kGameCpuTimerCount) - static_cast<size_t>(engine::kEngineCpuTimerCount));

} // namespace game
