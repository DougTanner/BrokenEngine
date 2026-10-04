#include "TweaksSliderMap.h"

#if defined(BT_CLIENT)

namespace engine
{

std::unordered_map<std::string_view, Wrapper*>& TweaksSliderMap::Get()
{
	// Heap: static unordered_map built once on first call, lives forever. Can't use workbuffer (data lost on Pop)
	// and can't pre-allocate (STL map manages its own hash buckets internally)
	ScopedSuppressAllocationTracking suppress;

	static std::unordered_map<std::string_view, Wrapper*> sSliderMap = {};
	return sSliderMap;
}

TweaksSliderMapRegistrar::TweaksSliderMapRegistrar(std::initializer_list<std::pair<const std::string_view, Wrapper*>> entries)
{
	// Heap: STL hash buckets allocate. Static-init runs before main-loop tracking begins; suppression mirrors TweaksSliderMap::Get() and stays explicit.
	ScopedSuppressAllocationTracking suppress;

	// Individual insertion detects duplicate keys; bulk insertion silently retains existing keys, and the first-open audit cannot detect the lost registration.
	// Pre-main logging relies on Log.cpp's library-phase mutex/file-sink initialization, LogWrite's alive flag, and LOG's gpThreadLocal == nullptr fallback buffer.
	// Assert reports the key, breaks under a debugger, and throws to terminate static initialization; formatting runs before allocation tracking is armed.
	for (const std::pair<const std::string_view, Wrapper*>& rEntry : entries)
	{
		bool bInserted = TweaksSliderMap::Get().insert(rEntry).second;
		if (!bInserted) [[unlikely]]
		{
			common::Assert(bInserted, std::format("Duplicate Tweaks slider key: \"{}\"", rEntry.first));
		}
	}
}

} // namespace engine

#endif // defined(BT_CLIENT)
