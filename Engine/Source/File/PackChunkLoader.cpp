#include "PackChunkLoader.h"

#include "PackChunks.h"

#if defined(BT_CLIENT)
#include "Graphics/Managers/TextureUploadManager.h"
#if defined(BT_DEBUG)
#include "Agent/Commands/AudioStreamingFixture.h"
#endif
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
	for (int64_t i = 0; i < kiLoadingThreadCount; ++i)
	{
		// kThreadLazyLoad tags log lines and participates in the DxDiag-thread check; shared state does not use it.
		mLoadingThreads[i] = std::thread(common::ThreadLocal::Entry(&PackChunkLoader::LoadingThread, common::kiMinWorkbufferSize, common::kThreadLazyLoad), this, i);
	}
}

void PackChunkLoader::Stop()
{
	{
		std::unique_lock lock(mQueueMutex);
		if (mShutdown.exchange(true, std::memory_order_release))
		{
			return;
		}
#if defined(BT_CLIENT)
		mrPackChunks.AcknowledgeQueuedAudioReads();
#endif // BT_CLIENT
	}
	PublishWake();
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
			if (rLazyChunk.eState.value.load(std::memory_order_acquire) >= ChunkState::kDiskLoaded)
			{
				continue;
			}

			if (rLazyChunk.eState.value.load(std::memory_order_acquire) < ChunkState::kLoadRequested)
			{
				// Heap: priority_queue insertion may allocate. Items must persist until the loading thread pops them,
				//   so a workbuffer (frame-scoped) can't own them, and the queue grows/shrinks unpredictably
				ScopedSuppressAllocationTracking suppress;

				mRequestQueue.push({.crc = crc, .ePriority = ePriority, .eKind = LoadRequestKind::kWholeChunk});
				rLazyChunk.eState.value.store(ChunkState::kLoadRequested, std::memory_order_release);
#if defined(BT_CLIENT) && defined(BT_DEBUG)
				if (common::gpMultithreading->IsMainThread())
				{
					AudioStreamingFixture::Record(AudioStreamingFixturePartition::kMain, AudioStreamingFixturePhase::kExistingQueued, std::numeric_limits<uint32_t>::max(), crc, 0, rLazyChunk.iDataSize, AudioStreamingFixtureQueueState::kQueued, 0, false);
				}
#endif
				bAddedAny = true;
			}
		}
	}

	if (bAddedAny)
	{
		PublishWake();
	}
}

void PackChunkLoader::RequestChunkRangeReload(common::crc_t crc, int64_t iOffset, int64_t iLength, LoadPriority ePriority)
{
	bool bAdded = false;

	{
		std::unique_lock lock(mQueueMutex);
		LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(crc);
		ChunkRangeReloadState eState = rLazyChunk.eRangeReloadState.value.load(std::memory_order_acquire);
		if (eState != ChunkRangeReloadState::kIdle)
		{
			// One LazyChunk owns one active range request. Consumers must reset its terminal state before selecting
			// another range, which prevents a late consumer from observing or resetting a different reload.
			ASSERT(rLazyChunk.iRangeReloadOffset == iOffset && rLazyChunk.iRangeReloadLength == iLength);
			return;
		}

		ASSERT(!common::IsCompressed(rLazyChunk.header.flags));
		ASSERT(rLazyChunk.eState.value.load(std::memory_order_acquire) >= ChunkState::kReady);
		rLazyChunk.iRangeReloadOffset = iOffset;
		rLazyChunk.iRangeReloadLength = iLength;
		{
			// Heap: priority_queue insertion may allocate. The request must remain alive until a loading thread pops it.
			ScopedSuppressAllocationTracking suppress;
			mRequestQueue.push({.crc = crc, .ePriority = ePriority, .eKind = LoadRequestKind::kRangeReload, .iOffset = iOffset, .iLength = iLength});
		}
		// The loading thread cannot pop until mQueueMutex unlocks. This release-store publishes the range metadata
		// together with the queued request, so an acquire state read observes the exact request it polls.
		rLazyChunk.eRangeReloadState.value.store(ChunkRangeReloadState::kPending, std::memory_order_release);
		bAdded = true;
	}

	if (bAdded)
	{
		PublishWake();
	}
}

ChunkRangeReloadState PackChunkLoader::GetChunkRangeReloadState(common::crc_t crc, int64_t iOffset, int64_t iLength) const
{
	std::unique_lock lock(mQueueMutex);
	const LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(crc);
	ChunkRangeReloadState eState = rLazyChunk.eRangeReloadState.value.load(std::memory_order_acquire);
	if (eState != ChunkRangeReloadState::kIdle)
	{
		ASSERT(rLazyChunk.iRangeReloadOffset == iOffset && rLazyChunk.iRangeReloadLength == iLength);
	}
	return eState;
}

void PackChunkLoader::ResetChunkRangeReloadState(common::crc_t crc, int64_t iOffset, int64_t iLength)
{
	std::unique_lock lock(mQueueMutex);
	LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(crc);
	ChunkRangeReloadState eState = rLazyChunk.eRangeReloadState.value.load(std::memory_order_acquire);
	if (eState == ChunkRangeReloadState::kPending)
	{
		ASSERT(false); // A pending range can still be writing into the lazy pool.
		return;
	}
	if (eState == ChunkRangeReloadState::kIdle)
	{
		return;
	}
	if (rLazyChunk.iRangeReloadOffset != iOffset || rLazyChunk.iRangeReloadLength != iLength)
	{
		ASSERT(false); // A consumer may only reset its own completed request.
		return;
	}

	rLazyChunk.iRangeReloadOffset = 0;
	rLazyChunk.iRangeReloadLength = 0;
	rLazyChunk.eRangeReloadState.value.store(ChunkRangeReloadState::kIdle, std::memory_order_release);
}

void PackChunkLoader::WaitForChunks(std::span<const common::crc_t> crcs)
{
	RequestChunkLoad(crcs, LoadPriority::kRealtime);

	std::unique_lock lock(mQueueMutex);
	mCompletionCondition.wait(lock, [&]
	{
		return std::ranges::none_of(crcs, [this](const common::crc_t& crc)
		{
			return mrPackChunks.mLazyChunkMap.at(crc).eState.value.load(std::memory_order_acquire) < ChunkState::kReady;
		});
	});
}

void PackChunkLoader::WaitForLoadersIdle()
{
	// Drain queued and active whole-chunk and range jobs after they publish terminal states.
	// ClientUpdate joins tick workers before Render; Graphics::Destroy drains on the main thread at or after Render.
	// This excludes concurrent enqueueing, including tick-worker audio requests from PlayOneShot3d through StaticVoice::LoadXAudio2SourceVoice.
	// Drain before the all-texture reset: a late kUploading store would escape the reset, and RequestChunkLoad skips
	// states >= kDiskLoaded, leaving that texture permanently unrequested.
#if defined(BT_CLIENT) && defined(BT_DEBUG)
	AudioStreamingFixture::PrepareLoaderDrain();
#endif
	std::unique_lock lock(mQueueMutex);
	LOG(kLoading, kInfo, "PackChunks loader drain begin queued={} active={}", mRequestQueue.size(), miActiveLoadJobs);
	mCompletionCondition.wait(lock, [this]
	{
#if defined(BT_CLIENT)
		return mRequestQueue.empty() && miActiveLoadJobs == 0 && !mrPackChunks.HasActiveAudioRead();
#else
		return mRequestQueue.empty() && miActiveLoadJobs == 0;
#endif
	});
	LOG(kLoading, kInfo, "PackChunks loader drain end queued={} active={}", mRequestQueue.size(), miActiveLoadJobs);
}

void PackChunkLoader::LoadingThread(int64_t iThreadIndex)
{
	// Note: do NOT use THREAD_MODE_BACKGROUND_BEGIN. That mode sets `IoPriorityVeryLow`, which during
	// app startup (or any contention with OS-level foreground I/O such as Defender, indexing, OneDrive)
	// causes large `ReadFile`s to stall for many seconds behind foreground requests. BELOW_NORMAL keeps
	// the thread out of frame-critical CPU paths without throttling its disk I/O.
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

	while (true)
	{
		int64_t iSavedWakeSequence = static_cast<int64_t>(mWakeSequence.load(std::memory_order_acquire));
		LoadRequest loadRequest {};
#if defined(BT_CLIENT)
		int64_t iAudioIndex = 0;
		uint64_t uiAudioGeneration = 0;
		bool bAudioRead = false;
#endif // BT_CLIENT
		bool bHaveWork = false;

		{
			std::unique_lock lock(mQueueMutex);
			if (mShutdown.load(std::memory_order_acquire))
			{
				break;
			}

			bool bExistingWork = !mRequestQueue.empty();
			bool bRealtimeWork = bExistingWork && mRequestQueue.top().ePriority == LoadPriority::kRealtime;
#if defined(BT_CLIENT)
			bool bAudioWork = mrPackChunks.HasQueuedAudioRead();
#else
			bool bAudioWork = false;
#endif
#if defined(BT_CLIENT) && defined(BT_DEBUG)
			if (AudioStreamingFixture* pFixture = gpAttachedAudioStreamingFixture.load(std::memory_order_acquire); pFixture != nullptr && pFixture->meStagingOwner.load(std::memory_order_seq_cst) != AudioStreamingFixture::StagingOwner::kNone)
			{
				bExistingWork = false;
				bRealtimeWork = false;
				bAudioWork = false;
			}
#endif
			if (bRealtimeWork || (bExistingWork && (!bAudioWork || !mbPreferAudio)))
			{
				loadRequest = mRequestQueue.top();
				mRequestQueue.pop();
				bHaveWork = true;
				if (bAudioWork)
				{
					mbPreferAudio = true;
				}
			}
#if defined(BT_CLIENT)
			else if (bAudioWork && mrPackChunks.TryClaimAudioRead(iAudioIndex, uiAudioGeneration))
			{
				bAudioRead = true;
				bHaveWork = true;
				if (bExistingWork)
				{
					mbPreferAudio = false;
				}
			}
#endif // BT_CLIENT
			if (bHaveWork)
			{
				++miActiveLoadJobs;
			}
		}

		if (!bHaveWork)
		{
			mWakeSequence.wait(static_cast<uint64_t>(iSavedWakeSequence), std::memory_order_acquire);
			continue;
		}

#if defined(BT_CLIENT)
		if (bAudioRead)
		{
			mrPackChunks.LoadAudioRead(iAudioIndex, uiAudioGeneration, iThreadIndex);
		}
		else
#endif // BT_CLIENT
		if (loadRequest.eKind == LoadRequestKind::kRangeReload)
		{
			LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(loadRequest.crc);
			bool bReloaded = mrPackChunks.RecommitAndReloadChunkRange(loadRequest.crc, loadRequest.iOffset, loadRequest.iLength);
			rLazyChunk.eRangeReloadState.value.store(bReloaded ? ChunkRangeReloadState::kReady : ChunkRangeReloadState::kFailed, std::memory_order_release);
		}
		else
		{
			LoadChunk(loadRequest, iThreadIndex);
		}

#if defined(BT_CLIENT) && defined(BT_DEBUG)
		if (!bAudioRead)
		{
			const LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(loadRequest.crc);
			AudioStreamingFixture::Record(iThreadIndex == 0 ? AudioStreamingFixturePartition::kLoader0 : AudioStreamingFixturePartition::kLoader1, AudioStreamingFixturePhase::kExistingComplete, std::numeric_limits<uint32_t>::max(), loadRequest.crc, loadRequest.iOffset, loadRequest.eKind == LoadRequestKind::kWholeChunk ? rLazyChunk.iDataSize : loadRequest.iLength, AudioStreamingFixtureQueueState::kReady, 0, false);
		}
#endif

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

void PackChunkLoader::PublishWake()
{
	uint64_t uiWakeSequence = mWakeSequence.load(std::memory_order_relaxed);
	while (true)
	{
		ASSERT(uiWakeSequence != std::numeric_limits<uint64_t>::max());
		if (mWakeSequence.compare_exchange_weak(uiWakeSequence, uiWakeSequence + 1, std::memory_order_release, std::memory_order_relaxed))
		{
			break;
		}
	}
	mWakeSequence.notify_all();
}


void PackChunkLoader::LoadChunk(const LoadRequest& rRequest, int64_t iThreadIndex)
{
	LazyChunk& rLazyChunk = mrPackChunks.mLazyChunkMap.at(rRequest.crc);

	bool bCompressed = common::IsCompressed(rLazyChunk.header.flags);

#if defined(BT_CLIENT)
	if (rLazyChunk.header.flags & common::ChunkFlags::kTexture)
	{
		VERIFY_SUCCESS(mrPackChunks.RecommitChunkRange(rRequest.crc, rLazyChunk, 0, rLazyChunk.iDataSize));
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
	std::byte* pReadDestination = bCompressed ? pDecompressScratch : rLazyChunk.pData;

	data::DataTypes eDataType = DataTypeFromFlags(rLazyChunk.header.flags);

	// Bound each disk read to kiSubReadSize plus sector-alignment padding.
	HANDLE hFile = mrPackChunks.mLazyPackFileHandles[eDataType];
	int64_t iFilePosition = iAlignedOffset;
	int64_t iDataCopied = 0;

	while (iDataCopied < iOnDiskSize)
	{
		// Read one sector-aligned sub-chunk from disk. The pack handle is shared across loading threads, so the
		// read must be positional (offset in an OVERLAPPED) rather than SetFilePointerEx + ReadFile — the latter
		// mutates the handle's shared file position and would race between threads, tearing reads. A synchronous
		// (non-FILE_FLAG_OVERLAPPED) handle still completes the read synchronously when given an OVERLAPPED; the
		// explicit offset supersedes the shared file pointer, so concurrent positional reads don't interfere.
		// iFilePosition stays sector-aligned (required by FILE_FLAG_NO_BUFFERING): it starts aligned and advances by
		// uiBytesRead, which equals the sector-multiple iReadSize on every read except the final (loop-exiting) one.
		int64_t iSourceOffset = (iDataCopied == 0) ? iPrefix : 0;
		int64_t iReadSize = common::RoundUp(std::min(mrPackChunks.kiSubReadSize, iOnDiskSize - iDataCopied) + iSourceOffset, mrPackChunks.miSectorSize);
		OVERLAPPED overlapped {};
		overlapped.Offset = static_cast<DWORD>(iFilePosition & 0xFFFFFFFF);
		overlapped.OffsetHigh = static_cast<DWORD>((iFilePosition >> 32) & 0xFFFFFFFF);
		DWORD uiBytesRead = 0;
		std::ignore = ReadFile(hFile, pReadBuffer, static_cast<DWORD>(iReadSize), &uiBytesRead, &overlapped);

		int64_t iCopySize = std::min(static_cast<int64_t>(uiBytesRead) - iSourceOffset, iOnDiskSize - iDataCopied);
		// A truncated .pack returns a 0-byte read that never advances iDataCopied; halt rather than spin.
		ASSERT(iCopySize > 0);
		std::byte* pSource = pReadBuffer + iSourceOffset;
		std::byte* pDestination = pReadDestination + iDataCopied;

		if (bCompressed)
		{
			// Compressed reads land in scratch; the decompress pass below will pull them back through cache anyway,
			// so use a regular memcpy (not _mm_stream_si128) so the bytes stay hot for the LZ4/zlib decompress.
			std::memcpy(pDestination, pSource, iCopySize);
		}
		else
		{
			// Non-temporal destination writes avoid cache pollution.
			bool bAligned = (reinterpret_cast<uintptr_t>(pSource) % 16 == 0) && (reinterpret_cast<uintptr_t>(pDestination) % 16 == 0);
			if (bAligned)
			{
				int64_t iStreamBytes = iCopySize & ~15i64;
				for (int64_t i = 0; i < iStreamBytes; i += 16)
				{
					_mm_stream_si128(reinterpret_cast<__m128i*>(pDestination + i), _mm_loadu_si128(reinterpret_cast<const __m128i*>(pSource + i)));
				}
				if (iCopySize > iStreamBytes)
				{
					std::memcpy(pDestination + iStreamBytes, pSource + iStreamBytes, iCopySize - iStreamBytes);
				}
			}
			else
			{
				std::memcpy(pDestination, pSource, iCopySize);
			}
			_mm_sfence();
		}

		iDataCopied += iCopySize;
		iFilePosition += uiBytesRead;
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
			int64_t iLz4Result = LZ4_decompress_safe(reinterpret_cast<const char*>(pDecompressScratch), reinterpret_cast<char*>(rLazyChunk.pData), static_cast<int>(rLazyChunk.header.iSize), static_cast<int>(rLazyChunk.iDataSize));
			// A short or negative decode leaves the pool slot partly filled, and the upload would publish it.
			ASSERT(iLz4Result == rLazyChunk.iDataSize);
		}
		else
		{
			uLongf uiUncompressedSize = static_cast<uLongf>(rLazyChunk.iDataSize);
			int64_t iZlibResult = uncompress(reinterpret_cast<Bytef*>(rLazyChunk.pData), &uiUncompressedSize, reinterpret_cast<const Bytef*>(pDecompressScratch), static_cast<uLong>(iOnDiskSize));
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
		rLazyChunk.eState.value.store(ChunkState::kUploading, std::memory_order_release);
		gpTextureUploadManager->RequestUpload(rRequest.crc, rRequest.ePriority);
	}
	else
#endif // BT_CLIENT
	{
		// Non-texture chunks are ready immediately after disk load
		rLazyChunk.eState.value.store(ChunkState::kReady, std::memory_order_release);
		NotifyChunkCompletion();
	}
}

void PackChunkLoader::NotifyChunkCompletion()
{
	std::unique_lock lock(mQueueMutex);
	mCompletionCondition.notify_all();
}

} // namespace engine
