<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-05T12:41:12.717Z","dependsOn":[]} -->
# Move AcquireTarget Into ShipQueries

## Context

`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp` measures 11,905
`bt-token-v1` with the ship avoidance change (10,105 at `5c2e6a8d`), above the 10,000 `.cpp`
threshold in `.agents/skills/reduce-file/references/worker.md`. Originating gap: a size observation from a
refactor log row, since removed but readable in `5c2e6a8d`, which named the height and plane policy
(`HighestFlightPlane` through
`RaiseAboveTerrain`) as a possible split without requiring one. Paths below are relative to
`Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/`, and line numbers are from
`5c2e6a8d`.

The file is over its threshold because of ordinary growth, and `AcquireTarget`
(`ShipBehaviours.cpp:687-759`) sits in it although it is a target pick over every enemy type:

- `ShipQueries` already owns the cross-type target picks, `FindAttackTarget`,
  `FindNearestEnemyShip`, and `FindNearestEnemyFlagship` (`ShipQueries.h:76-90`).
- Its only caller is `Omnis.cpp:699`, which already includes `ShipQueries.h`.
- It reads no file-local symbol of `ShipBehaviours.cpp`. `HasShipLineOfSight`, `NearestFlightPlane`,
  and `kiFiringClearanceTier` are declared in `ShipBehaviours.h`, and `ShipQueries.cpp` already uses
  them; `ForEachEnemyShip` and `VisitShip` are in `ShipQueries.h`.
- `ShipQueries.cpp` compiles in both the client and server projects, as `ShipBehaviours.cpp` does.

## Design

The author recommends moving `AcquireTarget` instead of the height and plane policy, because
`/reduce-file` asks for the least disruptive cohesive reduction, and this one needs no new file and
no project membership change. The plane policy move would add a new `.h`/`.cpp` pair, and its
`ClimbTowardsFlightPlane` would force the file-local `NavigationPlaneData` into a shared declaration.

- Cut the `AcquireTarget` definition from `ShipBehaviours.cpp` and paste it unchanged after
  `FindAttackTarget` in `ShipQueries.cpp`.
- Cut its declaration and its comment from `ShipBehaviours.h` (lines 113-117) and paste them
  unchanged after the `FindAttackTarget` declaration in `ShipQueries.h`.
- Leave `HasLineOfSight`, whose declaration stays in `ShipBehaviours.h`, and every other function
  where it is.

Expected sizes: `ShipBehaviours.cpp` about 11,200 tokens (from 11,905), `ShipQueries.cpp` about 3,830
(from 3,124), and `ShipBehaviours.h` about 2,580 (from 2,782). `AcquireTarget` measures about 710
tokens, so this move shrinks `ShipBehaviours.cpp` but leaves it above its threshold.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipBehaviours.cpp`, `.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.cpp`, `.h`

## In scope

- The `AcquireTarget` definition and declaration, moved verbatim between those files.

## Out of scope

- Any edit inside `AcquireTarget`, its signature, or its name.
- Merging `AcquireTarget` with `FindAttackTarget`.
- The height and plane policy, `HasLineOfSight`, and the rest of `ShipBehaviours`.
- `Omnis.cpp`, which is over its own threshold but outside this reduction.
- Project and filter membership: no file is added or removed.

## Risk and invariants

- Tier 1. Trigger: local behavior-preserving move with no signature change and no invariant
  exposure.
- Determinism: the moved function keeps its operation order and reads only the previous frame and
  cell static data, with no random draw and no frame write. Both translation units build with the
  same `/fp:strict` settings, so the frame CRC and `Frame::kiVersion` are unchanged.
- No serialization, wire, threading, or allocation exposure.

## Acceptance criteria

- `pwsh -NoProfile -File .agents/scripts/Measure-Tokens.ps1 -Path <ShipBehaviours.cpp path>` reports
  about 710 tokens fewer than the same command reported before the move. A result above 10,000 is
  expected: report it as the file's remaining excess, not as a failure.
- The diff is a pure move: the moved lines are byte-identical at both ends.
- The client and server build through `/compile`.
- The diff settles behavior, so no harness run is required.

## Notes

- `Documents/Plans/Game/RouteShipFleeReturnAndVeerOffGoals.md` adds a helper to `ShipBehaviours`.
  The two edit disjoint regions, and that helper adds to a file this move leaves above its
  threshold.
