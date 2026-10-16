#pragma once

namespace common
{

template<typename FLOAT_TYPE>
constexpr FLOAT_TYPE NanosecondsToFloatSeconds(std::chrono::nanoseconds nanoseconds)
{
	static_assert(std::is_floating_point_v<FLOAT_TYPE>, "NanosecondsToFloatSeconds requires a floating-point type");

	return std::chrono::duration_cast<std::chrono::duration<FLOAT_TYPE, std::ratio<1, 1>>>(nanoseconds).count();
}

// Nearest whole nanosecond, ties to even. Exact and deterministic: a float significand (24 bits) times 10^9
// (2^9 x 1953125, 21 bits) fits in a double's 53-bit significand, so the scale inside round never rounds.
constexpr std::chrono::nanoseconds FloatSecondsToNanoseconds(std::chrono::duration<float> seconds)
{
	return std::chrono::round<std::chrono::nanoseconds>(std::chrono::duration<double>(seconds));
}

class Timer
{
	static_assert(std::chrono::steady_clock::is_steady);

public:

	inline Timer()
	{
	}

	inline void Reset()
	{
		mLastTimePoint = std::chrono::steady_clock::now();
	}

	inline std::chrono::nanoseconds GetDeltaNs(bool bReset = false)
	{
		std::chrono::steady_clock::time_point currentTimePoint = std::chrono::steady_clock::now();
		std::chrono::nanoseconds elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTimePoint - mLastTimePoint);

		if (bReset)
		{
			mLastTimePoint = currentTimePoint;
		}

		return elapsedNanoseconds;
	}

private:

	std::chrono::steady_clock::time_point mLastTimePoint = std::chrono::steady_clock::now();
};

} // namespace common
