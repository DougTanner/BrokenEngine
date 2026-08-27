<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:31:13.700Z","dependsOn":[]} -->
# Reconcile deleted islands before leaf discovery

## Context

The frozen audit retained `CAI/shard-0007/001`. `BakeIslandIntermediates`
discovers only directories containing `Island.json`
(`DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp:312-329`),
while `ExportIsland::Handles` claims any surviving leaf with
`BakedDimensions.json` (`DataPacker/Source/ExportJobs/ExportIsland.cpp:493-519`).
Deleting only the authored config therefore leaves generated leaves claimable.
The source tree is unchanged from baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Make the island pre-pass reconcile every island directory, including one with
no `Island.json`: remove its generated source leaves, matching Gaea cache
leaves, diagnostics, and completion markers under the existing copy-on-write
ownership rules before `ExportIsland` discovery. Keep route-level stale
pruning and the leaf completion marker for still-authored islands.

## Critical files

- `DataPacker/Source/ExportJobs/Island/BakeIslandIntermediates.cpp` — pre-pass reconciliation.
- `DataPacker/Source/ExportJobs/ExportIsland.cpp` — leaf ownership predicate.
- `DataPacker/Source/Main.cpp` — pre-pass/discovery order.

## In scope

- Whole-island deletion detection and cleanup/quarantine before island job discovery.
- Ensuring removed islands produce no manifest/header/runtime template entries.

## Out of scope

- Route subdivision policy, leaf geometry validation, cache value validation, or runtime placement.
- Deleting unrelated source assets or changing generated-output ownership rules.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: source/cache asset lifecycle controls
serialized Islands pack contents consumed by runtime. A missing authored config
contributes no island jobs; authored islands retain current generated outputs.

Tier rationale: the change extends one offline DataPacker pre-pass to clean up
generated leaves for an island whose authored config is already gone, under the
existing generated-output ownership rules. It removes only regenerable derived
files, changes no pack or manifest format, and leaves authored islands and
their outputs untouched.

## Acceptance criteria

- Deleting `Island.json` from a warm island removes its surviving leaves and cache markers before discovery.
- The next export contains no chunk/header/runtime template for the deleted island.
- Route deletion and ordinary authored-island export behavior remain intact.

## Notes

Origin: `CAI/shard-0007/001`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0007.md:34`.
No source fix, build, or DataPacker run was part of this route.
