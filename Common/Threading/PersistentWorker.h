#pragma once

#include "ThreadLocal.h"

namespace common
{

// Call Wake and Wait once each from one thread; Wait before another Wake and before destruction. The worker lambda captures this, so the object must outlive its thread and must not move.
class PersistentWorker
{
public:

	PersistentWorker(std::optional<int64_t> iThreadId, int64_t iWorkbufferSize);
	~PersistentWorker();

	PersistentWorker(const PersistentWorker&) = delete;
	PersistentWorker& operator=(const PersistentWorker&) = delete;

	void Wake(std::move_only_function<void()> work);
	void Wait();

private:

	std::move_only_function<void()> mWork;
	std::binary_semaphore mWake = std::binary_semaphore(0);
	std::binary_semaphore mDone = std::binary_semaphore(0);
	std::atomic<bool> mShutdown = std::atomic<bool>(false); // acquire/release names the shutdown signal's intent; the mWake/mDone semaphores already carry the happens-before edge, so read only immediately after mWake.acquire()
	bool mbDispatched = false; // Only accessed by calling thread
	std::exception_ptr mException; // Only accessed between Wake/Wait synchronization points
	std::thread mThread; // Must be last: the thread lambda starts here and accesses mWork, mWake, mDone, mShutdown, and mException.
};

} // namespace common
