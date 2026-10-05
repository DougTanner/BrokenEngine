#include "FileManager.h"

#include "LaunchOptions.h"
#include "PackChunks.h"

#include "Game.h"

namespace engine
{

using enum FileFlags;

#if defined(BT_CLIENT)

ChunkReadRequest::~ChunkReadRequest()
{
	Reset();
}

void ChunkReadRequest::Reset()
{
	if (mpPackChunks != nullptr)
	{
		mpPackChunks->CancelChunkRead(*this);
	}
}

#endif // BT_CLIENT

[[noreturn]] static void FailCreateDirectory(std::string_view directoryKind, const std::filesystem::path& rDirectoryPath, const std::error_code& rError)
{
	LOG(kLoading, kError, "Failed to create {} directory \"{}\" (error {}: {})", directoryKind, rDirectoryPath.string(), rError.value(), rError.message());
	std::string message = "Failed to create the ";
	message += directoryKind;
	message += " directory:\n\n";
	message += rDirectoryPath.string();
	message += "\n\nError ";
	message += std::to_string(rError.value());
	message += ": ";
	message += rError.message();
	common::ScopedExpectedThrows scopedExpectedThrows;
	throw std::runtime_error(message);
}

class Sha256Hasher
{
public:

	Sha256Hasher()
	{
		if (::BCryptOpenAlgorithmProvider(&mpAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
		{
			return;
		}

		DWORD uiObjectLength = 0;
		DWORD uiResultLength = 0;
		if (::BCryptGetProperty(mpAlgorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&uiObjectLength), sizeof(uiObjectLength), &uiResultLength, 0) < 0)
		{
			return;
		}
		int64_t iResultLength = static_cast<int64_t>(uiResultLength);
		if (iResultLength != static_cast<int64_t>(sizeof(uiObjectLength)))
		{
			return;
		}
		mHashObject.resize(uiObjectLength);
		if (::BCryptCreateHash(mpAlgorithm, &mpHash, mHashObject.data(), uiObjectLength, nullptr, 0, 0) < 0)
		{
			return;
		}
		mbValid = true;
	}

	~Sha256Hasher()
	{
		if (mpHash != nullptr)
		{
			::BCryptDestroyHash(mpHash);
		}
		if (mpAlgorithm != nullptr)
		{
			::BCryptCloseAlgorithmProvider(mpAlgorithm, 0);
		}
	}

	Sha256Hasher(const Sha256Hasher&) = delete;
	Sha256Hasher& operator=(const Sha256Hasher&) = delete;

	bool Update(std::span<const std::byte> bytes)
	{
		return mbValid && bytes.size() <= std::numeric_limits<ULONG>::max()
#pragma warning(suppress: 26492) // CNG pbInput is SAL input-only and documented not modified.
		    && (bytes.empty() || ::BCryptHashData(mpHash, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())), static_cast<ULONG>(bytes.size()), 0) >= 0);
	}

	bool Finish(std::array<uint8_t, 32>& rDigest)
	{
		return mbValid && ::BCryptFinishHash(mpHash, rDigest.data(), static_cast<ULONG>(rDigest.size()), 0) >= 0;
	}

private:

	BCRYPT_ALG_HANDLE mpAlgorithm = nullptr;
	BCRYPT_HASH_HANDLE mpHash = nullptr;
	std::vector<uint8_t> mHashObject;
	bool mbValid = false;
};

class FileHandle
{
public:

	explicit FileHandle(HANDLE pFile)
		: mpFile(pFile)
	{
	}
	~FileHandle()
	{
		if (mpFile != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(mpFile);
		}
	}

	HANDLE mpFile = INVALID_HANDLE_VALUE;
};

FileManager::FileManager()
{
	ASSERT(gpFileManager == nullptr);

	if (!gLaunchOptions.appDataDirectory.empty())
	{
		mAppDataDirectory = gLaunchOptions.appDataDirectory;
		LOG(kLoading, kInfo, "Using explicit AppData directory: \"{}\"", mAppDataDirectory);
	}
	else
	{
		PWSTR pWideChar = nullptr;
		int64_t iHresult = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &pWideChar);
		if (SUCCEEDED(static_cast<HRESULT>(iHresult)) && pWideChar != nullptr)
		{
			mAppDataDirectory = pWideChar;
		}
		else
		{
			// OS failure (trust boundary): leave mAppDataDirectory empty so the append below yields a working-directory-relative path instead of constructing a std::filesystem::path from null.
			LOG(kLoading, kError, "SHGetKnownFolderPath(FOLDERID_RoamingAppData) failed (hresult {}); falling back to a working-directory-relative AppData path", iHresult);
		}
		CoTaskMemFree(pWideChar);
	}
	mAppDataDirectory.append(game::kGameName);
	std::error_code appDataDirectoryError;
	std::filesystem::create_directory(mAppDataDirectory, appDataDirectoryError);
	if (appDataDirectoryError)
	{
		FailCreateDirectory("AppData", mAppDataDirectory, appDataDirectoryError);
	}
	LOG(kLoading, kDebug, "AppData directory: \"{}\"", mAppDataDirectory.string());

	wchar_t pcDirectory[MAX_PATH] {};
	GetTempPathW(static_cast<DWORD>(std::size(pcDirectory) - 1), pcDirectory);
	mTempDirectory = pcDirectory;
	mTempDirectory.append(game::kGameName);
	LOG(kLoading, kDebug, "Temp directory: \"{}\"", mTempDirectory.string());
	std::error_code tempDirectoryError;
	std::filesystem::create_directory(mTempDirectory, tempDirectoryError);
	if (tempDirectoryError)
	{
		FailCreateDirectory("temp", mTempDirectory, tempDirectoryError);
	}

	std::filesystem::path dataDirectory;
	if (!gLaunchOptions.dataDirectory.empty())
	{
		dataDirectory = gLaunchOptions.dataDirectory;
		LOG(kLoading, kInfo, "Using explicit data directory: \"{}\"", dataDirectory);
	}
	else
	{
		// Get the file path of the executable, the /Data/ folder will be beside it
		GetModuleFileNameW(nullptr, pcDirectory, static_cast<DWORD>(std::size(pcDirectory) - 1));
		dataDirectory = pcDirectory;
		dataDirectory.remove_filename();
		dataDirectory /= "Data";
		LOG(kLoading, kDebug, "Data directory: \"{}\"", dataDirectory.string());
	}

	mpPackChunks = std::make_unique<PackChunks>(dataDirectory);

	gpFileManager = this;
}

FileManager::~FileManager()
{
	// Tear down PackChunks first (drains/joins loading threads, closes pack handles, frees the pool/buffers) before
	// nulling gpFileManager; the loading threads never touch gpFileManager, so this keeps them stopped before it clears.
	mpPackChunks.reset();

	if (gpFileManager == this)
	{
		gpFileManager = nullptr;
	}
}

std::filesystem::path FileManager::GetFilePath(const FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	std::filesystem::path filePath;
	if (rFlags & kAppDataDirectory)
	{
		filePath = mAppDataDirectory;
	}
	else if (rFlags & kTempDirectory)
	{
		filePath = mTempDirectory;
	}
	else
	{
		ASSERT(false);
		return "";
	}

	filePath /= rFilename;
	return filePath;
}

std::fstream FileManager::OpenFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	// kWrite must opt into raw streaming via kStreaming; one-shot writes use WriteFileAtomically.
	ASSERT(!(rFlags & kWrite) || (rFlags & kStreaming));

	if (rFlags & kBackup)
	{
		BackupExistingFile(rFlags, rFilename);
	}

	std::filesystem::path file = GetFilePath(rFlags, rFilename);
	std::fstream fileStream(file, (rFlags & kRead ? std::ios::in : std::ios::out) | std::ios::binary);
	LOG(kLoading, kDebug, "{} \"{}\" at \"{}\"", fileStream.is_open() ? (rFlags & kRead ? "Reading" : "Writing") : "Failed to open", rFilename.string(), file.string());
	return fileStream;
}

void FileManager::BackupExistingFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	ASSERT((rFlags & kWrite) != 0);

	std::filesystem::path file = GetFilePath(rFlags, rFilename);
	std::error_code existsErrorCode;
	bool bExists = std::filesystem::exists(file, existsErrorCode);
	if (existsErrorCode)
	{
		// OS trust boundary (permissions, unavailable media): the atomic write of the main file is unaffected, so continue without the backup
		LOG(kLoading, kError, "Backup status query for \"{}\" failed: {}", file, existsErrorCode.value());
		DEBUG_BREAK();
		return;
	}
	if (!bExists)
	{
		return;
	}

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	int64_t iTime = std::chrono::system_clock::to_time_t(now);
	int64_t iEpochMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	std::tm timeStruct = *std::localtime(&iTime);
	std::ostringstream timeStringStream;
	timeStringStream << "-" << std::put_time(&timeStruct, "%Y-%m-%d") << "-" << iEpochMilliseconds;
	std::filesystem::path backupFile = file.parent_path() / (file.stem().string() + timeStringStream.str() + file.extension().string());

	std::error_code copyErrorCode;
	std::filesystem::copy_file(file, backupFile, copyErrorCode);
	if (copyErrorCode)
	{
		// OS trust boundary (disk full, permissions, antivirus): the atomic write of the main file is unaffected, so continue without the backup
		LOG(kLoading, kError, "Backup copy to \"{}\" failed: {}", backupFile, copyErrorCode.value());
		DEBUG_BREAK();
	}
}

void FileManager::RemoveFile(const FileFlags_t& rFlags, const std::filesystem::path& rFilename)
{
	std::filesystem::path file = GetFilePath(rFlags, rFilename);
	LOG(kLoading, kDebug, "Remove \"{}\" at \"{}\"", rFilename.string(), file.string());
	std::filesystem::remove(file);
}

bool FileManager::ComputeSha256(std::span<const std::byte> bytes, std::array<uint8_t, 32>& rOut)
{
	Sha256Hasher hasher;
	std::array<uint8_t, 32> digest {};
	while (!bytes.empty())
	{
		int64_t iChunkSize = bytes.size() > std::numeric_limits<ULONG>::max() ? static_cast<int64_t>(std::numeric_limits<ULONG>::max()) : static_cast<int64_t>(bytes.size());
		if (!hasher.Update(bytes.first(static_cast<size_t>(iChunkSize))))
		{
			return false;
		}
		bytes = bytes.subspan(static_cast<size_t>(iChunkSize));
	}
	if (!hasher.Finish(digest))
	{
		return false;
	}
	rOut = digest;
	return true;
}

bool FileManager::ComputeOrdinaryFileSha256(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, FileContentDigest& rOut)
{
	std::filesystem::path filePath = GetFilePath(rFlags, rFilename);
	DWORD uiAttributes = ::GetFileAttributesW(filePath.c_str());
	if (uiAttributes == INVALID_FILE_ATTRIBUTES || (uiAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
	{
		return false;
	}

	FileHandle file(::CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
	if (file.mpFile == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	BY_HANDLE_FILE_INFORMATION information {};
	if (::GetFileInformationByHandle(file.mpFile, &information) == FALSE)
	{
		return false;
	}
	if ((information.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
	{
		return false;
	}

	Sha256Hasher hasher;
	auto bufferAllocation = common::gpThreadLocal->mWorkbuffer.PushBuffer<std::byte*>(64 * 1'024);
	std::span<std::byte> buffer(static_cast<std::byte*>(bufferAllocation.mpData), 64 * 1'024);
	int64_t iByteCount = 0;
	for (;;)
	{
		DWORD uiBytesRead = 0;
		if (::ReadFile(file.mpFile, buffer.data(), static_cast<DWORD>(buffer.size()), &uiBytesRead, nullptr) == FALSE)
		{
			return false;
		}
		int64_t iBytesRead = static_cast<int64_t>(uiBytesRead);
		if (iBytesRead > static_cast<int64_t>(buffer.size()))
		{
			return false;
		}
		if (iByteCount > std::numeric_limits<int64_t>::max() - iBytesRead)
		{
			return false;
		}
		if (!hasher.Update(std::span<const std::byte>(buffer.data(), static_cast<size_t>(iBytesRead))))
		{
			return false;
		}
		iByteCount += iBytesRead;
		if (iBytesRead == 0)
		{
			break;
		}
	}

	FileContentDigest digest {.iByteCount = iByteCount};
	if (!hasher.Finish(digest.sha256))
	{
		return false;
	}
	rOut = digest;
	return true;
}

bool FileManager::CommitAtomicWrite(const FileFlags_t& rFlags, const std::filesystem::path& rFilename, bool bWriteSucceeded)
{
	std::filesystem::path temporaryFilename = rFilename;
	temporaryFilename += ".tmp";
	std::filesystem::path temporaryPath = GetFilePath(rFlags, temporaryFilename);
	std::filesystem::path destinationPath = GetFilePath(rFlags, rFilename);

	if (!bWriteSucceeded)
	{
		LOG(kLoading, kError, "WriteFileAtomically stream bad after lambda for \"{}\"", rFilename.string());
		std::error_code removeErrorCode;
		std::filesystem::remove(temporaryPath, removeErrorCode);
		return false;
	}

	std::error_code renameErrorCode;
	std::filesystem::rename(temporaryPath, destinationPath, renameErrorCode);
	if (renameErrorCode)
	{
		LOG(kLoading, kError, "WriteFileAtomically rename failed for \"{}\": {}", rFilename.string(), renameErrorCode.message());
		std::error_code removeErrorCode;
		std::filesystem::remove(temporaryPath, removeErrorCode);
		return false;
	}

	LOG(kLoading, kDebug, "WriteFileAtomically committed \"{}\"", rFilename.string());
	return true;
}


} // namespace engine
