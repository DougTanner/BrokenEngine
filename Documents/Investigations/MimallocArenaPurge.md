# Mimalloc arena purge during gameplay

Open question: should mimalloc stop decommitting freed arena memory back to
Windows during gameplay? Not yet a Plan because the answer is a user decision
about runtime behavior: fewer OS calls, or memory returned to the OS. Measured
cost data that would inform it does not exist yet.

## Current behavior

- mimalloc's own process init reserves and commits a 10 GiB arena up front (a
  `ThirdParty/Prebuilts/Source/Engine/Mimalloc.cpp` wrapper default), and
  `MemoryInitializer` (`Engine/Source/Memory/GlobalAllocator.cpp`) verifies
  it. `Engine/Source/Memory/AGENTS.md` `## Architecture Notes` already notes
  that mimalloc's default purge may still decommit freed arena ranges during
  gameplay.
- mimalloc 2.2.7 defaults: `purge_delay` 10 ms (`ThirdParty/mimalloc/src/options.c:144`)
  times `arena_purge_mult` 10 (`options.c:153`) gives an arena purge delay of
  about 100 ms (`src/arena.c:467-469`). `purge_decommits` defaults to 1
  (`options.c:127`), so a purge decommits the range (`arena.c:485`, through
  `_mi_os_purge_ex` in `src/os.c`), and reuse recommits it.
- Segments purge too, on the same option (`src/segment.c:525-552`).
- Setting `mi_option_purge_delay` to -1 turns off both arena and segment purge
  (`arena.c:509`; the first check in `_mi_os_purge_ex`).

## Options

1. Keep the default purge. Freed memory goes back to Windows, but decommit and
   recommit OS calls happen during gameplay, which is what the up-front arena
   was meant to avoid.
2. Set `mi_option_purge_delay` to -1 in `MemoryInitializer`. No decommit or
   recommit calls during gameplay, but freed memory never goes back to
   Windows: committed memory and working set stay at their high-water mark for
   the life of the process.

Either way, committed pages that were never touched still take a soft page
fault on first touch.

## Context

Debug harness runs in the session that fixed the up-front arena measured: idle
server peak heap 149 MiB, workload server peak heap 1548 MiB, peak committed
about 10.3-10.5 GiB, and 2 arenas. Reserving the arena inside mimalloc's
process init removes the extra arena #0.

## Evidence that would inform the decision

A gameplay profile (for example `/analyze-diagsession` or an ETW capture)
counting `VirtualAlloc` commit and `VirtualFree` decommit calls, and the time
spent in them, with purge on and with purge off.
