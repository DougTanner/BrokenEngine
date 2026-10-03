#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace toolcli
{
	int RunHarnessLockCommand(std::span<const wchar_t* const> argumentValues);
	bool RefreshHarnessHeartbeat(std::wstring_view owner, int64_t iMaximumWaitMilliseconds = 10'000);
}
