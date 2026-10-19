<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-19T03:03:48.409Z","dependsOn":[]} -->
# Keep a transferred blaster's wind-trail tuning by firing ship type

## Context

Each ship type spawns its blasters with its own wind-trail tuning: Fighters
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp:434-436`)
and Battleships (`Battleships/Battleships.cpp:405-407`) use the
`gWindDepositFightersBlasters*` wrappers, Omnis (`Omnis/OmnisCombat.cpp:280-282`)
the `gWindDepositOmniBlasters*` wrappers.

`BlastersPostRender::Transfer`
(`Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp:139-152`)
resets a transferred blaster's tuning to wrapper defaults chosen by alignment:
fleet alignment takes the Omni defaults, any other alignment the Fighters
defaults. Before the three-type split this matched, because every fleet ship
was an Omni and every random ship a Spaceship. Now any type can have either
controller, so a fleet Fighter or Battleship shot changes its trail length
multiplier from 2.0 to 3.0 when it crosses a cell, and a random Omni shot from
3.0 to 2.0 (`Projects/BrokenEngineSandbox/Source/Ui/WindDepositsWrappers.cpp:16,20`;
width 1.0 and intensity 0.05 are equal in both sets). The split only renamed
the alignments at this site, so the mismatch is outside its approved scope.

Exposure: the three values feed only the client-only `ClientMembers()` columns
`pfWindTrailIntensities`, `pfWindTrailWidths`, `pfWindTrailLengthMultipliers`
(`Blasters/Blasters.h:54`), which the CRC excludes. They travel in
`BlastersPostRender::SpawnInfo` `Fields()`, whose layout does not change, so no
`kiProtocolVersion`, `Frame::kiVersion`, or replay version bump. Server and
client read the same `mfDefault` constants, so both compute equal values.

## Design

The author recommends keying the defaults on the blaster's registered type,
which each ship type already owns and the transfer already carries
(`puiTypeIndices`, read as `BlastersInterpolate::sTypes.at(...)` in
`Blasters.cpp`):

- Add three `engine::Wrapper*` members to `BlastersType` (`Blasters.h`) for
  the wind-trail intensity, width, and length-multiplier wrappers, mirroring
  the light types' `p...Wrapper` members.
- Set them in the three blaster registrations: `RegisterFighterBlasterType`
  (`Fighters.cpp`) and `RegisterBattleshipBlasterType` (`Battleships.cpp`) to
  the `gWindDepositFightersBlasters*` wrappers, the Omni blaster registration
  (`Omnis/Omnis.cpp`) to the `gWindDepositOmniBlasters*` wrappers — the same
  sets each type's spawn site uses.
- In `BlastersPostRender::Transfer`, read the row's type and take each
  wrapper's `mfDefault`; remove `bFleetAlignment`.

Rationale: the type index survives transfer and is present on server and
client, while the tuning columns are client-only, so the server cannot copy the
row's current values.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Blasters/Blasters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Omnis/Omnis.cpp`

## In scope

- `BlastersType`: the three wrapper-pointer members.
- `RegisterFighterBlasterType`, `RegisterBattleshipBlasterType`, and the Omni
  `BlastersInterpolate::RegisterType` call: set those members.
- `BlastersPostRender::Transfer`: the three `fWindTrail*` initializers and the
  `bFleetAlignment` local.

## Out of scope

- The spawn sites' `mfCurrent` reads and every wrapper value.
- Any other blaster, light, or alignment behaviour; `SpawnInfo` layout.
- The Tweaks screen labels in `TweaksScreenWindDeposits.cpp`.

## Acceptance criteria

- Code reading: `Transfer` picks each default from the row's `BlastersType`,
  and each registration names the wrapper set its spawn site uses.
- `/agent-harness`: a fleet of only Fighters and Battleships fighting random
  spawns; client `describe_scene` with `unitTypes` `["blaster"]` over the origin
  and its neighbours, sampled until a fleet-alignment blaster row appears
  outside the origin (up to 60 s). Expected: every fleet-alignment blaster row
  in every cell carries `windTrailLengthMultiplier` 2.0. If none crosses within
  60 s, the code reading settles it.
- Client and server build clean.

## Notes

- Change Workflow tier: Tier 2 — one subsystem's client-visible runtime
  behaviour; no CRC'd state, wire layout, save, or replay format changes.
- Client visual only; no determinism exposure.
