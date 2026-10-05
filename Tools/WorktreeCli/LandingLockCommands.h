#pragma once

#include <cstdint>
#include <span>

namespace toolcli
{
	int64_t RunLandingLockCommand(std::span<const wchar_t* const> argumentValues);
} // namespace toolcli
