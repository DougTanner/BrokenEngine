#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "ToolCliCommon.h"

namespace toolcli::coordination
{
	inline constexpr int kiSchemaVersion = 2;
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

		[[nodiscard]] bool IsValid() const;

	private:
		Handle mhFile;
		std::filesystem::path mPath;
	};

	std::string CurrentUtcTimestamp();
	bool ParseUtcTimestamp(const std::string& rValue, uint64_t& rTicks);
	uint64_t CurrentUtcTicks();
	std::string FormatUtcTimestamp(uint64_t uiTicks);
	std::optional<std::string> HashSha256(std::string_view value);
	std::optional<std::wstring> CanonicalizeDirectoryPath(const std::wstring& rValue);
	std::optional<std::wstring> NormalizeRelativeKey(const std::wstring& rValue);
	std::optional<std::wstring> NormalizeRepositoryRelativeKey(const std::wstring& rValue);
	std::optional<Locator> MakeLocator(const std::wstring& rDomain, const std::wstring& rLogicalKey);
	bool EnsureParentDirectory(const std::filesystem::path& rPath);
	bool ReadMetadata(const std::filesystem::path& rPath, nlohmann::json& rMetadata);
	// Durable byte replacement for small coordination artifacts, including paths beyond MAX_PATH.
	// The staging pair splits that replacement so a caller can prepare several files before publishing any of them;
	// both halves discard the temporary on failure, so a failed call leaves nothing behind.
	bool StageBytesAtomic(const std::filesystem::path& rPath, std::string_view contents, std::filesystem::path& rStagedPath);
	bool CommitStagedBytes(const std::filesystem::path& rStagedPath, const std::filesystem::path& rPath);
	bool WriteBytesAtomic(const std::filesystem::path& rPath, std::string_view contents);
	bool WriteMetadataAtomic(const std::filesystem::path& rPath, const nlohmann::json& rMetadata);
	void PrintMetadata(const nlohmann::json& rMetadata);
	bool HasOwner(const nlohmann::json& rMetadata, const std::wstring& rOwner);
	bool JsonIntegerEquals(const nlohmann::json& rValue, int64_t iExpected);
	std::optional<int64_t> JsonInt64(const nlohmann::json& rValue);
	bool ValidateMetadataEnvelope(const nlohmann::json& rMetadata, const Locator& rLocator, int64_t iExpectedSchemaVersion);
	nlohmann::json NewMetadata(const Locator& rLocator, const std::wstring& rOwner, const std::wstring& rSession, const std::wstring& rWorktree);
}
