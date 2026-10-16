<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-15T23:30:38.964Z","dependsOn":[]} -->
# Small Sandbox cleanups: redundant AllocateAndCopy overrides, duplicated navigation recompute, Players difference labels, and project-neutral engine Frame docs

Change Workflow Tier 1 (`.agents/references/risk-tiers.md`). Highest trigger: local behavior-preserving work. Every simulated value, CRC, collection layout, `kiVersion`, save, replay, and wire byte stays the same. The only runtime differences are diagnostic log text: corrected difference-log labels and one dropped `kWarning` (item 2). No public signature is used outside the code this Plan edits. Line numbers cite baseline `f1d140ae`.

## Context

A second game project will be created beside `Projects/BrokenEngineSandbox`, and Sandbox is the template it starts from. A project audit found four small defects that would be copied into it:

1. **Forwarding `AllocateAndCopy` overrides.** `engine::AllocateAndCopyCollections` (`Engine/Source/Frame/FrameUtils.h:346-357`) calls a collection's static `AllocateAndCopy` when one exists and otherwise calls `engine::AllocateAndCopyMembers`. Four Sandbox overrides do nothing except make that same call: `SpaceshipsPostRender::AllocateAndCopy` (`Spaceships/Spaceships.cpp:346-349`, declared `Spaceships.h:126`), and `BlastersInterpolate::AllocateAndCopy` and `BlastersPostRender::AllocateAndCopy` (`Blasters/Blasters.cpp:24-32`, declared `Blasters.h:34,89`). `Missiles` already relies on the default. Blasters and Spaceships reach the dispatch only through `GameInterpolateCollections`/`GamePostRenderCollections` (`Frame/FrameCollections.h:11-19`, `Frame/Frame.cpp:146,183`). Nothing calls these overrides by name, and `engine::Collection<T>` declares no `AllocateAndCopy` that deleting them could expose. `SpaceshipsInterpolate::AllocateAndCopy` (`Spaceships.cpp:340-344`) also opens a CPU profile scope (`kCpuTimerInterpolateAllocateAndCopySpaceships`), so it is not a pure forward and stays.
2. **Duplicated navigation recompute.** In `PlayersPostRender::ComputeNavigation`, the neighbouring-cell branch (`Players/PlayersNavigation.cpp:386-405`) repeats the body of the file-static `RecomputeNavigationPath` (`:183-201`), which the island-destination branch already calls (`:306,349`). The two are the same `NavQueryDirection` call, the same `> 0.001f` length test, the same normalized fallback, and the same `kbDebugRender` waypoint store. The copy differs only by `LOG(kNavData, kWarning, "Player {} navQuery returned zero, fallback dir={}", ...)` on the fallback path (`:398`).
3. **Wrong difference-log labels.** `PlayersInterpolate::LogDifferences` labels `pPushers` as `"puiPushers"` (`Players/Players.cpp:786`; the member is `engine::pusher_t* pPushers`, `Players.h:92`). `PlayersPostRender::LogDifferences` labels `pFleetWantedCoordinates` as `"pFleetWantedCoords.iX"`/`".iY"` and `puiPendingFleetWantedCoordinateTicks` as `"puiPendingFleetWantedCoordTicks"` (`Players.cpp:821-823`). Every other label in both functions is the exact member name. No tool or script reads these strings (`git grep` finds only these lines).
4. **Sandbox rules in engine Frame docs.** `Engine/Source/Frame/AGENTS.md:9` links Sandbox's `Source/Frame/AGENTS.md` as the place game phase bodies are documented. `:14` states a Sandbox rule as an engine invariant ("Every overload but the player one ... `PlayersPostRender::Spawn` asserts instead ..."; the assert is `Players.cpp:385-386`). `Engine/Source/Frame/Collections/AGENTS.md:28` cites `SpaceshipsPostRender` as its only example, and `:43` links Sandbox's Collections `AGENTS.md`. `Engine/Source/Frame/Collections/Pushers/AGENTS.md:3` names "players, spaceships" as pusher owners, and `:8` cites `PlayersPostRender::Update` as the same-phase consumer. No engine collection declares `SharedMembers()`, so the engine has no example to use in place of `SpaceshipsPostRender`.

## Design

1. Delete the three forwarding overrides from item 1 together with their header declarations. Dispatch then takes the `else` branch of `AllocateAndCopyCollections`, which makes the same `engine::AllocateAndCopyMembers` call, so the generated copy is unchanged. Keep `SpaceshipsInterpolate::AllocateAndCopy`. The `Live exemplar` cell of the shared-only game collection row in `.agents/skills/add-collection-member/references/worker.md:18` cites the deleted `Spaceships.cpp` `SpaceshipsPostRender::AllocateAndCopy` for `PersistentMembers()` carry-forward. Replace that citation with `SpaceshipsPostRender::PersistentMembers` (`Spaceships.h:176`), which is now where the carry-forward is declared.
2. Replace the inline block at `PlayersNavigation.cpp:386-405` with `RecomputeNavigationPath(rCurrent, i, vecPosition, vecDestination, rStaticData, rVecArtificialIntelligenceDirection);` inside the existing `if (bRecompute)`. The author recommends dropping the copy's `kWarning`. The island-destination branch already takes the same fallback silently, so one helper keeps a single fallback behavior. Moving the warning into the helper would add warnings, possibly every tick, on island paths whose zero-direction frequency is unmeasured. Logs are not part of the CRC.
3. Rename the three labels in item 3 to the exact member names: `"pPushers"`, `"pFleetWantedCoordinates.iX"`/`".iY"`, and `"puiPendingFleetWantedCoordinateTicks"`.
4. Make the engine Frame docs project-neutral and keep every engine rule.
   - `Engine/Source/Frame/AGENTS.md:9`: replace the Sandbox link with a plain reference to the game project's own `Source/Frame/AGENTS.md`.
   - `Engine/Source/Frame/AGENTS.md:14`: state the rule generically. Each `Spawn(SpawnInfo)` overload tests bounds before appending. An overload either refuses an outside position silently and returns whether it appended, or, where an outside position is a bug, asserts and always reports success. The producer pre-test rule then refers to an asserting overload, not "the player overload".
   - Move the Sandbox fact (`PlayersPostRender::Spawn` asserts because a human-controlled unit outside its cell is a bug) into `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/AGENTS.md`.
   - `Engine/Source/Frame/Collections/AGENTS.md:28`: drop the `(`SpaceshipsPostRender`)` parenthetical.
   - `Engine/Source/Frame/Collections/AGENTS.md:43`: replace the Sandbox link with a plain reference to the game project's Collections `AGENTS.md`.
   - `Pushers/AGENTS.md:3`: drop "(players, spaceships)".
   - `Pushers/AGENTS.md:8`: replace the `PlayersPostRender::Update` example with "a game collection's Update".

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Spaceships/Spaceships.h`, `Spaceships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h`, `Blasters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp`, `Players.cpp`, `AGENTS.md`
- `Engine/Source/Frame/AGENTS.md`, `Engine/Source/Frame/Collections/AGENTS.md`, `Engine/Source/Frame/Collections/Pushers/AGENTS.md`
- `.agents/skills/add-collection-member/references/worker.md`

## In scope

- Delete `SpaceshipsPostRender::AllocateAndCopy` (`Spaceships.cpp:346-349`, `Spaceships.h:126`), `BlastersInterpolate::AllocateAndCopy`, and `BlastersPostRender::AllocateAndCopy` (`Blasters.cpp:24-32`, `Blasters.h:34,89`).
- `PlayersPostRender::ComputeNavigation`: only the `if (bRecompute)` body of the neighbouring-cell branch (`PlayersNavigation.cpp:386-405`), which becomes the one `RecomputeNavigationPath` call.
- The three label string literals at `Players.cpp:786,821-823`.
- `Engine/Source/Frame/AGENTS.md` `## Architecture` bullets at `:9` and `:14`; `Engine/Source/Frame/Collections/AGENTS.md:28` and its `## See Also` entry at `:43`; `Engine/Source/Frame/Collections/Pushers/AGENTS.md:3,8`.
- One added bullet in `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/AGENTS.md` carrying the player-Spawn assert rule.
- The shared-only game collection row's `Live exemplar` cell in `.agents/skills/add-collection-member/references/worker.md:18`.

## Out of scope

- `SpaceshipsInterpolate::AllocateAndCopy` and its profile scope; `FrameInterpolate::AllocateAndCopy` and `FramePostRender::AllocateAndCopy`, including the direct `pPlayers` `AllocateAndCopyMembers` calls (`Frame.cpp:138-145,175-183`).
- The engine collections' own forwarding overrides (Puffs, Pushers, SmokeTrails, WindTrails, Explosions under `Engine/Source/Frame/Collections/`) and `AllocateAndCopyCollections` itself.
- Any change to `RecomputeNavigationPath`'s signature or body, to `ShouldRecomputeNavigation`, or to the destination switch at `PlayersNavigation.cpp:367-384`.
- `common::LogDifference` overloads for `ClientGuid` or `GridCoord`. The `Players.cpp:818-822` field splits stay. Both types are engine types that `Common/Log/LogDifference.h` cannot name, and this is the only site that splits them.
- Every other label, member, or `LogDifferences` body; every collection layout, `kiVersion`, `Members()` tuple, save, replay, and wire format.
- Sandbox names in engine C++ and in every other engine `AGENTS.md` (`Network/`, `Agent/`, `Ui/`, and the rest); sibling Plans own those or they remain outside this Plan.
- Project file membership: no file is added or removed.

## Risk triggers and invariants

- PostRender state stays bit-identical. Item 1 compiles to the same `AllocateAndCopyMembers` call. Item 2 is the same computation, with the same branch, threshold, and stores, minus a log line. Items 3 and 4 touch only log text and docs. No collection `kiVersion` bump: no serialized byte changes.
- A deleted override must not be replaced by anything that alters copy order or selection. `AllocateAndCopyCollections` still visits collections in tuple order.

## Acceptance criteria

| Criterion | Observable | Method |
|---|---|---|
| Both targets build | BrokenEngineSandbox client and server build with no new warnings | `/compile` |
| No forwarding overrides remain | `git grep -n "AllocateAndCopy" -- Projects/BrokenEngineSandbox/Source/Frame/Collections` lists only `SpaceshipsInterpolate::AllocateAndCopy` declaration and definition among function sites | `git grep` |
| One navigation recompute body | `NavQueryDirection` appears once in `PlayersNavigation.cpp`, inside `RecomputeNavigationPath` | `git grep` |
| Engine Frame docs name no Sandbox gameplay | `git grep -niE "sandbox|player|spaceship|blaster|missile|fleet" -- Engine/Source/Frame/AGENTS.md Engine/Source/Frame/Collections/AGENTS.md Engine/Source/Frame/Collections/Pushers/AGENTS.md` returns nothing | `git grep` |
| Replay determinism CRC unchanged | A replay recorded on the baseline build plays back on the changed build, and repeated playback loops of a fresh recording that includes player navigation across a cell edge and spaceship and blaster spawns, all with no `LogDifferences CRC` mismatch | `/agent-harness` replay check |

## Coordination

- `Documents/Plans/Game/ShipTypeSplit.md` replaces `Players/` and `Spaceships/` with Fighters, Omnis, and Battleships and moves `PlayersNavigation.cpp`'s body into `Ships/ShipBehaviours.cpp`. Whichever lands second rebases onto the other's edits. If it lands first, this Plan's items 1-3 apply to their successors: the new types declare no forwarding `AllocateAndCopy`, `ComputeNavigation`'s neighbouring-cell branch calls `RecomputeNavigationPath`, and each `LogDifferences` label matches its member name.

## Notes

- `/update-claude-docs` does not need to edit any `AGENTS.md` beyond the ones listed in `## In scope`. The Sandbox Collections `AGENTS.md` line 15 still points correctly at the engine Frame bounds rule.
