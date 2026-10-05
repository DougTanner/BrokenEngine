#pragma once

#include "CoordinationStore.h"
#include "ToolCliCommon.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace toolcli::landing
{
	inline constexpr int64_t kiLandingLeaseSchemaVersion = 3;

	struct LandingLease
	{
		std::string owner;
		std::string claimedAt;
		std::string heartbeatAt;
		std::string expiresAt;
		std::chrono::seconds duration {};
		int64_t iClaimedTicks = 0;
		int64_t iHeartbeatTicks = 0;
		int64_t iExpiresTicks = 0;
	};

	bool IsValidLeaseDuration(std::chrono::seconds leaseDuration);
	nlohmann::json NewLandingMetadata(const coordination::Locator& rLocator, std::wstring_view owner, std::wstring_view session, std::wstring_view worktree, std::chrono::seconds leaseDuration);
	std::optional<LandingLease> ValidateLandingLease(const nlohmann::json& rMetadata, const coordination::Locator& rLocator, int64_t iCurrentTicks);
	nlohmann::json LandingStatus(const nlohmann::json& rMetadata, const coordination::Locator& rLocator);
	bool AllRegisteredWorktreesClear(const coordination::Locator& rLocator);
} // namespace toolcli::landing
