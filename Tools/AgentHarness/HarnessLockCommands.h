#pragma once

#include <cstdint>
#include <string_view>

namespace toolcli
{
	int RunHarnessLockCommand(int iArgumentCount, wchar_t* pArgumentValues[]);
	bool RefreshHarnessHeartbeat(std::wstring_view owner, int64_t iMaximumWaitMilliseconds = 10'000);
}
