#pragma once

#if defined(BT_CLIENT)

namespace engine
{

class TextureUploadManager
{
public:

	TextureUploadManager();
	~TextureUploadManager();

	TextureUploadManager(const TextureUploadManager&) = delete; // The upload thread retains this object's address until it is joined.
	TextureUploadManager& operator=(const TextureUploadManager&) = delete;

	void InitializeTransferResources();
	void DestroyTransferResources();
	void StartThread();

	void RequestUpload(common::crc_t crc, LoadPriority ePriority);
	void WaitIdle();
	void SignalFrame();
	void RethrowException();

	// Trust boundary for signed on-disk texture metadata used by both upload paths.
	static void ValidateTextureDimensions(const LazyChunk& rLazyChunk);

	// Pending-adoption counter: tracks chunks in an adoptable state (kDiskLoaded / kGpuUploadComplete) awaiting
	// TextureManager::ProcessPendingTextures. Lives here (not on TextureManager) because this manager outlives the
	// device-loss Graphics recreate that destroys TextureManager — so the upload thread never touches a freed owner
	// and the count survives the recreate (PackChunks::ResetTextureChunkStates re-arms it through device loss).
	// Relaxed ordering: the count is decoupled from chunk-data visibility (eState's own acquire/release carries that);
	// the worst case is one frame of adoption latency.
	std::atomic<int64_t> miPendingAdoptions = 0;

	// Registered pre-blur lighting-texture CRCs. Lives here for the same reason as the counter above: this manager
	// outlives every Graphics recreation, while TextureManager — which registration would otherwise write and which
	// never re-registers, because registration runs once at startup — is destroyed and rebuilt empty by one.
	// Unlike the counter, this is main-thread-only and unsynchronized, not an atomic: it is written only by startup
	// registration (RegisterLightingTextureCrc) and read only from main-thread adoption and reblur, so the upload
	// thread must never touch it.
	std::unordered_set<common::crc_t> mLightingTextureCrcs;

	std::binary_semaphore mFrameSignal = std::binary_semaphore(0);

private:

	void UploadThread();

	// UploadThread seam helpers. Per-chunk dimensions are derived once from the chunk header and passed to
	// the image-create and staging-copy steps.
	struct ChunkDimensions
	{
		bool bCubemap = false;
		VkFormat vkFormat = VK_FORMAT_UNDEFINED;
		int64_t iBlockHeight = 0;
		int64_t iArrayLayers = 0;
		int64_t iMipLevels = 0;
		uint32_t uiBaseWidth = 0;
		uint32_t uiBaseHeight = 0;
	};
	bool DequeueNextUpload();
	bool HandleUploadEarlyOut(LazyChunk& rLazyChunk);
	void CreateTransferImage(LazyChunk& rLazyChunk, const ChunkDimensions& rDimensions);
	void RecordStagingCopies(const LazyChunk& rLazyChunk, const ChunkDimensions& rDimensions);
	void SubmitChunkUpload(LazyChunk& rLazyChunk, VkImageMemoryBarrier& rVkImageMemoryBarrier, bool bDone);

	static constexpr int64_t kiByteBudgetPerFrame = 4 * 1'024 * 1'024;

	// Zeroes the five "In-progress upload state" fields below (full reset between textures / on teardown)
	void ResetUploadProgress();

	// In-progress upload state (persists across frames for one texture at a time)
	common::crc_t mCurrentCrc = 0;
	int64_t miCurrentLayer = 0;
	uint32_t muiCurrentMip = 0;
	int64_t miCurrentMipY = 0;      // Y texel offset within current mip (for sub-mip partial copies)
	int64_t miCurrentDataOffset = 0;     // Byte offset into LazyChunk.pData

	std::thread mUploadThread;
	std::mutex mWorkMutex;
	std::condition_variable mIdleConditionVariable; // WaitIdle() waits on this; UploadThread notifies when it acks a drain probe (guarded by mWorkMutex)
	bool mbDrainRequested = false; // WaitIdle() sets this; UploadThread acks instead of submitting (guarded by mWorkMutex)
	bool mbDrained = false; // UploadThread sets this to confirm it reached a quiescent point (guarded by mWorkMutex)
	std::exception_ptr mException; // Unexpected upload-thread failure, published before mbThreadExited while mWorkMutex is held
	std::atomic<bool> mbThreadExited = false; // Shutdown-wake and exception exits publish this flag under mWorkMutex before notifying mIdleConditionVariable.
	static_assert(decltype(mbThreadExited)::is_always_lock_free);
	std::mutex mUploadMutex;
	std::priority_queue<LoadRequest> mUploadQueue;
	std::atomic<bool> mbShutdown = false;

	VkCommandPool mTransferVkCommandPool = VK_NULL_HANDLE;
	VkCommandBuffer mTransferVkCommandBuffer = VK_NULL_HANDLE;
	VkFence mTransferVkFence = VK_NULL_HANDLE;

	VkBuffer mStagingVkBuffer = VK_NULL_HANDLE;
	VmaAllocation mStagingVmaAllocation = VK_NULL_HANDLE;
	int64_t miStagingSize = 0;
	void* mpStagingMappedData = nullptr;
};

inline TextureUploadManager* gpTextureUploadManager = nullptr;

} // namespace engine

#endif // defined(BT_CLIENT)
