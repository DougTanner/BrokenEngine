#include "ProfileManager.h"

namespace game
{

ProfileManager* gpProfileManager = nullptr;

ProfileManager::ProfileManager()
: ProfileManagerBase(mGameCpuCounters, mGameCpuTimers, kGameCpuCounterNames, kGameCpuTimerNames)
, common::Singleton<ProfileManager>(gpProfileManager, this)
{
	if constexpr (kbProfiling)
	{
#if defined(BT_SERVER)
		RegisterRawCpuTimer(kCpuTimerPostRenderUpdateNavigationQuery);
#endif // BT_SERVER

		BootStart(engine::kBootTimerTotal);
	}
}

} // namespace game
