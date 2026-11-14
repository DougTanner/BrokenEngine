#include "PackChunks.h"

#include "Profile/ProfileRows.h"
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

// mimalloc x64 arena geometry (ThirdParty/mimalloc/include/mimalloc/types.h): a block over one 32 MiB arena chunk
// occupies whole chunks, the last one's tail unusable while it lives; a smaller block is counted in 64 KiB slices,
// which overstates blocks that share a mimalloc page.
constexpr int64_t kiMimallocArenaSliceSize = 64 * 1'024;
constexpr int64_t kiMimallocArenaChunkSize = 32 * 1'024 * 1'024;

static int64_t ArenaFootprint(int64_t iBytes)
{
	return common::RoundUp(iBytes, iBytes > kiMimallocArenaChunkSize ? kiMimallocArenaChunkSize : kiMimallocArenaSliceSize);
}

// Raises rPeak to iLive unless another thread already raised it higher.
static void RaisePeak(std::atomic<int64_t>& rPeak, int64_t iLive)
{
	int64_t iPeak = rPeak.load(std::memory_order_relaxed);
	while (iLive > iPeak && !rPeak.compare_exchange_weak(iPeak, iLive, std::memory_order_relaxed))
	{
	}
}

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

	for (HANDLE& rHandle : mpHandleLazyPackFile)
	{
		if (rHandle != nullptr && rHandle != INVALID_HANDLE_VALUE)
		{
			CloseHandle(rHandle);
		}
		rHandle = nullptr;
	}
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
	// Server reads terrain from Islands and ship hitboxes from Model outlines; Audio + Texture are client-only.
	return eDataType == data::kDataTypeIslands || eDataType == data::kDataTypeModel;
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
	if (eExpectedDataType == data::kDataTypeModel)
	{
		const common::ModelHeader& rModelHeader = rChunkHeader.modelHeader;
		if (!std::isfinite(rModelHeader.f3RotationRadians.x) || !std::isfinite(rModelHeader.f3RotationRadians.y)
		 || !std::isfinite(rModelHeader.f3RotationRadians.z) || !std::isfinite(rModelHeader.f3Scale.x)
		 || !std::isfinite(rModelHeader.f3Scale.y) || !std::isfinite(rModelHeader.f3Scale.z) || !std::isfinite(rModelHeader.f3Offset.x)
		 || !std::isfinite(rModelHeader.f3Offset.y) || !std::isfinite(rModelHeader.f3Offset.z))
		{
			return "model alignment is not finite";
		}
		if (rModelHeader.f3Scale.x <= 0.0f || rModelHeader.f3Scale.y <= 0.0f || rModelHeader.f3Scale.z <= 0.0f)
		{
			return "model alignment scale is not positive";
		}
	}
	// Audio streaming reads payload ranges straight from the pack, so a chunk flagged audio must be stored uncompressed.
	if ((rChunkHeader.flags & common::ChunkFlags::kChunkAudio) && common::IsCompressed(rChunkHeader.flags))
	{
		return "audio chunk is compressed";
	}
	// A model's outline and hardpoint sections are placed from iSize, which bounds the payload only when stored uncompressed.
	if (eExpectedDataType == data::kDataTypeModel && common::IsCompressed(rChunkHeader.flags))
	{
		return "model chunk is compressed";
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
	// Readers address the outline and hardpoint sections past iSize without bounds checks. The bounded iSize above keeps
	// this end offset from overflowing.
	if (eExpectedDataType == data::kDataTypeModel)
	{
		const common::ModelHeader& rModelHeader = rChunkHeader.modelHeader;
		int64_t iSectionsEnd = common::ModelHeader::FighterLaunchBaysOffset(rChunkHeader.iSize, rModelHeader.uiMissileTubeCount) + static_cast<int64_t>(rModelHeader.uiFighterLaunchBayCount) * static_cast<int64_t>(sizeof(common::PackedModelHardpoint));
		if (iSectionsEnd > static_cast<int64_t>(rChunkLocation.uiSize) - common::kiChunkDataOffset)
		{
			return "model sections outside chunk";
		}
	}
	// iUncompressedSize sizes the lazy chunk buffer and caps the decoder output for both codecs.
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
		// Server consumes only Islands and Model chunks, so it skips opening the other packs.
		if (!IsServerChunk(static_cast<data::DataTypes>(i)))
		{
			continue;
		}
#endif

		mpPackFilePaths[i] = GetDataFilePath(static_cast<data::DataTypes>(i), ".pack");
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
		mpChunkLocations[i].resize(dataHeader.iChunkCount);
		manifestStream.read(reinterpret_cast<char*>(mpChunkLocations[i].data()), dataHeader.iChunkCount * sizeof(common::ChunkLocation));
		if (!manifestStream)
		{
			FailMissingRequiredAsset(manifestPath, "manifest chunk table truncated");
		}
		for (const common::ChunkLocation& rChunkLocation : mpChunkLocations[i])
		{
			if (!seenChunkCrcs.insert(rChunkLocation.crc).second)
			{
				FailMissingRequiredAsset(manifestPath, "duplicate chunk CRC");
			}
		}
		manifestStream.close();

		if (IsEagerChunk(static_cast<data::DataTypes>(i)))
		{
			continue;
		}

		// Opened before the size and header reads, which go through a handle reopened from it. Both deny writes, so the validated file stays unchanged until shutdown, and share delete, so DataPacker can replace the pack's name while every read keeps using the file this handle opened.
		mpHandleLazyPackFile[i] = CreateFileW(mpPackFilePaths[i].c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		// Trust boundary: a missing required .pack yields INVALID_HANDLE_VALUE, fatal like a missing manifest.
		if (mpHandleLazyPackFile[i] == INVALID_HANDLE_VALUE)
		{
			FailMissingRequiredAsset(mpPackFilePaths[i], "pack file could not be opened for loading");
		}

		HANDLE handlePackValidation = ReOpenFile(mpHandleLazyPackFile[i], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, 0);
		// Trust boundary: a failed reopen leaves no handle to read the headers through; reading them would build
		// the lazy map from garbage. A required pack is as fatal as a missing manifest.
		if (handlePackValidation == INVALID_HANDLE_VALUE)
		{
			FailMissingRequiredAsset(mpPackFilePaths[i], "pack file missing or unreadable");
		}
		LARGE_INTEGER packFileSize {};
		if (GetFileSizeEx(handlePackValidation, &packFileSize) == FALSE)
		{
			FailMissingRequiredAsset(mpPackFilePaths[i], "pack file size unreadable");
		}
		int64_t iPackFileSize = packFileSize.QuadPart;
		for (const common::ChunkLocation& rChunkLocation : mpChunkLocations[i])
		{
			if (const char* pcReason = ValidateChunkLocation(iPackFileSize, rChunkLocation); pcReason != nullptr)
			{
				FailMissingRequiredAsset(mpPackFilePaths[i], pcReason);
			}
			common::ChunkHeader chunkHeader {};
			OVERLAPPED overlapped {};
			overlapped.Offset = static_cast<DWORD>(rChunkLocation.uiOffset & 0xffffffff);
			overlapped.OffsetHigh = static_cast<DWORD>(rChunkLocation.uiOffset >> 32);
			DWORD uiBytesRead = 0;
			if (ReadFile(handlePackValidation, &chunkHeader, static_cast<DWORD>(sizeof(chunkHeader)), &uiBytesRead, &overlapped) == FALSE)
			{
				int64_t iError = GetLastError();
				FailMissingRequiredAsset(mpPackFilePaths[i], std::format("chunk header read failed: Windows error {} at offset {}", iError, rChunkLocation.uiOffset));
			}
			if (uiBytesRead != sizeof(chunkHeader))
			{
				FailMissingRequiredAsset(mpPackFilePaths[i], std::format("chunk header short read: {} of {} bytes at offset {}", uiBytesRead, sizeof(chunkHeader), rChunkLocation.uiOffset));
			}
			if (const char* pcReason = ValidateChunkHeader(static_cast<data::DataTypes>(i), rChunkLocation, chunkHeader); pcReason != nullptr)
			{
				FailMissingRequiredAsset(mpPackFilePaths[i], pcReason);
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
		CloseHandle(handlePackValidation);
	}

	// The handshake token folds the Islands then the Model location table CRC, each kCrcSeed when its table is empty.
	auto LocationTableCrc = [this](data::DataTypes eDataType)
	{
		const std::vector<common::ChunkLocation>& rChunkLocations = mpChunkLocations[eDataType];
		return rChunkLocations.empty() ? common::kCrcSeed : common::Crc(std::span<const common::ChunkLocation>(rChunkLocations.data(), rChunkLocations.size()));
	};
	common::crc_t pTableCrcs[] = {LocationTableCrc(data::kDataTypeIslands), LocationTableCrc(data::kDataTypeModel)};
	mPackIntegrityToken = common::Crc(std::span<const common::crc_t>(pTableCrcs));

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

	mLoadingFuture = std::async(std::launch::async, common::ThreadLocal::Entry([this]()
	{
		for (int64_t i = 0; i < data::kDataTypeCount; ++i)
		{
			if (!IsEagerChunk(static_cast<data::DataTypes>(i)))
			{
				continue;
			}
#if defined(BT_SERVER)
			if (!IsServerChunk(static_cast<data::DataTypes>(i)))
			{
				continue;
			}
#endif

			std::vector<std::byte>& rPackBytes = mpPackFileData[i];

			std::error_code packFileSizeError;
			int64_t iPackFileSize = static_cast<int64_t>(std::filesystem::file_size(mpPackFilePaths[i], packFileSizeError));
			if (packFileSizeError)
			{
				FailMissingRequiredAsset(mpPackFilePaths[i], "pack file size unreadable");
			}
			rPackBytes.resize(static_cast<size_t>(iPackFileSize));
			std::fstream packStream(mpPackFilePaths[i], std::ios::in | std::ios::binary);
			if (!packStream)
			{
				FailMissingRequiredAsset(mpPackFilePaths[i], "pack file could not be opened for eager loading");
			}
			packStream.read(reinterpret_cast<char*>(rPackBytes.data()), static_cast<std::streamsize>(std::ssize(rPackBytes)));
			if (!packStream || packStream.gcount() != static_cast<std::streamsize>(std::ssize(rPackBytes)))
			{
				FailMissingRequiredAsset(mpPackFilePaths[i], "pack file truncated during eager loading");
			}
			packStream.close();

			iPackFileSize = std::ssize(rPackBytes);
			for (const common::ChunkLocation& rChunkLocation : mpChunkLocations[i])
			{
				if (const char* pcReason = ValidateChunkLocation(iPackFileSize, rChunkLocation); pcReason != nullptr)
				{
					FailMissingRequiredAsset(mpPackFilePaths[i], pcReason);
				}
				common::ChunkHeader* pChunkHeader = reinterpret_cast<common::ChunkHeader*>(rPackBytes.data() + rChunkLocation.uiOffset);
				if (const char* pcReason = ValidateChunkHeader(static_cast<data::DataTypes>(i), rChunkLocation, *pChunkHeader); pcReason != nullptr)
				{
					FailMissingRequiredAsset(mpPackFilePaths[i], pcReason);
				}
				int64_t iDataOffset = static_cast<int64_t>(rChunkLocation.uiOffset + common::kiChunkDataOffset);

				mEagerChunkMap.try_emplace(rChunkLocation.crc, EagerChunk {.pHeader = pChunkHeader, .pData = rPackBytes.data() + iDataOffset, .iDataSize = static_cast<int64_t>(rChunkLocation.uiSize - common::kiChunkDataOffset)});

				LOG(kLoading, kDebug, "Eager chunk {} \"{}\" size {}", rChunkLocation.crc, std::string_view(pChunkHeader->pcPath), rChunkLocation.uiSize);
			}
		}

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
	// A kReady texture's buffer was freed at adoption, and an uploading or upload-complete texture keeps its buffer, so
	// this reset never touches pData. LRU eviction targets resident, non-uploading textures, for which no whole-chunk
	// loader can be in flight.
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
			// Adoption freed the CPU buffer: need a full reload from disk
			rLazyChunk.eState.value.store(ChunkState::kNotLoaded, std::memory_order_release);
		}
		else if (eState == ChunkState::kGpuUploadComplete || eState == ChunkState::kUploading)
		{
			// CPU data still valid, just needs re-upload
			rLazyChunk.eState.value.store(ChunkState::kDiskLoaded, std::memory_order_release);
#if defined(BT_CLIENT)
			// Maintain the pending-adoption counter: kUploading (uncounted) -> kDiskLoaded (counted) arms it;
			// kGpuUploadComplete -> kDiskLoaded stays adoptable (already counted), so leave it unchanged. Only the
			// all-texture reset reaches this branch, because per-island eviction requires bGpuResident, so its targets
			// are kReady. gpTextureUploadManager outlives the device-loss Graphics recreate, so it is always valid here.
			if (eState == ChunkState::kUploading)
			{
				gpTextureUploadManager->miPendingAdoptions.fetch_add(1, std::memory_order_relaxed);
			}
#endif // BT_CLIENT
		}
	}
}

#if defined(BT_CLIENT)

ChunkReadResult PackChunks::PollChunkRead(ChunkReadRequest& rRequest, std::byte* pDestination)
{
	ASSERT(rRequest.miEntryIndex != kiInvalidAudioReadEntry);

	auto ClearRequest = [&rRequest]
	{
		rRequest.miEntryIndex = kiInvalidAudioReadEntry;
		rRequest.muiGeneration = 0;
	};

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
		return eState == AudioChunkReadState::kQueued || eState == AudioChunkReadState::kLoading ? ChunkReadResult::kPending : ChunkReadResult::kFailed;
	}

	std::memcpy(pDestination, rEntry.data.data(), static_cast<size_t>(rEntry.iLength));
#if defined(BT_DEBUG)
	if constexpr (kbAudioStreamingHarnessRig)
	{
		AudioStreamingHarnessRig::Record(AudioStreamingHarnessRigPartition::kMain, AudioStreamingHarnessRigPhase::kRefillReady, rRequest.miEntryIndex, rEntry.crc, rEntry.iOffset, rEntry.iLength, AudioStreamingHarnessRigQueueState::kReady, rRequest.muiGeneration, false);
	}
#endif
	// Only the main thread moves an entry out of ready, so no other thread can change it before this store
	rEntry.uiOwnership.store(PackAudioReadOwnership(AudioChunkReadState::kFree, rRequest.muiGeneration), std::memory_order_release);
	ClearRequest();
	return ChunkReadResult::kReady;
}

ChunkReadResult PackChunks::TryReadChunkData(ChunkReadRequest& rRequest, common::crc_t crc, int64_t iOffset, std::span<std::byte> buffer)
{
	ASSERT(rRequest.miEntryIndex == kiInvalidAudioReadEntry);
	ASSERT(!buffer.empty());
	ASSERT(buffer.size() <= sizeof(AudioChunkReadEntry::data));

	LazyChunk& rLazyChunk = mLazyChunkMap.at(crc);
	ASSERT(rLazyChunk.header.flags & common::ChunkFlags::kChunkAudio);
	ASSERT(static_cast<uint64_t>(iOffset) <= static_cast<uint64_t>(rLazyChunk.iDataSize));
	ASSERT(buffer.size() <= static_cast<uint64_t>(rLazyChunk.iDataSize) - static_cast<uint64_t>(iOffset));

	if (rLazyChunk.eState.value.load(std::memory_order_acquire) >= ChunkState::kDiskLoaded)
	{
		std::memcpy(buffer.data(), rLazyChunk.pData.get() + iOffset, buffer.size());
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
		rRequest.miEntryIndex = i;
		rRequest.muiGeneration = uiGeneration;
		// Only this main-thread path moves an entry out of free, so no other thread can change it before this store
		rEntry.uiOwnership.store(PackAudioReadOwnership(AudioChunkReadState::kQueued, uiGeneration), std::memory_order_release);
#if defined(BT_DEBUG)
		if constexpr (kbAudioStreamingHarnessRig)
		{
			AudioStreamingHarnessRig::Record(AudioStreamingHarnessRigPartition::kMain, AudioStreamingHarnessRigPhase::kRefillQueued, i, crc, iOffset, buffer.size(), AudioStreamingHarnessRigQueueState::kQueued, uiGeneration, false);
		}
#endif
		mLoader.PublishWake();
		return ChunkReadResult::kPending;
	}

#if defined(BT_DEBUG)
	if constexpr (kbAudioStreamingHarnessRig)
	{
		AudioStreamingHarnessRig::CountRetry();
	}
#endif
	return ChunkReadResult::kRetry;
}

void PackChunks::CancelChunkRead(ChunkReadRequest& rRequest)
{
	if (rRequest.miEntryIndex == kiInvalidAudioReadEntry)
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
			if constexpr (kbAudioStreamingHarnessRig)
			{
				AudioStreamingHarnessRig::Record(AudioStreamingHarnessRigPartition::kMain, AudioStreamingHarnessRigPhase::kRefillCancelled, rRequest.miEntryIndex, rEntry.crc, rEntry.iOffset, rEntry.iLength, static_cast<AudioStreamingHarnessRigQueueState>(eState), uiRequestGeneration, eState != AudioChunkReadState::kLoading);
			}
#endif
			break;
		}
	}

	rRequest.miEntryIndex = kiInvalidAudioReadEntry;
	rRequest.muiGeneration = 0;
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
	bool bHeld = false;
	if constexpr (kbAudioStreamingHarnessRig)
	{
		AudioStreamingHarnessRig::Record(ePartition, AudioStreamingHarnessRigPhase::kRefillLoading, iIndex, crc, iOffset, iLength, AudioStreamingHarnessRigQueueState::kLoading, uiGeneration, false);
		bHeld = AudioStreamingHarnessRig::HoldAudioRead(crc, iOffset, iLength, iIndex, uiGeneration);
	}
#endif

	const LazyChunk& rLazyChunk = mLazyChunkMap.at(crc);
	data::DataTypes eDataType = DataTypeFromFlags(rLazyChunk.header.flags);
	HANDLE handleFile = mpHandleLazyPackFile[eDataType];

	uint64_t uiOffset = static_cast<uint64_t>(iOffset);
	uint64_t uiLength = static_cast<uint64_t>(iLength);
	uint64_t uiDataFileOffset = rLazyChunk.location.uiOffset + common::kiChunkDataOffset;
	uint64_t uiLogicalFileOffset = uiDataFileOffset + uiOffset;
	uint64_t uiAlignedOffset = common::RoundDown(uiLogicalFileOffset, static_cast<uint64_t>(kiSectorAlignment));
	uint64_t uiPrefix = uiLogicalFileOffset - uiAlignedOffset;
	uint64_t uiPhysicalSize = common::RoundUp(uiPrefix + uiLength, static_cast<uint64_t>(kiSectorAlignment));
	ASSERT(uiPhysicalSize <= static_cast<uint64_t>(kiReadBufferSize));

	OVERLAPPED overlapped {};
	overlapped.Offset = static_cast<DWORD>(uiAlignedOffset & 0xffffffff);
	overlapped.OffsetHigh = static_cast<DWORD>(uiAlignedOffset >> 32);
	DWORD uiBytesRead = 0;
	BOOL bRead = ReadFile(handleFile, mpReadBuffers[iThreadIndex], static_cast<DWORD>(uiPhysicalSize), &uiBytesRead, &overlapped);
	if (bRead == FALSE)
	{
		FailMissingRequiredAsset(mpPackFilePaths[eDataType], "audio range read was incomplete");
	}
	if (uiBytesRead < uiPrefix + uiLength)
	{
		FailMissingRequiredAsset(mpPackFilePaths[eDataType], "audio range read was incomplete");
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
	if constexpr (kbAudioStreamingHarnessRig)
	{
		AudioStreamingHarnessRig::Record(ePartition, bCurrent ? AudioStreamingHarnessRigPhase::kRefillReady : AudioStreamingHarnessRigPhase::kRefillCancelled, iIndex, crc, iOffset, iLength, bCurrent ? AudioStreamingHarnessRigQueueState::kReady : AudioStreamingHarnessRigQueueState::kFree, uiGeneration, !bCurrent);
		AudioStreamingHarnessRig::CompleteAudioRead(bHeld);
	}
#endif
	mLoader.NotifyChunkCompletion();
}


#endif // BT_CLIENT

void PackChunks::AllocateLazyChunkBuffer(LazyChunk& rLazyChunk)
{
	// RequestChunkLoad queues only chunks below kDiskLoaded, which hold no buffer.
	ASSERT(rLazyChunk.pData == nullptr);
	// Heap: the chunk buffer outlives this load (until texture adoption, or the process for island and audio chunks).
	// MakeAligned uses the C allocator, which allocation tracking never sees.
	rLazyChunk.pData = common::MakeAligned<std::byte>(rLazyChunk.iDataSize);
	ASSERT(rLazyChunk.pData != nullptr);

	RaisePeak(miPeakLazyBufferBytes, miLazyBufferBytes.fetch_add(rLazyChunk.iDataSize, std::memory_order_relaxed) + rLazyChunk.iDataSize);
	int64_t iFootprint = ArenaFootprint(rLazyChunk.iDataSize);
	RaisePeak(miPeakLazyBufferFootprint, miLazyBufferFootprint.fetch_add(iFootprint, std::memory_order_relaxed) + iFootprint);
}

void PackChunks::FreeLazyChunkBuffer(LazyChunk& rLazyChunk)
{
	ASSERT(rLazyChunk.pData != nullptr);
	rLazyChunk.pData.reset();
	miLazyBufferBytes.fetch_sub(rLazyChunk.iDataSize, std::memory_order_relaxed);
	miLazyBufferFootprint.fetch_sub(ArenaFootprint(rLazyChunk.iDataSize), std::memory_order_relaxed);
}

MemoryStats PackChunks::GetEagerStatistics() const
{
	MemoryStats statistics;
	// mpPackFileData / mEagerChunkMap are async-populated; report nothing until the eager load publishes.
	if (!mbEagerLoadComplete.load(std::memory_order_acquire))
	{
		return statistics;
	}
	for (int64_t i = 0; i < data::kDataTypeCount; ++i)
	{
		if (IsEagerChunk(static_cast<data::DataTypes>(i)))
		{
			statistics.iBytes += std::ssize(mpPackFileData[i]);
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
	// An adopted texture (kReady) has freed its buffer; skip it or the readout overstates what is held.
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
	statistics.iPeakBytes = miPeakLazyBufferBytes.load(std::memory_order_relaxed);
	statistics.iFootprintBytes = miLazyBufferFootprint.load(std::memory_order_relaxed);
	statistics.iPeakFootprintBytes = miPeakLazyBufferFootprint.load(std::memory_order_relaxed);
	return statistics;
}

MemoryStats PackChunks::GetMemoryStatistics(data::DataTypes eDataType) const
{
	MemoryStats statistics;

	if (IsEagerChunk(eDataType))
	{
		// mpPackFileData is async-populated; report 0 resident bytes until published. mpChunkLocations is set
		// synchronously in LoadPackFiles, so the chunk count is always safe to read.
		statistics.iBytes = mbEagerLoadComplete.load(std::memory_order_acquire) ? std::ssize(mpPackFileData[eDataType]) : 0;
		statistics.iCount = std::ssize(mpChunkLocations[eDataType]);
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
