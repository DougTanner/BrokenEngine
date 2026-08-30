<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T18:24:39.650Z","dependsOn":[]} -->
# Defer Gaea executable resolution until a route is dirty

## Context

The false required condition is that every discovered island must have
`Gaea.Swarm.exe` available before its route cache state is inspected.
`BakeIslandIntermediates` resolves the executable unconditionally after finding
any island (`DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp:312-344`)
and passes that path into every `BakeRoute` call. `BakeRoute` computes raw and
split dirtiness and returns when both stages are clean
(`DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:536-555`). When only the
split stage is dirty, it reuses the existing raw bake and does not call Gaea
(`BakeRoute.cpp:563-591`). The Island authority states that clean route caches
remain usable, so the unconditional resolver can block both valid offline paths
on a machine without Gaea before either route can prove that it needs a launch.
For a dirty route, `BakeRoute` currently creates the cache directory, removes
`SplitVersion.meta`, and resets `GaeaStaging` before `RunGaeaExport`; lazy
resolution must precede that setup so a missing executable does not alter the
dirty-route failure state.

The originating candidate is `CSB/shard-0007/002`. The finding is pre-existing
at frozen source `80896f33661aaab99cf180a96db54600099be652`; the prepared audit
candidate is `c54ed87208df37d0607acbd1e98bd1d1aca7f2d6`. The user accepted this
residual for a follow-up Plan. No source implementation, build, or DataPacker
run is part of this Plan-writing session.

## Design

The author's recommendation is to make executable lookup lazy at the existing
Stage 1 boundary. Remove the executable path from `IslandBakeContext` and from
the `BakeOne` setup. Once `BakeRoute` has determined that a route is dirty, its
`bGaeaDirty` path must resolve the executable before the first dirty-route
mutation, including cache-directory creation, `SplitVersion.meta`
invalidation, and `GaeaStaging` reset. Pass that path into `RunGaeaExport` and
use it for the existing command construction and
`RunExecutableInNewConsole` call. This keeps island discovery, route cache
inspection, fingerprint comparison, clean early return, and split-only reuse
independent of Gaea availability. If resolution fails, the existing
missing-executable error is raised before any dirty-route marker or staging
state changes. Installed Gaea still follows the existing export guards, command
line, and successful output and cache behavior.

Do not change `ResolveGaeaExecutable`'s search order or error text, the
`BT_DATAPACKER_FORBID_*` guard behavior, `BakeFingerprint` or
`SplitFingerprint`, any cache version, or the staged-output publication order.

## Critical files

- `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp:114-116,312-344` — remove eager resolution and the context argument.
- `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediatesInternal.h:58-70` — remove the eager executable member from the route context.
- `DataPacker/Source/ExportJobs/Island/BakeRoute.cpp:242-311,536-575` — resolve at the start of the dirty-route path, before cache-directory creation, `SplitVersion.meta` invalidation, or `GaeaStaging` reset; pass the path to `RunGaeaExport` for the existing command and launch.
- `DataPacker/Source/ExportJobs/Island/GaeaArchetype.cpp:120-142` — existing resolver and missing-executable contract to preserve.
- `DataPacker/Source/ExportJobs/Island/AGENTS.md:7,15` — clean-cache and dirty-Gaea authority.

## In scope

- Deferring `ResolveGaeaExecutable()` until the `bGaeaDirty` branch in `BakeRoute`, before cache-directory creation, any dirty-route marker or staging mutation, and process launch; passing the path into `RunGaeaExport` for its existing command and launch; and removing the now-unneeded eager path from `BakeOne` and `IslandBakeContext`.
- Preserving clean-route early return, split-only raw-bake reuse, missing-Gaea failure before dirty-route marker/staging mutation or process launch, and ordinary installed-Gaea command, guard, staging, and publication behavior.
- Verifying the change through the existing DataPacker route and cache paths without changing their identity or output contracts.

## Out of scope

- Changing executable search paths, environment variables, default installation behavior, resolver error text, or either existing Gaea-export guard.
- Changing route fingerprints, cache markers or versions, raw/split algorithms, Gaea graph content, staged-output publication, pack/manifest formats, or runtime consumers.
- Adding a fallback executable, cache compatibility path, unit tests, or unrelated DataPacker cleanup.

## Risk tier and invariants

Expected Change Workflow Tier 2 (scoped DataPacker tool behavior). Trigger: the
fix changes one offline route-cache orchestration boundary but does not change
serialization, `.pack` contents, cache identity, determinism/CRC, wire/save/
replay data, threading, executable trust, or project/build affinity.

Preserve these invariants:

- A clean route and a split-only route never resolve or launch Gaea.
- A dirty route resolves through the existing resolver before any dirty-route
  marker or staging mutation and before process launch; missing Gaea retains the
  existing failure, leaves `SplitVersion.meta`, `BakeVersion.meta`, and
  `GaeaStaging` unchanged, and launches no process.
- An installed-Gaea dirty route uses the same command, guards, staged outputs,
  fingerprints, publication order, and resulting cache/output bytes.
- No cache marker/version, serialized island payload, pack/manifest contract,
  simulation CRC, wire, save, replay, or runtime behavior changes.

## Acceptance criteria

- A warm clean-cache DataPacker run succeeds on a machine without Gaea and
  produces the same outputs; no resolver or Gaea process is reached.
- A split-only reuse run succeeds on that machine, logs the existing reuse path,
  and does not resolve or launch Gaea.
- A dirty route on that machine fails through the existing missing-executable
  error before any dirty-route marker or staging mutation and before
  `RunExecutableInNewConsole`; existing `SplitVersion.meta`, `BakeVersion.meta`,
  and `GaeaStaging` state are unchanged, and no Gaea process launches.
- With Gaea installed, an ordinary dirty route still uses the existing command,
  export guards, staged publication, cache markers, and output behavior.
- DataPacker compilation passes through `/compile`; no unit tests are added.

## Notes

Origin: `CSB/shard-0007/002`, source selector
`Temp/CppScopeBoundaryAudit/80896f33661aaab99cf180a96db54600099be652/triage-0001.md`.
The ignored triage record is session evidence only; the durable source and
authority citations above define the implementation boundary.
