#pragma once

#include "CoordinationStore.h"
#include "ToolCliCommon.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace toolcli::landing
{
	inline constexpr int kiLandingLeaseSchemaVersion = 3;

	struct LandingLease
	{
		std::string owner;
		std::string claimedAt;
		std::string heartbeatAt;
		std::string expiresAt;
		std::chrono::seconds duration {};
		uint64_t uiClaimedTicks = 0;
		uint64_t uiHeartbeatTicks = 0;
		uint64_t uiExpiresTicks = 0;
	};

	bool IsValidLeaseDuration(std::chrono::seconds leaseDuration);
	nlohmann::json NewLandingMetadata(const coordination::Locator& rLocator, std::wstring_view owner, std::wstring_view session, std::wstring_view worktree, std::chrono::seconds leaseDuration);
	std::optional<LandingLease> ValidateLandingLease(const nlohmann::json& rMetadata, const coordination::Locator& rLocator, uint64_t uiCurrentTicks);
	nlohmann::json LandingStatus(const nlohmann::json& rMetadata, const coordination::Locator& rLocator);
	bool AllRegisteredWorktreesClear(const coordination::Locator& rLocator);
} // namespace toolcli::landing
