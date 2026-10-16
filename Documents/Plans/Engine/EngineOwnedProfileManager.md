<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-16T13:04:55.819Z","dependsOn":[]} -->
# Make the profile manager engine-owned and delete the game `ProfileManager` subclass

Tier 2 (`.agents/references/risk-tiers.md`): one subsystem's runtime behavior,
the profiling overlay and boot timing, with a public engine constructor
signature change. Profiling is outside the per-tick CRC and touches no wire,
save, replay, threading, or trust surface. Line numbers cite baseline
`45b5e347`. Where a statement here and the code disagree, the code wins; report
the contradiction instead of matching one side to the other. Bare `Profile/`,
`Agent/` paths mean `Projects/BrokenEngineSandbox/Source/<path>`.

## Context

The user's direction this session, quoted by the dispatching session: "Now's the time to do any large refactors, before we create the second game project, NOTE: Avoid 'hooks' the engine can just call functions/globals it expects the game:: to have available." Engine code may name expected `game::` classes, globals, functions, and members; it must not name Sandbox gameplay (`Blaster`, `Spaceship`, `Missile`, `Player`, `bUseMissiles`, `Fleet`).

Today the engine reaches the game's profile rows through a derived class that exists only to pass game storage into the engine:

- `engine::ProfileManagerBase` takes four spans in its constructor (`Engine/Source/Profile/ProfileManagerBase.h:318-320`, body `ProfileManagerBase.cpp:25-39`, which also allocates the server raw-timer state at `:33-38`) and stores them as `mpGameCpuCounters`, `mpGameCpuTimers`, `mpGameCpuCounterNames`, `mpGameCpuTimerNames` (`ProfileManagerBase.h:396-399`). It has a virtual destructor (`:321`) only because a game class derives from it.
- `game::ProfileManager` (`Profile/ProfileManager.h:90-102`) owns the two game
  row arrays, inherits `common::Singleton<ProfileManager>`, and its constructor
  (`Profile/ProfileManager.cpp:8-20`) passes the arrays and the name tables
  `kGameCpuCounterNames`/`kGameCpuTimerNames` (`ProfileManager.h:52-88`) to the
  base, then, under `kbProfiling`, registers
  `kCpuTimerPostRenderUpdateNavigationQuery` as a raw CPU timer on the server
  and calls `BootStart(engine::kBootTimerTotal)`. The global is
  `game::gpProfileManager` (`ProfileManager.h:104`, `ProfileManager.cpp:6`) with
  a global-scope `using game::gpProfileManager;` (`ProfileManager.h:108`).
- The engine constructs it at `Engine/Source/Main.cpp:134` (`std::make_unique<game::ProfileManager>()`).
- The engine already includes the game header and names game enumerators: `ProfileManagerBase.cpp:7` includes `Profile/ProfileManager.h`, and `:14-15` static-assert `game::kCpuTimerFrameUpdate` and `game::kGameCpuCounterFirst`. No code outside `ProfileManager.h/.cpp` names `game::ProfileManager`, `mGameCpuCounters`, or `mGameCpuTimers` except `Main.cpp:134`; 38 C++ files name the unqualified `gpProfileManager`.

The game enums sit in a game header included per translation unit, while `ProfileManagerBase.h` is in the `Engine.h` aggregation inside the precompiled header, so the engine class cannot hold member arrays sized by the game counts without an include-order change.

## Design

The author recommends this shape:

- `ProfileManagerBase` gets a default constructor. In `ProfileManagerBase.cpp`,
  which already includes the game header, two file-scope arrays hold the game
  rows: `CpuCounter` sized `game::kGameCpuCounterCount - kEngineCpuCounterCount`
  and `CpuTimer` sized `game::kGameCpuTimerCount - kEngineCpuTimerCount`, named
  per `Documents/C++StyleGuide.txt`. The constructor points the four existing
  `mpGame*` members at those arrays and at
  `game::kGameCpuCounterNames`/`kGameCpuTimerNames`, and sets
  `miCpuCounterCount`/`miCpuTimerCount` from the same extents. Rationale: static
  storage keeps the existing pointer members and every reader unchanged, needs
  no heap allocation, and avoids touching the `Pch.h`/`Engine.h` include order;
  the class is a process singleton.
- At the end of that constructor, after the raw-timer allocation it already makes, in today's order and under `if constexpr (kbProfiling)`: on the server `RegisterRawCpuTimer(game::kCpuTimerPostRenderUpdateNavigationQuery)`, then `BootStart(kBootTimerTotal)`.
- `ProfileManagerBase` inherits `common::Singleton<ProfileManagerBase>`; `ProfileManagerBase.h` declares `inline ProfileManagerBase* gpProfileManager = nullptr;` in `engine` (the `FileManager.h:226` pattern) and one global-scope `using engine::gpProfileManager;`, replacing today's `using game::gpProfileManager;`, so the unqualified call sites stay unchanged.
- Drop the virtual destructor (`ProfileManagerBase.h:321`) and the constructor comment about derived arrays (`:318-319`).
- `Main.cpp:134` constructs `engine::ProfileManagerBase`.
- `Profile/ProfileManager.h` keeps the two enums, the two name tables, and their `static_assert`s; the class, `extern`, and `using` go. Delete `Profile/ProfileManager.cpp`.
- The class and file names `ProfileManagerBase` stay, as the engine keeps `TweaksScreenBase`: renaming would touch every include and reader for no behavior change.

## Critical files

- `Engine/Source/Profile/ProfileManagerBase.h`, `ProfileManagerBase.cpp`
- `Engine/Source/Main.cpp`
- `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.h`; `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp` (deleted)
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj(.filters)`, `BrokenEngineSandboxServer.vcxproj(.filters)` through `/update-vcxproj`
- `Engine/Source/Profile/AGENTS.md`, `Projects/BrokenEngineSandbox/Source/Profile/AGENTS.md`

## In scope

- `ProfileManagerBase` constructor, destructor, base-class list, the new file-scope game row arrays, and the `gpProfileManager` global and `using` declaration.
- `Main.cpp:134`.
- Removal of `game::ProfileManager`, `game::gpProfileManager`, the `using game::gpProfileManager;` line, and `Profile/ProfileManager.cpp`, with its project membership in both projects.
- Docs this makes false: `Engine/Source/Profile/AGENTS.md:3` and the second `## Index and Lifetime Contracts` bullet (the base no longer stores derived addresses); game `Profile/AGENTS.md:3,5,9,17` (the global, who constructs it, and where the raw NavQuery timer is registered).

## Out of scope

- Moving any timer or counter between `GameCpuTimers`/`GameCpuCounters` and the engine enums, or changing any enumerator, name string, or overlay row order. That is the profile-timer ordering question, which no Plan or Investigation owns yet; this change does not depend on it, because it moves only where the game rows are stored, not which enum a row belongs to.
- Renaming `ProfileManagerBase` or its files; GPU timers; boot timer enumerators; the profile overlay screens.
- `Pch.h` and `Engine.h` include order.

## Acceptance criteria

1. `rg -n "game::ProfileManager\b|class ProfileManager\b|using game::gpProfileManager|virtual ~ProfileManagerBase" Engine Projects -g "*.h" -g "*.cpp"` prints nothing, and `Projects/BrokenEngineSandbox/Source/Profile/ProfileManager.cpp` no longer exists.
2. `/compile` builds the client and server in Debug and Profile with no new warnings; `/update-vcxproj` validation passes.
3. A Profile-build client launched through `/agent-harness` shows the CPU overlay with the same game rows in the same order and indentation as before, and the boot log reports `Total`; a Profile-build server `query_profile` still reports the NavQuery raw timer row.

## Notes

- `Documents/Plans/Engine/AgentHarnessIntoEngine.md` `## Design` says the moved `query_profile` handler names `game::gpProfileManager`; the code spells it unqualified, which resolves through the new `using engine::gpProfileManager;`, so that Plan needs no edit.
- `Documents/Plans/Game/ShipTypeSplit.md` renames enumerators and name strings in `Profile/ProfileManager.h`; that overlaps only textually with the class removal here.
