<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:16:47.585Z","dependsOn":[]} -->
# Give SyncFighterBeam a half-width parameter and one declaration

## Context

Line numbers are from commit `106613c6`; locate by symbol if they moved. Paths
without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`.

`SyncFighterBeam` is defined in `Fighters/Fighters.cpp:136-152` under
`BT_CLIENT` and no header declares it. It is declared by hand at
`Fighters/FightersCombat.cpp:35` and at `Omnis/Omnis.cpp:72`. The second
crosses from one collection into another, and it is the only hand declaration
under `Collections/` that does so: `giBattleshipExplosionTypeIndex`, which
`Wrecks/Wrecks.cpp` reads, is declared under `BT_CLIENT` in
`Battleships/Battleships.h`.

It has three callers. `Fighters/Fighters.cpp:349` and
`Fighters/FightersCombat.cpp:136` pass `giFighterBeamAreaLightTypeIndex`;
`Omnis/Omnis.cpp:492` passes `siHealBeamAreaLightTypeIndex` for the Omni heal
beam.

The function picks its width by comparing the type index
(`Fighters/Fighters.cpp:144`): the Fighter index takes half of
`gFighterBeamWidth`, anything else takes `kfFighterBeamHalfWidth`
(`Fighters/Fighters.cpp:74`, no other reader). So the else arm and that
constant serve only the Omni heal beam: `Fighters.cpp` holds an Omni-only
width under a Fighter name, chosen by a type-index comparison.

Originating gap: the architecture phase (dependency and duplication lenses) of
a findings-only analysis of the `Omnis` directory, confirmed by an independent
verification read. It is pre-existing debt. Unmet criterion: the Omni heal
beam's width is owned by Omnis, and the cross-collection call has one
declaration.

## Design

Recommended (the author's recommendation):

- Add a half-width parameter to `SyncFighterBeam` and remove the type-index
  comparison. The two Fighter callers pass half of
  `gFighterBeamWidth.mfCurrent`; the Omni caller passes a constant Omnis owns,
  holding the value `kfFighterBeamHalfWidth` has today.
- Delete `kfFighterBeamHalfWidth`.
- Declare the function once in `Fighters/Fighters.h` under `BT_CLIENT` and
  delete both hand declarations. `Omnis.cpp` already sees `Fighters.h` through
  `Ships/ShipHitboxes.h`.

Rationale: the explicit parameter removes the branch and puts the Omni width
with the other Omni heal-beam constants. The function stays in `Fighters.cpp`:
two of the three ship types use it and `Ships/` has no client-only visual file
today. `kfFighterBeamStartOffset` stays where it is, since all callers share
that geometry.

Risk tier: Tier 2 (the author's classification). Trigger: a signature change
exposed in a header and used across two collections. Exposure: the beams are
client-only visuals outside the CRC (`Omnis/AGENTS.md` `## Heal Beam`), so
there is no determinism, wire, layout, or version change, and both beams keep
their current widths. `Fighters.h` is in the game precompiled-header closure.

## Critical files

- `Fighters/Fighters.h`
- `Fighters/Fighters.cpp`
- `Fighters/FightersCombat.cpp`
- `Omnis/Omnis.cpp`

## In scope

- `SyncFighterBeam`: its signature, its width selection, and its three call
  sites.
- `kfFighterBeamHalfWidth` and the new Omni-owned half-width constant.
- The declaration in `Fighters/Fighters.h` and the hand declarations at
  `Fighters/FightersCombat.cpp:35` and `Omnis/Omnis.cpp:72`.

## Out of scope

- Moving `SyncFighterBeam` to `Ships/` or renaming it.
- `kfFighterBeamStartOffset`, the beam fade, and the heal-beam pulse.
- The other hand declarations in `FightersCombat.cpp` and `Omnis.cpp`, which
  stay inside their own collection.
- Any change to either beam's drawn width.

## Acceptance criteria

1. `SyncFighterBeam` has one declaration, in `Fighters/Fighters.h`, and no
   type-index comparison; `kfFighterBeamHalfWidth` is gone; the diff settles
   this.
2. `/compile` builds the BrokenEngineSandbox client and server.
3. Through `/agent-harness`, a client screenshot shows a Fighter beam and an
   Omni heal beam drawn as before. The supplied evidence records no harness
   setup; the executing session picks one.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
