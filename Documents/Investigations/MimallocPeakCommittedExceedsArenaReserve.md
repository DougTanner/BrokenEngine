# Mimalloc peak committed exceeds the arena reserve at server shutdown

Open question: why did a Debug server's mimalloc peak committed memory exceed
`kiMimallocArenaReserveMb` (10240 MiB) when its peak heap usage was only
1692 MiB? Not yet a Plan because the root cause is unproven, and the fix
depends on it (see `## What diagnosis must establish`).

## Observation

- Run: a Debug|x64 client and server in Shared data mode, driven through
  `/agent-harness`. The run spawned 8 players at cell `[0,0]`, then ran
  `replay_record` for about 12,600 ticks starting at tick 1853, followed by 20
  `inject_outward_transfer` calls. The server ran to about tick 14506 and then
  quit cleanly. The session log was scratch and is not kept.
- The server logged this at shutdown:
  `Mimalloc peak heap usage: 1692 MiB, peak committed: 11306 MiB (arena reserve: 10240 MiB)`
  It then hit `DEBUG_BREAK` in `MemoryInitializer::~MemoryInitializer`
  (`Engine/Source/Memory/GlobalAllocator.cpp`, the
  `iPeakCommittedMb > kiMimallocArenaReserveMb` check).
- The session's own change, which removed a 16-entry reserve and capacity
  check on a list that reached 85 small entries, cannot account for gigabytes
  of committed memory, so it is not a suspect.

## Relevant code

- `MemoryInitializer::MemoryInitializer` sets `mi_option_arena_eager_commit`
  to 1 and sets `mi_option_reserve_os_memory` to
  `kiMimallocArenaReserveMb * 1024` KiB.
- `~MemoryInitializer` (Debug only, not under `ENABLE_CRT_DEBUG_HEAP`) logs
  `stats.page_committed.peak` as "peak heap usage", compares
  `stats.committed.peak` against the reserve, and breaks when the reserve is
  exceeded.
- `Engine/Source/Memory/AGENTS.md` `## Architecture Notes` describes the
  break as "arena undersized".

## What diagnosis must establish

Run `/external-diagnose-bug`. It must establish:

1. What `stats.committed.peak` counts with eager commit enabled. Does it
   already include the eagerly committed 10 GiB arena, so any commit outside
   the arena trips the check? Or does the 11306 MiB reflect real arena growth?
   Mimalloc's own semantics decide this; do not assume either answer.
2. Where the roughly 1066 MiB above the reserve came from. It could be
   allocations served outside the pre-reserved arena (large or aligned
   allocations, or extra OS-reserved arenas), and it could come from specific
   callers.
3. Whether it reproduces without the long replay recording and outward
   transfer workload. Compare an idle Debug server run of similar length at
   the same baseline, so the check's baseline behavior is separated from any
   growth this workload causes.

Once those answers exist, the outcome decides the destination. A wrong check
or metric makes this a `Documents/Plans/Engine/` debt Plan for the shutdown
check. A real memory growth source makes it a Plan for that owner. An
undersized reserve means changing `kiMimallocArenaReserveMb` (a user decision
about the address-space budget).
