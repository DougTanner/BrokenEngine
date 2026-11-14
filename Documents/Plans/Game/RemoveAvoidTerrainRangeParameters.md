<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:11:52.480Z","dependsOn":[]} -->
# Remove the iStart and iEnd parameters from AvoidTerrain

## Context

Paths are under `Projects/BrokenEngineSandbox/Source/Frame/Collections/`.
Verified against commit `022915b6`.

`BattleshipsPostRender::AvoidTerrain` and `FightersPostRender::AvoidTerrain`
end their parameter lists with `int64_t iStart, int64_t iEnd` and loop
`for (int64_t i = iStart; i < iEnd; ++i)`
(`Battleships/BattleshipsNavigation.cpp:16`, `:21`;
`Fighters/FightersNavigation.cpp:16`, `:21`; declarations
`Battleships/Battleships.h:291`, `Fighters/Fighters.h:303`).

Each has one caller, the end of its own PostRender `Update`, which passes `0`
and the collection's own row count (`Battleships/Battleships.cpp:558`,
`Fighters/Fighters.cpp:745`). The body already holds that collection through
`rFrame`. No dispatch names `AvoidTerrain`, and Omnis has no such function.

Originating gap: an architecture review of the Battleships collection; unmet
criterion "no parameter that every caller passes as a value the callee already
has".

## Design

Recommended: delete both parameters from the declaration, the definition, and
the call in both collections, and loop from 0 to the collection's `iCount`,
read from the reference the body already has. Keep the two collections
parallel.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/BattleshipsNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Battleships/Battleships.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.h`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/FightersNavigation.cpp`
- `Projects/BrokenEngineSandbox/Source/Frame/Collections/Fighters/Fighters.cpp`

## In scope

- `BattleshipsPostRender::AvoidTerrain` and `FightersPostRender::AvoidTerrain`:
  the declaration and definition parameter lists and the loop bounds.
- The `AvoidTerrain` call at the end of `BattleshipsPostRender::Update` and
  `FightersPostRender::Update`: the last two arguments.

## Out of scope

- The rest of both `AvoidTerrain` bodies and their other parameters.
- Any other call or parameter in either `Update`.
- `AGENTS.md` files: none describes these parameters.

## Risk tier

Tier 2 — scoped behavior. Trigger: a member function signature declared in a
header changes; that rules out Tier 1. The loop covers the same rows as before,
so no determinism/CRC, layout, wire, version, or threading exposure.

## Acceptance criteria

- No `iStart` or `iEnd` identifier remains in the six files, and each
  `AvoidTerrain` takes three parameters at its declaration, definition, and
  call.
- The client and server build without errors or new warnings.

## Notes

The row count the caller passes and the count the body reads are the same
object: `Update` adds and removes no row before the call.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
