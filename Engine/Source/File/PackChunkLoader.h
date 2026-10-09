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
public:
	void RequestChunkLoad(std::span<const common::crc_t> crcs, LoadPriority ePriority);
public:
	// Queues a single uncompressed lazy-chunk range for background recommit/reload. Same-range requests deduplicate
	// while pending or ready; a failed request stays failed until its consumer resets it. State reads acquire the
	// worker's ready/failed publication, and reset refuses a pending request so it cannot invalidate an in-flight reload.
	void RequestChunkRangeReload(common::crc_t crc, int64_t iOffset, int64_t iLength, LoadPriority ePriority);
	ChunkRangeReloadState GetChunkRangeReloadState(common::crc_t crc, int64_t iOffset, int64_t iLength) const;
	void ResetChunkRangeReloadState(common::crc_t crc, int64_t iOffset, int64_t iLength);
	void WaitForChunks(std::span<const common::crc_t> crcs);
	// Blocks until no whole or range load is queued or running, with every accepted job's terminal state published.
	// Full graphics recovery calls this before texture-upload teardown so no loader can publish into the reset that
	// follows. Callers must not enqueue new work afterwards until recovery completes. No separate admission state
	// enforces that, because the exclusion is temporal: the one off-main producer (one-shot audio requesting its chunk
	// from tick workers) has joined by the end of ClientUpdate, which precedes Render and the Graphics::Destroy that
	// calls this.
	void WaitForLoadersIdle();
	// Notification for chunk completion (wakes WaitForChunks waiters)
	void NotifyChunkCompletion();
private:
	void LoadingThread(int64_t iThreadIndex);
	void LoadChunk(const LoadRequest& rRequest, int64_t iThreadIndex);
	void ReadChunkRange(const LazyChunk& rLazyChunk, int64_t iChunkOffset, int64_t iSize, std::byte* pReadDestination, int64_t iThreadIndex);
#if defined(BT_CLIENT) && defined(BT_DEBUG)
public:
#endif
	void PublishWake();
private:

	// N background loading threads, assigned inside the async eager-load task (PackChunks::mLoadingFuture), not
	// the PackChunks ctor body. Each LoadingThread pops from the shared priority queue and reads the sync members
	// below (mWakeSequence/mQueueMutex/mRequestQueue/mShutdown), owning a private read buffer + decompress scratch
	// (indexed by thread index). ~PackChunks first drains mLoadingFuture (ensuring these assignments have happened),
	// then calls Stop(), which sets mShutdown + notify_all()s + join()s every thread before those members destruct.
	// Count is deliberately small: each thread doubles the read-buffer + decompress-scratch memory footprint.
	static constexpr int64_t kiLoadingThreadCount = 2;
	PackChunks& mrPackChunks;
	std::thread mLoadingThreads[kiLoadingThreadCount];
	std::atomic<uint64_t> mWakeSequence = 0;
	std::condition_variable mCompletionCondition;
#if defined(BT_CLIENT) && defined(BT_DEBUG)
public:
#endif
	mutable std::mutex mQueueMutex;
private:
	std::priority_queue<LoadRequest> mRequestQueue;
	std::atomic<bool> mShutdown = false;

	// Jobs popped from mRequestQueue but not yet finished, guarded by mQueueMutex. Queue-empty alone cannot say the
	// loaders are idle, because a popped job runs outside the lock; WaitForLoadersIdle needs both.
	int64_t miActiveLoadJobs = 0;
	bool mbPreferAudio = true;
};

} // namespace engine
