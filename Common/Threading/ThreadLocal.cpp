#include "ThreadLocal.h"

namespace common
{

ThreadLocal::ThreadLocal(int64_t iWorkbufferSize, std::optional<int64_t> iThreadId, bool bSetupExceptionHandling, int64_t iWorkbufferReserveSize)
: miThreadId(iThreadId)
, mLogBufferMemory(kiLogBufferSize)
, mWorkbufferMemory(iWorkbufferReserveSize > 0 ? iWorkbufferReserveSize : 64 * std::max(iWorkbufferSize, static_cast<int64_t>(64 * 1024)))
, mpLogBuffer(mLogBufferMemory.data())
, mWorkbuffer(mWorkbufferMemory)
{
	// Sized ahead of the publish below, so this thread's own logging finds a usable workbuffer the moment gpThreadLocal is visible.
	mWorkbufferMemory.Resize(iWorkbufferSize);

	ASSERT(gpThreadLocal == nullptr);

	gpThreadLocal = this;

	ConfigureThreadFloatingPoint();

	if (bSetupExceptionHandling)
	{
		SetupExceptionHandling();
	}
}

ThreadLocal::~ThreadLocal()
{
	gpThreadLocal = nullptr;
}

} // namespace common
