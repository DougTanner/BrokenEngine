<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:38.374Z","dependsOn":[]} -->
# Add a by-locator ShipRegistryId accessor

## Context

Paths are relative to `Projects/BrokenEngineSandbox/Source/`; `Ships/` stands
for `Frame/Collections/Ships/`. Line numbers are as of commit `fe1fb69b`.

Four sites spell the same `VisitShip` lambda to read the registry id of a
located ship row. Each is inside a `ForEachEnemyShip` visitor and returns
`rInterpolate.pRegistryIds[iRow]`:

- `Ships/ShipQueries.cpp:51-57`, the `RegistryId` local lambda in
  `FindAttackTarget`, reading `rPreviousFrame`.
- `Ships/ShipQueries.cpp:183-186`, the `bKeptTarget` test, reading
  `rPreviousFrame`.
- `Frame/Collections/Fighters/FightersCombat.cpp:105-108`, the `bKeptTarget`
  test, reading the current `rFrame`.
- `Frame/Collections/Battleships/BattleshipsCombat.cpp:281-284`, the `target`
  read, reading the current `rFrame`.

The by-locator accessors that exist cover position and hull only: `ShipPosition`
and `ShipHull` (`Ships/ShipQueries.h:115-117`). So each kept-attack-target
comparison re-derives which frame part and column hold the registry id, a rule
`Ships/AGENTS.md` `## Ship Invariants` (last bullet) owns.

Originating gap: a cohesion finding of the architecture phase of a deep
analysis of `Ships/` (the directory, not its subdirectories) at commit
`fe1fb69b`. It is pre-existing debt, not an unmet acceptance criterion of any
change. The verifying review confirmed the four copies and accepted the finding
with one caveat, recorded under `## Design`; it rated the benefit small but
real, since four current copies in three files meet the root `AGENTS.md` DRY
directive's bar for a helper. Nothing was built or run.

## Design

Recommended: add
`[[nodiscard]] engine::registry_id_t ShipRegistryId(const FrameInterpolate& rInterpolate, ShipLocator locator);`
beside `ShipPosition` in `Ships/ShipQueries.h`, defined beside it in
`Ships/ShipQueries.cpp`, and call it at the four sites. Rationale: it follows
the pattern `ShipPosition` already sets, and it keeps the column choice in the
query module.

Caveat the executing session preserves: each site keeps its own source frame.
The two `ShipQueries.cpp` sites pass `rPreviousFrame.interpolate`; the two
Combat sites pass the current `rFrame.interpolate`. Swapping a site's frame
shifts CRC-visible behavior by one tick (`Ships/AGENTS.md` `## Ship
Invariants`, the previous-frame read bullet). The reads are integer loads, so
with the frames kept the change is bit-identical.

Not checked by the review: whether an out-of-line accessor inlines inside the
per-enemy visitors. `ShipPosition` is already out of line at the same kind of
site, so the recommendation accepts the same cost.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/ShipQueries.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersCombat.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`

## In scope

- A new `ShipRegistryId` declaration in `ShipQueries.h` and its definition in
  `ShipQueries.cpp`.
- The four `VisitShip` registry-id lambdas listed under `## Context`, each
  replaced by a call that passes the same frame's `interpolate` part.

## Out of scope

- Any other `pRegistryIds` read: the direct column reads in `Frame.cpp`,
  `AgentCommandsServerQueries.cpp`, `FindNearestEnemyFlagship`
  (`ShipQueries.cpp:249`), `BuildShipRegistryWindow`, and every write.
- Changing which frame any site reads.
- Any other change to `FindAttackTarget` or to the Combat functions.
- `AGENTS.md` files: none names the lambda.

## Acceptance criteria

1. No `VisitShip` lambda returning `pRegistryIds[iRow]` remains under
   `Frame/Collections/`; the four sites call `ShipRegistryId`.
2. The diff shows each site passing the frame it read before: `rPreviousFrame`
   at the two `ShipQueries.cpp` sites, `rFrame` at the two Combat sites.
3. The BrokenEngineSandbox client and server build.
4. A replay determinism check through `/agent-harness`, with Fighters,
   Battleships, and enemies fighting, reports no CRC mismatch. The originating
   analysis ran nothing, so it records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: a new
  declaration in a shared game header on a path that feeds the per-tick CRC,
  more than Tier 1's "no public signature or invariant exposure" allows. The
  values read are unchanged, so no determinism/CRC surface changes.
- Exposure: determinism/CRC only through the source-frame caveat above. No
  serialization, `.pack`, `kiVersion`, wire, replay-format, threading,
  allocation, or shader exposure.
