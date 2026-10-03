<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:25:52.123Z","dependsOn":[]} -->
# Diagnose discarded allocation tracking guards

## Context

`Common/AllocationTracking.h:8,25` defines `ScopedSuppressAllocationTracking()` and `ScopedResumeAllocationTracking()` without discard diagnostics. Their constructors adjust `giAllocationTrackingSuppressed`, and their destructors immediately reverse that adjustment. A discarded explicit construction therefore fails to cover subsequent work. Current callers are correct; this is a declaration-level clarity improvement, not a demonstrated caller bug.

`Engine/Source/Network/Server/ServerSessionRuntime.cpp:CompleteTick` (lines 231-245) demonstrates the existing contract: an outer named suppression guard covers bookkeeping while an inner named resume guard keeps publication construction tracked. `Engine/Source/Memory/GlobalAllocator.cpp:TrackAllocation` bypasses the allocation tripwire when suppression is positive. Expressing the guard-lifetime requirement in compiler diagnostics makes this existing API contract visible at a misuse site.

## Design

Recommendation: add precisely these attributes to the existing constructor declarations/definitions in `Common/AllocationTracking.h`, because construction is the point at which accidental discard can be diagnosed:

- `ScopedSuppressAllocationTracking()`: `[[nodiscard("Keep this guard alive for the scope that suppresses allocation tracking")]]`.
- `ScopedResumeAllocationTracking()`: `[[nodiscard("Keep this guard alive for the scope that resumes allocation tracking")]]`.

Keep constructor/destructor bodies, type declarations, callers, copy behavior, and counter operations unchanged. Do not annotate both the constructor and its type.

[WG21 P1771R1](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1771r1.pdf) establishes nodiscard constructor diagnostics for discarded explicit constructions; [WG21 P1301R4](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2019/p1301r4.html) supplies explanatory reason strings. [Microsoft C4834 documentation](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-warnings/c4834?view=msvc-170) describes the discarded-result warning, but installed-compiler coverage of constructor syntax must be verified. These annotations do not prevent explicit void discard, copying, or every premature destruction.

## Critical files

- `Common/AllocationTracking.h`: the two default constructors; sole expected source edit.
- `Engine/Source/Network/Server/ServerSessionRuntime.cpp:CompleteTick`: lifetime-contract evidence; this existing server project member also hosts a temporary, uncalled diagnostic probe that is removed after verification.
- `Engine/Source/Memory/GlobalAllocator.cpp:TrackAllocation`: read-only suppression interpretation.
- `Common/AGENTS.md` `## Allocation-Free Scratch` and `Engine/Source/Memory/AGENTS.md` `## Architecture Notes`: existing documentation owners, reviewed without anticipated edits.

## In scope

Only the two constructor attributes and their reason strings in `Common/AllocationTracking.h` remain in the source diff. During verification, temporarily add an uncalled diagnostic function inside the existing `engine` namespace in `Engine/Source/Network/Server/ServerSessionRuntime.cpp`, using the actual guard definitions already included through `Pch.h`; restore that file immediately after capturing diagnostics, including on build failure. No probe or project-membership change is retained.

## Out of scope

No caller changes, guard-body changes, class-level attributes, copy/move restrictions, counter or threading changes, workbuffer annotation changes, `ScopedLambda` changes, repository-wide guard sweep, warning suppressions, project/build-policy changes, documentation mandates, or unit tests.

## Acceptance criteria

1. The source diff contains exactly the two specified constructor annotations, with no runtime statement or type-layout change.
2. Through `/compile`'s approved workflow, the temporary diagnostic function in the existing server project member confirms that each of `ScopedSuppressAllocationTracking{};`, `ScopedSuppressAllocationTracking();`, `ScopedResumeAllocationTracking{};`, and `ScopedResumeAllocationTracking();` produces a discard diagnostic with the applicable useful reason. Never call the function or launch its resulting executable; nest all resume examples under a named suppression guard to represent the intended lifetime contract. Request server `Debug|x64`, Shared data mode, and selective `-Files 'Engine/Source/Network/Server/ServerSessionRuntime.cpp'` through `/compile`; this invalidates that member's object before a normal project build, not a standalone compile-only operation.
3. Corresponding named local constructions in brace and ordinary declaration forms compile without new discard warnings. Record the compiler version, options, and diagnostic output. Do not retain probe code as a unit test or add suppressions. If constructor placement is not diagnosed, report the failed acceptance condition for scope reconsideration rather than silently changing placement or runtime code.
4. After restoring the probe host, build the affected sandbox client and server `Debug|x64` through `/compile` with Shared data mode and confirm existing callers have no new discard warnings. Apply required C++ style, comment, affected-code, and affected-document reviews; expected follow-on edits are none.

## Notes

Change Workflow Tier 1: two mechanical diagnostic annotations, preserving public signatures, layout, and runtime invariants. There is no change to the thread-local counter, allocation, synchronization, destruction points, affinity, deterministic simulation/CRC, serialization, pack versions, replay, wire, or trust boundaries. No performance benchmark or live `/agent-harness` run is required: no executable operation is added or removed, and this Plan makes no speedup claim.

No documentation or style-guide update is proposed. `Common/AGENTS.md` `## Allocation-Free Scratch` already owns the guard usage and thread-local lifetime contract; `Engine/Source/Memory/AGENTS.md` `## Architecture Notes` owns allocator interpretation. Existing `[[nodiscard]]` types in `Common/Workbuffer.h` establish the annotation convention. Review against `Documents/C++StyleGuide.txt` during implementation without introducing a global nodiscard policy or repeating reason strings in prose. No comments are added, so rule 64 needs no new prose.

Searches of existing Plans by the constructor names, allocation tracking, header path, nodiscard, and the proposal identifiers found no Plan owning this boundary. No dependencies or reciprocal Coordination constraints are needed. This document records planned verification only; no compiler probe, build, or runtime verification has been performed while authoring it.
