#include "ErrorUtils.h"

namespace common
{

void LogDebugBreak(std::source_location sourceLocation)
{
	// Heap: formatting and the log-file sink may allocate, and DEBUG_BREAK() is itself the allocation tracker's
	// tripwire (Engine/Source/Memory/GlobalAllocator.cpp) — without this the tripwire would re-enter itself forever.
	ScopedSuppressAllocationTracking suppress;
	LOG(kDefault, kWarning, "DEBUG_BREAK at {}:{} in {}", sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name());
}

void Assert(bool bCondition, std::string_view expression, std::source_location sourceLocation)
{
	if (!bCondition) [[unlikely]]
	{
		LOG(kDefault, kError, "Assert failed: \"{}\" at {}:{} in {}", expression, sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name());
		DEBUG_BREAK();
		throw std::runtime_error(std::format("Assert failed: \"{}\" at {}:{}", expression, sourceLocation.file_name(), sourceLocation.line()));
	}
}

void CheckHresult(HRESULT hresult, std::string_view expression, std::source_location sourceLocation)
{
	if (hresult < 0) [[unlikely]]
	{
		char pcHex[20] {};
		LOG(kDefault, kError, "CheckHresult failed: \"{}\" at {}:{} in {} - {} {}: {}", expression, sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name(), hresult, ToHex(std::span(pcHex), static_cast<uint32_t>(hresult)), HresultToString(hresult).data());
		DEBUG_BREAK();
		throw std::runtime_error(std::format("CheckHresult failed: \"{}\" at {}:{}", expression, sourceLocation.file_name(), sourceLocation.line()));
	}
}

} // namespace common
