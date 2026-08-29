#include "FileManager.h"
#include "DiagnosticReporter.h"

namespace
{

using ScopedHandle = std::unique_ptr<void, decltype(&CloseHandle)>;

struct SymbolicLinkReparseDataBuffer
{
	ULONG uiReparseTag;
	USHORT uiReparseDataLength;
	USHORT uiReserved;
	USHORT uiSubstituteNameOffset;
	USHORT uiSubstituteNameLength;
	USHORT uiPrintNameOffset;
	USHORT uiPrintNameLength;
	ULONG uiFlags;
	WCHAR cPathBuffer[1];
};

std::filesystem::path PathFromUtf8(const std::string& rValue)
{
	const std::u8string value(reinterpret_cast<const char8_t*>(rValue.data()), rValue.size());
	return std::filesystem::path(value);
}

std::optional<std::filesystem::path> FindExecutableOnPath(const wchar_t* pcExecutable)
{
	DWORD uiCharacters = SearchPathW(nullptr, pcExecutable, nullptr, 0, nullptr, nullptr);
	if (uiCharacters == 0)
	{
		return std::nullopt;
	}
	std::vector<wchar_t> path(uiCharacters);
	DWORD uiWritten = SearchPathW(nullptr, pcExecutable, nullptr, static_cast<DWORD>(path.size()), path.data(), nullptr);
	if (uiWritten == 0 || uiWritten >= path.size())
	{
		return std::nullopt;
	}
	return std::filesystem::path(std::wstring(path.data(), uiWritten));
}

std::string TrimLine(std::string value)
{
	while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == '\0'))
	{
		value.pop_back();
	}
	return value;
}

std::optional<std::filesystem::path> RunGit(const std::filesystem::path& rGit, const std::filesystem::path& rRoot, const wchar_t* pcArguments)
{
	std::wstring parameters = L" -C \"" + rRoot.native() + L"\" " + pcArguments;
	common::ExecutableResult result;
	try
	{
		result = common::RunExecutable(rGit, parameters);
	}
	catch (const std::exception&)
	{
		return std::nullopt;
	}
	if (result.miExitCode != 0)
	{
		return std::nullopt;
	}
	std::string output = TrimLine(std::move(result.mOutput));
	if (output.empty())
	{
		return std::nullopt;
	}
	return std::filesystem::absolute(PathFromUtf8(output)).lexically_normal();
}

bool PathEqual(const std::filesystem::path& rLeft, const std::filesystem::path& rRight)
{
	return CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool PathLess(const std::filesystem::path& rLeft, const std::filesystem::path& rRight)
{
	int iResult = CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, TRUE);
	return iResult == CSTR_LESS_THAN || (iResult == CSTR_EQUAL && CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, FALSE) == CSTR_LESS_THAN);
}

bool IsOrdinaryDirectory(const std::filesystem::path& rPath)
{
	DWORD uiAttributes = GetFileAttributesW(rPath.native().c_str());
	return uiAttributes != INVALID_FILE_ATTRIBUTES && (uiAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

bool IsReparsePoint(const std::filesystem::path& rPath)
{
	DWORD uiAttributes = GetFileAttributesW(rPath.native().c_str());
	return uiAttributes != INVALID_FILE_ATTRIBUTES && (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

std::filesystem::path GetRepositoryRootFromExecutable()
{
	std::wstring executableBuffer(32'768, L'\0');
	DWORD uiLength = GetModuleFileNameW(nullptr, executableBuffer.data(), static_cast<DWORD>(executableBuffer.size()));
	if (uiLength == 0 || uiLength >= executableBuffer.size())
	{
		throw std::runtime_error(std::format("Unable to resolve DataPacker executable path (GetModuleFileNameW returned {}, Win32 {})", uiLength, GetLastError()));
	}

	std::filesystem::path executablePath(std::wstring(executableBuffer.data(), uiLength));
	const std::wstring executableName = executablePath.filename().native();
	if (CompareStringOrdinal(executableName.c_str(), -1, L"DataPacker.exe", -1, TRUE) != CSTR_EQUAL
		&& CompareStringOrdinal(executableName.c_str(), -1, L"DataPacker.Debug.exe", -1, TRUE) != CSTR_EQUAL)
	{
		throw std::runtime_error(std::format("DataPacker executable path has unexpected layout: {} (expected suffix DataPacker\\Platforms\\VisualStudio2026\\Output\\DataPacker.exe or DataPacker.Debug.exe)", executablePath.string()));
	}

	static constexpr const wchar_t* kpwcExpectedDirectories[] =
	{
		L"Output",
		L"VisualStudio2026",
		L"Platforms",
		L"DataPacker",
	};
	std::filesystem::path repositoryRoot = executablePath.parent_path();
	for (const wchar_t* pwcExpected : kpwcExpectedDirectories)
	{
		if (repositoryRoot.empty() || CompareStringOrdinal(repositoryRoot.filename().native().c_str(), -1, pwcExpected, -1, TRUE) != CSTR_EQUAL)
		{
			throw std::runtime_error(std::format("DataPacker executable path has unexpected layout: {} (expected suffix DataPacker\\Platforms\\VisualStudio2026\\Output\\DataPacker.exe or DataPacker.Debug.exe)", executablePath.string()));
		}
		repositoryRoot = repositoryRoot.parent_path();
	}
	return repositoryRoot;
}

void EstablishOutputDestinationParent(const std::filesystem::path& rDestination)
{
	const std::filesystem::path& rParent = rDestination.parent_path();
	std::filesystem::create_directories(rParent);
	if (!IsOrdinaryDirectory(rParent))
	{
		throw std::runtime_error(std::format("Output destination parent must be an ordinary non-reparse directory: {} (destination {})", rParent.string(), rDestination.string()));
	}
}

bool IsRecognizedLinkRaw(const std::filesystem::path& rLink, const std::filesystem::path& rExpected)
{
	ScopedHandle link(CreateFileW(rLink.native().c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr), CloseHandle);
	if (link.get() == nullptr || link.get() == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	std::vector<uint8_t> bytes(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
	DWORD uiReturned = 0;
	if (!DeviceIoControl(link.get(), FSCTL_GET_REPARSE_POINT, nullptr, 0, bytes.data(), static_cast<DWORD>(bytes.size()), &uiReturned, nullptr))
	{
		return false;
	}
	const SymbolicLinkReparseDataBuffer* pData = reinterpret_cast<const SymbolicLinkReparseDataBuffer*>(bytes.data());
	if (pData->uiReparseTag != IO_REPARSE_TAG_SYMLINK)
	{
		return false;
	}
	std::wstring target(pData->cPathBuffer + pData->uiSubstituteNameOffset / sizeof(wchar_t), pData->uiSubstituteNameLength / sizeof(wchar_t));
	if (target.rfind(L"\\??\\", 0) == 0)
	{
		target.erase(0, 4);
	}
	std::filesystem::path targetPath(target);
	if (targetPath.is_relative())
	{
		targetPath = rLink.parent_path() / targetPath;
	}
	targetPath = std::filesystem::absolute(targetPath).lexically_normal().make_preferred();
	std::filesystem::path expectedPath = std::filesystem::absolute(rExpected).lexically_normal().make_preferred();
	return PathEqual(targetPath, expectedPath);
}

uint64_t AddChecked(uint64_t uiLeft, uint64_t uiRight)
{
	if (uiLeft > std::numeric_limits<uint64_t>::max() - uiRight)
	{
		throw std::runtime_error("Output materialization size overflow");
	}
	return uiLeft + uiRight;
}

struct LinkedWorktreeIdentity
{
	std::filesystem::path primaryRoot;
	std::filesystem::path expectedOutput;
};

template <typename TReject>
std::optional<LinkedWorktreeIdentity> DiscoverLinkedWorktreeIdentity(const std::filesystem::path& rRepositoryRoot, std::string_view projectName, const std::filesystem::path& rOutputDirectory, const TReject& rReject)
{
	std::optional<std::filesystem::path> git = FindExecutableOnPath(L"git.exe");
	if (!git)
	{
		rReject();
		LOG(kDefault, kWarning, "Git unavailable; worktree output linking disabled");
		return std::nullopt;
	}
	std::optional<std::filesystem::path> gitDirectory = RunGit(*git, rRepositoryRoot, L"rev-parse --path-format=absolute --git-dir");
	std::optional<std::filesystem::path> commonDirectory = RunGit(*git, rRepositoryRoot, L"rev-parse --path-format=absolute --git-common-dir");
	if (!gitDirectory || !commonDirectory || PathEqual(*gitDirectory, *commonDirectory))
	{
		rReject();
		return std::nullopt;
	}

	std::wstring parameters = L" -C \"" + rRepositoryRoot.native() + L"\" worktree list --porcelain -z";
	common::ExecutableResult result {};
	try
	{
		result = common::RunExecutable(*git, parameters);
	}
	catch (const std::exception&)
	{
		rReject();
		LOG(kDefault, kWarning, "Git worktree discovery failed; output linking disabled");
		return std::nullopt;
	}
	if (result.miExitCode != 0 || result.mOutput.rfind("worktree ", 0) != 0)
	{
		rReject();
		LOG(kDefault, kWarning, "Malformed Git worktree metadata; output linking disabled");
		return std::nullopt;
	}
	size_t uiEnd = result.mOutput.find('\0');
	std::filesystem::path primaryRoot = PathFromUtf8(result.mOutput.substr(9, uiEnd - 9));
	std::optional<std::filesystem::path> primaryCommon = RunGit(*git, primaryRoot, L"rev-parse --path-format=absolute --git-common-dir");
	if (!primaryCommon || !PathEqual(*primaryCommon, *commonDirectory))
	{
		rReject();
		LOG(kDefault, kWarning, "Inconsistent Git worktree metadata; output linking disabled");
		return std::nullopt;
	}

	std::filesystem::path expected = (rRepositoryRoot / "Projects" / std::string(projectName) / "Platforms/VisualStudio2026/Output/Data").lexically_normal();
	if (!PathEqual(expected, rOutputDirectory))
	{
		// Exporting a linked worktree anywhere else leaves ThirdParty and the output roots on unvalidated
		// worktree symlinks, which only fails hours later in attribution collection.
		throw std::runtime_error(std::format("Linked worktree must export into its canonical output directory: supplied {}, expected {}", rOutputDirectory.string(), expected.string()));
	}
	return LinkedWorktreeIdentity { .primaryRoot = std::move(primaryRoot), .expectedOutput = std::move(expected), };
}

struct MaterializationInventory
{
	std::vector<std::filesystem::path> files;
	std::vector<size_t> order;
	uint64_t uiAllocation = 0;
};

MaterializationInventory BuildMaterializationInventory(const std::filesystem::path& rSource, const std::filesystem::path& rDestination)
{
	MaterializationInventory inventory {};
	DWORD uiSectorsPerCluster = 0;
	DWORD uiBytesPerSector = 0;
	DWORD uiFreeClusters = 0;
	DWORD uiTotalClusters = 0;
	if (!GetDiskFreeSpaceW(rDestination.root_path().native().c_str(), &uiSectorsPerCluster, &uiBytesPerSector, &uiFreeClusters, &uiTotalClusters))
	{
		throw std::runtime_error("GetDiskFreeSpaceW failed");
	}
	uint64_t uiClusterBytes = static_cast<uint64_t>(uiSectorsPerCluster) * uiBytesPerSector;
	for (const std::filesystem::directory_entry& rEntry : std::filesystem::recursive_directory_iterator(rSource))
	{
		DWORD uiAttributes = GetFileAttributesW(rEntry.path().native().c_str());
		if (uiAttributes == INVALID_FILE_ATTRIBUTES || (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			throw std::runtime_error(std::format("Nested output reparse point rejected: {}", rEntry.path().string()));
		}
		if (rEntry.is_regular_file())
		{
			inventory.files.push_back(rEntry.path());
			uint64_t uiSize = rEntry.file_size();
			inventory.uiAllocation = AddChecked(inventory.uiAllocation, AddChecked(uiSize, uiClusterBytes - 1) / uiClusterBytes * uiClusterBytes);
		}
		else if (!rEntry.is_directory())
		{
			throw std::runtime_error(std::format("Unsupported output entry: {}", rEntry.path().string()));
		}
	}
	inventory.order.resize(inventory.files.size());
	std::iota(inventory.order.begin(), inventory.order.end(), 0);
	std::sort(inventory.order.begin(), inventory.order.end(), [&inventory, &rSource](size_t uiLeftIndex, size_t uiRightIndex)
	{
		return PathLess(std::filesystem::relative(inventory.files.at(uiLeftIndex), rSource), std::filesystem::relative(inventory.files.at(uiRightIndex), rSource));
	});
	return inventory;
}

std::filesystem::path AcquireMaterializationStaging(const std::filesystem::path& rDestination)
{
	for (uint32_t uiAttempt = 0; uiAttempt <= 15; ++uiAttempt)
	{
		std::filesystem::path staging = rDestination;
		staging += std::format(".materializing.{}.{}", GetCurrentProcessId(), uiAttempt);
		std::error_code errorCode;
		if (std::filesystem::create_directory(staging, errorCode))
		{
			return staging;
		}
	}
	throw std::runtime_error("Unable to create unique output staging directory");
}

void PublishMaterializedOutput(const std::filesystem::path& rSource, const std::filesystem::path& rDestination, const std::vector<std::filesystem::path>& rFiles, const std::vector<size_t>& rOrder, const std::filesystem::path& rStaging, bool bRecognizedPrimaryLink)
{
	try
	{
		for (size_t uiIndex : rOrder)
		{
			const std::filesystem::path& rSourceFile = rFiles.at(uiIndex);
			std::filesystem::path destination = rStaging / std::filesystem::relative(rSourceFile, rSource);
			std::filesystem::create_directories(destination.parent_path());
			std::filesystem::copy_file(rSourceFile, destination);
			std::filesystem::last_write_time(destination, std::filesystem::last_write_time(rSourceFile));
		}
		if (bRecognizedPrimaryLink)
		{
			std::filesystem::remove(rDestination);
		}
		std::filesystem::rename(rStaging, rDestination);
	}
	catch (...)
	{
		bool bPreserveStaging = false;
		if (!std::filesystem::exists(rDestination) && bRecognizedPrimaryLink)
		{
			EstablishOutputDestinationParent(rDestination);
			if (!CreateSymbolicLinkW(rDestination.native().c_str(), rSource.native().c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
			{
				bPreserveStaging = true;
				LOG(kDefault, kError, "Failed to restore output link \"{}\"; complete recovery copy retained at \"{}\" (Win32 {})", rDestination.string(), rStaging.string(), GetLastError());
			}
		}
		if (!bPreserveStaging && std::filesystem::exists(rStaging))
		{
			std::filesystem::remove_all(rStaging);
		}
		throw;
	}
}

}

FileManager::FileManager(std::span<char*> argvSpan, InitializationMode eMode)
{
	ASSERT(gpFileManager == nullptr);

	gpFileManager = this;
	wchar_t pcForbidExpensiveExport[2] {};
	DWORD uiForbidExpensiveExportLength = GetEnvironmentVariableW(L"BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT", pcForbidExpensiveExport, static_cast<DWORD>(std::size(pcForbidExpensiveExport)));
	mbForbidExpensiveExport = uiForbidExpensiveExportLength == 1 && pcForbidExpensiveExport[0] == L'1';
	wchar_t pcForbidGaeaExport[2] {};
	DWORD uiForbidGaeaExportLength = GetEnvironmentVariableW(L"BT_DATAPACKER_FORBID_GAEA_EXPORT", pcForbidGaeaExport, static_cast<DWORD>(std::size(pcForbidGaeaExport)));
	mbForbidGaeaExport = uiForbidGaeaExportLength == 1 && pcForbidGaeaExport[0] == L'1';

	if (argvSpan.size() == 1)
	{
		const std::filesystem::path repositoryRoot = GetRepositoryRootFromExecutable();
		mpInputDirectories[0] = repositoryRoot / "Engine" / "Data";
		mpInputDirectories[1] = repositoryRoot / "Projects" / "BrokenEngineSandbox" / "Data";
		mOutputDirectory = repositoryRoot / "Projects" / "BrokenEngineSandbox" / "Platforms" / "VisualStudio2026" / "Output" / "Data";
	}
	else
	{
		ASSERT(argvSpan.size() == 4);
		mpInputDirectories[0] = argvSpan[1];
		mpInputDirectories[1] = argvSpan[2];
		mOutputDirectory = argvSpan[3];
	}

	mpInputDirectories[0] = std::filesystem::canonical(mpInputDirectories[0]);
	mpInputDirectories[1] = std::filesystem::canonical(mpInputDirectories[1]);

	// Repo ThirdParty dir, derived from the canonical engine-data dir (<repo>/Engine/Data) rather than a fragile output-relative .. chain
	mThirdPartyDirectory = mpInputDirectories[0].parent_path().parent_path() / "ThirdParty";
	VERIFY_SUCCESS(std::filesystem::exists(mThirdPartyDirectory));

	// Extract project name from project data directory
	mProjectName = mpInputDirectories[1].parent_path().filename().string();
	mOutputDirectory = std::filesystem::absolute(mOutputDirectory).lexically_normal();
	InitializeWorktreeOutputs(eMode);
	if (mDataOutput.meState == OutputRootState::kAbsent)
	{
		EstablishOutputDestinationParent(mOutputDirectory);
		std::filesystem::create_directories(mOutputDirectory);
		mDataOutput.meState = OutputRootState::kLocal;
	}
	if (eMode == InitializationMode::kDataOnly)
	{
		LOG(kDefault, kDebug, "Output directory: \"{}\"", mOutputDirectory.string());
		return;
	}

	// Input data directories
	LOG(kDefault, kDebug, "Engine data directory: \"{}\"", mpInputDirectories[0].string());
	LOG(kDefault, kDebug, "Game data directory: \"{}\"", mpInputDirectories[1].string());
	LOG(kDefault, kDebug, "Project name: \"{}\"", mProjectName);

	// Persistent cache directory. Deliberately NOT %TEMP%: Windows Disk Cleanup / Storage Sense reap
	// %LOCALAPPDATA%\Temp by last-access age, which deleted multi-hour Gaea bake payloads while the
	// always-read .meta sidecars survived — leaving a cache that looked warm and was empty.
	PWSTR pWideChar = nullptr;
	HRESULT hresult = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &pWideChar);
	std::filesystem::path localAppData = SUCCEEDED(hresult) && pWideChar != nullptr ? std::filesystem::path(pWideChar) : std::filesystem::path {};
	CoTaskMemFree(pWideChar);
	if (localAppData.empty())
	{
		// No fallback: a relocated cache root is indistinguishable from a cold one and costs a 3+ hour
		// Gaea re-bake, so fail the run rather than silently exporting everything somewhere else.
		throw std::runtime_error(std::format("SHGetKnownFolderPath(FOLDERID_LocalAppData) failed (hresult {})", static_cast<int32_t>(hresult)));
	}
	mCacheDirectory = localAppData / "BrokenEngine" / "DataPackerCache" / mProjectName;
	std::filesystem::create_directories(mCacheDirectory);
	LOG(kDefault, kDebug, "Cache directory: \"{}\"", mCacheDirectory.string());
	mGaeaCacheDirectory = mCacheDirectory / "Gaea";
	std::filesystem::create_directories(mGaeaCacheDirectory);

	mpInputFingerprintCache = std::make_unique<InputFingerprintCache>(mCacheDirectory / "InputFingerprints.cache");

	LOG(kDefault, kDebug, "Output directory: \"{}\"", mOutputDirectory.string());
}

void FileManager::InitializeWorktreeOutputs(InitializationMode eMode)
{
	mDataOutput.mDestination = mOutputDirectory;
	std::array<OutputRootInfo*, 2> roots { &mDataOutput, &mAttributionOutput };
	std::span<OutputRootInfo*> outputRoots(roots.data(), eMode == InitializationMode::kFull ? roots.size() : 1);
	if (eMode == InitializationMode::kFull)
	{
		mAttributionOutput.mDestination = mOutputDirectory.parent_path() / "Attribution";
	}
	for (OutputRootInfo* pRoot : outputRoots)
	{
		std::filesystem::file_status status = std::filesystem::symlink_status(pRoot->mDestination);
		pRoot->meState = status.type() == std::filesystem::file_type::not_found ? OutputRootState::kAbsent : OutputRootState::kLocal;
		DWORD uiAttributes = GetFileAttributesW(pRoot->mDestination.native().c_str());
		if (uiAttributes != INVALID_FILE_ATTRIBUTES && (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			pRoot->meState = OutputRootState::kUnvalidatedReparse;
		}
	}
	auto RejectUnvalidatedReparse = [outputRoots]()
	{
		for (const OutputRootInfo* pRoot : outputRoots)
		{
			if (pRoot->meState == OutputRootState::kUnvalidatedReparse)
			{
				throw std::runtime_error(std::format("Cannot validate output reparse point: {}", pRoot->mDestination.string()));
			}
		}
	};

	std::filesystem::path repositoryRoot = mpInputDirectories[0].parent_path().parent_path();
	std::optional<LinkedWorktreeIdentity> identity = DiscoverLinkedWorktreeIdentity(repositoryRoot, mProjectName, mOutputDirectory, RejectUnvalidatedReparse);
	if (!identity)
	{
		return;
	}
	diagnostic::MarkValidatedLinkedWorktree();
	std::filesystem::path primaryThirdPartyDirectory = identity->primaryRoot / "ThirdParty";
	if (!IsOrdinaryDirectory(primaryThirdPartyDirectory))
	{
		throw std::runtime_error(std::format("Primary ThirdParty source must be an ordinary non-reparse directory: {}", primaryThirdPartyDirectory.string()));
	}
	mThirdPartyDirectory = std::move(primaryThirdPartyDirectory);
	mDataOutput.mSource = identity->primaryRoot / identity->expectedOutput.lexically_relative(repositoryRoot);
	if (eMode == InitializationMode::kFull)
	{
		const std::filesystem::path expectedAttribution = identity->expectedOutput.parent_path() / "Attribution";
		mAttributionOutput.mSource = identity->primaryRoot / expectedAttribution.lexically_relative(repositoryRoot);
	}
	for (OutputRootInfo* pRoot : outputRoots)
	{
		ReconcileWorktreeOutput(*pRoot);
	}
}

void FileManager::ReconcileWorktreeOutput(OutputRootInfo& rRoot)
{
	EstablishOutputDestinationParent(rRoot.mDestination);
	if (rRoot.meState == OutputRootState::kAbsent && IsReparsePoint(rRoot.mSource))
	{
		throw std::runtime_error(std::format("Primary output source is a reparse point: {}", rRoot.mSource.string()));
	}
	if (rRoot.meState == OutputRootState::kUnvalidatedReparse)
	{
		if (!IsRecognizedLinkRaw(rRoot.mDestination, rRoot.mSource))
		{
			throw std::runtime_error(std::format("Unexpected output reparse point: {}", rRoot.mDestination.string()));
		}
		rRoot.meState = OutputRootState::kRecognizedPrimaryLink;
	}
	if (rRoot.meState == OutputRootState::kAbsent && IsOrdinaryDirectory(rRoot.mSource))
	{
		if (CreateSymbolicLinkW(rRoot.mDestination.native().c_str(), rRoot.mSource.native().c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
		{
			rRoot.meState = OutputRootState::kRecognizedPrimaryLink;
			LOG(kDefault, kDebug, "Linked worktree output \"{}\" to \"{}\"", rRoot.mDestination.string(), rRoot.mSource.string());
		}
		else
		{
			DWORD uiError = GetLastError();
			if (uiError != ERROR_PRIVILEGE_NOT_HELD && uiError != ERROR_INVALID_PARAMETER && uiError != ERROR_NOT_SUPPORTED)
			{
				throw std::runtime_error(std::format("CreateSymbolicLinkW failed for destination {} from source {} (Win32 {})", rRoot.mDestination.string(), rRoot.mSource.string(), uiError));
			}
			if (MaterializeOutput(rRoot) == EnsureLocalResult::kCancelled)
			{
				throw diagnostic::AlreadyReportedError("Output materialization cancelled");
			}
		}
	}
}

FileManager::OutputRootInfo& FileManager::GetOutputRoot(OutputRoot eRoot)
{
	return eRoot == OutputRoot::kData ? mDataOutput : mAttributionOutput;
}

std::filesystem::path FileManager::GetAttributionDirectory() const
{
	return mAttributionOutput.mDestination;
}

FileManager::EnsureLocalResult FileManager::EnsureLocal(OutputRoot eRoot)
{
	OutputRootInfo& rRoot = GetOutputRoot(eRoot);
	if (rRoot.meState == OutputRootState::kLocal)
	{
		return EnsureLocalResult::kAlreadyLocal;
	}
	return MaterializeOutput(rRoot);
}

FileManager::EnsureLocalResult FileManager::MaterializeOutput(OutputRootInfo& rRoot)
{
	EstablishOutputDestinationParent(rRoot.mDestination);
	if (rRoot.meState == OutputRootState::kLocal)
	{
		return EnsureLocalResult::kAlreadyLocal;
	}
	if (!rRoot.mSource.empty() && IsReparsePoint(rRoot.mSource))
	{
		throw std::runtime_error(std::format("Primary output source is a reparse point: {}", rRoot.mSource.string()));
	}
	if (rRoot.mSource.empty() || !IsOrdinaryDirectory(rRoot.mSource))
	{
		std::filesystem::create_directories(rRoot.mDestination);
		rRoot.meState = OutputRootState::kLocal;
		return EnsureLocalResult::kMaterialized;
	}
	MaterializationInventory inventory = BuildMaterializationInventory(rRoot.mSource, rRoot.mDestination);
	ULARGE_INTEGER available {};
	if (!GetDiskFreeSpaceExW(rRoot.mDestination.root_path().native().c_str(), &available, nullptr, nullptr))
	{
		const DWORD uiError = GetLastError();
		diagnostic::Record record
		{
			.eSeverity = diagnostic::Severity::kError,
			.title = "Data Packer - std::exception",
			.message = std::format("GetDiskFreeSpaceExW failed for \"{}\" (Win32 {})", rRoot.mDestination.string(), uiError),
			.eButtons = diagnostic::ButtonContract::kOk,
			.eIcon = diagnostic::ModalIcon::kNone,
		};
		diagnostic::Report(record);
		throw diagnostic::AlreadyReportedError(record.message);
	}
	diagnostic::DiskSpaceDecision eDiskSpaceDecision = diagnostic::ReportMaterializationDiskSpace(inventory.uiAllocation, available.QuadPart, rRoot.mSource, rRoot.mDestination);
	if (eDiskSpaceDecision == diagnostic::DiskSpaceDecision::kFailed)
	{
		throw diagnostic::AlreadyReportedError("Insufficient disk space to materialize worktree output");
	}
	if (eDiskSpaceDecision == diagnostic::DiskSpaceDecision::kCancelled)
	{
		return EnsureLocalResult::kCancelled;
	}
	std::filesystem::path staging = AcquireMaterializationStaging(rRoot.mDestination);
	PublishMaterializedOutput(rRoot.mSource, rRoot.mDestination, inventory.files, inventory.order, staging, rRoot.meState == OutputRootState::kRecognizedPrimaryLink);
	rRoot.meState = OutputRootState::kLocal;
	LOG(kDefault, kDebug, "Materialized worktree output \"{}\" from \"{}\" ({} bytes)", rRoot.mDestination.string(), rRoot.mSource.string(), inventory.uiAllocation);
	return EnsureLocalResult::kMaterialized;
}

std::string FileManager::GetFingerprint(const std::filesystem::path& rPath, InputFingerprintMode eMode)
{
	return mpInputFingerprintCache->Get(rPath, eMode);
}

std::string FileManager::GetSharedCacheFingerprint(const std::filesystem::path& rPath)
{
	return mpInputFingerprintCache->GetPersistent(rPath);
}

FileManager::~FileManager()
{
	if (gpFileManager == this)
	{
		gpFileManager = nullptr;
	}
}
