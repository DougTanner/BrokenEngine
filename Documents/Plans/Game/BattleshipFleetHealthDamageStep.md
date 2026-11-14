<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:11:42.005Z","dependsOn":[]} -->
# Give the Battleship fleet-health damage step one writer

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/Frame/Collections/`.
Verified against commit `022915b6`.

`Battleships/BattleshipsCombat.cpp` writes the same damage step three times:
subtract the damage from `pfHealths`; at 0 or below, hold a fleet-controlled
row at 0 and start no explosion, otherwise call `BeginExplosion`.

- `BattleshipsPostRender::ApplyBeamDamage`, lines 56-67.
- `BattleshipsPostRender::PostCollision`, lines 130 and 140-150, with the hit
  sound and hit flash between the subtraction and the test.
- `BattleshipsPostRender::AreaDamage`, lines 187-198.

The three behave identically; they differ only in how each leaves its
surrounding loop (`return`, `continue` or `break`, `continue`). The rule is
`Battleships/AGENTS.md` `## Fleet Health Invariants`, stated once there and
written three times in code, against the root `AGENTS.md` DRY directive.

Originating gap: an architecture review of the Battleships collection; unmet
criterion "the fleet-health rule has one writer".

## Design

Recommended: one file-local function beside `BeginExplosion` that takes the
frame, the emitter coordinate, the row, and the damage, applies the step, and
reports whether the row began its explosion; all three sites call it.

- `PostCollision` keeps its sound and hit flash and still leaves the collision
  loop with `break` once the row is dying. The sound and flash run before the
  call to the new function. They are `BT_CLIENT`-only and write no simulation
  state, so the simulation order is unchanged.
- A fleet-controlled row held at 0 keeps iterating its remaining collision
  results, as it does now.
- Do not route `AreaDamage` through `ApplyBeamDamage`: it would need a dummy
  shooter position.

The order of health writes, the `BeginExplosion` call, and every random draw
and UUID mint stays as it is.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsCombat.cpp`

## In scope

- A new file-local damage function next to `BeginExplosion`.
- The damage steps in `BattleshipsPostRender::ApplyBeamDamage`,
  `PostCollision`, and `AreaDamage`.

## Out of scope

- Fighters: its three sites differ in damage direction
  (`Fighters/FightersCombat.cpp:215`, `:276`, `:310`), and fleet health is a
  documented Battleships-only difference (`Battleships/AGENTS.md`, opening
  paragraph).
- Omnis: it already shares its step (`ApplyDamage` and `HandleArmorDepleted`,
  `Omnis/OmnisCombat.cpp`).
- `BeginExplosion`, the `ApplyBeamDamage` signature, the cell-exit transfer
  test in `PostCollision`, and the `AreaDamage` query.
- `AGENTS.md` files: the rule text is already correct.

## Risk tier

Tier 2 — scoped behavior. Trigger: it restructures the control flow of
simulation code that writes CRC'd health and lifecycle state, inside one file,
with behavior meant to be unchanged; a mistake would alter CRC'd state. No
layout, wire, version, or threading change.

## Acceptance criteria

- The subtract, clamp, and `BeginExplosion` step appears once in
  `BattleshipsCombat.cpp`.
- The client and server build.
- A replay determinism check on a scene where fleet and random-spawn
  Battleships take blaster, missile, and beam damage passes with no CRC
  mismatch, and the fleet Battleship's health holds at 0. The supplied evidence
  records no harness setup; the executing session builds one through
  `/agent-harness`.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
