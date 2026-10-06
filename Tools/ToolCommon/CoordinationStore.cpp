#include "CoordinationStore.h"

#include "ToolCliCommon.h"

#include <algorithm>
#include <bcrypt.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace toolcli::coordination
{
	static std::string FailureReasonFor(int64_t iError)
	{
		return iError == ERROR_SHARING_VIOLATION || iError == ERROR_LOCK_VIOLATION ? "timed out" : "Windows error " + std::to_string(iError);
	}

	Guard::Guard(const std::filesystem::path& rPath, bool& rbContentionObserved, std::string& rFailureReason, int64_t iMaximumWaitMilliseconds, int64_t iMaximumDeniedAccessMilliseconds) :
		mPath(ExtendedLengthPath(rPath))
	{
		rbContentionObserved = false;
		rFailureReason = FailureReasonFor(ERROR_SUCCESS);
		std::chrono::milliseconds maximumWait = std::chrono::milliseconds((std::max)(0i64, iMaximumWaitMilliseconds));
		if (maximumWait == std::chrono::milliseconds::zero())
		{
			return;
		}
		std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now() + maximumWait;
		std::optional<std::chrono::steady_clock::time_point> deniedRunStart;
		do
		{
			mhFile.Reset(::CreateFileW(mPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr));
			if (mhFile.IsValid())
			{
				mbValid = true;
				return;
			}
			int64_t iLastError = ::GetLastError();
			rFailureReason = FailureReasonFor(iLastError);
			// ERROR_ACCESS_DENIED covers the delete-pending window while a releasing holder unlinks the guard file.
			if (iLastError != ERROR_SHARING_VIOLATION && iLastError != ERROR_LOCK_VIOLATION && iLastError != ERROR_ACCESS_DENIED)
			{
				return;
			}
			rbContentionObserved = rbContentionObserved || iLastError == ERROR_SHARING_VIOLATION || iLastError == ERROR_LOCK_VIOLATION;
			std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			// The denied-access budget covers the current consecutive run only: a sharing or lock violation proves a live holder rather than a stuck delete-pending window, so the next denied observation starts a fresh budget.
			if (iLastError == ERROR_ACCESS_DENIED)
			{
				if (!deniedRunStart)
				{
					deniedRunStart = now;
				}
				if (std::chrono::duration_cast<std::chrono::milliseconds>(now - *deniedRunStart).count() >= iMaximumDeniedAccessMilliseconds)
				{
					return;
				}
			}
			else
			{
				deniedRunStart.reset();
			}
			std::chrono::milliseconds remainingWait = (std::max)(std::chrono::milliseconds::zero(), std::chrono::duration_cast<std::chrono::milliseconds>(endTime - now));
			if (remainingWait == std::chrono::milliseconds::zero())
			{
				return;
			}
			std::this_thread::sleep_for((std::min)(std::chrono::milliseconds(25), remainingWait));
		}
		while (std::chrono::steady_clock::now() < endTime);
	}

	Guard::~Guard()
	{
		if (mhFile.IsValid())
		{
			mhFile.Reset();
			// Best effort: a waiter that reopened the guard first keeps it alive and deletes it on its own release.
			::DeleteFileW(mPath.c_str());
		}
	}

	std::string CurrentUtcTimestamp()
	{
		SYSTEMTIME time {};
		::GetSystemTime(&time);
		char pBuffer[32] {};
		std::snprintf(pBuffer, sizeof(pBuffer), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
		return pBuffer;
	}

	bool ParseUtcTimestamp(const std::string& rValue, int64_t& rTicks)
	{
		SYSTEMTIME time {};
		char cSuffix = 0;
		int64_t iRead = std::sscanf(rValue.c_str(), "%hu-%hu-%huT%hu:%hu:%hu.%hu%c", &time.wYear, &time.wMonth, &time.wDay, &time.wHour, &time.wMinute, &time.wSecond, &time.wMilliseconds, &cSuffix);
		FILETIME fileTime {};
		if (iRead != 8)
		{
			return false;
		}
		if (cSuffix != 'Z')
		{
			return false;
		}
		if (rValue.size() != 24)
		{
			return false;
		}
		if (::SystemTimeToFileTime(&time, &fileTime) == FALSE)
		{
			return false;
		}
		rTicks = static_cast<int64_t>((static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime);
		return true;
	}

	uint64_t CurrentUtcTicks()
	{
		FILETIME fileTime {};
		::GetSystemTimeAsFileTime(&fileTime);
		return (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
	}

	std::string FormatUtcTimestamp(uint64_t uiTicks)
	{
		FILETIME fileTime
		{
			.dwLowDateTime = static_cast<DWORD>(uiTicks),
			.dwHighDateTime = static_cast<DWORD>(uiTicks >> 32),
		};
		SYSTEMTIME time {};
		::FileTimeToSystemTime(&fileTime, &time);
		char pBuffer[32] {};
		std::snprintf(pBuffer, sizeof(pBuffer), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
		return pBuffer;
	}

	std::optional<std::string> HashSha256(std::string_view value)
	{
		BCRYPT_ALG_HANDLE hAlgorithm = nullptr;
		if (::BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
		{
			return std::nullopt;
		}
		DWORD uiObjectLength = 0;
		DWORD uiResultLength = 0;
		if (::BCryptGetProperty(hAlgorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&uiObjectLength), sizeof(uiObjectLength), &uiResultLength, 0) < 0)
		{
			::BCryptCloseAlgorithmProvider(hAlgorithm, 0);
			return std::nullopt;
		}
		int64_t iObjectLength = uiObjectLength;
		std::vector<UCHAR> hashObject(static_cast<size_t>(iObjectLength));
		BCRYPT_HASH_HANDLE hHash = nullptr;
		if (::BCryptCreateHash(hAlgorithm, &hHash, hashObject.data(), static_cast<ULONG>(iObjectLength), nullptr, 0, 0) < 0
		 || ::BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())), static_cast<ULONG>(value.size()), 0) < 0)
		{
			if (hHash != nullptr)
			{
				::BCryptDestroyHash(hHash);
			}
			::BCryptCloseAlgorithmProvider(hAlgorithm, 0);
			return std::nullopt;
		}
		UCHAR pDigest[32] {};
		bool bSucceeded = ::BCryptFinishHash(hHash, pDigest, static_cast<ULONG>(sizeof(pDigest)), 0) >= 0;
		::BCryptDestroyHash(hHash);
		::BCryptCloseAlgorithmProvider(hAlgorithm, 0);
		if (!bSucceeded)
		{
			return std::nullopt;
		}
		static constexpr char pHex[] = "0123456789abcdef";
		std::string result;
		result.reserve(sizeof(pDigest) * 2);
		for (UCHAR uiByte : pDigest)
		{
			result.push_back(pHex[uiByte >> 4]);
			result.push_back(pHex[uiByte & 0x0f]);
		}
		return result;
	}

	std::optional<std::wstring> CanonicalizeDirectoryPath(std::wstring_view value)
	{
		std::error_code error;
		std::filesystem::path path = std::filesystem::canonical(ExtendedLengthPath(value), error);
		// canonical() strips the extended-length prefix from its result, so re-apply it for the remaining OS calls.
		std::filesystem::path osPath = ExtendedLengthPath(path);
		if (error)
		{
			return std::nullopt;
		}
		if (!std::filesystem::is_directory(osPath, error))
		{
			return std::nullopt;
		}
		Handle hDirectory(::CreateFileW(osPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
		if (!hDirectory.IsValid())
		{
			return std::nullopt;
		}
		std::wstring finalPath(32'768, L'\0');
		int64_t iWritten = ::GetFinalPathNameByHandleW(hDirectory.Get(), finalPath.data(), static_cast<DWORD>(finalPath.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		if (iWritten == 0 || std::cmp_greater_equal(iWritten, finalPath.size()))
		{
			return std::nullopt;
		}
		finalPath.resize(static_cast<size_t>(iWritten));
		// A network share normalizes to \\?\UNC\server\share\..., which the prefix strip below would turn into a
		// mangled relative key that is then persisted, hashed, and handed to Git.  No logical key form covers a UNC
		// checkout, so canonicalization fails here where the reason is still visible.
		if (finalPath.starts_with(LR"(\\?\UNC\)"))
		{
			Fail("network-share (UNC) checkouts are unsupported by the coordination tooling: " + WideToUtf8(finalPath));
			return std::nullopt;
		}
		if (finalPath.starts_with(LR"(\\?\)"))
		{
			finalPath.erase(0, 4);
		}
		std::replace(finalPath.begin(), finalPath.end(), L'/', L'\\');
		while (finalPath.size() > 3 && finalPath.back() == L'\\')
		{
			finalPath.pop_back();
		}
		finalPath = ToLowerInvariant(std::move(finalPath));
		return finalPath.empty() ? std::nullopt : std::optional<std::wstring>(std::move(finalPath));
	}

	std::optional<std::wstring> NormalizeRelativeKey(std::wstring_view value)
	{
		std::filesystem::path path(value);
		if (value.empty() || path.is_absolute())
		{
			return std::nullopt;
		}
		std::filesystem::path normalized = path.lexically_normal();
		if (normalized.empty() || normalized == L".")
		{
			return std::nullopt;
		}
		for (const std::filesystem::path& rPart : normalized)
		{
			if (rPart == L"..")
			{
				return std::nullopt;
			}
		}
		std::wstring result = normalized.native();
		std::replace(result.begin(), result.end(), L'/', L'\\');
		result = ToLowerInvariant(std::move(result));
		return result.empty() ? std::nullopt : std::optional<std::wstring>(std::move(result));
	}

	std::optional<std::wstring> NormalizeRepositoryRelativeKey(std::wstring_view value)
	{
		std::filesystem::path path(value);
		if (value.empty() || path.has_root_name() || path.has_root_directory())
		{
			return std::nullopt;
		}
		for (const std::filesystem::path& rPart : path)
		{
			if (rPart == L"..")
			{
				return std::nullopt;
			}
		}
		return NormalizeRelativeKey(value);
	}

	std::optional<Locator> MakeLocator(std::wstring_view domain, std::wstring_view logicalKey)
	{
		std::optional<std::string> hash = HashSha256(WideToUtf8(logicalKey));
		std::filesystem::path localApplicationData = GetLocalApplicationDataPath();
		if (!hash)
		{
			return std::nullopt;
		}
		if (localApplicationData.empty())
		{
			return std::nullopt;
		}
		return Locator
		{
			.domain = std::wstring(domain),
			.logicalKey = std::wstring(logicalKey),
			.path = localApplicationData / L"BrokenEngineLocks" / domain / Utf8ToWide(*hash + ".lock"),
		};
	}

	bool EnsureParentDirectory(const std::filesystem::path& rPath)
	{
		std::error_code error;
		std::filesystem::create_directories(ExtendedLengthPath(rPath).parent_path(), error);
		return !error;
	}

	bool ReadMetadata(const std::filesystem::path& rPath, nlohmann::json& rMetadata)
	{
		std::ifstream input(ExtendedLengthPath(rPath), std::ios::binary);
		if (!input)
		{
			return false;
		}
		try
		{
			input >> rMetadata;
			return rMetadata.is_object();
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	bool WriteMetadataAtomic(const std::filesystem::path& rPath, const nlohmann::json& rMetadata)
	{
		return WriteBytesAtomic(rPath, rMetadata.dump(2) + "\n");
	}

	bool StageBytesAtomic(const std::filesystem::path& rPath, std::string_view contents, std::filesystem::path& rStagedPath)
	{
		// The process-wide sequence gives sequential staged writes distinct temporary suffixes until it wraps.
		static int64_t siSequence = 0;
		std::filesystem::path temporaryPath = ExtendedLengthPath(rPath);
		siSequence = (siSequence + 1) % 4'294'967'296;
		temporaryPath += L".tmp." + std::to_wstring(::GetCurrentProcessId()) + L"." + std::to_wstring(siSequence);
		Handle hFile(::CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY, nullptr));
		if (!hFile.IsValid())
		{
			return false;
		}
		DWORD uiWritten = 0;
		bool bSucceeded = ::WriteFile(hFile.Get(), contents.data(), static_cast<DWORD>(contents.size()), &uiWritten, nullptr) != FALSE;
		int64_t iWritten = uiWritten;
		bSucceeded = bSucceeded && std::cmp_equal(iWritten, contents.size()) && ::FlushFileBuffers(hFile.Get()) != FALSE;
		hFile.Reset();
		if (!bSucceeded)
		{
			::DeleteFileW(temporaryPath.c_str());
			return false;
		}
		rStagedPath = std::move(temporaryPath);
		return true;
	}

	bool CommitStagedBytes(const std::filesystem::path& rStagedPath, const std::filesystem::path& rPath)
	{
		if (::MoveFileExW(rStagedPath.c_str(), ExtendedLengthPath(rPath).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE)
		{
			return true;
		}
		::DeleteFileW(rStagedPath.c_str());
		return false;
	}

	bool WriteBytesAtomic(const std::filesystem::path& rPath, std::string_view contents)
	{
		std::filesystem::path stagedPath;
		return StageBytesAtomic(rPath, contents, stagedPath) && CommitStagedBytes(stagedPath, rPath);
	}

	void PrintMetadata(const nlohmann::json& rMetadata)
	{
		std::cout << rMetadata.dump(2) << '\n';
	}

	bool HasOwner(const nlohmann::json& rMetadata, std::wstring_view owner)
	{
		return rMetadata.contains("owner") && rMetadata["owner"].is_string() && rMetadata["owner"].get<std::string>() == WideToUtf8(owner);
	}

	bool JsonIntegerEquals(const nlohmann::json& rValue, int64_t iExpected)
	{
		if (rValue.is_number_unsigned())
		{
			return std::cmp_equal(rValue.get<uint64_t>(), iExpected);
		}
		return rValue.is_number_integer() && rValue.get<int64_t>() == iExpected;
	}

	std::optional<int64_t> JsonInt64(const nlohmann::json& rValue)
	{
		if (rValue.is_number_unsigned())
		{
			uint64_t uiValue = rValue.get<uint64_t>();
			return std::in_range<int64_t>(uiValue) ? std::optional<int64_t>(static_cast<int64_t>(uiValue)) : std::nullopt;
		}
		return rValue.is_number_integer() ? std::optional<int64_t>(rValue.get<int64_t>()) : std::nullopt;
	}

	bool ValidateMetadataEnvelope(const nlohmann::json& rMetadata, const Locator& rLocator, int64_t iExpectedSchemaVersion)
	{
		const char* const pFields[] = { "owner", "session", "worktree", "claimedAt", "heartbeatAt" };
		if (!std::ranges::all_of(pFields, [&rMetadata](const char* const& rpField)
		{
			return rMetadata.contains(rpField) && rMetadata[rpField].is_string() && !rMetadata[rpField].get<std::string>().empty();
		}))
		{
			return false;
		}
		int64_t iClaimedTicks = 0;
		int64_t iHeartbeatTicks = 0;
		std::optional<int64_t> claimantPid = rMetadata.contains("claimantPid") ? JsonInt64(rMetadata["claimantPid"]) : std::nullopt;
		bool bValidMetadata = rMetadata.contains("schemaVersion") && JsonIntegerEquals(rMetadata["schemaVersion"], iExpectedSchemaVersion)
		    && rMetadata.contains("domain") && rMetadata["domain"].is_string()
		    && rMetadata["domain"].get<std::string>() == WideToUtf8(rLocator.domain) && rMetadata.contains("logicalKey")
		    && rMetadata["logicalKey"].is_string() && rMetadata["logicalKey"].get<std::string>() == WideToUtf8(rLocator.logicalKey)
		    && claimantPid && std::in_range<uint32_t>(*claimantPid)
		    && ParseUtcTimestamp(rMetadata["claimedAt"].get<std::string>(), iClaimedTicks)
		    && ParseUtcTimestamp(rMetadata["heartbeatAt"].get<std::string>(), iHeartbeatTicks);
		return bValidMetadata && iClaimedTicks <= iHeartbeatTicks;
	}

	nlohmann::json NewMetadata(const Locator& rLocator, std::wstring_view owner, std::wstring_view session, std::wstring_view worktree)
	{
		std::string timestamp = CurrentUtcTimestamp();
		return
		{
			{ "schemaVersion", kiSchemaVersion },
			{ "domain", WideToUtf8(rLocator.domain) },
			{ "logicalKey", WideToUtf8(rLocator.logicalKey) },
			{ "owner", WideToUtf8(owner) },
			{ "session", WideToUtf8(session) },
			{ "worktree", WideToUtf8(worktree) },
			{ "claimantPid", ::GetCurrentProcessId() },
			{ "claimedAt", timestamp },
			{ "heartbeatAt", std::move(timestamp) },
		};
	}
} // namespace toolcli::coordination
