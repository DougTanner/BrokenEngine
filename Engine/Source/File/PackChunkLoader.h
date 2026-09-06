#pragma once

#include "FileManager.h"

namespace engine
{

class PackChunks;

data::DataTypes DataTypeFromFlags(const common::ChunkFlags_t& rFlags);

class PackChunkLoader
{
public:

	PackChunkLoader(const PackChunkLoader&) = delete;
	PackChunkLoader& operator=(const PackChunkLoader&) = delete;
	PackChunkLoader(PackChunkLoader&&) = delete;
	PackChunkLoader& operator=(PackChunkLoader&&) = delete;

private:
	friend class PackChunks;

	explicit PackChunkLoader(PackChunks& rPackChunks);
	~PackChunkLoader();

	void Start();
	void Stop();
	void RequestChunkLoad(std::span<const common::crc_t> crcs, LoadPriority ePriority);
	void RequestChunkRangeReload(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, LoadPriority ePriority);
	ChunkRangeReloadState GetChunkRangeReloadState(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength) const;
	void ResetChunkRangeReloadState(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength);
	void WaitForChunks(std::span<const common::crc_t> crcs);
	void WaitForLoadersIdle();
	void NotifyChunkCompletion();
	void LoadingThread(int64_t iThreadIndex);
	void LoadChunk(const LoadRequest& rRequest, int64_t iThreadIndex);

	// N background loading threads, assigned inside the async eager-load task (PackChunks::mLoadingFuture), not
	// the PackChunks ctor body. Each LoadingThread pops from the shared priority queue and reads the sync members
	// below (mWakeCondition/mQueueMutex/mRequestQueue/mShutdown), owning a private read buffer + decompress scratch
	// (indexed by thread index). ~PackChunks first drains mLoadingFuture (ensuring these assignments have happened),
	// then calls Stop(), which sets mShutdown + notify_all()s + join()s every thread before those members destruct.
	// Count is deliberately small: each thread doubles the read-buffer + decompress-scratch memory footprint.
	static constexpr int64_t kiLoadingThreadCount = 2;
	PackChunks& mrPackChunks;
	std::thread mLoadingThreads[kiLoadingThreadCount];
	std::condition_variable mWakeCondition;
	std::condition_variable mCompletionCondition;
	mutable std::mutex mQueueMutex;
	std::priority_queue<LoadRequest> mRequestQueue;
	std::atomic<bool> mShutdown {false};

	// Jobs popped from mRequestQueue but not yet finished, guarded by mQueueMutex. Queue-empty alone cannot say the
	// loaders are idle, because a popped job runs outside the lock; WaitForLoadersIdle needs both.
	int64_t miActiveLoadJobs = 0;
};

} // namespace engine
