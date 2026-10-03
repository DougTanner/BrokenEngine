<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:27:03.887Z","dependsOn":[]} -->
# Express cancelled-subscription fixture membership with ranges::contains

## Context

C++23 inventory F097 covers `std::ranges::contains` and `std::ranges::contains_subrange`. The actionable opportunity is two Boolean-only membership tests in `CommandClientCancelledSubscriptionFixture`, at `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp:233` and `:237` (baseline `6c6245f4b1dee82d7721ef271ce4925ca754ff47`). Each currently compares a `std::ranges::find` result with the same vector's end; neither consumes or retains the iterator. The duplicated container expression obscures a simple membership guard.

`Engine/Source/Network/Client/ClientSessionRuntime.h:93-94` declares `mDesiredCoordinates` and `mSubscriptionQueue` as `std::vector<GridCoord>`. `Engine/Source/Frame/GridCoord.h:11` supplies defaulted equality. `Projects/BrokenEngineSandbox/Source/Game.cpp:93,182,210` already uses `std::ranges::contains` for comparable membership checks. This is a small clarity improvement using an established idiom, not a measured performance improvement.

## Design

The author's recommendation is to replace exactly these complete conditions:

| Existing condition | Replacement |
|---|---|
| `std::ranges::find(rRuntime.mDesiredCoordinates, coord) != rRuntime.mDesiredCoordinates.end()` | `std::ranges::contains(rRuntime.mDesiredCoordinates, coord)` |
| `std::ranges::find(rRuntime.mSubscriptionQueue, coord) != rRuntime.mSubscriptionQueue.end()` | `std::ranges::contains(rRuntime.mSubscriptionQueue, coord)` |

Keep the order and bodies of both guards, their error text, and the following `mUnwantedTimestamps.contains(coord)` guard unchanged.

The [standard draft, alg.contains paragraph 1](https://eel.is/c++draft/alg.contains#1) defines the non-policy membership result using the corresponding find-and-end comparison. Both ranges and the coordinate here are existing lvalues, with the same default equality and identity projection. Empty ranges, absent coordinates, and present coordinates therefore produce the same Boolean result. No iterator escapes, no range is materialized, and no ownership, element copy, allocation, or lifetime change is introduced. The same linear search can stop at the first match; no speedup is claimed.

## Critical files

- Edit: `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp`, the two conditions in `CommandClientCancelledSubscriptionFixture`.
- Read for the data contract: `Engine/Source/Network/Client/ClientSessionRuntime.h` and `Engine/Source/Frame/GridCoord.h`.
- Read for ownership and policy: `Projects/BrokenEngineSandbox/Source/Agent/AGENTS.md` and `Projects/BrokenEngineSandbox/Source/AGENTS.md`.
- Read for formatting: `Documents/C++StyleGuide.txt`, rules 1 and 2.

## In scope

Only the two vector membership conditions named in Design, preserving the source file's encoding, BOM status, line endings, and surrounding formatting.

## Out of scope

- `contains_subrange`: there is no demonstrated contiguous-subsequence operation to replace.
- Other finds, including iterator-consuming `Game.cpp:81`, other membership guards, and a repository-wide migration.
- Helpers, includes, containers, fixture sequencing, subscription policy, schemas, error messages, or new tests.
- Style-guide or AGENTS.md edits: existing rules 1 and 2 suffice for these expressions, and the ownership and fixture contracts do not change. A new adoption rule for this two-expression cleanup would add unnecessary policy.

## Risk and invariants

Future implementation is Change Workflow Tier 1: a local behavior-preserving expression substitution with no public signature or invariant exposure. Preserve all fixture state transitions, subscription checks, and guard ordering. No change reaches determinism/CRC, serialization, `.pack` formats, save/replay compatibility, wire messages, threading, trust policy, or project affinity. No format/version bump is warranted.

## Acceptance criteria

1. The two named conditions use exactly the corresponding `std::ranges::contains` expressions; the implementation diff changes no other source expressions or guard bodies.
2. The guard order, exception messages, subsequent associative-container check, debug gate, and all fixture state changes remain identical.
3. A reviewer confirms the same equality, projection, lvalue lifetimes, allocation behavior, and empty/absent/present results from the declarations and standard equivalence above.
4. BrokenEngineSandbox client `Debug|x64` compiles successfully through `/compile`, covering the `kbDebugInput` fixture body.

## Verification

Inspect the two-condition diff and owning declarations to settle criteria 1-3; run the repository's required C++ reviews and documentation synchronization check without inventing documentation changes. Build the affected client Debug target through `/compile` for criterion 4. Do not add unit tests. There is no runtime-observable behavior change requiring a harness scenario; no runtime run or benchmark is necessary for this substitution.

## Notes

This is an implementation recommendation, not a newly approved user decision. It is independently executable with no dependencies or Coordination constraint. Searches of live Plans for the function, file, both vector names, subscription-policy wording, and ranges membership found no overlapping Plan. The evidence and source link above are sufficient without an investigation document or temporary review receipt. Plan authoring performed no build or implementation.
