#include "LandingLockLifecycle.h"

#include "ToolCliCommon.h"

#include <filesystem>
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
		uint64_t uiHeartbeatTicks = 0;
		ParseUtcTimestamp(metadata["heartbeatAt"].get<std::string>(), uiHeartbeatTicks);
		uint64_t uiExpiresTicks = uiHeartbeatTicks + static_cast<uint64_t>(leaseDuration.count()) * 10'000'000ull;
		metadata["expiresAt"] = FormatUtcTimestamp(uiExpiresTicks);
		return metadata;
	}

	std::optional<LandingLease> ValidateLandingLease(const nlohmann::json& rMetadata, const Locator& rLocator, uint64_t uiCurrentTicks)
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
		if (!ParseUtcTimestamp(lease.claimedAt, lease.uiClaimedTicks) || !ParseUtcTimestamp(lease.heartbeatAt, lease.uiHeartbeatTicks) || !ParseUtcTimestamp(lease.expiresAt, lease.uiExpiresTicks))
		{
			return std::nullopt;
		}
		if (lease.uiHeartbeatTicks > (std::numeric_limits<uint64_t>::max)() - static_cast<uint64_t>(lease.duration.count()) * 10'000'000ull
		 || lease.uiExpiresTicks != lease.uiHeartbeatTicks + static_cast<uint64_t>(lease.duration.count()) * 10'000'000ull)
		{
			return std::nullopt;
		}
		if (lease.uiHeartbeatTicks > uiCurrentTicks)
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
		uint64_t uiCurrentTicks = CurrentUtcTicks();
		std::optional<LandingLease> lease = ValidateLandingLease(rMetadata, rLocator, uiCurrentTicks);
		if (lease)
		{
			status["leaseState"] = uiCurrentTicks < lease->uiExpiresTicks ? "live" : "expired";
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
		for (size_t i = 0; i < listing->size();)
		{
			size_t uiEnd = listing->find('\0', i);
			if (uiEnd == std::string::npos)
			{
				uiEnd = listing->size();
			}
			std::string_view field(listing->data() + i, uiEnd - i);
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
			i = uiEnd + 1;
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
			if (!std::filesystem::is_directory(ExtendedLengthPath(rWorktree), error) || error)
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
			std::filesystem::path extendedGitDirectory = ExtendedLengthPath(gitDirectory);
			if (!std::filesystem::is_directory(extendedGitDirectory, error) || error)
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
