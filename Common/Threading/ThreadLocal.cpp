#include "ThreadLocal.h"

namespace common
{

ThreadLocal::ThreadLocal(int64_t iWorkbufferSize, std::optional<int64_t> iThreadId, bool bSetupExceptionHandling)
: common::Singleton<ThreadLocal>(gpThreadLocal, nullptr)
, miThreadId(iThreadId)
, mWorkbufferMemory(64 * std::max(iWorkbufferSize, 64i64 * 1'024i64))
, mpLogBuffer(mLogBufferMemory.data())
, mWorkbuffer(mWorkbufferMemory)
{
	ASSERT(iWorkbufferSize >= kiMinWorkbufferSize);

	// Sized ahead of the publish below, so this thread's own logging finds a usable workbuffer the moment gpThreadLocal is visible.
	mWorkbufferMemory.Resize(iWorkbufferSize);

	Register();

	ConfigureThreadFloatingPoint();

	if (bSetupExceptionHandling)
	{
		SetupExceptionHandling();
	}
}

} // namespace common
