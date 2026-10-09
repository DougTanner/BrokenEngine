<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-09T12:12:29.055Z","dependsOn":[]} -->
# Make CollisionLayer max times and velocities required, and return one NavQueryDirection result

## Context

A `/plan-alternatives` step for an earlier engine runtime cleanup proposed "make contracts strict where they are declared". The user kept that change's own scope and recorded the simulation part of the alternative here as a follow-up Plan.

Two engine interfaces declare inputs or outputs as optional although every caller supplies or wants them, so the implementations carry null fallbacks that never run:

- **`CollisionLayer`** (`Engine/Source/Frame/Collision.h`) marks `pfMaxTimes` ("Optional exclusive entity-collision cutoff") and `pVecVelocities` ("Optional") as nullable. All four `Collision::AddLayer` callers — `SpaceshipsPostRender::PreCollision` (`Spaceships/SpaceshipsCombat.cpp`), `BlastersPostRender::PreCollision` (`Blasters/BlastersUpdate.cpp`), `MissilesPostRender::PreCollision` (`Missiles/MissilesUpdate.cpp`), and `PlayersPostRender::PreCollision` (`Players/Players.cpp`), under `Projects/BrokenEngineSandbox/Source/Frame/Collections/` — set both from storage sized to the layer count, and each returns before `AddLayer` when its count is zero. `Collision.cpp` still falls back: `RecordCollision` substitutes a zero velocity for a null `pVecVelocities`, and `TestAndCollectPair` substitutes `std::numeric_limits<float>::max()` for a null `pfMaxTimes` in the pair time cutoff. `CollisionResult::vecOtherVelocity`'s comment says it is zero when not provided.
- **`NavQueryDirection`** (`Engine/Source/Frame/NavQuery.h`, `NavQuery.cpp`) returns the direction and reports the next waypoint and, on the server only, whether A* ran through two defaulted nullable out-pointers, with separate `BT_SERVER` and `BT_CLIENT` declarations and definitions. `NavQuery.cpp` null-checks the waypoint pointer at every write (the seed write, the escape write, the unblocked write, and inside `AStarPath`) and the A* flag pointer inside `kbProfiling` blocks. Its two callers, `RecomputeNavigationPath` and `PlayersPostRender::ComputeNavigation` (`Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp`), each split the call with `#if defined(BT_SERVER)` to pass the flag pointer; `ComputeNavigation` passes a null waypoint pointer unless `kbDebugRender`. The server feeds the flag into `AddRawCpuTimerAuxiliaryCount(game::kCpuTimerPostRenderUpdateNavigationQuery, ...)` under `kbProfiling`.

## Design

### CollisionLayer

1. In `Collision::AddLayer`, add one `ASSERT` that `rLayer.pfMaxTimes` and `rLayer.pVecVelocities` are both non-null, unconditionally: every caller returns before `AddLayer` on a zero count, so no empty layer is ever registered.
2. In `RecordCollision`, read `rOtherLayer.pVecVelocities[iOther]` directly.
3. In `TestAndCollectPair`, read `rLayerA.pfMaxTimes[i]` and `rLayerB.pfMaxTimes[j]` directly; the cutoff comparison itself is unchanged.
4. In `Collision.h`, drop "Optional" from the `pfMaxTimes` and `pVecVelocities` comments and the "zero if not provided" clause from `CollisionResult::vecOtherVelocity`.
5. In `Engine/Source/Frame/AGENTS.md`, change "Optional per-object maximum-time cutoffs are exclusive" to "Per-object maximum-time cutoffs are exclusive".

Every caller already supplies both pointers, so each fallback branch is dead and the results are bit-identical.

### NavQueryDirection

1. Declare one result struct in `NavQuery.h` holding the direction, the next waypoint, and the entered-A* flag, and one declaration for both builds taking position, destination, and `rNavData` and returning that struct. The author recommends the name `NavQueryDirectionResult`, with `XMVECTOR` members declared as the existing `CollisionResult` does.
2. In `NavQuery.cpp`, keep one definition with no `#if` split. Every former waypoint write sets the result's waypoint unconditionally, with the same value at the same point in the control flow; the flag is set to `true` on the A* branch, and the `kbProfiling` gates and null checks around it are deleted. `AStarPath` takes the waypoint by reference instead of by nullable pointer. The direction computation is untouched, so the returned direction is bit-identical.
3. In `RecomputeNavigationPath` and `PlayersPostRender::ComputeNavigation`, replace each `#if` call split with one call that reads the direction, waypoint, and flag from the result. Delete the `bEnteredAStar` locals and their guards. The `AddRawCpuTimerAuxiliaryCount` call stays inside its existing `#if defined(BT_SERVER)` and `if constexpr (kbProfiling)` block, now reading the flag from the result, so the server profile timer row keeps its A* count and the client profile is unchanged. `ComputeNavigation` keeps reading the waypoint only under `kbDebugRender`.

The waypoint is computed on every call instead of only when requested. It is one `XMVectorSet` per exit, never read by simulation, and the client-only debug waypoint column it feeds is outside the CRC.

## Critical files

- `Engine/Source/Frame/Collision.h`, `Engine/Source/Frame/Collision.cpp`, `Engine/Source/Frame/AGENTS.md`
- `Engine/Source/Frame/NavQuery.h`, `Engine/Source/Frame/NavQuery.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Players/PlayersNavigation.cpp`

## In scope

- `CollisionLayer::pfMaxTimes` and `CollisionLayer::pVecVelocities` comments and `CollisionResult::vecOtherVelocity` comment.
- `Collision::AddLayer`: the one new `ASSERT`.
- `RecordCollision`: the `vecOtherVelocity` initializer.
- `TestAndCollectPair`: the two `pfMaxTimes` reads.
- `Engine/Source/Frame/AGENTS.md`: the word "Optional" in its maximum-time cutoff sentence.
- `NavQuery.h`: the new result struct and the single `NavQueryDirection` declaration replacing the two.
- `NavQuery.cpp`: `NavQueryDirection` (signature, waypoint writes, A* flag writes and their `kbProfiling`/`BT_SERVER` gates) and `AStarPath` (its waypoint parameter and the null check at its write).
- Every `NavQueryDirection` call site — today `RecomputeNavigationPath` and `PlayersPostRender::ComputeNavigation` in `PlayersNavigation.cpp`; if another Plan has moved them first, the same calls wherever they now live: the call, the `bEnteredAStar` local, and the profiling read.

## Out of scope

- The four `AddLayer` callers: they already supply both pointers and need no change.
- `CollisionLayer`'s other members, the zone, candidate, sort, and commit logic in `Collision.cpp`, and every other line of `TestAndCollectPair` (it has no `kAlreadyCollided` early return).
- `NavQuerySnapToNavigable`, `NavQueryPointBlocked`, `NavMissFallbackDirection`, the Z `ASSERT`s and `gBaseHeight` reads in `NavQueryDirection`, and the A* algorithm itself.
- The profile timer definitions and `ProfileManagerBase::AddRawCpuTimerAuxiliaryCount`.

## Acceptance criteria

- Client and server build clean in Debug and Release through `/compile`.
- `/agent-harness`: a client/server session with ships navigating around islands and blasters and missiles colliding runs with no per-tick CRC mismatch and no `ASSERT`, and a replay determinism check passes.
- The A* count is settled by the diff: on the server under `kbProfiling`, each call site still passes the result's entered-A* flag to `AddRawCpuTimerAuxiliaryCount(game::kCpuTimerPostRenderUpdateNavigationQuery, ...)`, and `NavQueryDirection` sets that flag on exactly the branch that calls `AStarPath`.

## Notes

- Change Workflow tier: **Tier 3** (`.agents/references/risk-tiers.md`). Highest trigger: edits to deterministic simulation code on the per-tick CRC surface (`TestAndCollectPair`, `RecordCollision`, `NavQueryDirection`, `AStarPath`), plus a public engine signature change used by the game across the `BT_CLIENT`/`BT_SERVER` split. Each edit is behavior-preserving by the arguments above; a mistaken equivalence would change CRC'd PostRender state.
- No wire, save, replay, or `.pack` format changes.
- No ordering dependency. `FlightPlaneData.md`, `FlightPlaneClimbing.md`, and `NavigationPerFlightPlane.md` edit the Z handling inside `NavQueryDirection` and its call sites, and `ShipTypeSplit.md` moves the `PlayersNavigation.cpp` body and the `Players` `AddLayer` caller into new ship collections. Their edits stay valid with either signature; locate each site by symbol, and whichever lands second rebases.
