#include "Multithreading.h"

namespace common
{

Multithreading::Multithreading(int64_t iWorkerCount)
: common::Singleton<Multithreading>(gpMultithreading)
{
	mWorkers.reserve(static_cast<size_t>(iWorkerCount));
	for (int64_t i = 0; i < iWorkerCount; ++i)
	{
		mWorkers.push_back(std::make_unique<PersistentWorker>(std::nullopt, 65'536));
	}
}

} // namespace common
