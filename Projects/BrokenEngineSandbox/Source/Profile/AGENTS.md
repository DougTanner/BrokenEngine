# /Projects/BrokenEngineSandbox/Source/Profile/

Game-specific profiling extending `engine::ProfileManagerBase` with game CPU counters and timers (no game GPU timers/counters).

Global: `game::gpProfileManager`

## Architecture

Game enums begin at `engine::kEngineCpuCounterCount` /
`engine::kEngineCpuTimerCount` so indices compose into one contiguous space;
base accessors route by index using the game arrays and name tables passed to
its constructor. The CPU-timer hierarchy mirrors the frame phases (interpolate /
post-render / render) and counters track per-collection populations and rendered
subsets. The ProfileManager is constructed in `Main.cpp` before the engine
managers; its constructor starts the total boot timer.

Overlay screens and their graphs are engine-owned, so this scope holds only the game counter and timer rows.

## Invariants

- The engine compiles against six game enumerators by name: the frame-phase
  timers `kCpuTimerFrameUpdate`, `kCpuTimerFrameInterpolate`,
  `kCpuTimerFramePostRender` (`GameBase.cpp` brackets the frame phases with
  them; the FPS header reads the first for total frame time), the PostRender
  timers `kCpuTimerPostRenderCollide` (`Collision.cpp` times
  `Collision::Collide`) and `kCpuTimerPostRenderUpdateNavigationQuery`
  (`NavQuery.cpp` times `NavQueryDirection` and, on the server, adds its A*
  count), and `kGameCpuCounterFirst`, the anchor the engine's compile-time check
  uses to pin the start of the game counter range. Renaming or removing any of
  them breaks the engine build.
- Display names live in `static_assert`-guarded name tables in `ProfileManager.h` (mirroring the engine convention; the struct rows carry only runtime state). Indented strings encode overlay hierarchy; preserve the leading-space convention when adding entries.
- The server registers `kCpuTimerPostRenderUpdateNavigationQuery` as a raw CPU timer, the only game timer registered that way (`ProfileManager.cpp`). Its latch and discard rules are engine-owned (`../../../../Engine/Source/Profile/AGENTS.md` `## CPU Timing`); the load/reset discard is in `../../../../Engine/Source/File/AGENTS.md` `## Grid Saves`.

## See Also
- Engine profiling base: `../../../../Engine/Source/Profile/AGENTS.md`
