#pragma warning(push, 0)
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)

// mi_process_init reserves and commits this 10 GiB arena (in KiB) from its TLS callback, before any C++ static
// allocates; Engine MemoryInitializer only verifies it. Eager commit applies to on-demand fallback arenas.
#define MI_DEFAULT_RESERVE_OS_MEMORY (10L * 1'024L * 1'024L)
#define MI_DEFAULT_ARENA_EAGER_COMMIT 1
#include "mimalloc/src/static.c"

#pragma warning(pop)
