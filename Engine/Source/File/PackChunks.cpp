#include "PackChunks.h"

#include "Profile/ProfileManager.h"
#include "Game.h"

#if defined(BT_CLIENT)
#include "Graphics/Managers/TextureUploadManager.h"
#if defined(BT_DEBUG)
#include "Agent/Commands/AudioStreamingHarnessRig.h"
#endif
#endif

namespace engine
{

#if defined(BT_CLIENT)
constexpr uint64_t kuiAudioReadStateMask = 0x3;
static_assert(static_cast<uint64_t>(AudioChunkReadState::kFree) == 0);
static_assert(static_cast<uint64_t>(AudioChunkReadState::kReady) == kuiAudioReadStateMask);

static constexpr uint64_t PackAudioReadOwnership(AudioChunkReadState eState, uint64_t uiGeneration)
{
	return (uiGeneration << 2) | static_cast<uint64_t>(eState);
}

static constexpr AudioChunkReadState AudioReadState(uint64_t uiOwnership)
{
	return static_cast<AudioChunkReadState>(uiOwnership & kuiAudioReadStateMask);
}

static constexpr uint64_t AudioReadGeneration(uint64_t uiOwnership)
{
	return uiOwnership >> 2;
}

static uint64_t NextAudioReadGeneration(uint64_t uiGeneration)
{
	ASSERT(uiGeneration != (std::numeric_limits<uint64_t>::max() >> 2));
	return uiGeneration + 1;
}
#endif // BT_CLIENT

PackChunks::PackChunks(const std::filesystem::path& rDataDirectory)
	: mDataDirectory(rDataDirectory)
	, mLoader(*this)
{
	LoadPackFiles();
}

PackChunks::~PackChunks()
{
	// Drain the eager-load task first: the loading threads are assigned inside it (see LoadPackFiles), so a join
	// before the task runs would hit not-yet-joinable threads. Startup normally drains it before using eager data
	// or waiting for lazy chunks (valid() is then false). get() rethrows if the task threw; swallow it so a
	// destructor never terminates the process, and let PackChunkLoader::Stop() skip never-started threads.
	if (mLoadingFuture.valid())
	{
		try
		{
			common::ScopedExpectedThrows scopedExpectedThrows;
			mLoadingFuture.get();
		}
		catch (...)
		{
			LOG(kLoading, kError, "Eager-load task threw during FileManager shutdown");
			DEBUG_BREAK();
		}
	}

	mLoader.Stop();

	for (HANDLE& rHandle : mLazyPackFileHandles)
	{
		if (rHandle != nullptr && rHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(rHandle);
		}
		rHandle = nullptr;
	}
	for (std::byte* pReadBuffer : mpReadBuffers)
	{
		_aligned_free(pReadBuffer);
	}
	VirtualFree(mpLazyPool, 0, MEM_RELEASE);
	for (std::byte* pDecompressScratch : mpDecompressScratches)
	{
		if (pDecompressScratch != nullptr)
		{
			VirtualFree(pDecompressScratch, 0, MEM_RELEASE);
		}
	}
}

std::filesystem::path PackChunks::GetDataFilePath(data::DataTypes eDataType, std::string_view extension) const
{
	return mDataDirectory / (std::string(data::kpcDataTypeNames[eDataType]) + std::string(extension));
}

constexpr bool IsEagerChunk(data::DataTypes eDataType)
{
	return eDataType == data::kDataTypeScene || eDataType == data::kDataTypeModel || eDataType == data::kDataTypeShader || eDataType == data::kDataTypeRaw;
}

constexpr bool IsServerChunk(data::DataTypes eDataType)
{
	// Server simulates terrain/physics from Islands only; Audio + Texture are client-only consumers.
	return eDataType == data::kDataTypeIslands;
}

data::DataTypes DataTypeFromFlags(const common::ChunkFlags_t& rFlags)
{
	if (rFlags & common::ChunkFlags::kScene)
	{
		return data::kDataTypeScene;
	}
	if (rFlags & common::ChunkFlags::kIsland)
	{
		return data::kDataTypeIslands;
	}
	if (rFlags & common::ChunkFlags::kModel)
	{
		return data::kDataTypeModel;
	}
	if (rFlags & common::ChunkFlags::kShader)
	{
		return data::kDataTypeShader;
	}
	if (rFlags & common::ChunkFlags::kTexture)
	{
		return data::kDataTypeTexture;
	}
	if (rFlags & common::ChunkFlags::kChunkAudio)
	{
		return data::kDataTypeAudio;
	}
	if (rFlags & common::ChunkFlags::kRaw)
	{
		return data::kDataTypeRaw;
	}
	// No type flag means a corrupt pack; ValidateChunkHeader rejects that at load, so lazy-map callers
	// index with the result.
	return data::kDataTypeCount;
}

// External-data trust boundary: a wholly missing or corrupt required .manifest/.pack is unrecoverable —
// limping with an empty/garbage chunk set defers the failure to every later consumer and ships a broken game.
// FileManager construction and its eager task both feed the startup-specific catch in Main.cpp. Keep the exact
// diagnostic here so synchronous and worker-carried failures use the same actionable reinstall message.
[[noreturn]] static void FailMissingRequiredAsset(const std::filesystem::path& rAssetPath, std::string_view reason)
{
	LOG(kLoading, kError, "Required asset \"{}\" is missing or corrupt: {}", rAssetPath.string(), reason);
	DEBUG_BREAK();
	std::string message = "A required game data file is missing or corrupt:\n\n";
	message += rAssetPath.string();
	message += "\n\n";
	message += reason;
	message += "\n\nPlease reinstall or verify your game files.";
	common::ScopedExpectedThrows scopedExpectedThrows;
	throw std::runtime_error(message);
}

// External-data trust boundary: every pointer, slice, and decoder argument built below comes from a manifest
// location or a chunk header, so one corrupt value would address memory outside the real pack file. Both helpers
// run before a chunk is published, so corrupt data is never consumed. Each returns nullptr when the data is
// valid, or the reason to report through FailMissingRequiredAsset.
static const char* ValidateChunkLocation(int64_t iPackFileSize, const common::ChunkLocation& rChunkLocation)
{
	if (rChunkLocation.uiOffset % static_cast<uint64_t>(common::kiAlignmentBytes) != 0)
	{
		return "chunk offset not aligned";
	}
	if (rChunkLocation.uiSize < static_cast<uint64_t>(common::kiChunkDataOffset))
	{
		return "chunk smaller than its header";
	}
	if (rChunkLocation.uiOffset > static_cast<uint64_t>(iPackFileSize))
	{
		return "chunk range outside pack file";
	}
	if (rChunkLocation.uiSize > static_cast<uint64_t>(iPackFileSize) - rChunkLocation.uiOffset)
	{
		return "chunk range outside pack file";
	}
	return nullptr;
}

// Call only after ValidateChunkLocation for the same location: the payload bounds here are relative to uiSize.
static const char* ValidateChunkHeader(data::DataTypes eExpectedDataType, const common::ChunkLocation& rChunkLocation, const common::ChunkHeader& rChunkHeader)
{
	if (rChunkHeader.iMagic != common::ChunkHeader::kiMagic)
	{
		return "chunk header magic mismatch";
	}
	// Consumers pick the pack file handle/path from these flags, but the offset and size above were validated
	// against eExpectedDataType's pack, so a mismatched (or flagless) type would address a different file.
	if (DataTypeFromFlags(rChunkHeader.flags) != eExpectedDataType)
	{
		return "chunk header data type mismatch";
	}
	if (rChunkHeader.crc != rChunkLocation.crc)
	{
		return "chunk header CRC mismatch";
	}
	// Every consumer reads pcPath as a C string, so an unterminated path would read past the field.
	if (std::memchr(rChunkHeader.pcPath, 0, std::size(rChunkHeader.pcPath)) == nullptr)
	{
		return "chunk path is not NUL-terminated";
	}
	// iSize is the exact LZ4 compressed length passed to LZ4_decompress_safe as an int; zlib reads iOnDiskSize instead.
	if (rChunkHeader.flags & common::ChunkFlags::kLz4Compressed)
	{
		if (rChunkHeader.iSize < 0)
		{
			return "chunk header size outside chunk";
		}
		if (rChunkHeader.iSize > static_cast<int64_t>(rChunkLocation.uiSize) - common::kiChunkDataOffset)
		{
			return "chunk header size outside chunk";
		}
		if (!std::in_range<int>(rChunkHeader.iSize))
		{
			return "chunk header size outside chunk";
		}
	}
	// For an uncompressed chunk iSize is the payload extent its consumers read against (audio streaming derives
	// its whole read range from it), so a header that outruns its own chunk is a corrupt pack and fails here at
	// load rather than mid-stream. Zlib chunks are excluded: they size their decode from the on-disk extent.
	if (!common::IsCompressed(rChunkHeader.flags))
	{
		if (rChunkHeader.iSize < 0)
		{
			return "chunk header size outside chunk";
		}
		if (rChunkHeader.iSize > static_cast<int64_t>(rChunkLocation.uiSize) - common::kiChunkDataOffset)
		{
			return "chunk header size outside chunk";
		}
	}
	// iUncompressedSize sizes the lazy pool slot and caps the decoder output for both codecs.
	if (common::IsCompressed(rChunkHeader.flags))
	{
		if (rChunkHeader.iUncompressedSize <= 0)
		{
			return "chunk uncompressed size invalid";
		}
		if (!std::in_range<int>(rChunkHeader.iUncompressedSize))
		{
			return "chunk uncompressed size invalid";
		}
	}
	return nullptr;
}

void PackChunks::LoadPackFiles()
{
	std::unordered_set<common::crc_t> seenChunkCrcs;
	for (int64_t i = 0; i < data::kDataTypeCount; ++i)
	{
#if defined(BT_SERVER)
		// Server consumes only Islands among lazy chunks; skipping the rest keeps Audio.pack/Texture.pack
		// unopened (and therefore unlocked) so DataPacker can rewrite them while the server is running.
		if (!IsServerChunk(static_cast<data::DataTypes>(i)))
		{
			continue;
		}
#endif

		mPackFilePaths[i] = GetDataFilePath(static_cast<data::DataTypes>(i), ".pack");
		std::filesystem::path manifestPath = GetDataFilePath(static_cast<data::DataTypes>(i), ".manifest");
		std::fstream manifestStream(manifestPath, std::ios::in | std::ios::binary);
		common::DataHeader dataHeader {};
		manifestStream.read(reinterpret_cast<char*>(&dataHeader), sizeof(dataHeader));
		// Trust boundary: a missing manifest leaves manifestStream failed and dataHeader zero-filled; a corrupt one
		// mismatches magic/version. Either way the asset type is unusable — fail loud rather than ASSERT-terminate.
		if (!manifestStream || dataHeader.iMagic != common::DataHeader::kiMagic || dataHeader.iVersion != common::DataHeader::kiVersion)
		{
			FailMissingRequiredAsset(manifestPath, "manifest header missing or version mismatch");
		}

		// Trust boundary: iChunkCount comes from the manifest header; a garbage count would drive an unbounded resize
		// (bad_alloc terminate) or a torn read. Bound it by what the file can actually hold before allocating.
		static constexpr int64_t kiChunkTableOffset = common::RoundUp<int64_t, common::kiAlignmentBytes>(static_cast<int64_t>(sizeof(common::DataHeader)));
		manifestStream.seekg(0, std::ios::end);
		int64_t iManifestSize = static_cast<int64_t>(manifestStream.tellg());
		int64_t iMaximumChunkCount = (iManifestSize - kiChunkTableOffset) / static_cast<int64_t>(sizeof(common::ChunkLocation));
		if (!manifestStream || dataHeader.iChunkCount < 0 || dataHeader.iChunkCount > iMaximumChunkCount)
		{
			FailMissingRequiredAsset(manifestPath, "manifest chunk-count out of range");
		}

		manifestStream.seekg(kiChunkTableOffset);
		mChunkLocations[i].resize(dataHeader.iChunkCount);
		manifestStream.read(reinterpret_cast<char*>(mChunkLocations[i].data()), dataHeader.iChunkCount * sizeof(common::ChunkLocation));
		if (!manifestStream)
		{
			FailMissingRequiredAsset(manifestPath, "manifest chunk table truncated");
		}
		for (const common::ChunkLocation& rChunkLocation : mChunkLocations[i])
		{
			if (!seenChunkCrcs.insert(rChunkLocation.crc).second)
			{
				FailMissingRequiredAsset(manifestPath, "duplicate chunk CRC");
			}
		}
		if (static_cast<data::DataTypes>(i) == data::kDataTypeIslands)
		{
			const std::vector<common::ChunkLocation>& rChunkLocations = mChunkLocations[i];
			mPackIntegrityToken = rChunkLocations.empty() ? common::kCrcSeed : common::Crc(std::span<const common::ChunkLocation>(rChunkLocations.data(), rChunkLocations.size()));
		}
		manifestStream.close();

		if (IsEagerChunk(static_cast<data::DataTypes>(i)))
		{
			continue;
		}

		std::fstream packStream(mPackFilePaths[i], std::ios::in | std::ios::binary);
		// Trust boundary: a missing/locked lazy .pack leaves packStream closed; reading headers from it would build
		// the lazy map (and pool layout) from garbage. A required pack is as fatal as a missing manifest.
		if (!packStream)
		{
			FailMissingRequiredAsset(mPackFilePaths[i], "pack file missing or unreadable");
		}
		packStream.seekg(0, std::ios::end);
		int64_t iPackFileSize = static_cast<int64_t>(packStream.tellg());
		if (!packStream.good())
		{
			FailMissingRequiredAsset(mPackFilePaths[i], "pack file size unreadable");
		}
		for (const common::ChunkLocation& rChunkLocation : mChunkLocations[i])
		{
			if (const char* pcReason = ValidateChunkLocation(iPackFileSize, rChunkLocation); pcReason != nullptr)
			{
				FailMissingRequiredAsset(mPackFilePaths[i], pcReason);
			}
			common::ChunkHeader chunkHeader {};
			packStream.seekg(rChunkLocation.uiOffset);
			packStream.read(reinterpret_cast<char*>(&chunkHeader), sizeof(chunkHeader));
			if (const char* pcReason = ValidateChunkHeader(static_cast<data::DataTypes>(i), rChunkLocation, chunkHeader); pcReason != nullptr)
			{
				FailMissingRequiredAsset(mPackFilePaths[i], pcReason);
			}

			// Add to lazy chunk map. iDataSize is the size of the data in pData after any decompression
			// (i.e., what consumers see). For compressed chunks, that's the uncompressed size; otherwise
			// it's the on-disk chunk-data size. The on-disk size is always recoverable from `location.uiSize`.
			int64_t iOnDiskSize = rChunkLocation.uiSize - common::kiChunkDataOffset;
			bool bCompressed = common::IsCompressed(chunkHeader.flags);
			int64_t iDataSize = bCompressed ? chunkHeader.iUncompressedSize : iOnDiskSize;
			mLazyChunkMap.try_emplace(rChunkLocation.crc, LazyChunk {.location = rChunkLocation, .header = chunkHeader, .iDataSize = iDataSize});

			if (bCompressed && iOnDiskSize > miDecompressScratchSize)
			{
				miDecompressScratchSize = iOnDiskSize;
			}
		}
		// Trust boundary: a truncated pack lets a per-chunk header seek/read run past EOF (failbit) — the headers
		// already emplaced would be garbage. Treat the whole pack as corrupt rather than building a bad pool.
		if (!packStream)
		{
			FailMissingRequiredAsset(mPackFilePaths[i], "pack header table truncated");
		}
	}

	// Pre-allocate memory pool for all lazy chunk data (eliminates heap lock contention during background loading)
	int64_t iPoolOffset = 0;
	for (const auto& [crc, rLazyChunk] : mLazyChunkMap)
	{
		if (rLazyChunk.iDataSize > std::numeric_limits<int64_t>::max() - (common::kiAlignmentBytes - 1))
		{
			FailMissingRequiredAsset(mPackFilePaths[DataTypeFromFlags(rLazyChunk.header.flags)], "lazy chunk size exceeds aligned pool-slot range");
		}
		int64_t iAlignedDataSize = common::RoundUp<int64_t, common::kiAlignmentBytes>(rLazyChunk.iDataSize);
		if (iPoolOffset > std::numeric_limits<int64_t>::max() - iAlignedDataSize)
		{
			FailMissingRequiredAsset(mDataDirectory, "aggregate lazy chunk pool size exceeds supported range");
		}
		iPoolOffset += iAlignedDataSize;
	}
	miLazyPoolSize = iPoolOffset;
	if (miLazyPoolSize > 0)
	{
		mpLazyPool = static_cast<std::byte*>(VirtualAlloc(nullptr, miLazyPoolSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		if (mpLazyPool == nullptr)
		{
			FailMissingRequiredAsset(mDataDirectory, "lazy chunk pool allocation failed");
		}

		iPoolOffset = 0;
		for (auto& [crc, rLazyChunk] : mLazyChunkMap)
		{
			rLazyChunk.pData = mpLazyPool + iPoolOffset;
			iPoolOffset += common::RoundUp<int64_t, common::kiAlignmentBytes>(rLazyChunk.iDataSize);
		}
	}

	// Per-thread decompress scratch (sized to largest compressed chunk on disk; only allocated if any chunks are
	// compressed). One per loading thread so concurrent decompresses never share a scratch buffer.
	if (miDecompressScratchSize > 0)
	{
		for (std::byte*& rpDecompressScratch : mpDecompressScratches)
		{
			rpDecompressScratch = static_cast<std::byte*>(VirtualAlloc(nullptr, miDecompressScratchSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
			if (rpDecompressScratch == nullptr)
			{
				FailMissingRequiredAsset(mDataDirectory, "decompression scratch allocation failed");
			}
		}
	}

	// Query disk sector size for FILE_FLAG_NO_BUFFERING alignment requirements
	DWORD uiSectorsPerCluster = 0, uiBytesPerSector = 0, uiNumberOfFreeClusters = 0, uiTotalNumberOfClusters = 0;
	GetDiskFreeSpaceW(mDataDirectory.root_path().c_str(), &uiSectorsPerCluster, &uiBytesPerSector, &uiNumberOfFreeClusters, &uiTotalNumberOfClusters);
	miSectorSize = uiBytesPerSector;

	SYSTEM_INFO systemInfo {};
	GetSystemInfo(&systemInfo);
	miPageSize = systemInfo.dwPageSize;

	for (int64_t i = 0; i < data::kDataTypeCount; ++i)
	{
		if (IsEagerChunk(static_cast<data::DataTypes>(i)))
		{
			continue;
		}
#if defined(BT_SERVER)
		if (!IsServerChunk(static_cast<data::DataTypes>(i)))
		{
			continue;
		}
#endif

		mLazyPackFileHandles[i] = CreateFileW(mPackFilePaths[i].c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		// Trust boundary: a missing required .pack yields INVALID_HANDLE_VALUE; storing it unchecked would later
		// spin LoadChunk's sub-read loop forever on 0-byte reads. Same severity as a missing manifest.
		if (mLazyPackFileHandles[i] == INVALID_HANDLE_VALUE)
		{
			FailMissingRequiredAsset(mPackFilePaths[i], "pack file could not be opened for loading");
		}
	}

	// Allocate per-thread sector-aligned read buffers (one sub-read + sector padding each) so concurrent
	// loading threads never share a read buffer.
	miReadBufferSize = common::RoundUp(kiSubReadSize + miSectorSize, miSectorSize);
	for (std::byte*& rpReadBuffer : mpReadBuffers)
	{
		rpReadBuffer = static_cast<std::byte*>(_aligned_malloc(miReadBufferSize, static_cast<size_t>(miSectorSize)));
		if (rpReadBuffer == nullptr)
		{
			FailMissingRequiredAsset(mDataDirectory, "aligned read-buffer allocation failed");
		}
	}

	mLoadingFuture = std::async(std::launch::async, common::ThreadLocal::Entry([this]()
	{
#if defined(BT_CLIENT)
		for (int64_t i = 0; i < data::kDataTypeCount; ++i)
		{
			if (!IsEagerChunk(static_cast<data::DataTypes>(i)))
			{
				continue;
			}

			std::vector<std::byte>& rPackBytes = mPackFileData[i];

			std::error_code packFileSizeError;
			int64_t iPackFileSize = static_cast<int64_t>(std::filesystem::file_size(mPackFilePaths[i], packFileSizeError));
			if (packFileSizeError)
			{
				FailMissingRequiredAsset(mPackFilePaths[i], "pack file size unreadable");
			}
			rPackBytes.resize(static_cast<size_t>(iPackFileSize));
			std::fstream packStream(mPackFilePaths[i], std::ios::in | std::ios::binary);
			if (!packStream)
			{
				FailMissingRequiredAsset(mPackFilePaths[i], "pack file could not be opened for eager loading");
			}
			packStream.read(reinterpret_cast<char*>(rPackBytes.data()), static_cast<std::streamsize>(std::ssize(rPackBytes)));
			if (!packStream || packStream.gcount() != static_cast<std::streamsize>(std::ssize(rPackBytes)))
			{
				FailMissingRequiredAsset(mPackFilePaths[i], "pack file truncated during eager loading");
			}
			packStream.close();

			iPackFileSize = std::ssize(rPackBytes);
			for (const common::ChunkLocation& rChunkLocation : mChunkLocations[i])
			{
				if (const char* pcReason = ValidateChunkLocation(iPackFileSize, rChunkLocation); pcReason != nullptr)
				{
					FailMissingRequiredAsset(mPackFilePaths[i], pcReason);
				}
				common::ChunkHeader* pChunkHeader = reinterpret_cast<common::ChunkHeader*>(rPackBytes.data() + rChunkLocation.uiOffset);
				if (const char* pcReason = ValidateChunkHeader(static_cast<data::DataTypes>(i), rChunkLocation, *pChunkHeader); pcReason != nullptr)
				{
					FailMissingRequiredAsset(mPackFilePaths[i], pcReason);
				}
				int64_t iDataOffset = static_cast<int64_t>(rChunkLocation.uiOffset + common::kiChunkDataOffset);

				mEagerChunkMap.try_emplace(rChunkLocation.crc, EagerChunk { .pHeader = pChunkHeader, .pData = rPackBytes.data() + iDataOffset, .iDataSize = static_cast<int64_t>(rChunkLocation.uiSize - common::kiChunkDataOffset), });

				LOG(kLoading, kDebug, "Eager chunk {} \"{}\" size {}", rChunkLocation.crc, std::string_view(pChunkHeader->pcPath), rChunkLocation.uiSize);
			}
		}
#endif

		// Eager map + pack buffers are now fully populated; publish to acquiring readers.
		mbEagerLoadComplete.store(true, std::memory_order_release);

		mLoader.Start();
	}, common::kiMinWorkbufferSize, common::kThreadEagerLoad));
}

const std::unordered_map<common::crc_t, EagerChunk>& PackChunks::GetEagerChunkMap() const
{
	if (mLoadingFuture.valid()) [[unlikely]]
	{
		gpProfileManager->BootStart(kBootTimerWaitForDataFile);
		common::ScopedExpectedThrows scopedExpectedThrows;
		mLoadingFuture.get();
		gpProfileManager->BootStop(kBootTimerWaitForDataFile);
	}

	return mEagerChunkMap;
}

bool PackChunks::IsChunkReady(common::crc_t crc) const
{
	// Eager map is async-populated; only assert the not-an-eager-chunk invariant once it is published.
	if (mbEagerLoadComplete.load(std::memory_order_acquire))
	{
		ASSERT(mEagerChunkMap.find(crc) == mEagerChunkMap.end());
	}
	auto it = mLazyChunkMap.find(crc);
	return it != mLazyChunkMap.end() ? it->second.eState.value.load(std::memory_order_acquire) >= ChunkState::kReady : false;
}

void PackChunks::WaitForChunks(std::span<const common::crc_t> crcs)
{
	std::ignore = GetEagerChunkMap();
	mLoader.WaitForChunks(crcs);
}

void PackChunks::ResetTextureChunkStates()
{
	LOG(kLoading, kInfo, "All-texture chunk state reset");
	ResetTextureChunkStates({});
}

void PackChunks::ResetTextureChunkStates(std::span<const common::crc_t> targetCrcs)
{
	// pData/iDataSize are fixed at construction and never rewritten, so this reset touches only the selected
	// textures' eState and GPU handles. Callers own exclusion against the loading threads: full recovery drains
	// them through mLoader.WaitForLoadersIdle and then waits the upload thread, while LRU eviction runs in RenderGlobal's
	// drained descriptor window (a Vulkan descriptor/image drain, not a loader drain) and targets resident,
	// non-uploading textures, for which no whole-chunk loader can be in flight.
	bool bResetAll = targetCrcs.empty();
	for (auto& [crc, rLazyChunk] : mLazyChunkMap)
	{
		if (!(rLazyChunk.header.flags & common::ChunkFlags::kTexture))
		{
			continue;
		}

		if (!bResetAll && !std::ranges::contains(targetCrcs, crc))
		{
			continue;
		}

		ChunkState eState = rLazyChunk.eState.value.load(std::memory_order_acquire);

		rLazyChunk.vkUploadImage = VK_NULL_HANDLE;
		rLazyChunk.vmaAllocation = VK_NULL_HANDLE;

		if (eState == ChunkState::kReady)
		{
			// Adoption decommitted the pool pages, so the CPU bytes are gone: need a full reload from disk
			rLazyChunk.eState.value.store(ChunkState::kNotLoaded, std::memory_order_release);
		}
		else if (eState == ChunkState::kGpuUploadComplete || eState == ChunkState::kUploading)
		{
			// CPU data still valid, just needs re-upload
			rLazyChunk.eState.value.store(ChunkState::kDiskLoaded, std::memory_order_release);
#if defined(BT_CLIENT)
			// Maintain the pending-adoption counter: kUploading (uncounted) -> kDiskLoaded (counted) arms it;
			// kGpuUploadComplete -> kDiskLoaded stays adoptable (already counted), so leave it unchanged. Only the
			// whole-pool reset reaches this branch, because per-island eviction requires bGpuResident, so its targets
			// are kReady. gpTextureUploadManager outlives the device-loss Graphics recreate, so it is always valid here.
			if (eState == ChunkState::kUploading)
			{
				gpTextureUploadManager->miPendingAdoptions.fetch_add(1, std::memory_order_relaxed);
			}
#endif // BT_CLIENT
		}
	}
}

bool PackChunks::ReadChunkData(common::crc_t crc, int64_t iOffset, std::span<std::byte> buffer)
{
	// Check eager chunks first. No locking needed: they are read-only once the async eager load publishes
	// mbEagerLoadComplete — skip the lookup until then so a boot-window read can't race the populating task.
	if (mbEagerLoadComplete.load(std::memory_order_acquire))
	{
		auto it = mEagerChunkMap.find(crc);
		if (it != mEagerChunkMap.end())
		{
			const EagerChunk& rEagerChunk = it->second;
			// Use the chunk-table data extent, not pHeader->iSize: a scene chunk's iSize excludes its appended animation section
			int64_t iDataSize = rEagerChunk.iDataSize;

			if (iOffset + buffer.size() > static_cast<uint64_t>(iDataSize))
			{
				return false;
			}

			std::memcpy(buffer.data(), rEagerChunk.pData + iOffset, buffer.size());
			return true;
		}
	}

	auto it = mLazyChunkMap.find(crc);
	if (it != mLazyChunkMap.end())
	{
		LazyChunk& rLazyChunk = it->second;

		// If chunk is loaded, read from memory. No lock: pData visibility comes from the eState release
		// (LoadChunk) / acquire (here) pair, not PackChunkLoader::mQueueMutex — the lockless writers never take it.
		// pData/iDataSize are fixed at construction, so no reset can move them out from under this read.
		if (rLazyChunk.eState.value.load(std::memory_order_acquire) >= ChunkState::kDiskLoaded)
		{
			// An adopted texture keeps its pData/iDataSize while its pool pages are decommitted, so a texture CRC
			// reaching this path would copy from decommitted memory. eState is the authority for non-texture reads.
			ASSERT(!(rLazyChunk.header.flags & common::ChunkFlags::kTexture));

			if (iOffset + buffer.size() > static_cast<uint64_t>(rLazyChunk.iDataSize))
			{
				return false;
			}

			std::memcpy(buffer.data(), rLazyChunk.pData + iOffset, buffer.size());
			return true;
		}

		data::DataTypes eDataType = DataTypeFromFlags(rLazyChunk.header.flags);

		// Chunk not loaded - read directly from pack file
		// This path is used for streaming audio data without loading entire chunk
		std::fstream packStream(mPackFilePaths[eDataType], std::ios::in | std::ios::binary);

		if (!packStream.is_open())
		{
			return false;
		}

		int64_t iDataOffset = rLazyChunk.location.uiOffset + common::kiChunkDataOffset;
		int64_t iDataSize = rLazyChunk.location.uiSize - common::kiChunkDataOffset;

		if (iOffset + buffer.size() > static_cast<uint64_t>(iDataSize))
		{
			packStream.close();
			return false;
		}

		packStream.seekg(iDataOffset + iOffset);
		packStream.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
		// Past the bounds check above, a short read means the pack file itself is truncated.
		ASSERT(packStream.good());
		packStream.close();

		return true;
	}

	return false;
}

#if defined(BT_CLIENT)

ChunkReadResult PackChunks::TryReadChunkData(ChunkReadRequest& rRequest, common::crc_t crc, int64_t iOffset, std::span<std::byte> buffer)
{
	auto ClearRequest = [&rRequest]
	{
		rRequest.mpPackChunks = nullptr;
		rRequest.miEntryIndex = kiInvalidAudioReadEntry;
		rRequest.muiGeneration = 0;
		rRequest.muiCrc = 0;
		rRequest.miOffset = 0;
		rRequest.miLength = 0;
	};

	if (rRequest.mpPackChunks != nullptr)
	{
		if (rRequest.mpPackChunks != this)
		{
			rRequest.Reset();
			return ChunkReadResult::kFailed;
		}
		if (rRequest.miEntryIndex >= std::ssize(mAudioReadEntries))
		{
			rRequest.Reset();
			return ChunkReadResult::kFailed;
		}
		if (rRequest.muiCrc != crc)
		{
			rRequest.Reset();
			return ChunkReadResult::kFailed;
		}
		if (rRequest.miOffset != iOffset)
		{
			rRequest.Reset();
			return ChunkReadResult::kFailed;
		}
		if (rRequest.miLength != std::ssize(buffer))
		{
			rRequest.Reset();
			return ChunkReadResult::kFailed;
		}

		AudioChunkReadEntry& rEntry = mAudioReadEntries[static_cast<size_t>(rRequest.miEntryIndex)];
		uint64_t uiOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
		if (AudioReadGeneration(uiOwnership) != rRequest.muiGeneration)
		{
			ClearRequest();
			return ChunkReadResult::kFailed;
		}

		AudioChunkReadState eState = AudioReadState(uiOwnership);
		if (eState != AudioChunkReadState::kReady)
		{
			return eState == AudioChunkReadState::kQueued || eState == AudioChunkReadState::kLoading
				? ChunkReadResult::kPending
				: ChunkReadResult::kFailed;
		}

		if (rEntry.crc != crc)
		{
			CancelChunkRead(rRequest);
			return ChunkReadResult::kFailed;
		}
		if (rEntry.iOffset != iOffset)
		{
			CancelChunkRead(rRequest);
			return ChunkReadResult::kFailed;
		}
		if (rEntry.iLength != std::ssize(buffer))
		{
			CancelChunkRead(rRequest);
			return ChunkReadResult::kFailed;
		}

		std::memcpy(buffer.data(), rEntry.data.data(), static_cast<size_t>(rEntry.iLength));
#if defined(BT_DEBUG)
		AudioStreamingHarnessRig::Record(AudioStreamingHarnessRigPartition::kMain, AudioStreamingHarnessRigPhase::kRefillReady, rRequest.miEntryIndex, crc, iOffset, rEntry.iLength, AudioStreamingHarnessRigQueueState::kReady, rRequest.muiGeneration, false);
#endif
		uint64_t uiFreeOwnership = PackAudioReadOwnership(AudioChunkReadState::kFree, rRequest.muiGeneration);
		if (!rEntry.uiOwnership.compare_exchange_strong(uiOwnership, uiFreeOwnership, std::memory_order_acq_rel))
		{
			ClearRequest();
			return ChunkReadResult::kFailed;
		}
		ClearRequest();
		return ChunkReadResult::kReady;
	}

	if (buffer.empty())
	{
		return ChunkReadResult::kFailed;
	}
	if (buffer.size() > 16 * 1'024)
	{
		return ChunkReadResult::kFailed;
	}

	auto it = mLazyChunkMap.find(crc);
	if (it == mLazyChunkMap.end())
	{
		return ChunkReadResult::kFailed;
	}
	LazyChunk& rLazyChunk = it->second;
	if (!(rLazyChunk.header.flags & common::ChunkFlags::kChunkAudio))
	{
		return ChunkReadResult::kFailed;
	}
	if (static_cast<uint64_t>(iOffset) > static_cast<uint64_t>(rLazyChunk.iDataSize))
	{
		return ChunkReadResult::kFailed;
	}
	if (buffer.size() > static_cast<uint64_t>(rLazyChunk.iDataSize) - static_cast<uint64_t>(iOffset))
	{
		return ChunkReadResult::kFailed;
	}

	if (rLazyChunk.location.uiSize < common::kiChunkDataOffset)
	{
		return ChunkReadResult::kFailed;
	}
	int64_t iPackDataSize = static_cast<int64_t>(rLazyChunk.location.uiSize - common::kiChunkDataOffset);
	if (iOffset > iPackDataSize)
	{
		return ChunkReadResult::kFailed;
	}
	if (std::ssize(buffer) > iPackDataSize - iOffset)
	{
		return ChunkReadResult::kFailed;
	}

	if (rLazyChunk.eState.value.load(std::memory_order_acquire) >= ChunkState::kDiskLoaded)
	{
		std::memcpy(buffer.data(), rLazyChunk.pData + iOffset, buffer.size());
		return ChunkReadResult::kReady;
	}

	for (int64_t i = 0; i < std::ssize(mAudioReadEntries); ++i)
	{
		AudioChunkReadEntry& rEntry = mAudioReadEntries[i];
		uint64_t uiFreeOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
		if (AudioReadState(uiFreeOwnership) != AudioChunkReadState::kFree)
		{
			continue;
		}

		uint64_t uiGeneration = NextAudioReadGeneration(AudioReadGeneration(uiFreeOwnership));
		rEntry.crc = crc;
		rEntry.iOffset = iOffset;
		rEntry.iLength = std::ssize(buffer);
		rRequest.mpPackChunks = this;
		rRequest.miEntryIndex = i;
		rRequest.muiGeneration = uiGeneration;
		rRequest.muiCrc = crc;
		rRequest.miOffset = iOffset;
		rRequest.miLength = std::ssize(buffer);
		uint64_t uiQueuedOwnership = PackAudioReadOwnership(AudioChunkReadState::kQueued, uiGeneration);
		if (!rEntry.uiOwnership.compare_exchange_strong(uiFreeOwnership, uiQueuedOwnership, std::memory_order_release, std::memory_order_acquire))
		{
			ClearRequest();
			continue;
		}
#if defined(BT_DEBUG)
		AudioStreamingHarnessRig::Record(AudioStreamingHarnessRigPartition::kMain, AudioStreamingHarnessRigPhase::kRefillQueued, i, crc, iOffset, buffer.size(), AudioStreamingHarnessRigQueueState::kQueued, uiGeneration, false);
#endif
		mLoader.PublishWake();
		return ChunkReadResult::kPending;
	}

#if defined(BT_DEBUG)
	AudioStreamingHarnessRig::CountRetry();
#endif
	return ChunkReadResult::kRetry;
}

void PackChunks::CancelChunkRead(ChunkReadRequest& rRequest)
{
	if (rRequest.mpPackChunks != this)
	{
		return;
	}
	if (rRequest.miEntryIndex >= std::ssize(mAudioReadEntries))
	{
		return;
	}

	AudioChunkReadEntry& rEntry = mAudioReadEntries[static_cast<size_t>(rRequest.miEntryIndex)];
	uint64_t uiRequestGeneration = rRequest.muiGeneration;
	uint64_t uiOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
	while (AudioReadGeneration(uiOwnership) == uiRequestGeneration)
	{
		AudioChunkReadState eState = AudioReadState(uiOwnership);
		if (eState != AudioChunkReadState::kQueued && eState != AudioChunkReadState::kReady && eState != AudioChunkReadState::kLoading)
		{
			break;
		}
		uint64_t uiCancelledGeneration = NextAudioReadGeneration(uiRequestGeneration);
		AudioChunkReadState eCancelledState = eState == AudioChunkReadState::kLoading
			? AudioChunkReadState::kLoading
			: AudioChunkReadState::kFree;
		uint64_t uiCancelledOwnership = PackAudioReadOwnership(eCancelledState, uiCancelledGeneration);
		if (rEntry.uiOwnership.compare_exchange_weak(uiOwnership, uiCancelledOwnership, std::memory_order_acq_rel))
		{
#if defined(BT_DEBUG)
			AudioStreamingHarnessRig::Record(AudioStreamingHarnessRigPartition::kMain, AudioStreamingHarnessRigPhase::kRefillCancelled, rRequest.miEntryIndex, rRequest.muiCrc, rRequest.miOffset, rRequest.miLength, static_cast<AudioStreamingHarnessRigQueueState>(eState), uiRequestGeneration, eState != AudioChunkReadState::kLoading);
#endif
			break;
		}
	}

	rRequest.mpPackChunks = nullptr;
	rRequest.miEntryIndex = kiInvalidAudioReadEntry;
	rRequest.muiGeneration = 0;
	rRequest.muiCrc = 0;
	rRequest.miOffset = 0;
	rRequest.miLength = 0;
}

bool PackChunks::HasQueuedAudioRead() const
{
	return std::ranges::any_of(mAudioReadEntries, [](const AudioChunkReadEntry& rEntry)
	{
		return AudioReadState(rEntry.uiOwnership.load(std::memory_order_acquire)) == AudioChunkReadState::kQueued;
	});
}

bool PackChunks::HasActiveAudioRead() const
{
	return std::ranges::any_of(mAudioReadEntries, [](const AudioChunkReadEntry& rEntry)
	{
		AudioChunkReadState eState = AudioReadState(rEntry.uiOwnership.load(std::memory_order_acquire));
		return eState == AudioChunkReadState::kQueued || eState == AudioChunkReadState::kLoading;
	});
}

bool PackChunks::HasOccupiedAudioRead() const
{
	return std::ranges::any_of(mAudioReadEntries, [](const AudioChunkReadEntry& rEntry)
	{
		return AudioReadState(rEntry.uiOwnership.load(std::memory_order_acquire)) != AudioChunkReadState::kFree;
	});
}

bool PackChunks::TryClaimAudioRead(int64_t& riIndex, uint64_t& ruiGeneration)
{
	for (int64_t i = 0; i < std::ssize(mAudioReadEntries); ++i)
	{
		AudioChunkReadEntry& rEntry = mAudioReadEntries[i];
		uint64_t uiQueuedOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
		if (AudioReadState(uiQueuedOwnership) != AudioChunkReadState::kQueued)
		{
			continue;
		}
		uint64_t uiGeneration = AudioReadGeneration(uiQueuedOwnership);
		uint64_t uiLoadingOwnership = PackAudioReadOwnership(AudioChunkReadState::kLoading, uiGeneration);
		if (rEntry.uiOwnership.compare_exchange_strong(uiQueuedOwnership, uiLoadingOwnership, std::memory_order_acq_rel))
		{
			riIndex = i;
			ruiGeneration = uiGeneration;
			return true;
		}
	}
	return false;
}

void PackChunks::AcknowledgeQueuedAudioReads()
{
	for (AudioChunkReadEntry& rEntry : mAudioReadEntries)
	{
		uint64_t uiQueuedOwnership = rEntry.uiOwnership.load(std::memory_order_acquire);
		while (AudioReadState(uiQueuedOwnership) == AudioChunkReadState::kQueued)
		{
			uint64_t uiFreeOwnership = PackAudioReadOwnership(AudioChunkReadState::kFree, NextAudioReadGeneration(AudioReadGeneration(uiQueuedOwnership)));
			if (rEntry.uiOwnership.compare_exchange_weak(uiQueuedOwnership, uiFreeOwnership, std::memory_order_acq_rel))
			{
				break;
			}
		}
	}
}

void PackChunks::LoadAudioRead(int64_t iIndex, uint64_t uiGeneration, int64_t iThreadIndex)
{
	AudioChunkReadEntry& rEntry = mAudioReadEntries[static_cast<size_t>(iIndex)];
	common::crc_t crc = rEntry.crc;
	int64_t iOffset = rEntry.iOffset;
	int64_t iLength = rEntry.iLength;
#if defined(BT_DEBUG)
	AudioStreamingHarnessRigPartition ePartition = iThreadIndex == 0 ? AudioStreamingHarnessRigPartition::kLoader0 : AudioStreamingHarnessRigPartition::kLoader1;
	AudioStreamingHarnessRig::Record(ePartition, AudioStreamingHarnessRigPhase::kRefillLoading, iIndex, crc, iOffset, iLength, AudioStreamingHarnessRigQueueState::kLoading, uiGeneration, false);
	bool bHeld = AudioStreamingHarnessRig::HoldAudioRead(crc, iOffset, iLength, iIndex, uiGeneration);
#endif

	const LazyChunk& rLazyChunk = mLazyChunkMap.at(crc);
	data::DataTypes eDataType = DataTypeFromFlags(rLazyChunk.header.flags);
	HANDLE hFile = mLazyPackFileHandles[eDataType];
	if (hFile == nullptr)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range pack handle is unavailable");
	}
	if (hFile == INVALID_HANDLE_VALUE)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range pack handle is unavailable");
	}

	// Pack file offsets are unsigned on disk, so the overflow guards below run in uint64_t.
	uint64_t uiOffset = static_cast<uint64_t>(iOffset);
	uint64_t uiLength = static_cast<uint64_t>(iLength);
	uint64_t uiDataFileOffset = rLazyChunk.location.uiOffset + common::kiChunkDataOffset;
	if (uiDataFileOffset < rLazyChunk.location.uiOffset)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range offset overflows");
	}
	if (uiOffset > std::numeric_limits<uint64_t>::max() - uiDataFileOffset)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range offset overflows");
	}
	uint64_t uiLogicalFileOffset = uiDataFileOffset + uiOffset;
	uint64_t uiAlignedOffset = common::RoundDown(uiLogicalFileOffset, static_cast<uint64_t>(miSectorSize));
	uint64_t uiPrefix = uiLogicalFileOffset - uiAlignedOffset;
	if (uiLength > std::numeric_limits<uint64_t>::max() - uiPrefix)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range size overflows");
	}
	uint64_t uiPhysicalSize = common::RoundUp(uiPrefix + uiLength, static_cast<uint64_t>(miSectorSize));
	if (uiPhysicalSize > static_cast<uint64_t>(miReadBufferSize))
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range physical extent is invalid");
	}
	if (uiAlignedOffset > std::numeric_limits<uint64_t>::max() - uiPhysicalSize)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range physical extent is invalid");
	}

	if (uiLength > std::numeric_limits<uint64_t>::max() - uiLogicalFileOffset)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range logical extent overflows");
	}
	uint64_t uiLogicalEnd = uiLogicalFileOffset + uiLength;
	LARGE_INTEGER fileSize {};
	if (!GetFileSizeEx(hFile, &fileSize))
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range exceeds the pack file");
	}
	if (fileSize.QuadPart < 0)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range exceeds the pack file");
	}
	if (std::cmp_greater(uiLogicalEnd, fileSize.QuadPart))
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range exceeds the pack file");
	}

	OVERLAPPED overlapped {};
	overlapped.Offset = static_cast<DWORD>(uiAlignedOffset & 0xffffffff);
	overlapped.OffsetHigh = static_cast<DWORD>(uiAlignedOffset >> 32);
	DWORD uiBytesRead = 0;
	BOOL bRead = ReadFile(hFile, mpReadBuffers[iThreadIndex], static_cast<DWORD>(uiPhysicalSize), &uiBytesRead, &overlapped);
	if (!bRead)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range read was incomplete");
	}
	if (uiBytesRead < uiPrefix + uiLength)
	{
		FailMissingRequiredAsset(mPackFilePaths[eDataType], "audio range read was incomplete");
	}
	std::memcpy(rEntry.data.data(), mpReadBuffers[iThreadIndex] + uiPrefix, uiLength);

	uint64_t uiExpectedOwnership = PackAudioReadOwnership(AudioChunkReadState::kLoading, uiGeneration);
	uint64_t uiReadyOwnership = PackAudioReadOwnership(AudioChunkReadState::kReady, uiGeneration);
	bool bCurrent = rEntry.uiOwnership.compare_exchange_strong(uiExpectedOwnership, uiReadyOwnership, std::memory_order_acq_rel);
	if (!bCurrent)
	{
		ASSERT(AudioReadState(uiExpectedOwnership) == AudioChunkReadState::kLoading);
		uint64_t uiFreeOwnership = PackAudioReadOwnership(AudioChunkReadState::kFree, AudioReadGeneration(uiExpectedOwnership));
		VERIFY_SUCCESS(rEntry.uiOwnership.compare_exchange_strong(uiExpectedOwnership, uiFreeOwnership, std::memory_order_acq_rel));
	}
#if defined(BT_DEBUG)
	AudioStreamingHarnessRig::Record(ePartition, bCurrent ? AudioStreamingHarnessRigPhase::kRefillReady : AudioStreamingHarnessRigPhase::kRefillCancelled, iIndex, crc, iOffset, iLength, bCurrent ? AudioStreamingHarnessRigQueueState::kReady : AudioStreamingHarnessRigQueueState::kFree, uiGeneration, !bCurrent);
	AudioStreamingHarnessRig::CompleteAudioRead(bHeld);
#endif
	mLoader.NotifyChunkCompletion();
}


#endif // BT_CLIENT

void PackChunks::DecommitChunkRange(common::crc_t crc, int64_t iOffset, int64_t iLength)
{
	// Reclaim a dead sub-range of a resident lazy chunk. Only the page-aligned interior is decommitted, so the
	// boundary partial-pages — which may share bytes with the neighbouring heightmap/hull payload — stay committed;
	// a sub-page range (tiny island) decommits nothing. Other chunks live at disjoint pages in the shared pool
	// reservation, so they are untouched. Main-thread only, with no concurrent reader of the range.
	// A failed MEM_DECOMMIT is benign (the pages simply stay committed) so its result is not checked.
	LazyChunk& rLazyChunk = mLazyChunkMap.at(crc);
	int64_t iRangeStart = reinterpret_cast<int64_t>(rLazyChunk.pData) + iOffset;
	int64_t iRangeEnd = iRangeStart + iLength;
	int64_t iAlignedStart = common::RoundUp(iRangeStart, miPageSize);
	int64_t iAlignedEnd = common::RoundDown(iRangeEnd, miPageSize);
	if (iAlignedEnd > iAlignedStart)
	{
		// MEM_RELEASE would discard the pool reservation that RecommitAndReloadChunkRange recommits at this address.
#pragma warning(suppress: 6250) // Intentional MEM_DECOMMIT; retaining the lazy-pool reservation is required.
		VirtualFree(reinterpret_cast<void*>(iAlignedStart), static_cast<SIZE_T>(iAlignedEnd - iAlignedStart), MEM_DECOMMIT);
	}
}

bool PackChunks::RecommitAndReloadChunkRange(common::crc_t crc, int64_t iOffset, int64_t iLength)
{
	// Inverse of DecommitChunkRange for device-loss recovery: re-commit the same page-aligned interior, then re-read
	// the whole [iOffset, iOffset + iLength) range straight from the pack file on disk. ReadChunkData cannot serve
	// this — for a loaded chunk it copies from the (now-decommitted) resident pool and would fault — so read directly.
	LazyChunk& rLazyChunk = mLazyChunkMap.at(crc);
	// Compressed chunks store decompressed bytes in the pool, so a raw disk reload corrupts that representation without
	// necessarily crashing. This path requires uncompressed chunks; island chunks satisfy that contract.
	ASSERT(!common::IsCompressed(rLazyChunk.header.flags));
	if (!RecommitChunkRange(crc, rLazyChunk, iOffset, iLength))
	{
		return false;
	}

	// Device-loss recovery can run inside the allocation-tracked main loop, so suppress tracking for the transient stream.
	data::DataTypes eDataType = DataTypeFromFlags(rLazyChunk.header.flags);
	int64_t iDataOffset = rLazyChunk.location.uiOffset + common::kiChunkDataOffset;
	ScopedSuppressAllocationTracking suppress; // Heap: transient std::fstream buffers on the device-loss recovery path
	std::fstream packStream(mPackFilePaths[eDataType], std::ios::in | std::ios::binary);
	if (!packStream.is_open())
	{
		LOG(kLoading, kError, "Recommit reload failed to open pack for chunk {}", crc);
		DEBUG_BREAK();
		return false;
	}
	packStream.seekg(iDataOffset + iOffset);
	packStream.read(reinterpret_cast<char*>(rLazyChunk.pData + iOffset), static_cast<std::streamsize>(iLength));
	ASSERT(packStream.good());

	return true;
}

bool PackChunks::RecommitChunkRange(common::crc_t crc, const LazyChunk& rLazyChunk, int64_t iOffset, int64_t iLength)
{
	int64_t iRangeStart = reinterpret_cast<int64_t>(rLazyChunk.pData) + iOffset;
	int64_t iRangeEnd = iRangeStart + iLength;
	int64_t iAlignedStart = common::RoundUp(iRangeStart, miPageSize);
	int64_t iAlignedEnd = common::RoundDown(iRangeEnd, miPageSize);
	if (iAlignedEnd > iAlignedStart)
	{
		// VirtualAlloc result is an OS trust boundary: on failure the interior stays decommitted and the read below
		// would fault, so fail the recommit soft (leave the range as-is) rather than crash the recovery path.
		if (VirtualAlloc(reinterpret_cast<void*>(iAlignedStart), static_cast<SIZE_T>(iAlignedEnd - iAlignedStart), MEM_COMMIT, PAGE_READWRITE) == nullptr)
		{
			LOG(kLoading, kError, "Recommit MEM_COMMIT failed for chunk {}", crc);
			DEBUG_BREAK();
			return false;
		}
	}
	return true;
}

MemoryStats PackChunks::GetEagerStatistics() const
{
	MemoryStats statistics;
	// mPackFileData / mEagerChunkMap are async-populated; report nothing until the eager load publishes.
	if (!mbEagerLoadComplete.load(std::memory_order_acquire))
	{
		return statistics;
	}
	for (int64_t i = 0; i < data::kDataTypeCount; ++i)
	{
		if (IsEagerChunk(static_cast<data::DataTypes>(i)))
		{
			statistics.iBytes += std::ssize(mPackFileData[i]);
		}
	}
	statistics.iCount = static_cast<int64_t>(mEagerChunkMap.size());
	return statistics;
}

MemoryStats PackChunks::GetLazyStatistics() const
{
	MemoryStats statistics;
	// No lock: mLazyChunkMap structure is frozen after boot, eState is atomic (acquire), and iDataSize is fixed
	// at construction. PackChunkLoader::mQueueMutex never guarded these value reads — the lockless writers never take it.
	// An adopted texture (kReady) has had its pool pages decommitted, so its retained pData/iDataSize describe
	// the reserved allocation rather than resident bytes; skip it or the readout overstates what is held.
	for (const auto& [crc, rLazyChunk] : mLazyChunkMap)
	{
		ChunkState eState = rLazyChunk.eState.value.load(std::memory_order_acquire);
		if (eState >= ChunkState::kReady && (rLazyChunk.header.flags & common::ChunkFlags::kTexture))
		{
			continue;
		}

		if (eState >= ChunkState::kDiskLoaded)
		{
			statistics.iBytes += rLazyChunk.iDataSize;
			++statistics.iCount;
		}
	}
	return statistics;
}

MemoryStats PackChunks::GetMemoryStatistics(data::DataTypes eDataType) const
{
	MemoryStats statistics;

	if (IsEagerChunk(eDataType))
	{
		// mPackFileData is async-populated; report 0 resident bytes until published. mChunkLocations is set
		// synchronously in LoadPackFiles, so the chunk count is always safe to read.
		statistics.iBytes = mbEagerLoadComplete.load(std::memory_order_acquire) ? std::ssize(mPackFileData[eDataType]) : 0;
		statistics.iCount = std::ssize(mChunkLocations[eDataType]);
	}
	else
	{
		// No lock, and adopted textures skipped: see GetLazyStatistics.
		for (const auto& [crc, rLazyChunk] : mLazyChunkMap)
		{
			ChunkState eState = rLazyChunk.eState.value.load(std::memory_order_acquire);
			if (eState >= ChunkState::kReady && (rLazyChunk.header.flags & common::ChunkFlags::kTexture))
			{
				continue;
			}

			if (DataTypeFromFlags(rLazyChunk.header.flags) == eDataType && eState >= ChunkState::kDiskLoaded)
			{
				statistics.iBytes += rLazyChunk.iDataSize;
				++statistics.iCount;
			}
		}
	}
	return statistics;
}


} // namespace engine
