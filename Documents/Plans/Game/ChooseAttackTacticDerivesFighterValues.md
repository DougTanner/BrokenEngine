<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:09:42.995Z","dependsOn":[]} -->
# Derive the Fighter tier and highest plane inside ChooseAttackTactic

## Context

Paths are relative to `Projects/BrokenEngineSandbox/Source/`; `Ships/` stands
for `Frame/Collections/Ships/`. Line numbers are as of commit `fe1fb69b`.

`ChooseAttackTactic` (`Ships/FleetCoordination.h:86`, defined at
`Ships/FleetCoordination.cpp:310`) takes `iClearanceTier` and
`iHighestFlightPlane` as parameters. All three call sites pass the Fighter
values:

- `Frame/Collections/Fighters/Fighters.cpp:661` and `:680` pass
  `ShipTypeClearanceTier(ShipType::kFighter)` and
  `kiFighterHighestFlightPlane`.
- `ChooseFighterFleetTactic` (`FleetCoordination.cpp:291`) passes its own
  `ShipTypeClearanceTier(ShipType::kFighter)` (`:287`) and
  `HighestFlightPlane(ShipType::kFighter)`, which is the same plane.

The body is Fighter-only whatever is passed: `FindSquadronTarget` reads
`pFighters` directly (`FleetCoordination.cpp:168-169`), and `kVeerOff`,
`kSharedTarget`, and `kSeekAndDestroy` are Fighter-row settings. So the two
parameters suggest a ship-type choice the body cannot honour: another type
passing its own values would still pick targets from Fighter rows.

Originating gap: a cohesion finding of the architecture phase of a deep
analysis of `Ships/` (the directory, not its subdirectories) at commit
`fe1fb69b`. It is pre-existing debt, not an unmet acceptance criterion of any
change. The verifying review confirmed it; nothing was built or run.

## Design

Recommended: remove both parameters and derive the values inside
`ChooseAttackTactic` as its sibling `ChooseFighterFleetTactic` does, with
`ShipTypeClearanceTier(ShipType::kFighter)` and
`HighestFlightPlane(ShipType::kFighter)`. Rationale: one concrete variant
exists at three call sites, and deriving the same values inside is
bit-identical.

Caveat the executing session preserves: the own-tier sight retest in
`ChooseAttackTactic` (`FleetCoordination.cpp:338-342`) stays unconditional.
`ChooseFleetTactic` skips its retest when the type's tier equals
`kiFiringClearanceTier` (`:260`); that skip is keyed by `eType`, stays where it
is, and is not copied into `ChooseAttackTactic`. The retest is live because the
Fighter's clearance tier is not `kiFiringClearanceTier`
(`Frame/TerrainUtils.h`, `ShipTypeClearanceTier`), so there is no dead branch
to remove.

The declaration comment (`FleetCoordination.h:79-85`) names both parameters and
is reworded to name the Fighter's tier and highest plane instead.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Ships/FleetCoordination.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`

## In scope

- `ChooseAttackTactic`: its declaration and comment in `FleetCoordination.h`,
  its definition's parameter list, and the two derived locals in its body.
- The `ChooseAttackTactic` call in `ChooseFighterFleetTactic`
  (`FleetCoordination.cpp:291`) and the two calls in `Fighters.cpp` (`:661`,
  `:680`): the two arguments only.

## Out of scope

- `ChooseFleetTactic`, including its tier-equality skip at
  `FleetCoordination.cpp:260`.
- `FindSquadronTarget`, `FindAttackTarget`, and their `iHighestFlightPlane`
  parameters. `FindAttackTarget` has callers of other ship types;
  `FindSquadronTarget` is file-static and its one caller is
  `ChooseAttackTactic`.
- Any change to which tier or plane a Fighter uses.
- `AGENTS.md` files: none names the two parameters.

## Acceptance criteria

1. `ChooseAttackTactic` has no `iClearanceTier` or `iHighestFlightPlane`
   parameter at its declaration, definition, and three calls.
2. The diff adds no tier-equality test to `ChooseAttackTactic`.
3. The BrokenEngineSandbox client and server build.
4. A replay determinism check through `/agent-harness`, with random-spawn
   Fighters and Seek and Destroy or Same Target fleet Fighters fighting,
   reports no CRC mismatch. The originating analysis ran nothing, so it
   records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.

## Notes

- Change Workflow tier: Tier 2 (the author's classification). Trigger: the
  signature of a function declared in a shared game header changes, on a path
  that feeds the per-tick CRC, more than Tier 1's "no public signature or
  invariant exposure" allows. The values used are unchanged, so no
  determinism/CRC surface changes.
- Exposure: determinism/CRC only through the retest caveat above. No
  serialization, `.pack`, `kiVersion`, wire, replay-format, threading,
  allocation, or shader exposure.
