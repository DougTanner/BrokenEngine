<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:49:08.801Z","dependsOn":[]} -->
# Reject non-integral agent coordinates before server command routing

## Context

The accepted finding `CAI/shard-0044/001` identifies a server agent JSON trust
boundary gap.  `CoordFromParam` checks only that `coord` is an array of two
values, then calls `get<int32_t>()` without requiring an integral JSON value or
checking that it is representable (`Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:563-572`).
The vendored nlohmann conversion routes numeric values through an arithmetic
`static_cast` (`ThirdParty/tinygltf/json.hpp:3976-3993`), so `[0.5,0]` becomes
`(0,0)`.  `BuildInjectedChange` checks the already-converted value for an
active cell and can mint an ID and queue a `SpawnPlayer`
(`AgentCommandsServer.cpp:763-796`); `QueryFrame` and the replay-transfer
fixture use the same helper (`AgentCommandsServerQueries.cpp:132-146`;
`AgentCommandsServer.cpp:650-655`).  The client-side parser's explicit
integer check is not on these server paths.

The source finding is recorded at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0044.md:56`
and the consolidated selector at
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:1047`.
All assigned source and authority hashes match frozen baseline
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; the missing validation is
pre-existing, unresolved, and outside the audit work.

Impact: an otherwise syntactically valid command can mutate or query a
different authoritative cell while returning success.

## Design

Author's recommendation: make `CoordFromParam` accept only signed or unsigned
integral JSON values that fit the `int32_t` `GridCoord` domain before either
conversion occurs.  Return the existing command-validation failure for a
fractional, nonrepresentable, or nonnumeric coordinate, and keep the one
helper on every injection, transfer-fixture, and frame-query path.  Preserve
the active-cell check and valid integral coordinate behavior after validation;
do not change the client parser or the vendored JSON library.

## Critical files

- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServer.cpp:563-572,650-655,763-796` — shared coordinate parser and mutation/fixture callers.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:132-146` — externally selected frame query.
- `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` — hostile JSON validation contract.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-server.md:31-35` — coordinate command contract.

## In scope

- Integral-type and `int32_t`-representability validation in `CoordFromParam`
  before `GridCoord` construction.
- Existing validation-error publication for all current server callers of the
  helper, with no ID minting or status-queue mutation on invalid input.
- The shared server agent command/query boundary named above.

## Out of scope

- Client-side coordinate parsing, the vendored nlohmann header, position bounds,
  active-cell policy, or status-change wire layout.
- Transfer-fixture semantics, frame-query pagination, or unrelated agent
  parameter validation except where the shared helper is required.
- Repairing a narrowed coordinate after conversion or adding compatibility for
  fractional coordinates.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3).  Trigger: hostile
agent JSON crosses a trust boundary into authoritative cell selection and
status-queue mutation.

Tier rationale: the fix adds one pre-specified type and range check inside the
single shared `CoordFromParam` helper and returns the command-validation
failure that already exists; valid integral coordinates, the wire format, and
all CRC/replay data are untouched.

Preserve these invariants:

- Every accepted agent coordinate is a complete, representable integral
  `GridCoord` before lookup, query selection, transfer routing, or mutation.
- Invalid coordinates fail before ID allocation or queue changes; valid integral
  coordinates retain current active-cell and response behavior.
- No simulation CRC, replay, save, or wire layout changes.

## Acceptance criteria

- `query_frame` and `inject_status_changes` reject fractional values such as
  `[0.5,0]`, values below/above the `int32_t` domain, and nonnumeric values
  before extracting a frame or minting/queuing a status change.
- Negative and both `int32_t` boundary values are accepted or rejected solely
  according to the documented active-cell rule, with no narrowing.
- A valid integral coordinate continues to query and inject the same cell.
- Server `Debug|x64` builds clean through `/compile`; the existing agent command
  scenario proves invalid requests leave ID and queue state unchanged.

## Notes

The consolidated index records external proposition `CAI-EXT-014` for the
vendored conversion behavior.  The accepted repository evidence already shows
the conversion call chain; it does not create a separate Plan for that
proposition.
