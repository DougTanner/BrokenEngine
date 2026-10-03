#include "PersistentWorker.h"

namespace common
{

PersistentWorker::PersistentWorker(std::optional<int64_t> iThreadId, int64_t iWorkbufferSize)
: mThread(ThreadLocal::Entry([this]()
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	while (true)
	{
		mWake.acquire();
		if (mShutdown.load(std::memory_order_acquire)) [[unlikely]]
		{
			break;
		}
		try
		{
			mWork();
		}
		catch (...)
		{
			mException = std::current_exception();
		}
		mDone.release();
	}
}, iWorkbufferSize, iThreadId))
{
}

PersistentWorker::~PersistentWorker()
{
	ASSERT(!mbDispatched); // Wait before destruction; a pending mWake token makes this release exceed the semaphore's maximum count.
	mShutdown.store(true, std::memory_order_release);
	mWake.release();
	mThread.join();
}

void PersistentWorker::Wake(std::move_only_function<void()> work)
{
	ASSERT(!mbDispatched); // Wait before another Wake; otherwise the assignment can race with the worker's use of mWork, and a second release can exceed mWake's maximum while the first token is pending.
	mWork = std::move(work);
	mbDispatched = true;
	mWake.release();
}

void PersistentWorker::Wait()
{
	if (mbDispatched)
	{
		mDone.acquire();
		mbDispatched = false;

		if (mException != nullptr) [[unlikely]]
		{
			std::exception_ptr exception = std::exchange(mException, nullptr);
			std::rethrow_exception(exception);
		}
	}
}

} // namespace common
