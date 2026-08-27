<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:54.642Z","dependsOn":[]} -->
# Reject non-topological serialized scene skeletons

## Context

The frozen audit retained `CAI/shard-0006/002`. `BuildNodeParentMap` preserves
source parent indices and `LoadSkeletonData` serializes nodes in source order
(`DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp:3-13,74-87`). The
runtime requires each parent index to precede its child and rejects otherwise
at `Engine/Source/Graphics/AnimationData.cpp:160-168`. Source and runtime are
unchanged from baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`.

## Design

Before writing an animated scene section, validate that every non-root parent
index is less than its child index. Reject a non-topological source graph
through the existing scene export aggregate with a path-specific diagnostic;
keep current node/animation indices and the runtime single-pass evaluator for
accepted graphs. This bounds scope without introducing a remapping format.

## Critical files

- `DataPacker/Source/ExportJobs/Scene/SceneSkeletonLoader.cpp` — source validation.
- `DataPacker/Source/ExportJobs/ExportScene.cpp` — publication failure path.
- `Engine/Source/Graphics/AnimationData.cpp` — runtime invariant.

## In scope

- Parent-before-child validation for every serialized animated scene skeleton.
- Structured export failure before scene chunk publication.

## Out of scope

- Topological reordering, animation-channel remapping, or runtime evaluator changes.
- Static non-animated scene transforms and unrelated hierarchy validation.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: serialized scene hierarchy data
crosses the DataPacker/runtime boundary. Every accepted parent index is `-1` or
less than its child index; existing valid scenes serialize unchanged.

Tier rationale: the fix is one parent-index comparison loop before the existing
write, failing through the export aggregate, with reordering and remapping
explicitly out of scope. Valid skeletons serialize byte-identically and the
runtime evaluator is untouched.

## Acceptance criteria

- A parseable child-before-parent animated glTF fails export before a scene chunk is published.
- A topologically ordered scene loads and evaluates as before.
- The runtime rejection remains as defense in depth for corrupt packs.

## Notes

Origin: `CAI/shard-0006/002`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0006.md:70`.
No source fix or build was performed while routing this candidate.
