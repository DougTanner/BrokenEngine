#include "ExportJob.h"

#include "FileManager.h"

static int64_t siNextJobId = 0;

namespace
{

constexpr int64_t kiFingerprintMetadataMagic = 0x465052494E544D31;
constexpr int64_t kiFingerprintMetadataVersion = 1;
std::optional<std::string> ReadFingerprintMetadata(const std::filesystem::path& rPath)
{
	std::fstream stream(rPath, std::ios::in | std::ios::binary);
	int64_t iMagic = 0;
	int64_t iVersion = 0;
	int64_t iFingerprintCharacters = 0;
	stream.read(reinterpret_cast<char*>(&iMagic), sizeof(iMagic));
	stream.read(reinterpret_cast<char*>(&iVersion), sizeof(iVersion));
	stream.read(reinterpret_cast<char*>(&iFingerprintCharacters), sizeof(iFingerprintCharacters));
	if (!stream || iMagic != kiFingerprintMetadataMagic || iVersion != kiFingerprintMetadataVersion || iFingerprintCharacters <= 0 || iFingerprintCharacters > 1024 * 1024)
	{
		return std::nullopt;
	}
	std::string fingerprint(static_cast<size_t>(iFingerprintCharacters), '\0');
	stream.read(fingerprint.data(), fingerprint.size());
	return stream ? std::optional(std::move(fingerprint)) : std::nullopt;
}

void WriteFingerprintMetadata(const std::filesystem::path& rPath, std::string_view fingerprint)
{
	std::filesystem::path temporaryPath = rPath;
	temporaryPath += ".tmp";
	std::fstream stream(temporaryPath, std::ios::out | std::ios::binary);
	int64_t iFingerprintCharacters = static_cast<int64_t>(fingerprint.size());
	stream.write(reinterpret_cast<const char*>(&kiFingerprintMetadataMagic), sizeof(kiFingerprintMetadataMagic));
	stream.write(reinterpret_cast<const char*>(&kiFingerprintMetadataVersion), sizeof(kiFingerprintMetadataVersion));
	stream.write(reinterpret_cast<const char*>(&iFingerprintCharacters), sizeof(iFingerprintCharacters));
	stream.write(fingerprint.data(), fingerprint.size());
	stream.close();
	VERIFY_SUCCESS(stream.good());
	VERIFY_SUCCESS(MoveFileExW(temporaryPath.native().c_str(), rPath.native().c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
}

// Trust boundary: a cached chunk body is opaque bytes an earlier run left on disk, and CheckDirty only covers the
// outer marker and the source fingerprint, so a same-size edit of the body would be republished into a new pack.
// Checks identity and extent only, and returns nullptr when they hold or the reason to report. Payload bytes are
// deliberately not covered; the runtime enforces the compressed-payload contract when the pack is loaded.
const char* ValidateCachedChunkBody(std::span<const std::byte> body, common::crc_t crc, const std::string& rRelativeFile, bool bAllowTail)
{
	if (body.size() < static_cast<size_t>(common::kiChunkDataOffset))
	{
		return "body smaller than its header";
	}

	const common::ChunkHeader* pHeader = reinterpret_cast<const common::ChunkHeader*>(body.data());
	if (pHeader->iMagic != common::ChunkHeader::kiMagic)
	{
		return "chunk header magic mismatch";
	}
	if (pHeader->crc != crc)
	{
		return "chunk header CRC mismatch";
	}
	// Comparing one byte past the path compares the terminator too, so this also proves the cached path is
	// NUL-terminated where every consumer that reads pcPath as a C string expects it to end.
	if (rRelativeFile.size() >= std::size(pHeader->pcPath) || std::memcmp(pHeader->pcPath, rRelativeFile.c_str(), rRelativeFile.size() + 1) != 0)
	{
		return "chunk header path mismatch";
	}

	// Bound iSize by the body before rounding it up, so a corrupt near-maximum value cannot overflow the round.
	if (pHeader->iSize < 0 || pHeader->iSize > static_cast<int64_t>(body.size()))
	{
		return "chunk header size outside chunk";
	}
	// AllocateHeaderAndData gives a fresh body exactly the header plus the alignment-rounded data, so any other
	// extent was not produced by this job. Scene jobs append animation data past that point (ExportScene::Export).
	int64_t iExpectedSize = common::kiChunkDataOffset + common::RoundUp<int64_t, common::kiAlignmentBytes>(pHeader->iSize);
	if (bAllowTail ? static_cast<int64_t>(body.size()) < iExpectedSize : static_cast<int64_t>(body.size()) != iExpectedSize)
	{
		return "chunk header size does not match body";
	}

	return nullptr;
}

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

std::tuple<common::ChunkHeader*, std::span<std::byte>> ExportJob::AllocateHeaderAndData(int64_t iDataSize)
{
	int64_t iDataOffset = common::kiChunkDataOffset;
	int64_t iTotalSizeAligned = iDataOffset;
	iTotalSizeAligned += common::RoundUp<int64_t, common::kiAlignmentBytes>(iDataSize);

	mHeaderAndData.resize(iTotalSizeAligned);
	reinterpret_cast<common::ChunkHeader*>(mHeaderAndData.data())->iSize = iDataSize;
	return std::make_tuple(reinterpret_cast<common::ChunkHeader*>(mHeaderAndData.data()), std::span(&mHeaderAndData.at(iDataOffset), mHeaderAndData.size() - iDataOffset));
}

bool ExportJob::CheckDirty(const std::filesystem::path& rPackFile)
{
	// The pack is not inspected per job: RunExportJobs compares the published pack's timestamp against
	// every clean job's .meta fingerprint, so an export killed before the pack rename stays dirty there.
	(void)rPackFile;

	// Clean export?
	if (gpFileManager->mbCleanExport)
	{
		mbDirty = true;
		return mbDirty;
	}

	// Does the chunk file exist?
	if (!std::filesystem::exists(mChunkFile))
	{
		LOG(kDefault, kDebug, "Chunk file \"{}\" does not exist", mChunkFile.string());
		mbDirty = true;
		return mbDirty;
	}

	// Verify chunk file magic and version
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

	if (!chunkFileStream || piMagicAndVersion[0] != kiMagic || piMagicAndVersion[1] != GetVersion())
	{
		LOG(kDefault, kWarning, "Chunk file \"{}\" has invalid magic {:#018x} or version {}", mChunkFile.string(), piMagicAndVersion[0], piMagicAndVersion[1]);
		mbDirty = true;
		return mbDirty;
	}

	std::string inputFingerprint = GetInputFingerprint();
	std::optional<std::string> cachedFingerprint = ReadFingerprintMetadata(mCacheMetadataFile);
	if (!cachedFingerprint.has_value() || cachedFingerprint.value() != inputFingerprint)
	{
		LOG(kDefault, kDebug, "Input fingerprint changed for \"{}\"", mInputPath.string());
		mbDirty = true;
		return mbDirty;
	}

	mbDirty = false;
	return mbDirty;
}

std::vector<std::byte>& ExportJob::RunExport()
{
	common::ThreadLocal threadLocal(4 * 1024, miId, false);
	ScopedLogIndent scopedLogIndentOuter;
	ScopedLogIndent scopedLogIndentInner;

	// Load cached chunk file
	if (!mbDirty)
	{
		int64_t iChunkFileSize = std::filesystem::file_size(mChunkFile);
		int64_t iHeaderAndDataSize = iChunkFileSize - sizeof(kiMagic) - sizeof(int64_t);
		mHeaderAndData.resize(iHeaderAndDataSize);

		std::fstream fileStream(mChunkFile, std::ios::in | std::ios::binary);
		fileStream.seekg(sizeof(kiMagic) + sizeof(int64_t)); // Skip magic and version
		fileStream.read(reinterpret_cast<char*>(mHeaderAndData.data()), mHeaderAndData.size());

		// CheckDirty only validated the 16-byte header, and resize() zero-inits the buffer, so a short read
		// (truncated/interrupted chunk) would silently pack a zero tail. Verify the full body was read; if not,
		// discard the cache and fall through to a full dirty re-export rather than shipping the zeroed bytes.
		if (!fileStream.good() || fileStream.gcount() != static_cast<std::streamsize>(mHeaderAndData.size()))
		{
			LOG(kDefault, kWarning, "Cached chunk file \"{}\" is truncated ({} of {} bytes read); re-exporting", mChunkFile.string(), fileStream.gcount(), mHeaderAndData.size());
			mbDirty = true;
		}
		else if (const char* pReason = ValidateCachedChunkBody(mHeaderAndData, mCrc, mRelativeFile, mChunkFlags & common::ChunkFlags::kScene); pReason != nullptr)
		{
			LOG(kDefault, kWarning, "Cached chunk file \"{}\" failed validation ({}); re-exporting", mChunkFile.string(), pReason);
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

	// Write chunk file with magic and version
	std::fstream fileStream(mChunkFile, std::ios::out | std::ios::binary);
	int64_t piMagicAndVersion[2] = { kiMagic, GetVersion() };
	fileStream.write(reinterpret_cast<char*>(piMagicAndVersion), sizeof(piMagicAndVersion));
	fileStream.write(reinterpret_cast<char*>(mHeaderAndData.data()), mHeaderAndData.size());
	fileStream.close();
	VERIFY_SUCCESS(fileStream.good());

	UpdateCacheMetadata();
	WriteFingerprintMetadata(mCacheMetadataFile, inputFingerprint);

	return mHeaderAndData;
}

std::string ExportJob::GetInputFingerprint() const
{
	return gpFileManager->GetFingerprint(mInputPath);
}
