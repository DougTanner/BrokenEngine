#pragma once

#include "Data/DataTypes.h"

namespace engine
{

enum class FileFlags : uint64_t
{
	kAppDataDirectory = 0x01,
	kTempDirectory    = 0x02,

	kRead      = 0x08,
	kWrite     = 0x10,
	kBackup    = 0x20,
	kStreaming = 0x40, // Required when calling OpenFile with kWrite. Opt-out of atomic write; one-shots use WriteFileAtomically.
};
using FileFlags_t = common::Flags<FileFlags>;

// Eager chunk (loaded at boot)
struct EagerChunk
{
	common::ChunkHeader* pHeader = nullptr;
	std::byte* pData = nullptr;
	int64_t iDataSize = 0; // True in-memory data extent (ChunkLocation::uiSize - kiChunkDataOffset); unlike pHeader->iSize this includes a scene chunk's appended animation section
};

// Lazy chunk (loaded on demand)
enum class ChunkState : uint32_t
{
	kNotLoaded = 0,
	kLoadRequested = 1,
	kDiskLoaded = 2,
	kUploading = 3,
	kGpuUploadComplete = 4,
	kReady = 5,
};

// Movable atomic wrapper (std::atomic deletes copy/move, breaking aggregate types in containers)
struct MovableAtomicChunkState
{
	std::atomic<ChunkState> value = ChunkState::kNotLoaded;

	MovableAtomicChunkState() = default;
	MovableAtomicChunkState(const MovableAtomicChunkState& rOther) : value(rOther.value.load(std::memory_order_relaxed))
	{
	}
	MovableAtomicChunkState(MovableAtomicChunkState&& rOther) noexcept : value(rOther.value.load(std::memory_order_relaxed))
	{
	}
	MovableAtomicChunkState& operator=(const MovableAtomicChunkState&) = delete;
	MovableAtomicChunkState& operator=(MovableAtomicChunkState&&) = delete;

};

struct LazyChunk
{
	common::ChunkLocation location;                   // Manifest entry for pack offset, size, path CRC, and content CRC
	MovableAtomicChunkState eState;
	common::ChunkHeader header {};

	// Owning buffer of iDataSize bytes, allocated by a loader thread's whole-chunk load; null before it. Texture
	// adoption frees it on the main thread before the kReady store; island and audio buffers live for the process.
	common::AlignedUniquePtr<std::byte> pData;
	int64_t iDataSize = 0;

	// GPU upload results (written by upload thread, read by main thread)
	VkImage vkUploadImage = VK_NULL_HANDLE;
	VmaAllocation vmaAllocation = VK_NULL_HANDLE;
};

enum class LoadPriority : uint32_t
{
	kLow = 0,
	kNormal = 1,
	kHigh = 2,
	kRealtime = 3,
};

struct LoadRequest
{
	common::crc_t crc = 0;
	LoadPriority ePriority = LoadPriority::kLow;

	bool operator<(const LoadRequest& rOther) const
	{
		return ePriority < rOther.ePriority;
	}
};

struct MemoryStats
{
	int64_t iBytes = 0;
	int64_t iCount = 0;

	// Lazy chunk buffer counters, filled only by PackChunks::GetLazyStatistics: peak requested bytes, and live and
	// peak mimalloc arena footprint.
	int64_t iPeakBytes = 0;
	int64_t iFootprintBytes = 0;
	int64_t iPeakFootprintBytes = 0;
};

class PackChunks;

#if defined(BT_CLIENT)

enum class ChunkReadResult : uint8_t
{
	kRetry,
	kPending,
	kReady,
	kFailed,
};

class ChunkReadRequest
{
public:
	ChunkReadRequest() = default;
	~ChunkReadRequest();

	ChunkReadRequest(const ChunkReadRequest&) = delete;
	ChunkReadRequest& operator=(const ChunkReadRequest&) = delete;
	ChunkReadRequest(ChunkReadRequest&&) = delete;
	ChunkReadRequest& operator=(ChunkReadRequest&&) = delete;

	void Reset();

private:
	friend class PackChunks;

	int64_t miEntryIndex = static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
	uint64_t muiGeneration = 0;
};

#endif // BT_CLIENT

constexpr bool IsEagerChunk(data::DataTypes eDataType);
// Server-only predicate: which data types the headless server actually consumes.
// Used to skip opening (and locking) pack files the server never reads — Audio, Texture, etc.
constexpr bool IsServerChunk(data::DataTypes eDataType);

class FileManager : public common::Singleton<FileManager>
{
public:

	FileManager();
	~FileManager();

	FileManager(const FileManager&) = delete;
	FileManager& operator=(const FileManager&) = delete;

	std::fstream OpenFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename);
	void RemoveFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename);
	[[nodiscard]] bool ComputeSha256(std::span<std::byte> bytes, std::array<uint8_t, 32>& rOut);
	[[nodiscard]] bool ComputeOrdinaryFileSha256(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, std::array<uint8_t, 32>& rOut);

	// Writes through "<rFilename>.tmp"; a stream or rename failure leaves the destination intact and attempts temporary-file cleanup.
	template <typename FN>
	[[nodiscard]] bool WriteFileAtomically(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, FN&& rWrite);

	
	

public:
	std::filesystem::path GetFilePath(const FileFlags_t& rFlags, const std::filesystem::path& rFilename);
private:
	bool CommitAtomicWrite(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, bool bWriteSucceeded);
	void BackupExistingFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename);

public:

	std::filesystem::path mAppDataDirectory;
	std::filesystem::path mTempDirectory;

	// The out-of-line destructor destroys the unique_ptr where PackChunks is complete; consumers that access its
	// members include PackChunks.h.
	std::unique_ptr<PackChunks> mpPackChunks;
};

inline FileManager* gpFileManager = nullptr;

template <typename T>
concept HasBinaryStreamOperators =
	requires
	{
		std::declval<std::ostream&>() << std::declval<const T&>();
		std::declval<std::istream&>() >> std::declval<T&>();
	};

template <typename FN>
bool FileManager::WriteFileAtomically(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, FN&& rWrite)
{
	if (rFlags & FileFlags::kBackup)
	{
		BackupExistingFile(rFlags, rFilename);
	}

	// Strip kBackup because the destination was backed up above; enable kStreaming for the temporary-file write.
	FileFlags_t openFlags = rFlags;
	openFlags.Set(FileFlags::kBackup, false);
	openFlags.Set(FileFlags::kStreaming);

	std::filesystem::path temporaryFilename = rFilename;
	temporaryFilename += ".tmp";

	std::fstream stream = OpenFile(openFlags, temporaryFilename);
	if (!stream.is_open())
	{
		LOG(kLoading, kError, "WriteFileAtomically failed to open \"{}.tmp\"", rFilename.string());
		return false;
	}

	rWrite(stream);
	stream.close();
	bool bGood = !stream.fail();

	return CommitAtomicWrite(rFlags, rFilename, bGood);
}

// Shared version+size on-disk header convention. Writes int64 version then int64 size (sizeof for
// trivially-copyable types, 0 otherwise — non-trivial types validate version only). Single source for
// WriteVersionedFile/ReadVersionedFile, DifferenceStream save/load, WriteGridSave grid saves, and the TextureCache file cache.
template <typename STRUCT_TYPE>
void WriteVersionHeader(std::fstream& rFileStream)
{
	common::Write(rFileStream, static_cast<int64_t>(STRUCT_TYPE::kiVersion));
	common::Write(rFileStream, std::is_trivially_copyable_v<STRUCT_TYPE> ? static_cast<int64_t>(sizeof(STRUCT_TYPE)) : 0i64);
}

// Callers need the decoded version and size to diagnose mismatches and detect missing layout-version bumps.
template <typename STRUCT_TYPE>
bool ReadAndValidateVersionHeader(std::fstream& rFileStream, int64_t& riVersion, int64_t& riSize)
{
	common::Read(rFileStream, riVersion);
	common::Read(rFileStream, riSize);
	bool bSizeValid = std::is_trivially_copyable_v<STRUCT_TYPE> ? (riSize == static_cast<int64_t>(sizeof(STRUCT_TYPE))) : true;
	return riVersion == STRUCT_TYPE::kiVersion && bSizeValid;
}

template <typename STRUCT_TYPE>
bool WriteVersionedFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, const STRUCT_TYPE& rStructure)
{
	return gpFileManager->WriteFileAtomically(rFlags, rFilename, [&](std::fstream& rFileStream)
	{
		int64_t iVersion = STRUCT_TYPE::kiVersion;
		int64_t iSize = std::is_trivially_copyable_v<STRUCT_TYPE> ? sizeof(STRUCT_TYPE) : 0;
		WriteVersionHeader<STRUCT_TYPE>(rFileStream);
		LOG(kLoading, kDebug, "WriteVersionedFile {} iVersion: {} iSize: {}", rFilename, iVersion, iSize);

		if constexpr (HasBinaryStreamOperators<STRUCT_TYPE>)
		{
			rFileStream << rStructure;
		}
		else
		{
			common::Write(rFileStream, rStructure);
		}
	});
}

template <typename STRUCT_TYPE>
bool ReadVersionedFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, STRUCT_TYPE& rStructure)
{
	std::fstream fileStream = gpFileManager->OpenFile(rFlags, rFilename);

	LOG(kLoading, kDebug, "ReadVersionedFile {} iVersion: {} iSize: {}", rFilename, STRUCT_TYPE::kiVersion, sizeof(STRUCT_TYPE));
	int64_t iVersion = 0;
	int64_t iSize = 0;
	bool bHeaderValid = ReadAndValidateVersionHeader<STRUCT_TYPE>(fileStream, iVersion, iSize);
	LOG(kLoading, kDebug, "    iVersion: {} == {} iSize: {} == {}", iVersion, STRUCT_TYPE::kiVersion, iSize, sizeof(STRUCT_TYPE));
	if (bHeaderValid)
	{
		if constexpr (HasBinaryStreamOperators<STRUCT_TYPE>)
		{
			fileStream >> rStructure;
			return fileStream.good();
		}
		else
		{
			common::Read(fileStream, rStructure);
			int64_t iBytesRead = fileStream.gcount();
			int64_t iExpectedBytes = sizeof(STRUCT_TYPE);
			return iBytesRead == iExpectedBytes;
		}
	}

	LOG(kLoading, kDebug, "    Failed to load versioned file");

	if constexpr (std::is_trivially_copyable_v<STRUCT_TYPE>)
	{
		if (iVersion == STRUCT_TYPE::kiVersion && iSize != sizeof(STRUCT_TYPE))
		{
			// A matching version with a different serialized size indicates a missing layout-version bump.
			DEBUG_BREAK();
		}
	}

	return false;
}

} // namespace engine
