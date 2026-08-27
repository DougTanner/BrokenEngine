#include "CoordinationStore.h"

#include "ToolCliCommon.h"

#include <bcrypt.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace toolcli::coordination
{
	namespace
	{
		std::string FailureReasonFor(DWORD uiError)
		{
			return uiError == ERROR_SHARING_VIOLATION || uiError == ERROR_LOCK_VIOLATION ? "timed out" : "Windows error " + std::to_string(uiError);
		}
	}

	Guard::Guard(const std::filesystem::path& rPath, bool& rbContentionObserved, std::string& rFailureReason, int64_t iMaximumWaitMilliseconds, int64_t iMaximumDeniedAccessMilliseconds) :
		mPath(ExtendedLengthPath(rPath))
	{
		rbContentionObserved = false;
		rFailureReason = FailureReasonFor(ERROR_SUCCESS);
		const std::chrono::milliseconds maximumWait = std::chrono::milliseconds((std::max)(static_cast<int64_t>(0), iMaximumWaitMilliseconds));
		if (maximumWait == std::chrono::milliseconds::zero())
		{
			return;
		}
		const std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now() + maximumWait;
		std::optional<std::chrono::steady_clock::time_point> deniedRunStart;
		do
		{
			mhFile.Reset(::CreateFileW(mPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr));
			if (mhFile.IsValid())
			{
				return;
			}
			const DWORD uiLastError = ::GetLastError();
			rFailureReason = FailureReasonFor(uiLastError);
			// ERROR_ACCESS_DENIED covers the delete-pending window while a releasing holder unlinks the guard file.
			if (uiLastError != ERROR_SHARING_VIOLATION && uiLastError != ERROR_LOCK_VIOLATION && uiLastError != ERROR_ACCESS_DENIED)
			{
				return;
			}
			rbContentionObserved = rbContentionObserved || uiLastError == ERROR_SHARING_VIOLATION || uiLastError == ERROR_LOCK_VIOLATION;
			const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			// The denied-access budget covers the current consecutive run only: a sharing or lock violation proves a live holder rather than a stuck delete-pending window, so the next denied observation starts a fresh budget.
			if (uiLastError == ERROR_ACCESS_DENIED)
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
			const std::chrono::milliseconds remainingWait = (std::max)(std::chrono::milliseconds::zero(), std::chrono::duration_cast<std::chrono::milliseconds>(endTime - now));
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

	bool Guard::IsValid() const
	{
		return mhFile.IsValid();
	}

	std::string CurrentUtcTimestamp()
	{
		SYSTEMTIME time {};
		::GetSystemTime(&time);
		char pBuffer[32] {};
		std::snprintf(pBuffer, sizeof(pBuffer), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
		return pBuffer;
	}

	bool ParseUtcTimestamp(const std::string& rValue, uint64_t& rTicks)
	{
		SYSTEMTIME time {};
		char cSuffix = 0;
		int iRead = std::sscanf(rValue.c_str(), "%hu-%hu-%huT%hu:%hu:%hu.%hu%c", &time.wYear, &time.wMonth, &time.wDay, &time.wHour, &time.wMinute, &time.wSecond, &time.wMilliseconds, &cSuffix);
		FILETIME fileTime {};
		if (iRead != 8 || cSuffix != 'Z' || rValue.size() != 24 || ::SystemTimeToFileTime(&time, &fileTime) == FALSE)
		{
			return false;
		}
		rTicks = (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
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
		std::vector<UCHAR> hashObject(uiObjectLength);
		BCRYPT_HASH_HANDLE hHash = nullptr;
		if (::BCryptCreateHash(hAlgorithm, &hHash, hashObject.data(), uiObjectLength, nullptr, 0, 0) < 0 ||
			::BCryptHashData(hHash, reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())), static_cast<ULONG>(value.size()), 0) < 0)
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

	std::optional<std::wstring> CanonicalizeDirectoryPath(const std::wstring& rValue)
	{
		std::error_code error;
		std::filesystem::path path = std::filesystem::canonical(ExtendedLengthPath(rValue), error);
		// canonical() strips the extended-length prefix from its result, so re-apply it for the remaining OS calls.
		const std::filesystem::path osPath = ExtendedLengthPath(path);
		if (error || !std::filesystem::is_directory(osPath, error))
		{
			return std::nullopt;
		}
		Handle hDirectory(::CreateFileW(osPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
		if (!hDirectory.IsValid())
		{
			return std::nullopt;
		}
		std::wstring finalPath(32'768, L'\0');
		DWORD uiWritten = ::GetFinalPathNameByHandleW(hDirectory.Get(), finalPath.data(), static_cast<DWORD>(finalPath.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
		if (uiWritten == 0 || uiWritten >= finalPath.size())
		{
			return std::nullopt;
		}
		finalPath.resize(uiWritten);
		// A network share normalizes to \\?\UNC\server\share\..., which the prefix strip below would turn into a
		// mangled relative key that is then persisted, hashed, and handed to Git.  No logical key form covers a UNC
		// checkout, so canonicalization fails here where the reason is still visible.
		if (finalPath.starts_with(L"\\\\?\\UNC\\"))
		{
			Fail("network-share (UNC) checkouts are unsupported by the coordination tooling: " + WideToUtf8(finalPath));
			return std::nullopt;
		}
		if (finalPath.starts_with(L"\\\\?\\"))
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

	std::optional<std::wstring> NormalizeRelativeKey(const std::wstring& rValue)
	{
		const std::filesystem::path path(rValue);
		if (rValue.empty() || path.is_absolute())
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

	std::optional<std::wstring> NormalizeRepositoryRelativeKey(const std::wstring& rValue)
	{
		const std::filesystem::path path(rValue);
		if (rValue.empty() || path.has_root_name() || path.has_root_directory())
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
		return NormalizeRelativeKey(rValue);
	}

	std::optional<Locator> MakeLocator(const std::wstring& rDomain, const std::wstring& rLogicalKey)
	{
		std::optional<std::string> hash = HashSha256(WideToUtf8(rLogicalKey));
		std::filesystem::path localApplicationData = GetLocalApplicationDataPath();
		if (!hash || localApplicationData.empty())
		{
			return std::nullopt;
		}
		return Locator
		{
			.domain = rDomain,
			.logicalKey = rLogicalKey,
			.path = localApplicationData / L"BrokenEngineLocks" / rDomain / Utf8ToWide(*hash + ".lock"),
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
		// Every staged write in the process shares this sequence, so a temporary still waiting to be committed can
		// never be overwritten by a later write to the same path.
		static uint32_t suiSequence = 0;
		std::filesystem::path temporaryPath = ExtendedLengthPath(rPath);
		temporaryPath += L".tmp." + std::to_wstring(::GetCurrentProcessId()) + L"." + std::to_wstring(++suiSequence);
		Handle hFile(::CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY, nullptr));
		if (!hFile.IsValid())
		{
			return false;
		}
		DWORD uiWritten = 0;
		const bool bSucceeded = ::WriteFile(hFile.Get(), contents.data(), static_cast<DWORD>(contents.size()), &uiWritten, nullptr) != FALSE && uiWritten == contents.size() && ::FlushFileBuffers(hFile.Get()) != FALSE;
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

	bool HasOwner(const nlohmann::json& rMetadata, const std::wstring& rOwner)
	{
		return rMetadata.contains("owner") && rMetadata["owner"].is_string() && rMetadata["owner"].get<std::string>() == WideToUtf8(rOwner);
	}

	bool JsonIntegerEquals(const nlohmann::json& rValue, int64_t iExpected)
	{
		if (rValue.is_number_unsigned())
		{
			return iExpected >= 0 && rValue.get<uint64_t>() == static_cast<uint64_t>(iExpected);
		}
		return rValue.is_number_integer() && rValue.get<int64_t>() == iExpected;
	}

	std::optional<int64_t> JsonInt64(const nlohmann::json& rValue)
	{
		if (rValue.is_number_unsigned())
		{
			const uint64_t uiValue = rValue.get<uint64_t>();
			return uiValue <= static_cast<uint64_t>(INT64_MAX) ? std::optional<int64_t>(static_cast<int64_t>(uiValue)) : std::nullopt;
		}
		return rValue.is_number_integer() ? std::optional<int64_t>(rValue.get<int64_t>()) : std::nullopt;
	}

	bool ValidateMetadataEnvelope(const nlohmann::json& rMetadata, const Locator& rLocator, int64_t iExpectedSchemaVersion)
	{
		for (const char* pField : { "owner", "session", "worktree", "claimedAt", "heartbeatAt" })
		{
			if (!rMetadata.contains(pField) || !rMetadata[pField].is_string() || rMetadata[pField].get<std::string>().empty())
			{
				return false;
			}
		}
		uint64_t uiClaimedTicks = 0;
		uint64_t uiHeartbeatTicks = 0;
		const std::optional<int64_t> claimantPid = rMetadata.contains("claimantPid") ? JsonInt64(rMetadata["claimantPid"]) : std::nullopt;
		return rMetadata.contains("schemaVersion") && JsonIntegerEquals(rMetadata["schemaVersion"], iExpectedSchemaVersion) &&
			rMetadata.contains("domain") && rMetadata["domain"].is_string() && rMetadata["domain"].get<std::string>() == WideToUtf8(rLocator.domain) &&
			rMetadata.contains("logicalKey") && rMetadata["logicalKey"].is_string() && rMetadata["logicalKey"].get<std::string>() == WideToUtf8(rLocator.logicalKey) &&
			claimantPid && *claimantPid >= 0 && *claimantPid <= UINT32_MAX &&
			ParseUtcTimestamp(rMetadata["claimedAt"].get<std::string>(), uiClaimedTicks) &&
			ParseUtcTimestamp(rMetadata["heartbeatAt"].get<std::string>(), uiHeartbeatTicks) &&
			uiClaimedTicks <= uiHeartbeatTicks;
	}

	nlohmann::json NewMetadata(const Locator& rLocator, const std::wstring& rOwner, const std::wstring& rSession, const std::wstring& rWorktree)
	{
		const std::string timestamp = CurrentUtcTimestamp();
		return {
			{ "schemaVersion", kiSchemaVersion },
			{ "domain", WideToUtf8(rLocator.domain) },
			{ "logicalKey", WideToUtf8(rLocator.logicalKey) },
			{ "owner", WideToUtf8(rOwner) },
			{ "session", WideToUtf8(rSession) },
			{ "worktree", WideToUtf8(rWorktree) },
			{ "claimantPid", ::GetCurrentProcessId() },
			{ "claimedAt", timestamp },
			{ "heartbeatAt", timestamp },
		};
	}
}
