#include "ProfileManager.h"

namespace game
{

ProfileManager* gpProfileManager = nullptr;

ProfileManager::ProfileManager()
: ProfileManagerBase(mGameCpuCounters, mGameCpuTimers, kGameCpuCounterNames, kGameCpuTimerNames, kGameCpuCounterCount, kGameCpuTimerCount)
{
	ASSERT(gpProfileManager == nullptr);

	gpProfileManager = this;

	if constexpr (kbProfiling)
	{
#if defined(BT_SERVER)
		RegisterRawCpuTimer(kCpuTimerPostRenderUpdateNavQuery);
		RegisterRawCpuTimerEvent(kCpuTimerPostRenderUpdateNavQuery);
#endif // BT_SERVER

		BootStart(engine::kBootTimerTotal);
	}
}

#if defined(BT_SERVER)

void ProfileManager::OnRawCpuTimersLatched(int64_t iSampleTick)
{
	if constexpr (kbProfiling)
	{
		const engine::RawCpuTimerRecord rawRecord = GetRawCpuTimer(kCpuTimerPostRenderUpdateNavQuery);
		if (rawRecord.iInvocationCount == 8)
		{
			PublishRawCpuTimerEvent(kCpuTimerPostRenderUpdateNavQuery, iSampleTick);
		}
	}
}

#endif // BT_SERVER

ProfileManager::~ProfileManager()
{
	if (gpProfileManager == this)
	{
		gpProfileManager = nullptr;
	}
}

} // namespace game
