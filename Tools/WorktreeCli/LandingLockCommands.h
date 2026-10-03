#pragma once

#include <span>

namespace toolcli
{
	int RunLandingLockCommand(std::span<const wchar_t* const> argumentValues);
} // namespace toolcli
