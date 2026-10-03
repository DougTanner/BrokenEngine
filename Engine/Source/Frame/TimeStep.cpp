#include "TimeStep.h"

#include "Profile/ProfileManager.h"

namespace engine
{

int64_t TimeStep::TickRealtime()
{
	std::chrono::nanoseconds realDeltaNanoseconds = mRealTime.GetDeltaNs(true);

	if constexpr (kbProfilingFrameSpike)
	{
		float fDelta = common::NanosecondsToFloatSeconds<float>(realDeltaNanoseconds);
		if (mAverageDelta.miCount > 200 && fDelta > 1.9f * mAverageDelta.Average())
		{
			LOG(kDefault, kWarning, "\n\n\n  deltaNs spike {} > {}", common::Wb(fDelta, 4), common::Wb(mAverageDelta.Average(), 4));
			static bool sbOnce = false;
			if (!sbOnce)
			{
				sbOnce = true;
				gpProfileManager->LogTimers();
			}
		}
		LOG(kDefault, kVerbose, "\n\n");

		mAverageDelta = fDelta;
	}

	mTickRemainderNanoseconds += WallToSimulation(realDeltaNanoseconds);

#if defined(BT_SERVER)
	// Death spiral prevention: detect excessive updates and auto-reduce time scale. Server only, because
	// the server broadcasts the new ratio to every client; the client only consumes a broadcast ratio.
	if constexpr (kbDebugInput)
	{
		int64_t iEstimatedTicks = mTickRemainderNanoseconds / kTickNanoseconds;
		if (iEstimatedTicks > kiMaximumTicksPerFrame && miTimeMultiply > 1) [[unlikely]]
		{
			LOG(kDefault, kWarning, "Death spiral detected: {} ticks at {}x speed", iEstimatedTicks, miTimeMultiply);
			DecreaseTimeScale(false);
		}
	}
#endif

	// Clamp accumulator to prevent backlog cascade (e.g., after background/focus loss)
	std::chrono::nanoseconds maximumAccumulator = kTickNanoseconds * kiMaximumAccumulatorTicks;
	if (mTickRemainderNanoseconds > maximumAccumulator)
	{
		LOG(kDefault, kVerbose, "TimeStep::TickRealtime Accumulator clamped");
		mTickRemainderNanoseconds = maximumAccumulator;
	}

	int64_t iTicks = mTickRemainderNanoseconds / kTickNanoseconds;
	mTickRemainderNanoseconds %= kTickNanoseconds;

	return iTicks;
}

void TimeStep::SetTimeScale(int64_t iMultiply, int64_t iDivide)
{
	miTimeMultiply = iMultiply;
	miTimeDivide = iDivide;
	mbTimeScaleChanged = true;
}

bool TimeStep::DecreaseTimeScale(bool bAllowSlowMotion)
{
	if (miTimeMultiply > 1)
	{
		miTimeMultiply /= 2;
		LOG(kDefault, kDebug, "Time ratio: {}x", miTimeMultiply);
		mbTimeScaleChanged = true;
		return true;
	}
	else if (bAllowSlowMotion && miTimeDivide < kiMaximumTimeDivide)
	{
		miTimeDivide *= 2;
		LOG(kDefault, kDebug, "Time ratio: 1/{}x", miTimeDivide);
		mbTimeScaleChanged = true;
		return true;
	}
	return false;
}

void TimeStep::IncreaseTimeScale()
{
	if (miTimeDivide > 1)
	{
		miTimeDivide /= 2;
		LOG(kDefault, kDebug, "Time ratio: 1/{}x", miTimeDivide);
	}
	else
	{
		miTimeMultiply *= 2;
		LOG(kDefault, kDebug, "Time ratio: {}x", miTimeMultiply);
	}
	mbTimeScaleChanged = true;
}

} // namespace engine
