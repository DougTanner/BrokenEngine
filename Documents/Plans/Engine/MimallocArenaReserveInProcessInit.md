<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-05T22:58:27.301Z","dependsOn":[]} -->
# Reserve the mimalloc startup arena inside mimalloc's process init

## Context

`MemoryInitializer` in `Engine/Source/Memory/GlobalAllocator.cpp` reserves and commits the fixed 10 GiB mimalloc arena (`kiMimallocArenaReserveMebibytes`) with a direct `mi_reserve_os_memory` call from its default-phase static constructor. Setting `mi_option_reserve_os_memory` from that constructor had no effect, because mimalloc reads the option in `mi_process_init` (`ThirdParty/mimalloc/src/init.c:660-663`), which runs from the TLS callback before any C++ static.

The direct reserve still runs too late. Every harness run of the client and the server, idle and under workload, logged `arenas: 2 (after reserve: 2)` at shutdown. For example, the workload server logged `Mimalloc peak heap usage: 1548 MiB, peak committed: 10452 MiB, arenas: 2 (after reserve: 2), arena reserve: 10240 MiB`. So one arena already exists when the constructor reserves:

- An earlier allocation creates it through `mi_arena_reserve` (`ThirdParty/mimalloc/src/arena.c:368-398`). It is 1 GiB (`MI_DEFAULT_ARENA_RESERVE`, `ThirdParty/mimalloc/src/options.c:69-75`). It is created with the default `arena_eager_commit` of 2, which leaves it uncommitted on Windows (`_mi_os_has_overcommit`, `ThirdParty/mimalloc/src/prim/windows/prim.c:129`). The constructor's `mi_option_set(mi_option_arena_eager_commit, 1)` has not run yet when it is created.
- Arena search is first-fit from index 0 (`mi_arena_try_alloc`, `arena.c:339-352`). So roughly the first 1 GiB of heap comes from this lazily committed arena before the eagerly committed 10 GiB arena serves anything. That partly defeats the user's decision to commit the full 10 GiB up front in all builds.
- The arena most likely comes from allocations by `init_seg(lib)` statics such as `Common/Log/Log.cpp:9`, which are constructed before default-phase statics. Which allocation creates it is unproven. The design below does not depend on knowing it.

The arena-count growth check in `~MemoryInitializer` still holds, because it compares against `iArenaCountAfterReserve` captured after the direct reserve. It cannot see the arena that existed earlier.

Change Workflow tier: **Tier 3**. Trigger: build/bootstrap coordination that can block other sessions. The fix edits a repository-owned ThirdParty wrapper. Worktree consumers link the primary `Output/ThirdParty.<Config>.lib` (`ThirdParty/Prebuilts/Platforms/VisualStudio2026/AGENTS.md` `## Consumer Provisioning`), so the change reaches every active worktree at the next primary ThirdParty rebuild, whatever each worktree's own source base is.

## Design

The user already chose this approach (approach B) as the follow-up, to apply if the direct-reserve session's run showed the early arena. The run did show it.

1. In `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp`, define before `#include "mimalloc/src/static.c"`:
   - `MI_DEFAULT_RESERVE_OS_MEMORY` as 10 GiB in KiB (`10L*1024L*1024L`). With this, `mi_process_init` commits the arena itself through `mi_reserve_os_memory(..., true /*commit*/, true /*allow large*/)` (`init.c:660-663`), before any C++ static allocates. The option is `long` KiB (`options.c:89-90`).
   - `MI_DEFAULT_ARENA_EAGER_COMMIT` as `1` (`options.c:64-65`). Recommended, so that on-demand fallback arenas are committed eagerly from process start and not only after the engine constructor runs. This keeps the existing "eagerly commit arenas" intent with one owner.
2. In `MemoryInitializer::MemoryInitializer`, remove the direct `mi_reserve_os_memory` call and the `mi_option_set(mi_option_arena_eager_commit, 1)` call, because mimalloc's own defaults now carry both.
3. Keep the warning that startup logs when the OS refuses the reserve. mimalloc's own reserve does not report failure to the engine. The recommended check: in the constructor, call the public `mi_arena_area(1 /*arena index 0*/, &size)` (`ThirdParty/mimalloc/include/mimalloc.h:305`, `arena.c:452`; arena ids are index + 1, `arena.c:79-82`). Log the existing warning when it returns `nullptr` or a size below the reserve. A count-based check is not recommended: after a refused reserve, an early allocation may create an on-demand arena, so the count cannot tell the two cases apart.
4. Keep `kiMimallocArenaReserveMebibytes` as the engine-side size used by the warning, the shutdown log, and the step 3 check. Add no `static_assert` tying it to the wrapper's KiB value: the two values live in separate translation units, and this plan adds no shared header for them.
5. Keep the `iArenaCountAfterReserve` baseline and the shutdown arena-count growth check unchanged. After this change, a healthy run logs `arenas: 1 (after reserve: 1)`.
6. Update the static-initializer bullet in `Engine/Source/Memory/AGENTS.md` so it says mimalloc's own process init reserves the arena, and the engine constructor only verifies it.

Exposure:

- Allocation and startup behavior of both executables. No determinism/CRC, serialization, `.pack`/`kiVersion`, replay, wire, or threading exposure.
- The wrapper object is pulled only by binaries that reference mimalloc symbols. Today only `BT_ENGINE` client and server builds include `mimalloc.h` (`Common/ExternalHeaders.h:249-256`); DataPacker links `ThirdParty.<Config>.lib` but does not define `BT_ENGINE`. Verify during implementation that DataPacker and any `ENABLE_CRT_DEBUG_HEAP` build do not link the mimalloc object, so neither gains a 10 GiB commit.
- Cross-session hazard: once the primary ThirdParty libraries are rebuilt with this change, a worktree whose source still contains the direct reserve commits two 10 GiB arenas. Land the wrapper change and the `GlobalAllocator.cpp` removal together, then rebuild the primary ThirdParty libraries. In a worktree, the change cannot be verified at runtime before landing.

## Critical files

- `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp`
- `Engine/Source/Memory/GlobalAllocator.cpp`
- `Engine/Source/Memory/AGENTS.md`

## In scope

- `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp`: the two `MI_DEFAULT_*` defines before the `static.c` include.
- `MemoryInitializer::MemoryInitializer` in `Engine/Source/Memory/GlobalAllocator.cpp`: remove the direct reserve and the eager-commit `mi_option_set`; add the arena-0 size check that drives the existing refusal warning.
- The static-initializer bullet in `Engine/Source/Memory/AGENTS.md`.

## Out of scope

- mimalloc's default arena purge, which decommits freed arena ranges (`purge_delay` × `arena_purge_mult`, `arena.c:467-469`, `:485`). Changing it needs a separate user behavior decision.
- The reserve size, the commit-everything policy, and the continue-on-refusal policy. These are fixed user decisions.
- `~MemoryInitializer`'s shutdown log and arena-count growth check, apart from the logged values changing.
- Upstream mimalloc sources under `ThirdParty/mimalloc/`.
- Finding which `init_seg(lib)` allocation creates the early arena.

## Acceptance criteria

- After the primary ThirdParty rebuild, `/agent-harness` runs of the Debug|x64 server and client (idle and workload; the shutdown arena line exists only in Debug, where `kbMimallocDiagnostics` is true) log at shutdown peak committed >= 10240 MiB, the decisive reserve-success signal, together with `arenas: 1 (after reserve: 1)` and `arena reserve: 10240 MiB`, and do not hit the arena-growth `DEBUG_BREAK`. A refused reserve can also log `arenas: 1 (after reserve: 1)`, so peak committed below 10240 MiB means the reserve failed; stop and report that, not arena overflow.
- Code reading (refusal branch; no harness run reaches it, and its `kWarning` is logged before `ProcessMain` opens the `--log-file` sink at `Engine/Source/Main.cpp:829`): when `mi_arena_area(1, &size)` returns `nullptr` or a size below the reserve, the constructor logs the existing `kWarning` and returns normally, with no `ASSERT`, throw, or `DEBUG_BREAK` on that path.
- DataPacker's peak memory and commit behavior are unchanged.

## Notes

- Origin: residual from the Verify (acceptance) step of the change that replaced the option-based reserve with the direct constructor reserve. Its plan recorded arena #0 as a residual, to become a follow-up if proven. The harness logs above prove it.
- Verification and landing need primary ThirdParty maintenance through `/compile` after landing, followed by a `/agent-harness` run against the rebuilt library.
