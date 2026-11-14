<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:43:52.626Z","dependsOn":[]} -->
# Test the flagship's wanted cell once in TickFleetTimers

## Context

Found by a deep analysis of `Projects/BrokenEngineSandbox/Source/Network`
(in-function phase), verified by code read against commit `4cf9d607`.

`FleetNavigationController::TickFleetTimers`
(`Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp:62`)
tests `rFlagship.coordinate == rFleet.wantedCoordinate` twice per fleet: at
`:78` to gate the timer drain, and negated at `:89` to `continue`, under a
comment (`:88`) about a cross-cell move between ticks. Nothing between the two
tests can change either coordinate, so:

- when the flagship is not at its wanted cell, the code drains nothing and
  leaves through the `continue` at `:85` or the one at `:91`, with no write to
  the fleet, no fleet RNG draw, and no frame input change;
- when it is at its wanted cell, the test at `:89` is always false.

One leading not-at-wanted-cell `continue` followed by an unconditional drain
and the existing `> 0.0f` test therefore behaves identically, including for a
NaN timer, and leaves the drain order and the draw order per fleet unchanged.
`Documents/C++StyleGuide.txt` rule 62 asks for early returns in guard
sequences.

Exposure: this function feeds the fleet RNG and the fleet checksum
(`Projects/BrokenEngineSandbox/Source/Network/Server/AGENTS.md`
`## Deterministic Tick Contracts`,
`Projects/BrokenEngineSandbox/Source/Save/AGENTS.md` `## Replay Contract`).
The change must leave both bit-identical.

## Design

In the per-fleet loop of `TickFleetTimers`, after the dead-flagship
`continue`:

1. Add one `continue` when the flagship is not at `rFleet.wantedCoordinate`.
2. Make the `cellChangeTimerSeconds` drain unconditional.
3. Keep the `> 0.0f` `continue`.
4. Delete the second coordinate test with its `continue` and its comment.
5. Keep the content of the comment above the drain (drain only while the
   flagship is at its wanted cell; drain there even in cross-cell mode; one
   tick per pass), placed with the new guard and the drain.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Network/Server/FleetNavigationController.cpp`

## In scope

- `FleetNavigationController::TickFleetTimers`: the lines from the comment
  above the drain through the second coordinate test, as `## Design` lists.

## Out of scope

- The rest of `TickFleetTimers` (the pick logic, the member loop, the log),
  `FlagshipReachesEdge`, and `ReachableNeighbourDirections`.
- Any change to when a timer drains, when a fleet picks, or how many fleet
  RNG draws a tick makes.
- Turning `FleetNavigationController` into a free function (judged cosmetic).

## Risk tier

Tier 2 — scoped behavior. Trigger: the function is on a determinism path (it
feeds the fleet RNG and the fleet checksum), which Tier 1's "no invariant
exposure" excludes; it stays Tier 2 because the change does not alter what
that path computes.

## Acceptance criteria

- Code read: `TickFleetTimers` compares the flagship's coordinate with
  `wantedCoordinate` once per fleet, before the drain.
- `/compile` builds the BrokenEngineSandbox server.
- The repository static checks pass.
- Harness: the replay determinism acceptance in
  `.agents/skills/agent-harness/references/replay.md`
  `### Replay determinism acceptance`, over a recording in which at least one
  fleet's flagship picks a new cell, shows no frame CRC mismatch and no fleet
  checksum mismatch. The supplied evidence records no harness setup for such
  a recording.

## Notes

`Documents/Plans/Game/FleetOwnedSquadronState.md` edits the tail of the same
function (the member loop after a pick), not these lines; no ordering is
needed between the two.
