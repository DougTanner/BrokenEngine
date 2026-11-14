<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T17:16:38.568Z","dependsOn":[]} -->
# Reject non-finite Omni armor and shield in PostRead

## Context

Line numbers are from commit `106613c6`; locate by symbol if they moved. Paths
without a root are under
`Projects/BrokenEngineSandbox/Source/Frame/Collections/`.

`OmnisPostRender::PostRead` (`Omnis/Omnis.h:227-260`) tests neither `pfArmors`
nor `pfShields`. Fighters and Battleships reject a non-finite health at the
same boundary, each with a stated reason (`Fighters/Fighters.h:186`,
`:199-202`; `Battleships/Battleships.h:179`, `:192-195`).

No other check at that read boundary covers the two columns:
`ValidateAfterRead` tests only type indices and then calls `PostRead`
(`Engine/Source/Frame/Collections/AGENTS.md`
`## Identity, Serialization, and CRC`), and the game frame read tests only the
spawn timer and a position height
(`Projects/BrokenEngineSandbox/Source/Frame/Frame.cpp:13`, `:100`).

Reachable effect of a save that holds such a value, traced in code and not run
live:

- NaN armor fails every death test (`Omnis/OmnisCombat.cpp:155`, `:174`,
  `:207`), so the Omni never dies.
- `std::min` keeps a NaN first argument, so a healing row with NaN armor or
  shield never passes the test at `Omnis/Omnis.cpp:725`, never clears
  `kHealing`, and never fires again (`Omnis/OmnisCombat.cpp:235`).
- A +inf shield absorbs all damage for good (`Omnis/OmnisCombat.cpp:66-68`),
  and NaN survives regeneration (`Ships/ShipBehaviours.cpp:1050`).

Authority: root `AGENTS.md` `## Key Patterns` (the server fully validates
every grid save) and the bad-value rule in
`.agents/references/cpp-conventions.md` (reject, never clamp).

Originating gap: the architecture phase (trust-boundary lens) of a
findings-only analysis of the `Omnis` directory, confirmed by an independent
verification read. It is pre-existing debt, not an unmet acceptance criterion
of any change. Unmet criterion: a grid save with non-finite Omni armor or
shield fails the load.

## Design

Recommended (the author's recommendation): add one `std::isfinite` test for
`pfArmors` and one for `pfShields` to `OmnisPostRender::PostRead`, each
throwing `std::ios_base::failure` in the form the neighbouring tests use,
mirroring the Fighters and Battleships health test. Extend the comment above
`PostRead` with the reason, and add the rejection to
`Omnis/AGENTS.md` `## State Boundaries`.

Rationale: a finite-only test cannot reject a value the simulation writes.
Armor and shield start at their maximums and only take finite subtractions and
capped additions (`Omnis/Omnis.cpp:301-302`, `:723-724`;
`Omnis/OmnisCombat.cpp:66-67`, `:94`, `:122`). A tighter range test is not
needed and is not part of this Plan.

Risk tier: Tier 2 (the author's classification). Trigger: tightening a check
inside one unit at an existing read boundary, with the format and the trust
unchanged. Exposure: no CRC, wire, save layout, or `kiVersion` change; a save
that holds a non-finite value is now rejected at load, as the bad-value rule
requires.

## Critical files

- `Omnis/Omnis.h`
- `Omnis/AGENTS.md`

## In scope

- `OmnisPostRender::PostRead`: the two finite tests and the comment above the
  function.
- `Omnis/AGENTS.md` `## State Boundaries`: one bullet for the rejection.

## Out of scope

- A range test on armor or shield, and any other `PostRead` check.
- A shared `PostRead` helper across the three ship types.
- Any format or version change.
- `OmnisInterpolate::PostRead`, Fighters, and Battleships.

## Acceptance criteria

1. `OmnisPostRender::PostRead` throws for a row whose `pfArmors` or
   `pfShields` value is NaN or infinite; the diff settles this.
2. `/compile` builds the BrokenEngineSandbox client and server.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
