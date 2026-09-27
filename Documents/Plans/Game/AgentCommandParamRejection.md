<!-- broken-engine-plan/v1 {"createdUtc":"2026-09-27T15:48:14.891Z","dependsOn":[]} -->
# Reject out-of-range agent command parameters instead of clamping them

## Context

The `.agents/references/cpp-conventions.md` bullet "Never fix a bad value
automatically" says a value read from a network packet or a file that a check
finds invalid rejects the input and is never clamped, substituted, or
normalized. The user decided that bad values from any external or
trust-boundary input are rejected, never fixed. The change that added that
bullet applied the rule to one agent helper, `NavigationDelayFromParam`
(`Projects/BrokenEngineSandbox/Source/Agent/Commands/ServerSimulationFixtures.cpp`),
which now fails the command instead of clamping, and left the other agent
command clamps out of its change. They still fix bad values (line numbers at
`46241779`, re-derive at claim):

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp:237` — `screenshot`
  `quality` is clamped to `[1, 100]`.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp:19-35`
  — `OptionalCount` reads `offset`/`limit` unchecked and `ClampWindow` lifts a
  negative `offset` to 0 and a negative `limit` to 0 (`query_players`,
  `query_collection`).
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp:136` —
  `describe_scene` `maxUnits` is floored at 0.

`Documents/Plans/Engine/EnetChecksumTrustPolicy.md` records the user's
decision that the agent command channel is an exempt developer tool whose
handlers need not range-check arguments. That exemption governs whether a check
is required; these handlers already check, and the new convention governs what
a check does when it finds a bad value. `Engine/Source/Agent/AGENTS.md`
`## Architecture` already routes invalid external input through a handler
throw, which `Drain()` turns into the protocol failure envelope.

## Design

Author's recommendation, mirroring `NavigationDelayFromParam`: replace each
clamp with a `std::runtime_error` throw naming the command, parameter, and
valid range, so the command returns `ok:false` before any side effect.

- `screenshot`: a present `quality` outside `1..100` throws; valid values pass
  through unchanged.
- `query_players` / `query_collection`: a present negative `offset` or `limit`
  throws in `OptionalCount` (or its callers). `ClampWindow` keeps bounding the
  window to the live count for non-negative input, because an offset or limit
  past the end of the collection is ordinary paging that yields a short or
  empty page, not an invalid value; drop its now-dead negative handling.
- `describe_scene`: a present negative `maxUnits` throws; `0` stays valid
  (`.agents/skills/agent-harness/scripts/Wait-IslandSceneReady.ps1` sends it).

## Critical files

- `Engine/Source/Agent/AgentCommandsClientGeneric.cpp` — screenshot handler.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentCommandsServerQueries.cpp`
  — `OptionalCount`, `ClampWindow`.
- `Projects/BrokenEngineSandbox/Source/Agent/AgentScene.cpp` —
  `CommandDescribeScene`.
- `Projects/BrokenEngineSandbox/Documents/AgentHarness/commands-client.md`,
  `commands-server.md`.

## In scope

- The `quality` read in the `screenshot` handler.
- `OptionalCount` and `ClampWindow` in `AgentCommandsServerQueries.cpp`.
- The `maxUnits` read in `CommandDescribeScene`.
- `commands-client.md` `screenshot` (the "Quality range is `1..100`" sentence
  becomes a parameter-error statement) and `describe_scene` (negative
  `maxUnits` is a parameter error); `commands-server.md` `query_players` and
  `query_collection` (negative `offset`/`limit` is a parameter error).

## Out of scope

- Adding range checks to agent parameters that have none today (the
  `EnetChecksumTrustPolicy.md` exemption).
- Past-the-end `offset`/`limit` paging behavior.
- `screenshot` `maxWidth` (`<= 0` is a documented mode, not a clamp).
- `NavigationDelayFromParam` (already rejects) and
  coordinate parsing (`Documents/Plans/Game/AgentCoordinateIntegralValidation.md`).
- AgentHarness transport and response envelope.

## Risk tier and invariants

Expected Change Workflow Tier 2. Trigger: tightening checks inside the agent
command handlers at their existing boundary with the protocol and trust
unchanged (`.agents/references/risk-tiers.md`). Developer channel only; no
simulation CRC, wire, save, replay, or `.pack` exposure.

Invariant: every call with in-range parameters returns exactly what it returns
today.

## Acceptance criteria

- `screenshot` with `quality` 0 or 101, `query_players`/`query_collection` with
  a negative `offset` or `limit`, and `describe_scene` with a negative
  `maxUnits` each return `ok:false` naming the parameter, with no capture
  started.
- The same commands with in-range values, including `maxUnits: 0` and an
  offset past the end, return the same results as before.
- Client and server Debug builds pass `/compile`; one `/agent-harness` session
  exercises one rejected and one accepted call per command.
