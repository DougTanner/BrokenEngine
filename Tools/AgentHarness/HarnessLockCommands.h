#pragma once

#include <cstdint>
#include <string>

namespace toolcli
{
	int RunHarnessLockCommand(int iArgumentCount, wchar_t* pArgumentValues[]);
	bool RefreshHarnessHeartbeat(const std::wstring& rOwner, int64_t iMaximumWaitMilliseconds = 10'000);
}
