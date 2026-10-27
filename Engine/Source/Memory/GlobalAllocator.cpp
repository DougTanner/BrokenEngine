#include "GlobalAllocator.h"

#include "CrashReport.h"

constinit std::atomic<int64_t> giAllocationsThisFrame = 0;

constinit std::atomic<bool> gbAllocationTrackingReady = false;

static void TrackAllocation()
{
	if constexpr (kbProfiling)
	{
		giAllocationsThisFrame.fetch_add(1, std::memory_order_relaxed);
	}

	if (!gbAllocationTrackingReady.load(std::memory_order_relaxed))
	{
		return;
	}

	if (common::gpThreadLocal == nullptr)
	{
		return;
	}

	if (giAllocationTrackingSuppressed > 0)
	{
		return;
	}

	// Heap allocation during main loop, use Workbuffer or filter out with ScopedSuppressAllocationTracking
	DEBUG_BREAK();
}

#if defined(ENABLE_CRT_DEBUG_HEAP)

[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new(std::size_t n) noexcept(false)
{
	TrackAllocation();
	void* pMemory = std::malloc(n);
	__assume(pMemory);
	return pMemory;
}
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new[](std::size_t n) noexcept(false)
{
	TrackAllocation();
	void* pMemory = std::malloc(n);
	__assume(pMemory);
	return pMemory;
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new  (std::size_t n, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return std::malloc(n);
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return std::malloc(n);
}
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new  (std::size_t n, std::align_val_t eAlignment) noexcept(false)
{
	TrackAllocation();
	void* pMemory = _aligned_malloc(n, static_cast<std::size_t>(eAlignment));
	__assume(pMemory);
	return pMemory;
}
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new[](std::size_t n, std::align_val_t eAlignment) noexcept(false)
{
	TrackAllocation();
	void* pMemory = _aligned_malloc(n, static_cast<std::size_t>(eAlignment));
	__assume(pMemory);
	return pMemory;
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new  (std::size_t n, std::align_val_t eAlignment, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return _aligned_malloc(n, static_cast<std::size_t>(eAlignment));
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new[](std::size_t n, std::align_val_t eAlignment, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return _aligned_malloc(n, static_cast<std::size_t>(eAlignment));
}

void operator delete(void* pMemory) noexcept
{
	std::free(pMemory);
}
void operator delete[](void* pMemory) noexcept
{
	std::free(pMemory);
}
void operator delete  (void* pMemory, const std::nothrow_t&) noexcept
{
	std::free(pMemory);
}
void operator delete[](void* pMemory, const std::nothrow_t&) noexcept
{
	std::free(pMemory);
}
void operator delete  (void* pMemory, std::size_t) noexcept
{
	std::free(pMemory);
}
void operator delete[](void* pMemory, std::size_t) noexcept
{
	std::free(pMemory);
}
void operator delete  (void* pMemory, std::align_val_t) noexcept
{
	_aligned_free(pMemory);
}
void operator delete[](void* pMemory, std::align_val_t) noexcept
{
	_aligned_free(pMemory);
}
void operator delete  (void* pMemory, std::size_t, std::align_val_t) noexcept
{
	_aligned_free(pMemory);
}
void operator delete[](void* pMemory, std::size_t, std::align_val_t) noexcept
{
	_aligned_free(pMemory);
}
void operator delete  (void* pMemory, std::align_val_t, const std::nothrow_t&) noexcept
{
	_aligned_free(pMemory);
}
void operator delete[](void* pMemory, std::align_val_t, const std::nothrow_t&) noexcept
{
	_aligned_free(pMemory);
}

#else

// These replacements track allocations because ThirdParty/mimalloc/include/mimalloc-new-delete.h has no tracking hook.
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new(std::size_t n) noexcept(false)
{
	TrackAllocation();
	return mi_new(n);
}
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new[](std::size_t n) noexcept(false)
{
	TrackAllocation();
	return mi_new(n);
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new  (std::size_t n, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return mi_new_nothrow(n);
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return mi_new_nothrow(n);
}
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new  (std::size_t n, std::align_val_t eAlignment) noexcept(false)
{
	TrackAllocation();
	return mi_new_aligned(n, static_cast<std::size_t>(eAlignment));
}
[[nodiscard]] _Ret_notnull_ _Post_writable_byte_size_(n) void* operator new[](std::size_t n, std::align_val_t eAlignment) noexcept(false)
{
	TrackAllocation();
	return mi_new_aligned(n, static_cast<std::size_t>(eAlignment));
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new  (std::size_t n, std::align_val_t eAlignment, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return mi_new_aligned_nothrow(n, static_cast<std::size_t>(eAlignment));
}
[[nodiscard]] _Ret_maybenull_ _Success_(return != NULL) _Post_writable_byte_size_(n) void* operator new[](std::size_t n, std::align_val_t eAlignment, const std::nothrow_t&) noexcept
{
	TrackAllocation();
	return mi_new_aligned_nothrow(n, static_cast<std::size_t>(eAlignment));
}

void operator delete(void* pMemory) noexcept
{
	mi_free(pMemory);
}
void operator delete[](void* pMemory) noexcept
{
	mi_free(pMemory);
}
void operator delete  (void* pMemory, const std::nothrow_t&) noexcept
{
	mi_free(pMemory);
}
void operator delete[](void* pMemory, const std::nothrow_t&) noexcept
{
	mi_free(pMemory);
}
void operator delete  (void* pMemory, std::size_t uiSize) noexcept
{
	mi_free_size(pMemory, uiSize);
}
void operator delete[](void* pMemory, std::size_t uiSize) noexcept
{
	mi_free_size(pMemory, uiSize);
}
void operator delete  (void* pMemory, std::align_val_t eAlignment) noexcept
{
	mi_free_aligned(pMemory, static_cast<std::size_t>(eAlignment));
}
void operator delete[](void* pMemory, std::align_val_t eAlignment) noexcept
{
	mi_free_aligned(pMemory, static_cast<std::size_t>(eAlignment));
}
void operator delete  (void* pMemory, std::size_t uiSize, std::align_val_t eAlignment) noexcept
{
	mi_free_size_aligned(pMemory, uiSize, static_cast<std::size_t>(eAlignment));
}
void operator delete[](void* pMemory, std::size_t uiSize, std::align_val_t eAlignment) noexcept
{
	mi_free_size_aligned(pMemory, uiSize, static_cast<std::size_t>(eAlignment));
}
void operator delete  (void* pMemory, std::align_val_t eAlignment, const std::nothrow_t&) noexcept
{
	mi_free_aligned(pMemory, static_cast<std::size_t>(eAlignment));
}
void operator delete[](void* pMemory, std::align_val_t eAlignment, const std::nothrow_t&) noexcept
{
	mi_free_aligned(pMemory, static_cast<std::size_t>(eAlignment));
}

#endif

struct MemoryInitializer
{
	MemoryInitializer()
	{
		// Write crash report on any abort() call (catches mimalloc assertions, std::terminate on a thread
		// outside MainThread's try/catch, and other CRT aborts)
		std::signal(SIGABRT, [](int)
		{
			engine::HandleException();
			engine::PromptCrashReportDesktopCopy();
		});

#if defined(ENABLE_CRT_DEBUG_HEAP)
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#else
		// Purge off so freed pages are never decommitted and recommitted during gameplay; set here because upstream has no compile-time default for it.
		mi_option_set(mi_option_purge_delay, -1);

		// mimalloc's process init reserves and commits the arena from the Mimalloc.cpp wrapper default; mimalloc exposes no id for that arena, so this
		// only verifies the process totals cover it. mimalloc's OS layer counts reserved and committed bytes even with MI_STATS off.
		mi_stats_t statistics = {};
		statistics.size = sizeof(mi_stats_t);
		statistics.version = MI_STAT_VERSION;
		bool bStatisticsRead = mi_stats_get(&statistics);
		int64_t iArenaReserveMebibytes = mi_option_get_size(mi_option_reserve_os_memory) / (1'024 * 1'024);
		int64_t iReservedMebibytes = statistics.reserved.current / (1'024 * 1'024);
		int64_t iCommittedMebibytes = statistics.committed.current / (1'024 * 1'024);
		if (!bStatisticsRead || iReservedMebibytes < iArenaReserveMebibytes || iCommittedMebibytes < iArenaReserveMebibytes)
		{
			LOG(kDefault, kWarning, "mimalloc arena reserve of {} MiB not found (reserved: {} MiB, committed: {} MiB); continuing on on-demand arenas", iArenaReserveMebibytes, iReservedMebibytes, iCommittedMebibytes);
		}

		if constexpr (kbMimallocDiagnostics)
		{
			iArenaCountAfterReserve = statistics.arena_count.total;

			mi_register_output([](const char* pcMessage, [[maybe_unused]] void* pArgument)
			{
				OutputDebugStringA(pcMessage);
			}, nullptr);
		}
#endif
	}

	~MemoryInitializer()
	{
#if !defined(ENABLE_CRT_DEBUG_HEAP)
		if constexpr (kbMimallocDiagnostics)
		{
			mi_stats_t statistics = {};
			statistics.size = sizeof(mi_stats_t);
			statistics.version = MI_STAT_VERSION;
			mi_stats_get(&statistics);

			int64_t iArenaReserveMebibytes = mi_option_get_size(mi_option_reserve_os_memory) / (1'024 * 1'024);
			int64_t iPeakCommittedMebibytes = statistics.committed.peak / (1'024 * 1'024);

			// May log during static destruction: Log.cpp initializes via init_seg(lib), so it is destroyed after this default-phase object.
			LOG(kDefault, kInfo, "Mimalloc peak heap usage: {} MiB, peak committed: {} MiB, arenas: {} (after reserve: {}), arena reserve: {} MiB", statistics.page_committed.peak / (1'024 * 1'024), iPeakCommittedMebibytes, statistics.arena_count.total, iArenaCountAfterReserve, iArenaReserveMebibytes);

			// More arenas than after the reserve means the reserve failed or was undersized.
			if (statistics.arena_count.total > iArenaCountAfterReserve)
			{
				DEBUG_BREAK();
			}
		}
#endif
	}

	int64_t iArenaCountAfterReserve = 0;
};

static MemoryInitializer sMemoryInitializer;
