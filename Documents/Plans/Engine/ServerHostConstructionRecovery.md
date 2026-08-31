<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-31T01:59:42.587Z","dependsOn":[]} -->
# Fail server startup when ENet host construction fails

## Context

Final survivor `S012-C009` is a retained HIGH server startup finding. `Server::Server` publishes `gpServer`, calls `enet_host_create`, and logs a warning/returns when `mpHost` is null. `ServerSessionRuntime` still stores the object; `Poll` and `Flush` return silently, so simulation and display continue without a listening transport (`Engine/Source/Network/Server/Server.cpp:21-42,62-78`; `ServerSessionRuntime.cpp:23-32`).

The final disposition is in `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-015.md` under `S012-C009 — FINAL: RETAIN_HIGH`; the source report is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-012.md:192` and the consolidated selector is `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:224`. The target remained at baseline `a20acf1e31a24a0f61ae638e8976602b49655788`.

## Design

The author's recommendation is to propagate a null `enet_host_create` result as a startup failure before publishing a usable server singleton or retaining a runtime server object. Use the existing `kError` startup reporting and agent/non-agent policy; preserve normal host creation, polling, discovery, peer lifecycle, and clean teardown.

## Critical files

- `Engine/Source/Network/Server/Server.cpp:21-42,62-143` — host construction and null-host consumers.
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:23-32` — server ownership at construction.
- `Engine/Source/Main.cpp:323-414` — server startup/update caller.
- `Engine/Source/Network/Server/AGENTS.md` and `Engine/Source/AGENTS.md` — host/lifecycle/startup contracts.

## In scope

- Null ENet-host construction failure propagation and cleanup.
- Startup state before `gpServer`/runtime publication and existing failure reporting.
- Valid server construction, poll/flush, discovery, peer, and teardown behavior.

## Out of scope

- ENet library/socket tuning, discovery redesign, retry/backoff, protocol handling, or server simulation policy.
- Client host construction, game Fleet state, or unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: a third-party network resource result controls global server singleton publication, transport lifetime, and startup/update ownership across engine runtime boundaries.

Preserve these invariants:

- A runtime server always owns a usable ENet host; failed construction reaches controlled startup failure.
- No stale `gpServer` or null-host runtime remains after failure.
- Successful server startup, polling, discovery, peer management, and teardown remain unchanged.

## Acceptance criteria

- A forced null `enet_host_create` result logs the existing startup error and exits through the documented server failure policy before the update loop.
- No client can observe a retained null-host server singleton; successful host construction and clean teardown still work.
- Server `Debug|x64` builds pass through `/compile`.

## Notes

Origin: `S012-C009`; disposition selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/dispositions/group-015.md` (`S012-C009 — FINAL: RETAIN_HIGH`), source selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/shard-012.md:192`, and consolidated selector `Temp/CppDirectiveVerificationAudit/012bbf1f-4b21-4e4e-8821-0ab4ff28d520/consolidated-index.md:224`. No exact existing Plan was found; the client host Plan is analogous context only. No source fix or build was performed during routing.
