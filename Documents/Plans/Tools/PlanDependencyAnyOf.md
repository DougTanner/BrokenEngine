<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:50:50.452Z","dependsOn":[]} -->
# Express Plan Dependency Blocking with any_of

## Context

`Tools/WorktreeCli/PlanMetadata.cpp:307`, `toolcli::IsBlockedByDependencies`, scans `rPlan.dependencies`, searches each key in `rPlans`, and returns true at the first present key; otherwise it returns false. Expressing this existential condition with an algorithm removes the temporary iterator and explicit branch while retaining the same meaning. This is the minimal accepted B55 adoption: the `any_of` algorithm family originated in C++11; the selected `std::ranges::any_of` range overload is C++20 and fits the repository's C++23 baseline.

Source evidence at `d29fed456d3ede935c5e672f95f13d6733f0660c`: `PlanMetadata.cpp` already includes `<algorithm>`; `PlanMetadata.h` declares `Plan::dependencies` as `std::vector<std::wstring>` and the function's map parameter as a const reference. `Tools/WorktreeCli/PlanScheduler.cpp:45`, `IsLowerHex`, already uses the algorithm family through `std::all_of`; it needs no change. A search of existing Plans for the function, source path, `any_of`, and B55 found no competing owner.

## Design

Replace only the loop and trailing false return in `toolcli::IsBlockedByDependencies` with a returned `std::ranges::any_of` call over `rPlan.dependencies`. Give its lambda an explicit `[&rPlans]` capture and a `const std::wstring& rDependency` parameter; return `rPlans.find(rDependency) != rPlans.end()` from the predicate. Preserve the function signature and existing include list. Follow existing lambda formatting and the applicable C++ style rules 2, 41, and 51.

The predicate continues to ask only whether the key exists. It must not inspect `Plan::bValid`, `Plan::bDependenciesKnown`, or any other metadata. There is no additional container, string copy, allocation, or parallel algorithm policy. The search remains linear in dependency count with logarithmic map lookups and stops when it finds a present key; no performance improvement is claimed or required.

No documentation or style amendment is warranted: the public contract, ownership, and scheduler behavior are unchanged, and existing style rules already cover the expression. Future required documentation review should record that conclusion rather than invent a broader adoption policy.

## Critical files

- `Tools/WorktreeCli/PlanMetadata.cpp` — the sole implementation edit, in `toolcli::IsBlockedByDependencies`.
- `Tools/WorktreeCli/PlanMetadata.h` — read-only signature and dependency element type evidence.
- `Tools/WorktreeCli/PlanScheduler.cpp` — read-only existing algorithm-family usage.

## In scope

- Replace the existence-search loop and final return in `toolcli::IsBlockedByDependencies` exactly as described above.
- Verify the local equivalence and compile the changed WorktreeCli candidate.

## Out of scope

- Other loops, `IsLowerHex`, algorithm migrations elsewhere, or new helpers.
- Signature, include, Plan layout, scheduler eligibility, dependency metadata, lock, claim, or persistence changes.
- Documentation/style policy changes, unit tests, benchmarks, game builds, and game runtime scenarios.

## Risk and invariants

Future implementation is Tier 1: a local behavior-preserving expression change with no public signature or invariant exposure. Editing coordination semantics would exceed this scope and require reclassification.

- Empty dependencies and dependencies whose keys are all missing return false.
- Any present dependency key returns true, regardless of its mapped Plan metadata.
- Dependencies retain their iteration order; lookup stops on the first present key and never modifies the vector or map.
- The predicate introduces no string copies, allocation, extra map lookup, or parallel execution; runtime complexity is unchanged.

## Acceptance criteria and verification

1. Diff inspection shows the sole C++ change is the specified `std::ranges::any_of` return in `IsBlockedByDependencies`, with explicit reference capture and const-reference parameter; no header or caller propagation is needed.
2. A source equivalence review checks empty, all-missing, first-present, and later-present dependency cases, including a present key whose mapped Plan metadata is invalid. Review confirms the invariants above and the unchanged lookup expression. These cases are settled by the local diff; do not add unit tests or scheduler fixtures.
3. Run `/compile` for `WorktreeCli Release|x64` using its isolated candidate path and obtain a successful compile/link result. Do not overwrite the provisioned shared tool output; any later promotion is owned by `/finalize-changes`. No AgentHarness scenario, client/server build, or `/agent-harness` invocation is required for this pure local refactor.

These are future implementation checks; this Plan does not claim they have passed.
