#include "ExportJob.h"

#include "FileManager.h"

static int64_t siNextJobId = 0;


constexpr int64_t kiFingerprintMetadataMagic = 0x465052494E544D31;
constexpr int64_t kiFingerprintMetadataVersion = 2;
static std::optional<std::string> ReadFingerprintMetadata(const std::filesystem::path& rPath, common::crc_t& ruiChunkCrc)
{
	std::fstream stream(rPath, std::ios::in | std::ios::binary);
	int64_t iMagic = 0;
	int64_t iVersion = 0;
	int64_t iFingerprintCharacters = 0;
	stream.read(reinterpret_cast<char*>(&iMagic), sizeof(iMagic));
	stream.read(reinterpret_cast<char*>(&iVersion), sizeof(iVersion));
	stream.read(reinterpret_cast<char*>(&ruiChunkCrc), sizeof(ruiChunkCrc));
	stream.read(reinterpret_cast<char*>(&iFingerprintCharacters), sizeof(iFingerprintCharacters));
	if (!stream)
	{
		return std::nullopt;
	}
	if (iMagic != kiFingerprintMetadataMagic)
	{
		return std::nullopt;
	}
	if (iVersion != kiFingerprintMetadataVersion)
	{
		return std::nullopt;
	}
	if (iFingerprintCharacters <= 0 || iFingerprintCharacters > 1'024 * 1'024)
	{
		return std::nullopt;
	}
	std::string fingerprint(static_cast<size_t>(iFingerprintCharacters), '\0');
	stream.read(fingerprint.data(), fingerprint.size());
	return stream ? std::optional(std::move(fingerprint)) : std::nullopt;
}

static void WriteFingerprintMetadata(const std::filesystem::path& rPath, std::string_view fingerprint, common::crc_t uiChunkCrc)
{
	std::fstream stream(rPath, std::ios::out | std::ios::binary);
	int64_t iFingerprintCharacters = static_cast<int64_t>(fingerprint.size());
	stream.write(reinterpret_cast<const char*>(&kiFingerprintMetadataMagic), sizeof(kiFingerprintMetadataMagic));
	stream.write(reinterpret_cast<const char*>(&kiFingerprintMetadataVersion), sizeof(kiFingerprintMetadataVersion));
	stream.write(reinterpret_cast<const char*>(&uiChunkCrc), sizeof(uiChunkCrc));
	stream.write(reinterpret_cast<const char*>(&iFingerprintCharacters), sizeof(iFingerprintCharacters));
	stream.write(fingerprint.data(), fingerprint.size());
	stream.close();
	VERIFY_SUCCESS(stream.good());
}


std::optional<std::string> ExportJob::ReadMarkerFile(const std::filesystem::path& rPath)
{
	std::ifstream stream(rPath, std::ios::binary);
	if (!stream)
	{
		return std::nullopt;
	}
	std::string fingerprint = std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
	return !stream.bad() ? std::optional(std::move(fingerprint)) : std::nullopt;
}

// Writing to a temporary file keeps a partial fingerprint from replacing the marker.
void ExportJob::WriteMarkerFile(const std::filesystem::path& rPath, std::string_view fingerprint)
{
	std::filesystem::path temporaryPath = rPath;
	temporaryPath += ".tmp";
	std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
	stream.write(fingerprint.data(), static_cast<std::streamsize>(fingerprint.size()));
	stream.close();
	VERIFY_SUCCESS(stream.good());
	VERIFY_SUCCESS(MoveFileExW(temporaryPath.native().c_str(), rPath.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
}

ExportJob::ExportJob(common::ChunkFlags_t rChunkFlags, const std::filesystem::path& rFile, int64_t iVersion)
: miId(siNextJobId++)
, miVersion(iVersion)
, mChunkFlags(rChunkFlags)
, mInputPath(rFile)
{
	if (mInputPath.native().starts_with(gpFileManager->mpInputDirectories[0].native()))
	{
		mRelativeDirectory = mInputPath.native().substr(gpFileManager->mpInputDirectories[0].native().size() + 1);
	}
	else
	{
		mRelativeDirectory = mInputPath.native().substr(gpFileManager->mpInputDirectories[1].native().size() + 1);
	}
	mRelativeDirectory.remove_filename();

	// The cache directory is shared across worktrees, so the exporter version is part of the entry name:
	// exporters built from different sources then keep separate entries instead of invalidating each other's.
	// SetContentKey does the same for inputs that differ between worktrees.
	mChunkFile = gpFileManager->mCacheDirectory;
	mChunkFile /= mRelativeDirectory;
	std::filesystem::create_directories(mChunkFile);
	mChunkFile /= mInputPath.filename();
	mChunkFile += std::format(".v{}.chunk", miVersion);

	mCacheMetadataFile = gpFileManager->mCacheDirectory;
	mCacheMetadataFile /= mRelativeDirectory;
	mCacheMetadataFile /= mInputPath.filename();
	mCacheMetadataFile += std::format(".v{}.meta", miVersion);

	mRelativeFile = mRelativeDirectory.string() + mInputPath.filename().string();
	mCrc = common::Crc(mRelativeFile);
}

void ExportJob::SetContentKey(common::crc_t uiContentKey)
{
	mContentKey = std::format("{:016x}", uiContentKey);
	mChunkFile.replace_extension(std::format("{}.chunk", mContentKey));
	mCacheMetadataFile.replace_extension(std::format("{}.meta", mContentKey));
}

std::tuple<common::ChunkHeader*, std::span<std::byte>> ExportJob::AllocateHeaderAndData(int64_t iDataSize)
{
	int64_t iDataOffset = common::kiChunkDataOffset;
	int64_t iTotalSizeAligned = iDataOffset;
	iTotalSizeAligned += common::RoundUp<int64_t, common::kiAlignmentBytes>(iDataSize);

	mHeaderAndData.resize(iTotalSizeAligned);
	reinterpret_cast<common::ChunkHeader*>(mHeaderAndData.data())->iSize = iDataSize;
	return std::make_tuple(reinterpret_cast<common::ChunkHeader*>(mHeaderAndData.data()), std::span(&mHeaderAndData.at(iDataOffset), static_cast<size_t>(std::ssize(mHeaderAndData) - iDataOffset)));
}

bool ExportJob::CheckDirty([[maybe_unused]] const std::filesystem::path& rPackFile)
{
	// The pack is not inspected per job: RunExportJobs compares the published pack's timestamp against
	// every clean job's .meta fingerprint, so an export killed before the pack rename stays dirty there,
	// and every clean job's .meta chunk CRC against the published manifest, so an entry returned to by a revert does too.

	// A fingerprint throws on an unreadable file, and discovery has no per-asset catch. A dirty job fingerprints again
	// in RunExport, which throws into the job's future, so the failure stays per asset.
	std::string inputFingerprint;
	try
	{
		inputFingerprint = GetInputFingerprint();
	}
	catch (const std::exception& rException)
	{
		LOG(kDefault, kWarning, "Could not fingerprint \"{}\": {}", mInputPath.string(), rException.what());
		mbDirty = true;
		return mbDirty;
	}
	if (mContentKey.empty())
	{
		SetContentKey(common::Crc(inputFingerprint));
	}

	if (!std::filesystem::exists(mChunkFile))
	{
		LOG(kDefault, kDebug, "Chunk file \"{}\" does not exist", mChunkFile.string());
		mbDirty = true;
		return mbDirty;
	}

	if (std::filesystem::file_size(mChunkFile) < sizeof(kiMagic) + sizeof(int64_t) + common::kiChunkDataOffset)
	{
		LOG(kDefault, kWarning, "Chunk file \"{}\" is truncated", mChunkFile.string());
		mbDirty = true;
		return mbDirty;
	}
	std::fstream chunkFileStream(mChunkFile, std::ios::in | std::ios::binary);

	int64_t piMagicAndVersion[2] = {};
	chunkFileStream.read(reinterpret_cast<char*>(piMagicAndVersion), sizeof(piMagicAndVersion));
	chunkFileStream.close();

	if (!chunkFileStream || piMagicAndVersion[0] != kiMagic || piMagicAndVersion[1] != miVersion)
	{
		LOG(kDefault, kWarning, "Chunk file \"{}\" has invalid magic {:#018x} or version {}", mChunkFile.string(), piMagicAndVersion[0], piMagicAndVersion[1]);
		mbDirty = true;
		return mbDirty;
	}

	std::optional<std::string> optionalCachedFingerprint = ReadFingerprintMetadata(mCacheMetadataFile, muiCachedChunkCrc);
	if (!optionalCachedFingerprint.has_value() || optionalCachedFingerprint.value() != inputFingerprint)
	{
		LOG(kDefault, kDebug, "Input fingerprint changed for \"{}\"", mInputPath.string());
		mbDirty = true;
		return mbDirty;
	}

	mbDirty = false;
	return mbDirty;
}

bool ExportJob::ReadCachedChunk(std::vector<std::byte>& rHeaderAndData) const
{
	int64_t iChunkFileSize = std::filesystem::file_size(mChunkFile);
	int64_t iHeaderAndDataSize = iChunkFileSize - sizeof(kiMagic) - sizeof(int64_t);
	rHeaderAndData.resize(iHeaderAndDataSize);

	std::fstream fileStream(mChunkFile, std::ios::in | std::ios::binary);
	fileStream.seekg(sizeof(kiMagic) + sizeof(int64_t)); // Skip magic and version
	fileStream.read(reinterpret_cast<char*>(rHeaderAndData.data()), static_cast<std::streamsize>(std::ssize(rHeaderAndData)));

	// CheckDirty only validated the 16-byte header, and resize() zero-inits the buffer, so a short read
	// (truncated/interrupted chunk) would silently pack a zero tail. Verify the full body was read; if not,
	// the caller discards the cache and takes a full dirty re-export rather than shipping the zeroed bytes.
	if (!fileStream.good() || fileStream.gcount() != std::ssize(rHeaderAndData))
	{
		LOG(kDefault, kWarning, "Cached chunk file \"{}\" is truncated ({} of {} bytes read); re-exporting", mChunkFile.string(), fileStream.gcount(), std::ssize(rHeaderAndData));
		return false;
	}
	return true;
}

std::vector<std::byte>& ExportJob::RunExport()
{
	ScopedLogIndent scopedLogIndentOuter;
	ScopedLogIndent scopedLogIndentInner;

	if (!mbDirty)
	{
		// Trust boundary: a cached chunk body is opaque bytes an earlier run left on disk, and CheckDirty does not read
		// them. The .meta CRC proves the body is the one RunExport wrote, so a truncated or edited body re-exports. The
		// header CRC still needs its own check: a case-only rename keeps the fingerprint and so the key, the cache's
		// case-insensitive volume resolves the new path to the old entry, and that header holds the old path's CRC,
		// which the runtime halts on beside a manifest holding the new one.
		if (!ReadCachedChunk(mHeaderAndData))
		{
			mbDirty = true;
		}
		else if (common::Crc(std::span<const std::byte>(mHeaderAndData)) != muiCachedChunkCrc)
		{
			LOG(kDefault, kWarning, "Cached chunk file \"{}\" failed validation (body CRC mismatch); re-exporting", mChunkFile.string());
			mbDirty = true;
		}
		else if (reinterpret_cast<const common::ChunkHeader*>(mHeaderAndData.data())->crc != mCrc)
		{
			LOG(kDefault, kWarning, "Cached chunk file \"{}\" failed validation (chunk header CRC mismatch); re-exporting", mChunkFile.string());
			mbDirty = true;
		}
		else
		{
			return mHeaderAndData;
		}

		// Export() reaches the buffer through AllocateHeaderAndData's resize, which leaves surviving elements
		// untouched, so the rejected cache bytes have to go before they can survive into a fresh chunk.
		mHeaderAndData.clear();
	}

	std::string inputFingerprint = GetInputFingerprint();
	try
	{
		Export();
	}
	catch (...)
	{
		CleanupOnFailure();
		throw;
	}

	std::filesystem::path relativeFile = mRelativeDirectory;
	relativeFile /= mInputPath.filename();

	common::ChunkHeader* pChunkHeader = reinterpret_cast<common::ChunkHeader*>(mHeaderAndData.data());
	pChunkHeader->iMagic = common::ChunkHeader::kiMagic;
	pChunkHeader->crc = common::Crc(relativeFile.string());
	LOG(kDefault, kDebug, "\"{}\" -> {:#018x}", relativeFile.string(), pChunkHeader->crc);
	pChunkHeader->flags = mChunkFlags;
	std::string relativeFileString = common::ToString(relativeFile.native());
	ASSERT(relativeFileString.length() < MAX_PATH);
	std::memcpy(pChunkHeader->pcPath, relativeFileString.c_str(), sizeof(*relativeFileString.c_str()) * relativeFileString.length());

	// The fingerprint is the cache completion marker. Remove it before mutating the chunk so any write
	// or derived-metadata failure leaves the job unambiguously dirty.
	std::filesystem::remove(mCacheMetadataFile);

	std::fstream fileStream(mChunkFile, std::ios::out | std::ios::binary);
	int64_t piMagicAndVersion[2] = {kiMagic, miVersion};
	fileStream.write(reinterpret_cast<char*>(piMagicAndVersion), sizeof(piMagicAndVersion));
	fileStream.write(reinterpret_cast<char*>(mHeaderAndData.data()), static_cast<std::streamsize>(std::ssize(mHeaderAndData)));
	fileStream.close();
	VERIFY_SUCCESS(fileStream.good());

	UpdateCacheMetadata();
	WriteFingerprintMetadata(mCacheMetadataFile, inputFingerprint, common::Crc(std::span<const std::byte>(mHeaderAndData)));

	return mHeaderAndData;
}

std::string ExportJob::GetInputFingerprint() const
{
	return gpFileManager->mpInputFingerprintCache->Get(mInputPath);
}
