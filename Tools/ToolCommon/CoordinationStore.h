#pragma once

#include "ToolCliCommon.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace toolcli::coordination
{
	inline constexpr int64_t kiSchemaVersion = 2;
	// A denied-access budget this large can never bind before an overall wait deadline.
	inline constexpr int64_t kiUnboundedDeniedAccessMilliseconds = INT64_MAX;

	struct Locator
	{
		std::wstring domain;
		std::wstring logicalKey;
		std::filesystem::path path;
	};

	class Guard
	{
	public:
		// rbContentionObserved: a live holder was proven at any point during acquisition; the final error can be a delete-pending flicker instead.
		// rFailureReason: "timed out" for contention, otherwise the acquisition's Windows error.
		explicit Guard(const std::filesystem::path& rPath, bool& rbContentionObserved, std::string& rFailureReason, int64_t iMaximumWaitMilliseconds = 10'000, int64_t iMaximumDeniedAccessMilliseconds = kiUnboundedDeniedAccessMilliseconds);
		~Guard();

		Guard(const Guard&) = delete;
		Guard& operator=(const Guard&) = delete;

		bool mbValid = false;

	private:
		Handle mhFile;
		std::filesystem::path mPath;
	};

	std::string CurrentUtcTimestamp();
	bool ParseUtcTimestamp(const std::string& rValue, uint64_t& rTicks);
	uint64_t CurrentUtcTicks();
	std::string FormatUtcTimestamp(uint64_t uiTicks);
	std::optional<std::string> HashSha256(std::string_view value);
	std::optional<std::wstring> CanonicalizeDirectoryPath(std::wstring_view value);
	std::optional<std::wstring> NormalizeRelativeKey(std::wstring_view value);
	std::optional<std::wstring> NormalizeRepositoryRelativeKey(std::wstring_view value);
	std::optional<Locator> MakeLocator(std::wstring_view domain, std::wstring_view logicalKey);
	bool EnsureParentDirectory(const std::filesystem::path& rPath);
	bool ReadMetadata(const std::filesystem::path& rPath, nlohmann::json& rMetadata);
	// Durable byte replacement for small coordination artifacts, including paths beyond MAX_PATH.
	// The staging pair splits that replacement so a caller can prepare several files before publishing any of them;
	// A failed write after file creation and a failed commit both attempt to remove the temporary file.
	bool StageBytesAtomic(const std::filesystem::path& rPath, std::string_view contents, std::filesystem::path& rStagedPath);
	bool CommitStagedBytes(const std::filesystem::path& rStagedPath, const std::filesystem::path& rPath);
	bool WriteBytesAtomic(const std::filesystem::path& rPath, std::string_view contents);
	bool WriteMetadataAtomic(const std::filesystem::path& rPath, const nlohmann::json& rMetadata);
	void PrintMetadata(const nlohmann::json& rMetadata);
	bool HasOwner(const nlohmann::json& rMetadata, std::wstring_view owner);
	bool JsonIntegerEquals(const nlohmann::json& rValue, int64_t iExpected);
	std::optional<int64_t> JsonInt64(const nlohmann::json& rValue);
	bool ValidateMetadataEnvelope(const nlohmann::json& rMetadata, const Locator& rLocator, int64_t iExpectedSchemaVersion);
	nlohmann::json NewMetadata(const Locator& rLocator, std::wstring_view owner, std::wstring_view session, std::wstring_view worktree);
} // namespace toolcli::coordination
