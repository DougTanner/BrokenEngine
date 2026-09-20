#include "Workbuffer.h"

namespace common
{

void Workbuffer::Append(std::string_view text)
{
	ASSERT(miDepth > 0);
	int64_t iNeeded = miSize + static_cast<int64_t>(text.size());
	if (iNeeded > mBuffer.Size()) [[unlikely]]
	{
		Grow(iNeeded);
	}

	std::memcpy(reinterpret_cast<char*>(mBuffer.Data()) + miSize, text.data(), text.size());
	miSize += static_cast<int64_t>(text.size());
}

void Workbuffer::Append(std::wstring_view text)
{
	ASSERT(miDepth > 0);
	if (text.empty())
	{
		return;
	}

	int iSize = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
	int64_t iNeeded = miSize + iSize;
	if (iNeeded > mBuffer.Size()) [[unlikely]]
	{
		Grow(iNeeded);
	}

	WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), reinterpret_cast<char*>(mBuffer.Data()) + miSize, iSize, nullptr, nullptr);
	miSize += iSize;
}

void Workbuffer::Append(int64_t iValue)
{
	ASSERT(miDepth > 0);
	char* pStart = reinterpret_cast<char*>(mBuffer.Data()) + miSize;
	char* pEnd = reinterpret_cast<char*>(mBuffer.Data()) + mBuffer.Size();
	std::to_chars_result result = std::to_chars(pStart, pEnd, iValue);
	if (result.ec == std::errc::value_too_large) [[unlikely]]
	{
		Grow(miSize + 32);
		pStart = reinterpret_cast<char*>(mBuffer.Data()) + miSize;
		pEnd = reinterpret_cast<char*>(mBuffer.Data()) + mBuffer.Size();
		result = std::to_chars(pStart, pEnd, iValue);
	}

	miSize = result.ptr - reinterpret_cast<char*>(mBuffer.Data());
}

void Workbuffer::AppendFloat(float fValue, int iPrecision)
{
	ASSERT(miDepth > 0);
	char* pStart = reinterpret_cast<char*>(mBuffer.Data()) + miSize;
	char* pEnd = reinterpret_cast<char*>(mBuffer.Data()) + mBuffer.Size();
	std::to_chars_result result = std::to_chars(pStart, pEnd, fValue, std::chars_format::fixed, iPrecision);
	if (result.ec == std::errc::value_too_large) [[unlikely]]
	{
		Grow(miSize + 64);
		pStart = reinterpret_cast<char*>(mBuffer.Data()) + miSize;
		pEnd = reinterpret_cast<char*>(mBuffer.Data()) + mBuffer.Size();
		result = std::to_chars(pStart, pEnd, fValue, std::chars_format::fixed, iPrecision);
	}

	miSize = result.ptr - reinterpret_cast<char*>(mBuffer.Data());
}

std::string_view Workbuffer::View() const
{
	ASSERT(miDepth > 0);
	return std::string_view(reinterpret_cast<const char*>(mBuffer.Data() + miBase), miSize - miBase);
}

void Workbuffer::Grow(int64_t iNeededCapacity)
{
	// The buffer is sized up front: a grow means it was under-sized. DEBUG_BREAK alerts in debug; the resize then
	// commits more of the ThreadLocal-owned reservation in place, so the base address never moves and every
	// View/Span/PushBuffer handle taken before this grow keeps pointing at its own bytes.
	DEBUG_BREAK();
	if (miDepth > 0)
	{
		// Logged at kError so this under-size is diagnosable in Release post-mortem crash reports (DEBUG_BREAK is debug-only).
		// Caution: args here must stay non-workbuffer-formatting (plain int64 — no Wb/path/wstring wrappers). The workbuffer
		// is mid-resize; a workbuffer-formatting arg would Push/Pop a frame and re-enter the very buffer being grown.
		LOG(kDefault, kError, "Workbuffer under-sized: grew while a frame was open (depth {}, needed {}, capacity {}) — outstanding pointers stay valid; size the buffer correctly up front", miDepth, iNeededCapacity, mBuffer.Size());
	}
	mBuffer.Resize(iNeededCapacity * 2);
}

} // namespace common
