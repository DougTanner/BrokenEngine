#pragma once

#include <cstdint>
#include <span>

namespace toolcli
{
	int64_t RunPlanSchedulerCommand(std::span<wchar_t* const> argumentValues);
} // namespace toolcli
