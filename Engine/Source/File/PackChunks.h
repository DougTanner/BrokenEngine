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
	int64_t iOffset = 0;
	int64_t iLength = 0;
	std::array<std::byte, 16 * 1'024> data {};
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

	PackChunks(const PackChunks&) = delete; // The by-value loader retains this object's address.
	PackChunks& operator=(const PackChunks&) = delete;

	const std::unordered_map<common::crc_t, EagerChunk>& GetEagerChunkMap() const;

	bool IsChunkReady(common::crc_t crc) const;
	void WaitForChunks(std::span<const common::crc_t> crcs);

	bool ReadChunkData(common::crc_t crc, int64_t iOffset, std::span<std::byte> buffer);

#if defined(BT_CLIENT)
	ChunkReadResult TryReadChunkData(ChunkReadRequest& rRequest, common::crc_t crc, int64_t iOffset, std::span<std::byte> buffer);
	void CancelChunkRead(ChunkReadRequest& rRequest);
#endif // BT_CLIENT

	// Reset texture eState and GPU handles: all texture chunks, or only targetCrcs. Chunk pool pointers and sizes are
	// fixed at construction and are not touched. The caller owns exclusion for the chunks it resets — full recovery
	// through WaitForLoadersIdle plus the upload-thread wait, island eviction through RenderGlobal's drained
	// descriptor window, which excludes Vulkan descriptor/image use and is not a loader drain.
	void ResetTextureChunkStates();
	void ResetTextureChunkStates(std::span<const common::crc_t> targetCrcs);

	// Reclaim a dead sub-range of a resident lazy chunk's decompressed pool memory. Decommits only the
	// page-aligned interior of [iOffset, iOffset + iLength); the boundary partial-pages (which may share
	// bytes with the neighbouring payload) and every other chunk stay committed, and the chunk's pData pointer
	// is unchanged. A consumer must recommit and reload the range before reading it again. Main-thread
	// only (boot / device-loss recovery / transfer-complete texture adoption) — the range must have no concurrent reader.
	void DecommitChunkRange(common::crc_t crc, int64_t iOffset, int64_t iLength);
	// Inverse of DecommitChunkRange: MEM_COMMITs the interior and re-reads [iOffset, iOffset + iLength)
	// straight from the pack file on disk into the pool (NOT via the decommitted resident copy). Uncompressed chunks only.
	// Returns true on success; false on soft-fail (MEM_COMMIT failure / pack-open failure). On false the
	// caller must NOT read the range — the interior may be decommitted or hold partial data.
	[[nodiscard]] bool RecommitAndReloadChunkRange(common::crc_t crc, int64_t iOffset, int64_t iLength);

	MemoryStats GetEagerStatistics() const;
	MemoryStats GetLazyStatistics() const;
	MemoryStats GetMemoryStatistics(data::DataTypes eDataType) const;

private:
	friend class PackChunkLoader;

	void LoadPackFiles();
	[[nodiscard]] bool RecommitChunkRange(common::crc_t crc, const LazyChunk& rLazyChunk, int64_t iOffset, int64_t iLength);
	std::filesystem::path GetDataFilePath(data::DataTypes eDataType, std::string_view extension) const;

#if defined(BT_CLIENT)
#if defined(BT_DEBUG)
public:
#endif
	bool HasQueuedAudioRead() const;
	bool HasOccupiedAudioRead() const;
	static constexpr int64_t kiAudioReadEntryCount = 6;

private:
	static constexpr int64_t kiInvalidAudioReadEntry = 4'294'967'295i64;
#if defined(BT_DEBUG)
public:
#endif
	std::array<AudioChunkReadEntry, kiAudioReadEntryCount> mAudioReadEntries {};

private:
	bool HasActiveAudioRead() const;
	bool TryClaimAudioRead(int64_t& riIndex, uint64_t& ruiGeneration);
	void LoadAudioRead(int64_t iIndex, uint64_t uiGeneration, int64_t iThreadIndex);
	void AcknowledgeQueuedAudioReads();
#endif // BT_CLIENT

	std::filesystem::path mDataDirectory;

	// Cached pack file paths (initialized once in LoadPackFiles)
	std::filesystem::path mPackFilePaths[data::kDataTypeCount];

	// Only available for eager pack files
	std::vector<std::byte> mPackFileData[data::kDataTypeCount];

	// Per-data-type chunk-location tables (offset/size/path CRC/content CRC), read from each manifest in LoadPackFiles
	std::vector<common::ChunkLocation> mChunkLocations[data::kDataTypeCount];
public:
	common::crc_t mPackIntegrityToken = common::kCrcSeed;
private:

	std::unordered_map<common::crc_t, EagerChunk> mEagerChunkMap;  // Scene, Model, Shader, Raw
public:
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
	static constexpr int64_t kiSubReadSize = 256 * 1'024;

public:
	PackChunkLoader mLoader;
};

} // namespace engine
