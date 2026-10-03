<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T16:33:47.335Z","dependsOn":[]} -->
# Permit reference-preserving callable return deduction

## Context

`Documents/C++StyleGuide.txt` rule 15 lists permitted uses of `auto`, but none explicitly covers the `decltype(auto)` return of the generic lambda produced by `common::ThreadLocal::Entry` in `Common/Threading/ThreadLocal.h`. That lambda directly returns `std::invoke(function, std::forward<ARGS>(args)...)`; its existing comment documents reference-result preservation. `Common/Threading/AGENTS.md` under `Thread-Local State` requires `Entry` as the installation boundary for repository-started threads and both process entry points.

The existing return spelling preserves the wrapped invocation's result type, including references. Replacing it with ordinary `auto` would lose reference preservation; an explicit invocation-result type would duplicate type machinery. The concrete benefit is removing a policy ambiguity around an existing intentional mechanism. No source adoption or runtime improvement is proposed.

## Design

Recommend appending one exception to rule 15, using the next available letter and preserving the file's existing indentation and format:

> It is a decltype(auto) return type on a generic callable wrapper that directly returns the wrapped invocation and preserves its result type and reference category.

The exception covers the inner generic lambda of `ThreadLocal::Entry`. It does not permit `decltype(auto)` local variables, arbitrary accessor return deduction, or general ordinary `auto` function returns. Preserve every existing exception and the range-based-for prohibition.

## Critical files

- `Documents/C++StyleGuide.txt`: rule 15, the sole implementation target.
- `Common/Threading/ThreadLocal.h`: `common::ThreadLocal::Entry`, read-only evidence for the generic lambda and its direct invocation return.
- `Common/Threading/AGENTS.md`: `Thread-Local State`, read-only evidence for the required installation boundary.

## In scope

- Append the recommended narrow `decltype(auto)` return exception to rule 15 in `Documents/C++StyleGuide.txt`.
- Check its wording against the existing inner generic lambda returned by `common::ThreadLocal::Entry`.

## Out of scope

- All C++ changes, including `Entry`, its comments, callers, and further return-deduction adoption.
- Ordinary `auto` lambda-factory return policy, local-variable and accessor deduction exceptions, and changes to existing rule 15 exceptions.
- Other style rules, AGENTS.md changes, or duplicated policy. No additional documentation change is justified beyond the owning rule.
- Builds, harness runs, benchmarks, and unit tests.

## Coordination

`Documents/Plans/Engine/LambdaFactoryReturnDeduction.md` separately owns the ordinary `auto` exception for functions directly returning lambda expressions in rule 15(e). Neither Plan depends on the other. Preserve that exception if present; this Plan neither implements nor removes it. Keep the outer closure-return and inner invocation-return permissions separate.

## Risk and invariants

Tier 1: documentation-only clarification, with no source or public-signature change. Preserve all existing rule 15 permissions and restrictions, including the range-based-for prohibition. The wrapper implementation, result/reference behavior, lifetime, allocations, copies, ABI, and deterministic state remain unchanged.

Performance is unchanged because executable code remains identical. The existing `decltype(auto)` is appropriate in C++23; no older spelling replaces a clearer current construct.

## Acceptance criteria

- Rule 15 explicitly permits `decltype(auto)` return types only for generic callable wrappers directly returning their wrapped invocation to preserve its result type and reference category.
- The exception clearly applies to the inner generic lambda of `ThreadLocal::Entry` without granting general local-variable, accessor, or ordinary function return deduction.
- Existing exceptions, including the separate lambda-factory permission if present, and the range-based-for prohibition remain intact.
- The implementation diff changes only the appended exception in `Documents/C++StyleGuide.txt`; no source or other documentation changes occur.

## Verification

Read the final rule beside `ThreadLocal::Entry` and verify the direct invocation return and narrow scope. Inspect the diff and surrounding exceptions against every acceptance criterion, including preservation of the separate lambda-factory permission if it has landed. Obtain independent documentation coherence review. The documentation diff is decisive; no compilation, runtime verification, benchmark, or unit test is needed.
