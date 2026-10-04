#include "ProfileManager.h"

namespace game
{

ProfileManager* gpProfileManager = nullptr;

ProfileManager::ProfileManager()
: ProfileManagerBase(mGameCpuCounters, mGameCpuTimers, kGameCpuCounterNames, kGameCpuTimerNames)
{
	ASSERT(gpProfileManager == nullptr);

	gpProfileManager = this;

	if constexpr (kbProfiling)
	{
#if defined(BT_SERVER)
		RegisterRawCpuTimer(kCpuTimerPostRenderUpdateNavigationQuery);
		RegisterRawCpuTimerEvent(kCpuTimerPostRenderUpdateNavigationQuery);
#endif // BT_SERVER

		BootStart(engine::kBootTimerTotal);
	}
}

#if defined(BT_SERVER)

void ProfileManager::OnRawCpuTimersLatched(int64_t iSampleTick)
{
	if constexpr (kbProfiling)
	{
		engine::RawCpuTimerRecord rawRecord = mpRawCpuTimers[static_cast<size_t>(kCpuTimerPostRenderUpdateNavigationQuery)].record;
		if (rawRecord.iInvocationCount == 8)
		{
			PublishRawCpuTimerEvent(kCpuTimerPostRenderUpdateNavigationQuery, iSampleTick);
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
