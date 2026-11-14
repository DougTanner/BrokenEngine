<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:40:15.501Z","dependsOn":[]} -->
# Remove the dead checks in NetworkDiscoveryScanner::IsScanning

## Context

Paths are under `Engine/Source/Network/`. Verified against commit `10d84ca6`;
the lines cited are those at commit `c675b6b2`.

`NetworkDiscoveryScanner::IsScanning` opens with two early returns no caller
can reach:

- The `kStarted` test (`NetworkDiscoveryScanner.cpp:91`).
  `DiscoveryScannerFlags::kStarted` (`NetworkDiscoveryScanner.h:21`) is set
  at `NetworkDiscoveryScanner.cpp:69` and read nowhere else. The only
  construction site calls `StartScan()` on the next line
  (`Client/ClientSessionRuntime.cpp:375-376`), and nothing clears the flag,
  so it is always set when `IsScanning` runs.
- The `kFound` test (`NetworkDiscoveryScanner.cpp:96-99`). The only
  `IsScanning` call (`Client/ClientSessionRuntime.cpp:392`) is the `else` of
  the caller's own `kFound` test (`:386`), with no `Poll` between them.

`.agents/references/cpp-conventions.md` rules out a check for a state no
caller produces.

Originating gap: the in-function refactoring phase of an analysis of
`Engine/Source/Network` at that commit. Unmet criterion: "no check for a
state no caller produces".

## Design

Recommended: delete both early returns, the `kStarted` enumerator, and the
statement that sets it. `IsScanning` then reports only whether the scan
timer has not expired, and relies on its caller having tested `kFound`,
which the one caller does. This is deletion in place; no helper is added.

## Critical files

- `Engine/Source/Network/NetworkDiscoveryScanner.cpp`
- `Engine/Source/Network/NetworkDiscoveryScanner.h`

## In scope

- `NetworkDiscoveryScanner::IsScanning`: the two early returns.
- `NetworkDiscoveryScanner::StartScan`: the statement setting `kStarted`.
- `DiscoveryScannerFlags::kStarted`.

## Out of scope

- `DiscoveryScannerFlags::kFound`, `Poll`, and the scan timer.
- The caller in `Client/ClientSessionRuntime.cpp`.
- The discovery wire protocol and `NetworkDiscoveryResponder`.

## Risk tier

Tier 2 — scoped behavior. Trigger: it removes `kStarted`, an enumerator in
the public section of a class in a header `Engine.h` aggregates
(`NetworkDiscoveryScanner.h:19-23`), and removing a public header name is
what Tier 1 excludes. No runtime behavior, determinism/CRC, wire,
serialization, version, or threading exposure: `IsScanning` keeps its
signature and returns the same value at its one call site.

## Acceptance criteria

1. `kStarted` no longer exists and `IsScanning` holds neither early return.
2. The BrokenEngineSandbox client builds through `/compile`.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
