#pragma once

namespace engine
{

inline constexpr int64_t kiTickRate = 32;

// Set tick rate to 32/64/128 tps (kfDeltaTime: 0.03125f/0.015625f/0.0078125f)
inline constexpr std::chrono::nanoseconds kTickNanoseconds = 1'000'000'000ns / kiTickRate;
inline constexpr float kfDeltaTime = common::NanosecondsToFloatSeconds<float>(kTickNanoseconds);

// Manages fixed timestep accumulator and time scaling for Frame updates
class TimeStep
{
public:

	TimeStep() = default;

	// Add real delta time and return number of ticks needed
	// Returns 0 if not enough time accumulated for a tick
	int64_t TickRealtime();



	// Convert a wall-clock duration into sim-clock duration using the current time ratio.
	std::chrono::nanoseconds WallToSimulation(std::chrono::nanoseconds wallNanoseconds) const
	{
		return (wallNanoseconds * miTimeMultiply) / miTimeDivide;
	}

	// Inverse of WallToSimulation: convert a sim-clock duration into the wall-clock duration it occupies
	// at the current time ratio. Use for waiters/sleepers (e.g. ServerSessionRuntime::WaitForTick) that
	// need real-time pacing for a fixed amount of sim time.
	std::chrono::nanoseconds SimulationToWall(std::chrono::nanoseconds simulationNanoseconds) const
	{
		return (simulationNanoseconds * miTimeDivide) / miTimeMultiply;
	}

	void SetTimeScale(int64_t iMultiply, int64_t iDivide);

	static constexpr int64_t kiMaximumTicksPerFrame = 12;  // Threshold for the server's auto-reduction
	static constexpr int64_t kiMaximumAccumulatorTicks = 4;

	// Slowest slow motion (1/32). The server's per-update client packet and byte budgets span one scheduled
	// update, which slow motion lengthens: at 1/32 an update holds ~32 honest acks and a stall too short to
	// open the server's stall grace ~128, under both budgets; at 1/64 a 3 s hiccup can already cross the byte
	// budget and strike an honest client.
	static constexpr int64_t kiMaximumTimeDivide = 32;

	// Decrease time scale (halve multiplier if > 1, else double divider if bAllowSlowMotion and below kiMaximumTimeDivide)
	// Returns true if time scale was changed (false at the 1/kiMaximumTimeDivide limit)
	bool DecreaseTimeScale(bool bAllowSlowMotion = true);

	// Increase time scale (halve divider if > 1, else double multiplier)
	void IncreaseTimeScale();

	common::Timer mRealTime;
	int64_t miTimeMultiply = 1;
	int64_t miTimeDivide = 1;
	std::chrono::nanoseconds mTickRemainderNanoseconds = 0ns;
	common::Smoothed<float, 256> mAverageDelta;
	bool mbTimeScaleChanged = false;
};

} // namespace engine
