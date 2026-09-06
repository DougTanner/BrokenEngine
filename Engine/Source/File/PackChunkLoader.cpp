#include "PackChunkLoader.h"

#include "PackChunks.h"

#if defined(BT_CLIENT)
#include "Graphics/Managers/TextureUploadManager.h"
#endif

namespace engine
{

PackChunkLoader::PackChunkLoader(PackChunks& rPackChunks)
	: mrPackChunks(rPackChunks)
{
}

PackChunkLoader::~PackChunkLoader()
{
	Stop();
}

void PackChunkLoader::Start()
{
	// Start background loading threads (each services the shared priority queue with its own read/scratch buffers)
	for (int64_t iThreadIndex = 0; iThreadIndex < kiLoadingThreadCount; ++iThreadIndex)
	{
		mLoadingThreads[iThreadIndex] = std::thread(&PackChunkLoader::LoadingThread, this, iThreadIndex);
	}
}

void PackChunkLoader::Stop()
{
	// Shutdown background loading threads. notify_all (not notify_one): every waiting loading thread must wake
	// to observe mShutdown, otherwise a thread left asleep would never reach join() below and hang teardown.
	{
		std::unique_lock lock(mQueueMutex);
		mShutdown = true;
	}
	mWakeCondition.notify_all();
	// joinable() is false only if the eager-load task threw before assigning the threads (catch in ~PackChunks).
	for (std::thread& rLoadingThread : mLoadingThreads)
	{
		if (rLoadingThread.joinable())
		{
			rLoadingThread.join();
		}
	}
}

void PackChunkLoader::RequestChunkLoad(std::span<const common::crc_t> crcs, LoadPriority ePriority)
{
	bool bAddedAny = false;

	{
		std::unique_lock lock(mQueueMutex);

		for (common::crc_t crc : crcs)
		{
			// A requested CRC can be absent: these come from pack data as cross-pack references (scene texture
			// lists, island channel headers), so a mixed pack generation can name a chunk this set never published.
			// Requesting is not the place to classify that — each consumer decides (developer error vs. soft failure).
			auto it = mrPackChunks.mLazyChunkMap.find(crc);
			if (it == mrPackChunks.mLazyChunkMap.end())
			{
				continue;
			}

			LazyChunk& rLazyChunk = it->second;
			if (rLazyChunk.eState.load(std::memory_order_acquire) >= ChunkState::kDiskLoaded)
			{
				continue;
			}

			if (rLazyChunk.eState.load(std::memory_order_acquire) < ChunkState::kLoadRequested)
			{
				// Heap: priority_queue insertion may allocate. Items must persist until the loading thread pops them,
				//   so a workbuffer (frame-scoped) can't own them, and the queue grows/shrinks unpredictably
				ScopedSuppressAllocationTracking suppress;

				mRequestQueue.push({crc, ePriority, LoadRequestKind::kWholeChunk});
				rLazyChunk.eState.store(ChunkState::kLoadRequested, std::memory_order_release);
				bAddedAny = true;
			}
		}
	}

	if (bAddedAny)
	{
		// notify_all (not notify_one): a single call can enqueue a burst of chunks (e.g. an island subscription
		// or WaitForChunks span). notify_one would wake just one loading thread, which would drain the whole
		// burst serially while the others slept — defeating the parallelism. Waking all engages every thread;
		// any woken with nothing left to pop simply returns to wait (a cheap, harmless spurious wakeup).
		mWakeCondition.notify_all();
	}
}

void PackChunkLoader::RequestChunkRangeReload(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, LoadPriority ePriority)
{
	bool bAdded = false;

	{
		std::unique_lock lock(mQueueMutex);
		LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(crc);
		ChunkRangeReloadState eState = rLazyChunk.eRangeReloadState.load(std::memory_order_acquire);
		if (eState != ChunkRangeReloadState::kIdle)
		{
			// One LazyChunk owns one active range request. Consumers must reset its terminal state before selecting
			// another range, which prevents a late consumer from observing or resetting a different reload.
			ASSERT(rLazyChunk.uiRangeReloadOffset == uiOffset && rLazyChunk.uiRangeReloadLength == uiLength);
			return;
		}

		ASSERT(!common::IsCompressed(rLazyChunk.header.flags));
		ASSERT(rLazyChunk.eState.load(std::memory_order_acquire) >= ChunkState::kReady);
		rLazyChunk.uiRangeReloadOffset = uiOffset;
		rLazyChunk.uiRangeReloadLength = uiLength;
		{
			// Heap: priority_queue insertion may allocate. The request must remain alive until a loading thread pops it.
			ScopedSuppressAllocationTracking suppress;
			mRequestQueue.push({crc, ePriority, LoadRequestKind::kRangeReload, uiOffset, uiLength});
		}
		// The loading thread cannot pop until mQueueMutex unlocks. This release-store publishes the range metadata
		// together with the queued request, so an acquire state read observes the exact request it polls.
		rLazyChunk.eRangeReloadState.store(ChunkRangeReloadState::kPending, std::memory_order_release);
		bAdded = true;
	}

	if (bAdded)
	{
		mWakeCondition.notify_all();
	}
}

ChunkRangeReloadState PackChunkLoader::GetChunkRangeReloadState(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength) const
{
	std::unique_lock lock(mQueueMutex);
	const LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(crc);
	ChunkRangeReloadState eState = rLazyChunk.eRangeReloadState.load(std::memory_order_acquire);
	if (eState != ChunkRangeReloadState::kIdle)
	{
		ASSERT(rLazyChunk.uiRangeReloadOffset == uiOffset && rLazyChunk.uiRangeReloadLength == uiLength);
	}
	return eState;
}

void PackChunkLoader::ResetChunkRangeReloadState(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength)
{
	std::unique_lock lock(mQueueMutex);
	LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(crc);
	ChunkRangeReloadState eState = rLazyChunk.eRangeReloadState.load(std::memory_order_acquire);
	if (eState == ChunkRangeReloadState::kPending)
	{
		ASSERT(false); // A pending range can still be writing into the lazy pool.
		return;
	}
	if (eState == ChunkRangeReloadState::kIdle)
	{
		return;
	}
	if (rLazyChunk.uiRangeReloadOffset != uiOffset || rLazyChunk.uiRangeReloadLength != uiLength)
	{
		ASSERT(false); // A consumer may only reset its own completed request.
		return;
	}

	rLazyChunk.uiRangeReloadOffset = 0;
	rLazyChunk.uiRangeReloadLength = 0;
	rLazyChunk.eRangeReloadState.store(ChunkRangeReloadState::kIdle, std::memory_order_release);
}

void PackChunkLoader::WaitForChunks(std::span<const common::crc_t> crcs)
{
	RequestChunkLoad(crcs, LoadPriority::kRealtime);

	std::unique_lock lock(mQueueMutex);
	mCompletionCondition.wait(lock, [&]
	{
		for (common::crc_t crc : crcs)
		{
			if (mrPackChunks.mLazyChunkMap.at(crc).eState.load(std::memory_order_acquire) < ChunkState::kReady)
			{
				return false;
			}
		}
		return true;
	});
}

void PackChunkLoader::WaitForLoadersIdle()
{
	// Full-recovery boundary: returns once every accepted whole and range job has left the loading threads with its
	// terminal state published, and nothing is left queued. The exclusion that lets it run without admission state is
	// temporal, not ownership: one producer is off-main (PlayOneShot3d -> StaticVoice::LoadXAudio2SourceVoice requests
	// its audio chunk from tick workers), but ClientUpdate joins every dispatch worker before Render, and every
	// Graphics::Destroy call site runs on the main thread at or after Render, so nothing can enqueue while this waits;
	// Documents/Plans/Engine/AudioStreamingBackgroundRangeReads.md owns reconciling this once audio adds a producer
	// outside that window.
	// Without the drain, a loader that popped a whole-texture request before the all-texture reset can store
	// kUploading after that reset already ran. RequestChunkLoad skips any chunk at >= kDiskLoaded, so such a chunk
	// is never requested again and its texture never recovers.
	std::unique_lock lock(mQueueMutex);
	LOG(kLoading, kInfo, "PackChunks loader drain begin queued={} active={}", mRequestQueue.size(), miActiveLoadJobs);
	mCompletionCondition.wait(lock, [this] { return mRequestQueue.empty() && miActiveLoadJobs == 0; });
	LOG(kLoading, kInfo, "PackChunks loader drain end queued={} active={}", mRequestQueue.size(), miActiveLoadJobs);
}

void PackChunkLoader::LoadingThread(int64_t iThreadIndex)
{
	// All loading threads share the semantically-correct kThreadLazyLoad id: nothing keys shared state off the
	// thread id (it only tags log lines and gates the DxDiag-thread check), so distinct ids are unnecessary.
	common::ThreadLocal threadLocal(0, common::kThreadLazyLoad);

	// Note: do NOT use THREAD_MODE_BACKGROUND_BEGIN. That mode sets `IoPriorityVeryLow`, which during
	// app startup (or any contention with OS-level foreground I/O such as Defender, indexing, OneDrive)
	// causes large `ReadFile`s to stall for many seconds behind foreground requests. BELOW_NORMAL keeps
	// the thread out of frame-critical CPU paths without throttling its disk I/O.
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

	while (!mShutdown)
	{
		LoadRequest loadRequest {};

		{
			std::unique_lock lock(mQueueMutex);

			if (!mShutdown && mRequestQueue.empty())
			{
				// Wait for requests
				mWakeCondition.wait(lock, [this] { return mShutdown || !mRequestQueue.empty(); });
			}

			if (mShutdown)
			{
				break;
			}

			loadRequest = mRequestQueue.top();
			mRequestQueue.pop();
			// Counted in the same locked scope as the pop, so a request is never invisible to WaitForLoadersIdle:
			// it is either still queued or already active.
			++miActiveLoadJobs;
		}

		if (loadRequest.eKind == LoadRequestKind::kRangeReload)
		{
			LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(loadRequest.crc);
			bool bReloaded = mrPackChunks.RecommitAndReloadChunkRange(loadRequest.crc, loadRequest.uiOffset, loadRequest.uiLength);
			rLazyChunk.eRangeReloadState.store(bReloaded ? ChunkRangeReloadState::kReady : ChunkRangeReloadState::kFailed, std::memory_order_release);
		}
		else
		{
			LoadChunk(loadRequest, iThreadIndex);
		}

		// Decremented here rather than inside LoadChunk, which already takes mQueueMutex through
		// NotifyChunkCompletion, and only after the job published its terminal state above — so a drain that
		// returns has seen every accepted job's result.
		{
			std::unique_lock lock(mQueueMutex);
			--miActiveLoadJobs;
			mCompletionCondition.notify_all();
		}
	}
}

void PackChunkLoader::LoadChunk(const LoadRequest& rRequest, int64_t iThreadIndex)
{
	LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(rRequest.crc);

	bool bCompressed = common::IsCompressed(rLazyChunk.header.flags);

#if defined(BT_CLIENT)
	if ((rLazyChunk.header.flags & common::ChunkFlags::kTexture) && !mrPackChunks.RecommitChunkRange(rRequest.crc, rLazyChunk, 0, rLazyChunk.iDataSize))
	{
		rLazyChunk.eState.store(ChunkState::kReady, std::memory_order_release);
		NotifyChunkCompletion();
		return;
	}
#endif // BT_CLIENT

	// Calculate sector-aligned read parameters for unbuffered I/O
	int64_t iFileOffset = rLazyChunk.location.uiOffset + common::kiChunkDataOffset;
	int64_t iOnDiskSize = rLazyChunk.location.uiSize - common::kiChunkDataOffset;
	int64_t iAlignedOffset = common::RoundDown(iFileOffset, mrPackChunks.miSectorSize);
	int64_t iPrefix = iFileOffset - iAlignedOffset;

	// Compressed chunks read into this thread's scratch and decompress into pData; uncompressed chunks read directly into pData.
	std::byte* pDecompressScratch = mrPackChunks.mpDecompressScratches[iThreadIndex];
	std::byte* pReadBuffer = mrPackChunks.mpReadBuffers[iThreadIndex];
	std::byte* pReadDst = bCompressed ? pDecompressScratch : rLazyChunk.pData;

	data::DataTypes eDataType = DataTypeFromFlags(rLazyChunk.header.flags);

	// Read in sub-chunks, yielding between each to reduce main-thread scheduling latency
	HANDLE hFile = mrPackChunks.mLazyPackFileHandles[eDataType];
	int64_t iFilePos = iAlignedOffset;
	int64_t iDataCopied = 0;

	while (iDataCopied < iOnDiskSize)
	{
		// Read one sector-aligned sub-chunk from disk. The pack handle is shared across loading threads, so the
		// read must be positional (offset in an OVERLAPPED) rather than SetFilePointerEx + ReadFile — the latter
		// mutates the handle's shared file position and would race between threads, tearing reads. A synchronous
		// (non-FILE_FLAG_OVERLAPPED) handle still completes the read synchronously when given an OVERLAPPED; the
		// explicit offset supersedes the shared file pointer, so concurrent positional reads don't interfere.
		// iFilePos stays sector-aligned (required by FILE_FLAG_NO_BUFFERING): it starts aligned and advances by
		// uiBytesRead, which equals the sector-multiple uiReadSize on every read except the final (loop-exiting) one.
		int64_t iSrcOffset = (iDataCopied == 0) ? iPrefix : 0;
		DWORD uiReadSize = static_cast<DWORD>(common::RoundUp(std::min(mrPackChunks.kiSubReadSize, iOnDiskSize - iDataCopied) + iSrcOffset, mrPackChunks.miSectorSize));
		OVERLAPPED overlapped {};
		overlapped.Offset = static_cast<DWORD>(iFilePos & 0xFFFFFFFF);
		overlapped.OffsetHigh = static_cast<DWORD>((iFilePos >> 32) & 0xFFFFFFFF);
		DWORD uiBytesRead = 0;
		static_cast<void>(ReadFile(hFile, pReadBuffer, uiReadSize, &uiBytesRead, &overlapped));

		int64_t iCopySize = std::min(static_cast<int64_t>(uiBytesRead) - iSrcOffset, iOnDiskSize - iDataCopied);
		// A truncated .pack returns a 0-byte read that never advances iDataCopied; halt rather than spin.
		ASSERT(iCopySize > 0);
		std::byte* pSrc = pReadBuffer + iSrcOffset;
		std::byte* pDst = pReadDst + iDataCopied;

		if (bCompressed)
		{
			// Compressed reads land in scratch; the decompress pass below will pull them back through cache anyway,
			// so use a regular memcpy (not _mm_stream_si128) so the bytes stay hot for the LZ4/zlib decompress.
			std::memcpy(pDst, pSrc, iCopySize);
		}
		else
		{
			// Non-temporal copy: bypass L3 cache for destination writes (consumed later by upload thread)
			bool bAligned = (reinterpret_cast<uintptr_t>(pSrc) % 16 == 0) && (reinterpret_cast<uintptr_t>(pDst) % 16 == 0);
			if (bAligned)
			{
				int64_t iStreamBytes = iCopySize & ~15LL;
				for (int64_t i = 0; i < iStreamBytes; i += 16)
				{
					_mm_stream_si128(reinterpret_cast<__m128i*>(pDst + i), _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSrc + i)));
				}
				if (iCopySize > iStreamBytes)
				{
					std::memcpy(pDst + iStreamBytes, pSrc + iStreamBytes, iCopySize - iStreamBytes);
				}
			}
			else
			{
				std::memcpy(pDst, pSrc, iCopySize);
			}
			_mm_sfence();
		}

		iDataCopied += iCopySize;
		iFilePos += uiBytesRead;
	}

	if (bCompressed)
	{
		// Trust boundary (both codecs): a corrupted .pack or a raw payload mistagged compressed makes the
		// decompress fail or under-fill; bad pack data halts.
		// Texture chunks are LZ4 today; kZlibCompressed stays a legal decode branch.
		if (rLazyChunk.header.flags & common::ChunkFlags::kLz4Compressed)
		{
			// LZ4_decompress_safe bounds writes to the pool-slot capacity and returns bytes produced (< 0 on
			// malformed input); it allocates nothing, so it is allocation-tracking safe on the loading thread.
			// Unlike zlib (self-terminating deflate stream), the LZ4 block format has no end marker: a full
			// decode requires the EXACT compressed length or it errors on the final-literals parse check. Pass
			// header.iSize (the exact compressed payload byte count), not iOnDiskSize, which is rounded up to
			// kiAlignmentBytes and so carries up to 15 trailing pad bytes.
			int iLz4Result = LZ4_decompress_safe(reinterpret_cast<const char*>(pDecompressScratch), reinterpret_cast<char*>(rLazyChunk.pData), static_cast<int>(rLazyChunk.header.iSize), static_cast<int>(rLazyChunk.iDataSize));
			// A short or negative decode leaves the pool slot partly filled, and the upload would publish it.
			ASSERT(static_cast<int64_t>(iLz4Result) == rLazyChunk.iDataSize);
		}
		else
		{
			uLongf uiUncompressedSize = static_cast<uLongf>(rLazyChunk.iDataSize);
			int iZlibResult = uncompress(reinterpret_cast<Bytef*>(rLazyChunk.pData), &uiUncompressedSize, reinterpret_cast<const Bytef*>(pDecompressScratch), static_cast<uLong>(iOnDiskSize));
			ASSERT(iZlibResult == Z_OK && static_cast<int64_t>(uiUncompressedSize) == rLazyChunk.iDataSize);
		}
	}

	LOG(kLoading, kDebug, "Lazy chunk {} \"{}\" size {}", rRequest.crc, std::string_view(rLazyChunk.header.pcPath), rLazyChunk.location.uiSize);

	// The disk-read bytes in pData are published to ReadChunkData's resident-copy path and the stats
	// readers by the eState release-stores below; that release / acquire pairing — not mQueueMutex — is
	// the memory-visibility contract for lazy chunk data.
#if defined(BT_CLIENT)
	if (rLazyChunk.header.flags & common::ChunkFlags::kTexture)
	{
		// Request GPU upload on the dedicated upload thread (texture only)
		rLazyChunk.eState.store(ChunkState::kUploading, std::memory_order_release);
		gpTextureUploadManager->RequestUpload(rRequest.crc, rRequest.ePriority);
	}
	else
#endif // BT_CLIENT
	{
		// Non-texture chunks are ready immediately after disk load
		rLazyChunk.eState.store(ChunkState::kReady, std::memory_order_release);
		NotifyChunkCompletion();
	}
}

void PackChunkLoader::NotifyChunkCompletion()
{
	std::unique_lock lock(mQueueMutex);
	mCompletionCondition.notify_all();
}

} // namespace engine
