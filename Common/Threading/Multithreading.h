#pragma once

#include "../ScopedLambda.h"
#include "PersistentWorker.h"

namespace common
{

class Multithreading;
inline Multithreading* gpMultithreading = nullptr;

class Multithreading
{
public:

	explicit Multithreading(int64_t iWorkerCount);
	Multithreading(Threads eThread, int64_t iWorkerCount, int64_t iWorkbufferSize);
	~Multithreading();

	int64_t WorkerCount() const
	{
		return std::ssize(mWorkers);
	}

	bool IsMainThread() const
	{
		return std::this_thread::get_id() == mMainThreadId;
	}

	template <typename FUNC>
	void Dispatch(int64_t iCount, FUNC& rProcessRange)
	{
		if (iCount <= 0)
		{
			return;
		}

		bool bExpected = false;
		bool bWasInactive = mbDispatchActive.compare_exchange_strong(bExpected, true, std::memory_order_acq_rel);
		ASSERT(bWasInactive);
		ScopedLambda dispatchActiveGuard([this]()
		{
			mbDispatchActive.store(false, std::memory_order_release);
		});

		int64_t iThreadCount = std::ssize(mWorkers) + 1;
		int64_t iPerThread = iCount / iThreadCount;
		int64_t iRemainder = iCount % iThreadCount;

		int64_t iLogTickCounter = gpThreadLocal->miLogTickCounter;
		int64_t iLogIndent = gpThreadLocal->miLogIndent;

		int64_t iPos = 0;
		for (int64_t i = 0; i < std::ssize(mWorkers); ++i)
		{
			int64_t iThreadItems = iPerThread + (i < iRemainder ? 1 : 0);
			if (iThreadItems == 0)
			{
				break;
			}

			int64_t iStart = iPos;
			int64_t iEnd = iPos + iThreadItems;
			mWorkers.at(static_cast<size_t>(i))->Wake([&rProcessRange, iStart, iEnd, iLogTickCounter, iLogIndent]()
			{
				// Propagate the caller's tick/indent so worker logs tag under the dispatching scope
				LogTickScope logTickScope(iLogTickCounter);
				LogIndentScope logIndentScope(iLogIndent);
				rProcessRange(iStart, iEnd);
			});
			iPos += iThreadItems;
		}

		std::exception_ptr firstException = nullptr;

		if (iPos < iCount)
		{
			try
			{
				rProcessRange(iPos, iCount);
			}
			catch (...)
			{
				firstException = std::current_exception();
			}
		}

		// Drain every worker even on a throw (Wait is a no-op if not dispatched): a skipped Wait leaves that worker running against this unwound stack frame, and its unconsumed mDone token would early-join the next Dispatch
		for (const std::unique_ptr<PersistentWorker>& pWorker : mWorkers)
		{
			try
			{
				pWorker->Wait();
			}
			catch (const std::exception& rException)
			{
				if (firstException == nullptr)
				{
					firstException = std::current_exception();
				}
				else
				{
					LOG(kDefault, kError, "Multithreading::Dispatch discarded later worker exception: {}", rException.what());
				}
			}
			catch (...)
			{
				if (firstException == nullptr)
				{
					firstException = std::current_exception();
				}
				else
				{
					LOG(kDefault, kError, "Multithreading::Dispatch discarded later non-standard worker exception");
				}
			}
		}

		if (firstException != nullptr) [[unlikely]]
		{
			std::rethrow_exception(firstException);
		}
	}

private:

	std::thread::id mMainThreadId = std::this_thread::get_id();
	std::vector<std::unique_ptr<PersistentWorker>> mWorkers;
	std::atomic<bool> mbDispatchActive = false;
};

} // namespace common
