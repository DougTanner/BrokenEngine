#include "LandingLockLifecycle.h"

#include "ToolCliCommon.h"

#include <filesystem>
#include <iterator>
#include <limits>
#include <vector>

namespace toolcli::landing
{
	using coordination::CurrentUtcTicks;
	using coordination::FormatUtcTimestamp;
	using coordination::JsonInt64;
	using coordination::Locator;
	using coordination::NewMetadata;
	using coordination::ParseUtcTimestamp;

	constexpr std::chrono::seconds kMinimumLeaseDuration = std::chrono::seconds(60);
	constexpr std::chrono::seconds kMaximumLeaseDuration = std::chrono::seconds(86'400);

	bool IsValidLeaseDuration(std::chrono::seconds leaseDuration)
	{
		return leaseDuration >= kMinimumLeaseDuration && leaseDuration <= kMaximumLeaseDuration;
	}

	nlohmann::json NewLandingMetadata(const Locator& rLocator, std::wstring_view owner, std::wstring_view session, std::wstring_view worktree, std::chrono::seconds leaseDuration)
	{
		nlohmann::json metadata = NewMetadata(rLocator, owner, session, worktree);
		metadata["schemaVersion"] = kiLandingLeaseSchemaVersion;
		metadata["leaseDurationSeconds"] = leaseDuration.count();
		int64_t iHeartbeatTicks = 0;
		ParseUtcTimestamp(metadata["heartbeatAt"].get<std::string>(), iHeartbeatTicks);
		int64_t iExpiresTicks = iHeartbeatTicks + leaseDuration.count() * 10'000'000i64;
		metadata["expiresAt"] = FormatUtcTimestamp(static_cast<uint64_t>(iExpiresTicks));
		return metadata;
	}

	std::optional<LandingLease> ValidateLandingLease(const nlohmann::json& rMetadata, const Locator& rLocator, int64_t iCurrentTicks)
	{
		if (!coordination::ValidateMetadataEnvelope(rMetadata, rLocator, kiLandingLeaseSchemaVersion))
		{
			return std::nullopt;
		}
		if (!rMetadata.contains("expiresAt") || !rMetadata["expiresAt"].is_string() || rMetadata["expiresAt"].get<std::string>().empty())
		{
			return std::nullopt;
		}
		if (!rMetadata.contains("leaseDurationSeconds"))
		{
			return std::nullopt;
		}
		std::optional<int64_t> durationSeconds = JsonInt64(rMetadata["leaseDurationSeconds"]);
		if (!durationSeconds || !IsValidLeaseDuration(std::chrono::seconds(*durationSeconds)))
		{
			return std::nullopt;
		}
		LandingLease lease;
		lease.owner = rMetadata["owner"].get<std::string>();
		lease.claimedAt = rMetadata["claimedAt"].get<std::string>();
		lease.heartbeatAt = rMetadata["heartbeatAt"].get<std::string>();
		lease.expiresAt = rMetadata["expiresAt"].get<std::string>();
		lease.duration = std::chrono::seconds(*durationSeconds);
		// The envelope already proved claimedAt and heartbeatAt parse and are ordered; these calls exist to fill the lease ticks.
		ParseUtcTimestamp(lease.claimedAt, lease.iClaimedTicks);
		ParseUtcTimestamp(lease.heartbeatAt, lease.iHeartbeatTicks);
		if (!ParseUtcTimestamp(lease.expiresAt, lease.iExpiresTicks))
		{
			return std::nullopt;
		}
		if (lease.iHeartbeatTicks > (std::numeric_limits<int64_t>::max)() - lease.duration.count() * 10'000'000i64
		 || lease.iExpiresTicks != lease.iHeartbeatTicks + lease.duration.count() * 10'000'000i64)
		{
			return std::nullopt;
		}
		if (lease.iHeartbeatTicks > iCurrentTicks)
		{
			return std::nullopt;
		}
		return lease;
	}

	nlohmann::json LandingStatus(const nlohmann::json& rMetadata, const Locator& rLocator)
	{
		nlohmann::json status = { { "held", true }, { "leaseState", "unverifiable" } };
		for (const char* pField : { "domain", "logicalKey", "owner", "session", "worktree", "claimedAt", "heartbeatAt", "expiresAt" })
		{
			if (rMetadata.contains(pField) && rMetadata[pField].is_string())
			{
				status[pField] = rMetadata[pField];
			}
		}
		for (const char* pField : { "schemaVersion", "leaseDurationSeconds" })
		{
			if (rMetadata.contains(pField) && rMetadata[pField].is_number_integer())
			{
				status[pField] = rMetadata[pField];
			}
		}
		int64_t iCurrentTicks = static_cast<int64_t>(CurrentUtcTicks());
		std::optional<LandingLease> lease = ValidateLandingLease(rMetadata, rLocator, iCurrentTicks);
		if (lease)
		{
			status["leaseState"] = iCurrentTicks < lease->iExpiresTicks ? "live" : "expired";
		}
		return status;
	}

	bool AllRegisteredWorktreesClear(const Locator& rLocator)
	{
		std::optional<std::string> listing = RunGit({ L"--git-dir", rLocator.logicalKey, L"worktree", L"list", L"--porcelain", L"-z" });
		if (!listing)
		{
			return false;
		}
		std::vector<std::wstring> worktrees;
		std::wstring currentWorktree;
		bool bInvalidEntry = false;
		for (int64_t i = 0; i < std::ssize(*listing);)
		{
			int64_t iEnd = static_cast<int64_t>(listing->find('\0', static_cast<std::string::size_type>(i)));
			if (iEnd == static_cast<int64_t>(std::string::npos))
			{
				iEnd = std::ssize(*listing);
			}
			std::string_view field(listing->data() + i, static_cast<std::string_view::size_type>(iEnd - i));
			if (field.empty())
			{
				if (currentWorktree.empty())
				{
					return false;
				}
				if (bInvalidEntry)
				{
					return false;
				}
				worktrees.push_back(std::move(currentWorktree));
				currentWorktree.clear();
				bInvalidEntry = false;
			}
			else if (field.starts_with("worktree "))
			{
				currentWorktree = Utf8ToWide(field.substr(9));
			}
			else if (field == "bare" || field.starts_with("prunable"))
			{
				bInvalidEntry = true;
			}
			i = iEnd + 1;
		}
		if (!currentWorktree.empty())
		{
			if (bInvalidEntry)
			{
				return false;
			}
			worktrees.push_back(std::move(currentWorktree));
		}
		if (worktrees.empty())
		{
			return false;
		}
		const std::filesystem::path markers[] = { L"MERGE_HEAD", L"rebase-merge", L"rebase-apply", L"CHERRY_PICK_HEAD", L"REVERT_HEAD", L"BISECT_LOG", L"sequencer" };
		for (const std::wstring& rWorktree : worktrees)
		{
			std::error_code error;
			if (!std::filesystem::is_directory(ExtendedLengthPath(rWorktree), error))
			{
				return false;
			}
			std::optional<std::string> gitDirectoryText = RunGit({ L"-C", rWorktree, L"rev-parse", L"--path-format=absolute", L"--git-dir" });
			if (!gitDirectoryText)
			{
				return false;
			}
			while (!gitDirectoryText->empty() && (gitDirectoryText->back() == '\r' || gitDirectoryText->back() == '\n'))
			{
				gitDirectoryText->pop_back();
			}
			std::filesystem::path gitDirectory = Utf8ToWide(*gitDirectoryText);
			std::filesystem::path extendedGitDirectory = ExtendedLengthPath(std::move(gitDirectory));
			if (!std::filesystem::is_directory(extendedGitDirectory, error))
			{
				return false;
			}
			for (const std::filesystem::path& rMarker : markers)
			{
				if (std::filesystem::exists(extendedGitDirectory / rMarker, error) || error)
				{
					return false;
				}
			}
		}
		return true;
	}
} // namespace toolcli::landing
