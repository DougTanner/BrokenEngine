<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-25T21:35:23.235Z","dependsOn":[]} -->
# Correct the cross-cell recipe's out-of-range coordinate probe

## Context

`Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md` step 9
lists the probe `set_client_grid_coord {"coord":[1000001,0]}` as failing "one
past the command limit". No such limit exists. `ClientGridCoordValue`
(`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsClient.cpp`) rejects
only values that do not fit in `int32`, and its comment states the intent:
every signed-int32 coordinate identifies a usable cell. The
`commands-client.md` `set_client_grid_coord` entry says the same.

Observed in a live harness run (session executing
`Documents/Plans/Engine/SubscriptionPlaceholderIndexMismatch.md`, 2026-09-25):
the probe returned `{"ok":true,"result":{"clientGridCoord":[1000001,0]}}`, and
the server logged `Server::ClientSubscribe Rejected (not adjacent) Client: 1
Coord: (1000001,0)` on each re-sent subscribe. The probe therefore behaves like
the step's unauthorized `[100,100]` probe, not like a command failure.

The authority order favors the code: the handler comment and
`commands-client.md` agree, and only the recipe line disagrees. The wording
predates that session's change and is unrelated to it.

## Design

The author recommends deleting the `[1000001,0]` probe from step 9 rather than
rewording it. Rationale: as an accepted non-adjacent cell it only repeats the
`[100,100]` probe, and the `[2147483648,0]` probe already covers the
command's one real rejection.

## Critical files

- `Projects/BrokenEngineSandbox/Documents/AgentHarness/cross-cell.md` — step 9.

## In scope

- The `{"coord":[1000001,0]}` clause of step 9 in `cross-cell.md`.

## Out of scope

- `ClientGridCoordValue`, `CommandSetClientGridCoord`, and all other source.
- `commands-client.md` and the other steps of `cross-cell.md`.

## Risk

Change Workflow Tier 1, trigger: documentation-only edit with no behavior,
signature, or invariant exposure. Bounded.

## Acceptance criteria

1. Step 9 lists no probe that the command accepts as a failure, and every
   remaining probe's stated outcome matches `ClientGridCoordValue` and
   `commands-client.md`.
