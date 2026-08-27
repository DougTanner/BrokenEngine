<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:30:47.305Z","dependsOn":[]} -->
# Validate packed island hulls before SAT placement

## Context

The frozen audit retained `CAI/shard-0003/001`. `IslandTerrain::WaitForElevationMaps`
checks island payload sizes/counts but publishes the valid-area vertices without
finite, winding, convexity, or zero-edge checks (`Engine/Source/Frame/IslandTerrain.cpp:166-218`).
`IslandChainPlacement.cpp:277-281` feeds those vertices to
`ConvexHullsOverlap`, whose SAT implementation assumes a non-degenerate CCW
convex polygon (`Common/Math/ConvexHull.h:51-99`). Runtime/source bytes match
baseline `76d303f0eeeb86c1ed241edc81634e60070ba5a5`, proving this is pre-existing.

## Design

At island payload adoption, validate finite vertices, at least three vertices,
strict CCW convexity, and nonzero edges before publishing the hull. Reject the
chunk through the existing corruption path when validation fails; leave
`ConvexHullsOverlap`'s precondition and deterministic SAT math unchanged.

## Critical files

- `Engine/Source/Frame/IslandTerrain.cpp` — payload validation/publication.
- `Common/Math/ConvexHull.h` — SAT precondition.
- `Engine/Source/Frame/IslandChainPlacement.cpp` — consumer path.

## In scope

- Runtime validation of `mpf2ValidAreaVertices` before any placement consumer sees it.
- Failure state and client/server handling for malformed packed hulls.

## Out of scope

- SAT algorithm changes, bake-time hull generation, or pack content-CRC verification.
- Broadphase/placement policy for valid hulls.

## Risk tier and invariants

Tier 2 (downgraded from Tier 3). Trigger: an opaque serialized island payload
feeds deterministic client/server placement. Every published hull satisfies the
SAT precondition; valid placement order and results remain unchanged.

Tier rationale: the change adds the four pre-specified vertex checks at one
payload-adoption site and rejects a bad chunk through the existing corruption
path. The SAT math, pack layout, and placement results for valid hulls are
untouched.

## Acceptance criteria

- A finite duplicate-edge or non-convex packed hull is rejected before template publication and never reaches SAT.
- Valid CCW convex hulls still place deterministically on client and server.
- The runtime reports one normal corrupt-island failure rather than accepting a false SAT result.

## Notes

Origin: `CAI/shard-0003/001`, source selector
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0003.md:21`.
No source fix, build, or harness was performed here.
