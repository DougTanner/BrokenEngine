<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T19:40:28.936Z","dependsOn":[]} -->
# Merge the duplicated code in NetworkSimulation.h

## Context

Paths are under `Engine/Source/`. Verified against commit `10d84ca6`; the
lines cited are those at commit `c675b6b2`, except the `NetworkSimulation.h`
and `EngineSwitches.h` lines, which `## Coordination` places.

`Network/NetworkSimulation.h` holds two duplications:

- `GetNetworkSimulationBounds`: the `kDisabled` arm (`:53`) holds the same
  initializer as the `default` arm (`:59`). The sibling switches in the file
  (`:14-25`, `:27-38`) carry `kDisabled` through `default` alone. The one
  caller, `Profile/ProfileNetworkScreen.cpp:209`, sits inside
  `if constexpr (keNetworkSimulation != kDisabled)` (`:207`), so the disabled
  row is never read.
- `ProcessDelayed` and `FlushDelayed` (`:202-234` with their caller) are one
  drain loop. They differ only by the `releaseTime <= now` term (`:206`
  against `:216`), and each is called only from `ProcessOrFlush` (`:228`,
  `:232`), whose only callers are `Network/Client/Client.cpp:163` and
  `Network/Server/Server.cpp:133`.

The three delayed-packet functions are templates instantiated only inside
`if constexpr (keNetworkSimulation != kDisabled)` (`Client.cpp:161`,
`Server.cpp:131`). The committed value is `kDisabled`
(`EngineSwitches.h:100`), so the default build parses their bodies and
never instantiates them.

Originating gap: the in-function refactoring phase of an analysis of
`Engine/Source/Network` at that commit. Unmet criterion: one drain loop and
one row per distinct tuning value.

## Design

Recommended, both by merging in place as
`.agents/skills/code-quality-metrics/references/Remediation.md` prefers for
excess decisions:

- Delete the `kDisabled` arm at `:53`, so `default` carries it as in the
  sibling switches.
- Fold `ProcessDelayed` and `FlushDelayed` into `ProcessOrFlush` as one loop
  that drains while the queue is non-empty and either fast-forward is on or
  the front packet's release time has passed. That single condition is
  equivalent to the two loops.

No helper is added and no behavior changes.

## Critical files

- `Engine/Source/Network/NetworkSimulation.h`

## In scope

- `GetNetworkSimulationBounds`: the `kDisabled` arm.
- `ProcessDelayed`, `FlushDelayed`, and `ProcessOrFlush`.

## Out of scope

- The values of every tuning row, and the other two switches.
- The drop simulation and its log, `DelayedPacket`, and
  `NetworkSimulationState`.
- The committed value and location of `keNetworkSimulation`.
- Both callers of `ProcessOrFlush`.

## Risk tier

Tier 2 — scoped behavior. Trigger: it removes `ProcessDelayed` and
`FlushDelayed`, two namespace-scope functions in a header `Engine.h`
aggregates, and removing a public header name is what Tier 1 excludes. No
runtime behavior, determinism/CRC, wire, serialization, version, or
threading exposure: `ProcessOrFlush` keeps its signature, and the two folded
functions have no other caller.

## Acceptance criteria

1. `GetNetworkSimulationBounds` has no `kDisabled` arm, and one loop drains
   the delayed packets.
2. With a temporary, uncommitted non-disabled `keNetworkSimulation`, the
   BrokenEngineSandbox client and server build through `/compile`, and an
   `/agent-harness` session at that level stays connected with fast-forward
   toggled on and off. The originating evidence records no harness setup;
   the executing session sets it up from the harness documentation.
3. The landed tree keeps `keNetworkSimulation` at `kDisabled` and builds the
   client and the server.

## Coordination

`Documents/Plans/Engine/RemoveCoordUpdateTickReader.md` edits
`NetworkSimulation.h` too and needs the same temporary non-disabled build.
Neither depends on the other. Whichever lands later re-derives its regions
from the landed tree. `keNetworkSimulation` is defined in
`Engine/Source/EngineSwitches.h`, moved there after commit `c675b6b2`; the
`NetworkSimulation.h` and `EngineSwitches.h` lines cited here are those after
that move.
