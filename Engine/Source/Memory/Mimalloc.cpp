#pragma warning(push, 0)
#pragma warning(disable: ALL_CODE_ANALYSIS_WARNINGS)

// The consuming project selects the startup arena size in KiB. mi_process_init reserves and commits it from its TLS callback,
// before any C++ static allocates; Engine MemoryInitializer only verifies it. Eager commit applies to on-demand fallback arenas.
#define MI_DEFAULT_ARENA_EAGER_COMMIT 1
#include "mimalloc/src/static.c"

#pragma warning(pop)
