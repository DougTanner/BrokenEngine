#pragma once

#include "FileManager.h" // EagerChunk/LazyChunk/ChunkState/MovableAtomicChunkState/LoadRequest/LoadPriority/MemoryStats + IsEagerChunk/IsServerChunk decls live here
#include "PackChunkLoader.h"

namespace engine
{

#if defined(BT_CLIENT)

enum class AudioChunkReadState : uint8_t
{
	kFree,
	kQueued,
	kLoading,
	kReady,
};

struct AudioChunkReadEntry
{
	std::atomic<uint64_t> uiOwnership {0};
	common::crc_t crc = 0;
	uint64_t uiOffset = 0;
	uint64_t uiLength = 0;
	std::array<std::byte, 16 * 1024> data {};
};

#endif // BT_CLIENT

// The packed-asset chunk engine, owned by FileManager via std::unique_ptr.
// Holds the eager pack buffers, the lazy chunk maps + atomic eState machine, the private background loader,
// and the single-VirtualAlloc lazy memory pool. Not a *Manager: no gp* global, not
// aggregated into Engine.h.
class PackChunks
{
public:

	explicit PackChunks(const std::filesystem::path& rDataDirectory);
	~PackChunks();

	PackChunks(const PackChunks&) = delete; // Its by-value loader borrows `this`; deleting copy also suppresses the implicit move
	PackChunks& operator=(const PackChunks&) = delete;

	const std::unordered_map<common::crc_t, EagerChunk>& GetEagerChunkMap() const;
	const std::unordered_map<common::crc_t, LazyChunk>& GetLazyChunkMap() const;
	common::crc_t GetPackIntegrityToken() const;

	// Lazy loading APIs
	bool IsChunkReady(common::crc_t crc) const;
	void RequestChunkLoad(std::span<const common::crc_t> crcs, LoadPriority ePriority);
	void WaitForChunks(std::span<const common::crc_t> crcs);

	// Streaming API for reading data at specific offset within a chunk
	bool ReadChunkData(common::crc_t crc, uint64_t uiOffset, std::span<std::byte> buffer);

#if defined(BT_CLIENT)
	ChunkReadResult TryReadChunkData(ChunkReadRequest& rRequest, common::crc_t crc, uint64_t uiOffset, std::span<std::byte> buffer);
	void CancelChunkRead(ChunkReadRequest& rRequest);
#endif // BT_CLIENT

	// Notification for chunk completion (wakes WaitForChunks waiters)
	void NotifyChunkCompletion();
	LazyChunk& GetLazyChunk(common::crc_t crc);

	void WaitForLoadersIdle();

	void ResetTextureChunkStates();
	void ResetTextureChunkStates(std::span<const common::crc_t> targetCrcs);

	void DecommitChunkRange(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength);
	[[nodiscard]] bool RecommitAndReloadChunkRange(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength);
	void RequestChunkRangeReload(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength, LoadPriority ePriority);
	ChunkRangeReloadState GetChunkRangeReloadState(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength) const;
	void ResetChunkRangeReloadState(common::crc_t crc, uint64_t uiOffset, uint64_t uiLength);

	// Memory profiling
	MemoryStats GetEagerStats() const;
	MemoryStats GetLazyStats() const;
	MemoryStats GetMemoryStats(data::DataTypes eDataType) const;

private:
	friend class PackChunkLoader;

	void LoadPackFiles();
	[[nodiscard]] bool RecommitChunkRange(common::crc_t crc, const LazyChunk& rLazyChunk, uint64_t uiOffset, uint64_t uiLength);
	std::filesystem::path GetDataFilePath(data::DataTypes eDataType, std::string_view extension) const;

#if defined(BT_CLIENT)
#if defined(BT_DEBUG)
public:
#endif
	bool HasQueuedAudioRead() const;
	bool HasOccupiedAudioRead() const;
	static constexpr uint32_t kiAudioReadEntryCount = 6;

private:
	static constexpr uint32_t kiInvalidAudioReadEntry = std::numeric_limits<uint32_t>::max();
#if defined(BT_DEBUG)
public:
#endif
	std::array<AudioChunkReadEntry, kiAudioReadEntryCount> mAudioReadEntries {};

private:
	bool HasActiveAudioRead() const;
	bool TryClaimAudioRead(uint32_t& ruiIndex, uint64_t& ruiGeneration);
	void LoadAudioRead(uint32_t uiIndex, uint64_t uiGeneration, int64_t iThreadIndex);
	void AcknowledgeQueuedAudioReads();
#endif // BT_CLIENT

	std::filesystem::path mDataDirectory;

	// Cached pack file paths (initialized once in LoadPackFiles)
	std::filesystem::path mPackFilePaths[data::kDataTypeCount];

	// Only available for eager pack files
	std::vector<std::byte> mPackFileData[data::kDataTypeCount];

	// Per-data-type chunk-location tables (offset/size/path CRC/content CRC), read from each manifest in LoadPackFiles
	std::vector<common::ChunkLocation> mChunkLocations[data::kDataTypeCount];
	common::crc_t mPackIntegrityToken = common::kCrcSeed;

	// Split chunk maps for eager and lazy loading
	std::unordered_map<common::crc_t, EagerChunk> mEagerChunkMap;  // Scene, Model, Shader, Raw
#if defined(BT_CLIENT) && defined(BT_DEBUG)
public:
#endif
	std::unordered_map<common::crc_t, LazyChunk> mLazyChunkMap;  // Audio, Islands, Texture
private:

	// Eager-load completion, assigned in LoadPackFiles. mutable: the first GetEagerChunkMap() drains it
	// (a lazy completion behind the const accessor).
	mutable std::future<void> mLoadingFuture;

	// Published (release) at the end of the async eager-load task; eager-map readers (ReadChunkData,
	// IsChunkReady, the memory-stats getters) acquire it before touching mEagerChunkMap / mPackFileData,
	// which the task populates. Gates the boot window only — always true once the first frame runs.
	std::atomic<bool> mbEagerLoadComplete {false};

	// Persistent pack file handles for lazy loading (opened with FILE_FLAG_NO_BUFFERING)
	HANDLE mLazyPackFileHandles[data::kDataTypeCount] {};

	// Per-loading-thread sector-aligned read buffers (one per thread, indexed by thread index; reused across that
	// thread's chunk reads). Size is shared — identical for every thread.
	std::byte* mpReadBuffers[PackChunkLoader::kiLoadingThreadCount] {};
	int64_t miReadBufferSize = 0;
	int64_t miSectorSize = 0;
	int64_t miPageSize = 0; // VM page granularity for lazy-chunk sub-range decommit/recommit

	// Pre-allocated memory pool for all lazy chunk data (VirtualAlloc MEM_COMMIT — committed, not pre-faulted)
	std::byte* mpLazyPool = nullptr;
	int64_t miLazyPoolSize = 0;

	// Per-loading-thread scratch buffers for compressed chunks (LZ4 or zlib; one per thread, indexed by thread index).
	// Each sized at boot to the largest compressed chunk on disk; reused per chunk. Size is shared.
	std::byte* mpDecompressScratches[PackChunkLoader::kiLoadingThreadCount] {};
	int64_t miDecompressScratchSize = 0;

	// Sub-read size for chunked disk reads (256KB balances NVMe throughput vs L3 cache pressure)
	static constexpr int64_t kiSubReadSize = 256 * 1024;

#if defined(BT_CLIENT) && defined(BT_DEBUG)
public:
#endif
	PackChunkLoader mLoader;
};

} // namespace engine
