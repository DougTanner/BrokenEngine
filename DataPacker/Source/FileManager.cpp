#include "FileManager.h"

#include "DiagnosticReporter.h"


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

static std::filesystem::path PathFromUtf8(std::string_view utf8Value)
{
	std::u8string value(utf8Value.begin(), utf8Value.end());
	return std::filesystem::path(value);
}

static std::optional<std::filesystem::path> FindExecutableOnPath(const wchar_t* pcExecutable)
{
	int64_t iCharacters = SearchPathW(nullptr, pcExecutable, nullptr, 0, nullptr, nullptr);
	if (iCharacters == 0)
	{
		return std::nullopt;
	}
	std::wstring path(static_cast<size_t>(iCharacters), L'\0');
	int64_t iWritten = SearchPathW(nullptr, pcExecutable, nullptr, static_cast<DWORD>(path.size()), path.data(), nullptr);
	if (iWritten == 0 || iWritten >= std::ssize(path))
	{
		return std::nullopt;
	}
	path.resize(static_cast<size_t>(iWritten));
	return std::filesystem::path(std::move(path));
}

static std::string TrimLine(std::string value)
{
	while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == '\0'))
	{
		value.pop_back();
	}
	return value;
}

static std::optional<std::filesystem::path> RunGit(const std::filesystem::path& rGit, const std::filesystem::path& rRoot, const wchar_t* pcArguments)
{
	std::wstring parameters = L" -C \"" + rRoot.native() + L"\" " + pcArguments;
	std::optional<common::ExecutableResult> result = common::RunExecutable(rGit, parameters);
	if (!result)
	{
		return std::nullopt;
	}

	if (result->iExitCode != 0)
	{
		return std::nullopt;
	}
	std::string output = TrimLine(std::move(result->output));
	if (output.empty())
	{
		return std::nullopt;
	}
	return std::filesystem::absolute(PathFromUtf8(output)).lexically_normal();
}

static bool PathEqual(const std::filesystem::path& rLeft, const std::filesystem::path& rRight)
{
	return CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, TRUE) == CSTR_EQUAL;
}

static bool PathLess(const std::filesystem::path& rLeft, const std::filesystem::path& rRight)
{
	int64_t iResult = CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, TRUE);
	return iResult == CSTR_LESS_THAN || (iResult == CSTR_EQUAL && CompareStringOrdinal(rLeft.native().c_str(), -1, rRight.native().c_str(), -1, FALSE) == CSTR_LESS_THAN);
}

static bool IsOrdinaryDirectory(const std::filesystem::path& rPath)
{
	DWORD uiAttributes = GetFileAttributesW(rPath.native().c_str());
	return uiAttributes != INVALID_FILE_ATTRIBUTES && (uiAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 && (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

static bool IsReparsePoint(const std::filesystem::path& rPath)
{
	DWORD uiAttributes = GetFileAttributesW(rPath.native().c_str());
	return uiAttributes != INVALID_FILE_ATTRIBUTES && (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

static std::filesystem::path GetRepositoryRootFromExecutable()
{
	std::wstring executableBuffer(32'768, L'\0');
	int64_t iLength = GetModuleFileNameW(nullptr, executableBuffer.data(), static_cast<DWORD>(executableBuffer.size()));
	if (iLength == 0 || iLength >= std::ssize(executableBuffer))
	{
		throw std::runtime_error(std::format("Unable to resolve DataPacker executable path (GetModuleFileNameW returned {}, Win32 {})", iLength, GetLastError()));
	}

	std::filesystem::path executablePath(std::wstring(executableBuffer.data(), static_cast<size_t>(iLength)));
	std::wstring executableName = executablePath.filename().native();
	if (CompareStringOrdinal(executableName.c_str(), -1, L"DataPacker.exe", -1, TRUE) != CSTR_EQUAL
	 && CompareStringOrdinal(executableName.c_str(), -1, L"DataPacker.Debug.exe", -1, TRUE) != CSTR_EQUAL)
	{
		throw std::runtime_error(std::format(R"(DataPacker executable path has unexpected layout: {} (expected suffix DataPacker\Platforms\VisualStudio2026\Output\DataPacker.exe or DataPacker.Debug.exe))", executablePath.string()));
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
		if (CompareStringOrdinal(repositoryRoot.filename().native().c_str(), -1, pwcExpected, -1, TRUE) != CSTR_EQUAL)
		{
			throw std::runtime_error(std::format(R"(DataPacker executable path has unexpected layout: {} (expected suffix DataPacker\Platforms\VisualStudio2026\Output\DataPacker.exe or DataPacker.Debug.exe))", executablePath.string()));
		}
		repositoryRoot = repositoryRoot.parent_path();
	}
	return repositoryRoot;
}

static void EstablishOutputDestinationParent(const std::filesystem::path& rDestination)
{
	const std::filesystem::path& rParent = rDestination.parent_path();
	std::filesystem::create_directories(rParent);
	if (!IsOrdinaryDirectory(rParent))
	{
		throw std::runtime_error(std::format("Output destination parent must be an ordinary non-reparse directory: {} (destination {})", rParent.string(), rDestination.string()));
	}
}

static bool IsRecognizedLinkRaw(const std::filesystem::path& rLink, const std::filesystem::path& rExpected)
{
	using ScopedHandle = std::unique_ptr<void, decltype([](HANDLE hHandle) noexcept
	{
		CloseHandle(hHandle);
	})>;
	ScopedHandle link(CreateFileW(rLink.native().c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr));
	if (link.get() == nullptr || link.get() == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	std::vector<uint8_t> bytes(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
	DWORD uiReturned = 0;
	if (!DeviceIoControl(link.get(), FSCTL_GET_REPARSE_POINT, nullptr, 0, bytes.data(), static_cast<DWORD>(std::ssize(bytes)), &uiReturned, nullptr))
	{
		return false;
	}
	const SymbolicLinkReparseDataBuffer* pData = reinterpret_cast<const SymbolicLinkReparseDataBuffer*>(bytes.data());
	if (pData->uiReparseTag != IO_REPARSE_TAG_SYMLINK)
	{
		return false;
	}
	std::wstring target(pData->cPathBuffer + pData->uiSubstituteNameOffset / sizeof(wchar_t), pData->uiSubstituteNameLength / sizeof(wchar_t));
	if (target.rfind(LR"(\??\)", 0) == 0)
	{
		target.erase(0, 4);
	}
	std::filesystem::path targetPath(std::move(target));
	if (targetPath.is_relative())
	{
		targetPath = rLink.parent_path() / targetPath;
	}
	targetPath = std::filesystem::absolute(targetPath).lexically_normal().make_preferred();
	std::filesystem::path expectedPath = std::filesystem::absolute(rExpected).lexically_normal().make_preferred();
	return PathEqual(targetPath, expectedPath);
}

static uint64_t AddChecked(uint64_t uiLeft, uint64_t uiRight)
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

template <typename TREJECT>
static std::optional<LinkedWorktreeIdentity> DiscoverLinkedWorktreeIdentity(const std::filesystem::path& rRepositoryRoot, std::string_view projectName, const std::filesystem::path& rOutputDirectory, const TREJECT& rReject)
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
	std::optional<common::ExecutableResult> result = common::RunExecutable(*git, parameters);
	if (!result)
	{
		rReject();
		LOG(kDefault, kWarning, "Git worktree discovery failed; output linking disabled");
		return std::nullopt;
	}
	if (result->iExitCode != 0 || result->output.rfind("worktree ", 0) != 0)
	{
		rReject();
		LOG(kDefault, kWarning, "Malformed Git worktree metadata; output linking disabled");
		return std::nullopt;
	}
	int64_t iEnd = static_cast<int64_t>(result->output.find('\0'));
	std::filesystem::path primaryRoot = PathFromUtf8(result->output.substr(9, static_cast<size_t>(iEnd - 9)));
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
		// The canonical output root anchors the ThirdParty and attribution paths validated below.
		throw std::runtime_error(std::format("Linked worktree must export into its canonical output directory: supplied {}, expected {}", rOutputDirectory.string(), expected.string()));
	}
	return LinkedWorktreeIdentity { .primaryRoot = std::move(primaryRoot), .expectedOutput = std::move(expected), };
}

struct MaterializationInventory
{
	std::vector<std::filesystem::path> files;
	std::vector<int64_t> order;
	uint64_t uiAllocation = 0;
};

static MaterializationInventory BuildMaterializationInventory(const std::filesystem::path& rSource, const std::filesystem::path& rDestination)
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
	inventory.order.resize(static_cast<size_t>(std::ssize(inventory.files)));
	std::ranges::iota(inventory.order, 0);
	std::sort(inventory.order.begin(), inventory.order.end(), [&inventory, &rSource](int64_t iLeftIndex, int64_t iRightIndex)
	{
		return PathLess(std::filesystem::relative(inventory.files.at(iLeftIndex), rSource), std::filesystem::relative(inventory.files.at(iRightIndex), rSource));
	});
	return inventory;
}

static std::filesystem::path AcquireMaterializationStaging(const std::filesystem::path& rDestination)
{
	for (int64_t i = 0; i <= 15; ++i)
	{
		std::filesystem::path staging = rDestination;
		staging += std::format(".materializing.{}.{}", GetCurrentProcessId(), i);
		std::error_code errorCode;
		if (std::filesystem::create_directory(staging, errorCode))
		{
			return staging;
		}
	}
	throw std::runtime_error("Unable to create unique output staging directory");
}

static void PublishMaterializedOutput(const std::filesystem::path& rSource, const std::filesystem::path& rDestination, const std::vector<std::filesystem::path>& rFiles, const std::vector<int64_t>& rOrder, const std::filesystem::path& rStaging, bool bRecognizedPrimaryLink)
{
	try
	{
		for (int64_t iIndex : rOrder)
		{
			const std::filesystem::path& rSourceFile = rFiles.at(iIndex);
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

void WriteEntireFile(const std::filesystem::path& rPath, std::string_view contents)
{
	std::ofstream stream(rPath, std::ios::binary | std::ios::trunc);
	stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
	stream.close();
	VERIFY_SUCCESS(stream.good());
}


FileManager::FileManager(std::span<char*> argvSpan, EnsureLocalResult& reInitializationResult, InitializationMode eMode)
: common::Singleton<FileManager>(gpFileManager)
{
	reInitializationResult = EnsureLocalResult::kAlreadyLocal;
	wchar_t pcForbidExpensiveExport[2] {};
	int64_t iForbidExpensiveExportLength = GetEnvironmentVariableW(L"BT_DATAPACKER_FORBID_EXPENSIVE_EXPORT", pcForbidExpensiveExport, static_cast<DWORD>(std::size(pcForbidExpensiveExport)));
	mbForbidExpensiveExport = iForbidExpensiveExportLength == 1 && pcForbidExpensiveExport[0] == L'1';
	wchar_t pcForbidGaeaExport[2] {};
	int64_t iForbidGaeaExportLength = GetEnvironmentVariableW(L"BT_DATAPACKER_FORBID_GAEA_EXPORT", pcForbidGaeaExport, static_cast<DWORD>(std::size(pcForbidGaeaExport)));
	mbForbidGaeaExport = iForbidGaeaExportLength == 1 && pcForbidGaeaExport[0] == L'1';

	if (argvSpan.size() == 1)
	{
		std::filesystem::path repositoryRoot = GetRepositoryRootFromExecutable();
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

	// The canonical Engine/Data path identifies the repository root for ThirdParty lookup.
	mThirdPartyDirectory = mpInputDirectories[0].parent_path().parent_path() / "ThirdParty";
	VERIFY_SUCCESS(std::filesystem::exists(mThirdPartyDirectory));

	mProjectName = mpInputDirectories[1].parent_path().filename().string();
	mOutputDirectory = std::filesystem::absolute(mOutputDirectory).lexically_normal();
	reInitializationResult = InitializeWorktreeOutputs(eMode);
	if (reInitializationResult == EnsureLocalResult::kCancelled || reInitializationResult == EnsureLocalResult::kFailed)
	{
		return;
	}
	if (mDataOutput.eState == OutputRootState::kAbsent)
	{
		EstablishOutputDestinationParent(mOutputDirectory);
		std::filesystem::create_directories(mOutputDirectory);
		mDataOutput.eState = OutputRootState::kLocal;
	}
	if (eMode == InitializationMode::kDataOnly)
	{
		LOG(kDefault, kDebug, "Output directory: \"{}\"", mOutputDirectory.string());
		return;
	}

	LOG(kDefault, kDebug, "Engine data directory: \"{}\"", mpInputDirectories[0].string());
	LOG(kDefault, kDebug, "Game data directory: \"{}\"", mpInputDirectories[1].string());
	LOG(kDefault, kDebug, "Project name: \"{}\"", mProjectName);

	// Persistent cache directory. Deliberately NOT %TEMP%: Windows Disk Cleanup / Storage Sense can reap
	// %LOCALAPPDATA%\Temp files by last-access age, which would delete long-lived Gaea bake payloads even
	// while their always-read .meta sidecars stay fresh, producing a cache that looks warm but is empty.
	PWSTR pWideChar = nullptr;
	int64_t iHresult = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &pWideChar);
	std::filesystem::path localAppData = SUCCEEDED(static_cast<HRESULT>(iHresult)) && pWideChar != nullptr ? std::filesystem::path(pWideChar) : std::filesystem::path {};
	CoTaskMemFree(pWideChar);
	if (localAppData.empty())
	{
		// No fallback: a relocated cache root is indistinguishable from a cold one and costs a 3+ hour
		// Gaea re-bake, so fail the run rather than silently exporting everything somewhere else.
		throw std::runtime_error(std::format("SHGetKnownFolderPath(FOLDERID_LocalAppData) failed (hresult {})", iHresult));
	}
	mCacheDirectory = localAppData / "BrokenEngine" / "DataPackerCache" / mProjectName;
	std::filesystem::create_directories(mCacheDirectory);
	LOG(kDefault, kDebug, "Cache directory: \"{}\"", mCacheDirectory.string());
	mGaeaCacheDirectory = mCacheDirectory / "Gaea";
	std::filesystem::create_directories(mGaeaCacheDirectory);

	mpInputFingerprintCache = std::make_unique<InputFingerprintCache>(mCacheDirectory / "InputFingerprints.cache");

	LOG(kDefault, kDebug, "Output directory: \"{}\"", mOutputDirectory.string());
}

FileManager::EnsureLocalResult FileManager::InitializeWorktreeOutputs(InitializationMode eMode)
{
	mDataOutput.destination = mOutputDirectory;
	std::array<OutputRootInfo*, 2> roots { &mDataOutput, &mAttributionOutput };
	std::span<OutputRootInfo*> outputRoots(roots.data(), eMode == InitializationMode::kFull ? roots.size() : 1);
	if (eMode == InitializationMode::kFull)
	{
		mAttributionOutput.destination = mOutputDirectory.parent_path() / "Attribution";
	}
	for (OutputRootInfo* pRoot : outputRoots)
	{
		std::filesystem::file_status status = std::filesystem::symlink_status(pRoot->destination);
		pRoot->eState = status.type() == std::filesystem::file_type::not_found ? OutputRootState::kAbsent : OutputRootState::kLocal;
		DWORD uiAttributes = GetFileAttributesW(pRoot->destination.native().c_str());
		if (uiAttributes != INVALID_FILE_ATTRIBUTES && (uiAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		{
			pRoot->eState = OutputRootState::kUnvalidatedReparse;
		}
	}
	auto RejectUnvalidatedReparse = [outputRoots]()
	{
		for (const OutputRootInfo* pRoot : outputRoots)
		{
			if (pRoot->eState == OutputRootState::kUnvalidatedReparse)
			{
				throw std::runtime_error(std::format("Cannot validate output reparse point: {}", pRoot->destination.string()));
			}
		}
	};

	std::filesystem::path repositoryRoot = mpInputDirectories[0].parent_path().parent_path();
	std::optional<LinkedWorktreeIdentity> identity = DiscoverLinkedWorktreeIdentity(repositoryRoot, mProjectName, mOutputDirectory, RejectUnvalidatedReparse);
	if (!identity)
	{
		return EnsureLocalResult::kAlreadyLocal;
	}
	diagnostic::MarkValidatedLinkedWorktree();
	std::filesystem::path primaryThirdPartyDirectory = identity->primaryRoot / "ThirdParty";
	if (!IsOrdinaryDirectory(primaryThirdPartyDirectory))
	{
		throw std::runtime_error(std::format("Primary ThirdParty source must be an ordinary non-reparse directory: {}", primaryThirdPartyDirectory.string()));
	}
	mThirdPartyDirectory = std::move(primaryThirdPartyDirectory);
	mDataOutput.source = identity->primaryRoot / identity->expectedOutput.lexically_relative(repositoryRoot);
	if (eMode == InitializationMode::kFull)
	{
		std::filesystem::path expectedAttribution = identity->expectedOutput.parent_path() / "Attribution";
		mAttributionOutput.source = identity->primaryRoot / expectedAttribution.lexically_relative(repositoryRoot);
	}
	EnsureLocalResult eInitializationResult = EnsureLocalResult::kAlreadyLocal;
	for (OutputRootInfo* pRoot : outputRoots)
	{
		EnsureLocalResult eResult = ReconcileWorktreeOutput(*pRoot);
		if (eResult == EnsureLocalResult::kCancelled || eResult == EnsureLocalResult::kFailed)
		{
			return eResult;
		}
		if (eResult == EnsureLocalResult::kMaterialized)
		{
			eInitializationResult = eResult;
		}
	}
	return eInitializationResult;
}

FileManager::EnsureLocalResult FileManager::ReconcileWorktreeOutput(OutputRootInfo& rRoot)
{
	EstablishOutputDestinationParent(rRoot.destination);
	if (rRoot.eState == OutputRootState::kAbsent && IsReparsePoint(rRoot.source))
	{
		throw std::runtime_error(std::format("Primary output source is a reparse point: {}", rRoot.source.string()));
	}
	if (rRoot.eState == OutputRootState::kUnvalidatedReparse)
	{
		if (!IsRecognizedLinkRaw(rRoot.destination, rRoot.source))
		{
			throw std::runtime_error(std::format("Unexpected output reparse point: {}", rRoot.destination.string()));
		}
		rRoot.eState = OutputRootState::kRecognizedPrimaryLink;
	}
	if (rRoot.eState == OutputRootState::kAbsent && IsOrdinaryDirectory(rRoot.source))
	{
		if (CreateSymbolicLinkW(rRoot.destination.native().c_str(), rRoot.source.native().c_str(), SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
		{
			rRoot.eState = OutputRootState::kRecognizedPrimaryLink;
			LOG(kDefault, kDebug, "Linked worktree output \"{}\" to \"{}\"", rRoot.destination.string(), rRoot.source.string());
		}
		else
		{
			int64_t iError = GetLastError();
			if (iError != ERROR_PRIVILEGE_NOT_HELD && iError != ERROR_INVALID_PARAMETER && iError != ERROR_NOT_SUPPORTED)
			{
				throw std::runtime_error(std::format("CreateSymbolicLinkW failed for destination {} from source {} (Win32 {})", rRoot.destination.string(), rRoot.source.string(), iError));
			}
			return MaterializeOutput(rRoot);
		}
	}
	return EnsureLocalResult::kAlreadyLocal;
}

FileManager::EnsureLocalResult FileManager::EnsureLocal(OutputRoot eRoot)
{
	OutputRootInfo& rRoot = eRoot == OutputRoot::kData ? mDataOutput : mAttributionOutput;
	if (rRoot.eState == OutputRootState::kLocal)
	{
		return EnsureLocalResult::kAlreadyLocal;
	}
	return MaterializeOutput(rRoot);
}

FileManager::EnsureLocalResult FileManager::MaterializeOutput(OutputRootInfo& rRoot)
{
	EstablishOutputDestinationParent(rRoot.destination);
	if (!rRoot.source.empty() && IsReparsePoint(rRoot.source))
	{
		throw std::runtime_error(std::format("Primary output source is a reparse point: {}", rRoot.source.string()));
	}
	if (rRoot.source.empty() || !IsOrdinaryDirectory(rRoot.source))
	{
		std::filesystem::create_directories(rRoot.destination);
		rRoot.eState = OutputRootState::kLocal;
		return EnsureLocalResult::kMaterialized;
	}
	MaterializationInventory inventory = BuildMaterializationInventory(rRoot.source, rRoot.destination);
	ULARGE_INTEGER available {};
	if (!GetDiskFreeSpaceExW(rRoot.destination.root_path().native().c_str(), &available, nullptr, nullptr))
	{
		int64_t iError = GetLastError();
		diagnostic::Record record
		{
			.eSeverity = diagnostic::Severity::kError,
			.title = "Data Packer - std::exception",
			.message = std::format("GetDiskFreeSpaceExW failed for \"{}\" (Win32 {})", rRoot.destination.string(), iError),
			.eButtons = diagnostic::ButtonContract::kOk,
			.eIcon = diagnostic::ModalIcon::kNone,
		};
		diagnostic::Report(record);
		return EnsureLocalResult::kFailed;
	}
	diagnostic::DiskSpaceDecision eDiskSpaceDecision = diagnostic::ReportMaterializationDiskSpace(inventory.uiAllocation, available.QuadPart, rRoot.source, rRoot.destination);
	if (eDiskSpaceDecision == diagnostic::DiskSpaceDecision::kFailed)
	{
		return EnsureLocalResult::kFailed;
	}
	if (eDiskSpaceDecision == diagnostic::DiskSpaceDecision::kCancelled)
	{
		return EnsureLocalResult::kCancelled;
	}
	std::filesystem::path staging = AcquireMaterializationStaging(rRoot.destination);
	PublishMaterializedOutput(rRoot.source, rRoot.destination, inventory.files, inventory.order, staging, rRoot.eState == OutputRootState::kRecognizedPrimaryLink);
	rRoot.eState = OutputRootState::kLocal;
	LOG(kDefault, kDebug, "Materialized worktree output \"{}\" from \"{}\" ({} bytes)", rRoot.destination.string(), rRoot.source.string(), inventory.uiAllocation);
	return EnsureLocalResult::kMaterialized;
}
