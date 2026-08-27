<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:52:18.338Z","dependsOn":[]} -->
# Recover cleanly when ENet host construction fails

## Context

The retained survivor `CAI/shard-0035/002` identifies a failed-client
construction state. `Client::Client` publishes `gpClient = this` before calling
`enet_host_create` and returns normally with `mpHost == nullptr` at
`Engine/Source/Network/Client/Client.cpp:21-40`. `ClientSessionRuntime::Connect`
still stores that object at `:120-128`; `Client::Poll` returns early for the
null host at `:146-154`, so no disconnect cleanup occurs. The menu derives
`kClientPresent` from the non-null client at
`Projects/BrokenEngineSandbox/Source/Game.cpp:490-500`, and a retry constructs
another client before the first is destroyed.

The direct evidence is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0035.md:85`
and consolidated selector `Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:930`. Frozen/live source
identity matches baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this
routing session has not changed source. A null result from the third-party
host constructor is an explicit failure result, not an impossible game state.

## Design

The author's recommendation is to complete ENet host construction before
publishing `gpClient`, propagate a failed construction through
`ClientSessionRuntime::Connect`, and leave no unusable `mpClient`/global behind
for the menu to treat as present. Keep ordinary remote rejection and later
ENet disconnects on their existing cleanup paths, and preserve the singleton
assertion as an internal invariant after failed construction has been handled.

## Critical files

- `Engine/Source/Network/Client/Client.cpp:21-40,120-154` — global
  publication, host creation, polling, and destruction.
- `Engine/Source/Network/Client/ClientSessionRuntime.cpp:120-148,205-231`
  — ownership and failure/disconnect cleanup.
- `Projects/BrokenEngineSandbox/Source/Game.cpp:490-500,614-623` — client
  presence model and retry action.
- `Engine/Source/Network/Client/AGENTS.md` — peer/runtime lifetime contract.

## In scope

- Null-host construction failure propagation and cleanup before the client is
  published as present.
- The normal retry path after a failed construction.
- Existing successful-connect, remote-disconnect, and singleton-lifetime
  behavior.

## Out of scope

- ENet library changes, socket buffer tuning, remote-server discovery, or
  protocol handshakes.
- Menu redesign and unrelated subscription/reconciliation failures.
- New retry backoff or an alternate client ownership architecture.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: this crosses a third-party network
resource result, global peer identity, unique ownership, and client session
lifetime.

Preserve these invariants:

- `mpClient != nullptr` denotes a usable client object or an explicitly
  transitioned failure state, never a permanently null-host transport.
- A failed host creation leaves `gpClient` clear and permits the next retry to
  construct one client.
- Successful and remote-disconnected clients retain their current handshake,
  event, and cleanup behavior.

## Acceptance criteria

- A forced/null `enet_host_create` result leaves no client-present state and the
  normal menu retry can construct a fresh client without an assertion or
  process termination.
- Remote connection refusal/disconnect still follows the existing cleanup and
  retry behavior.
- No stale `gpClient` remains after a failed construction or retry.
- Client and server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `CAI/shard-0035/002`; source selector is the shard line above and the
consolidated selector is `consolidated-index.md:930`. No source fix or build
was performed during routing.
