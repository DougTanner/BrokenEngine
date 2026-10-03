<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T16:32:16.846Z","dependsOn":[]} -->
# Permit deduced returns for lambda factories

## Context

`Documents/C++StyleGuide.txt` rule 15(e) permits `auto` for a variable initialized directly with a lambda expression, but does not cover a function returning a lambda expression. `common::ThreadLocal::Entry` in `Common/Threading/ThreadLocal.h` already has an outer `static auto` return and directly returns a captured generic lambda. `Common/Threading/AGENTS.md` under `Thread-Local State` requires this factory for owned threads and both process entry points. Its outer return deduction therefore has a concrete conflict with the written policy.

The returned closure has an unnamed type. Keeping the existing deduced return avoids introducing a named callable solely to satisfy the style restriction. No source conversion is needed; the benefit is a precise policy exception for an existing required mechanism.

## Design

Recommend replacing only rule 15(e) with this text, preserving the surrounding indentation and file format:

> e) It is a lambda expression assigned directly to the variable, or the deduced return type of a function that directly returns a lambda expression: auto callback = []() {}; auto MakeCallback() { return []() {}; }

This retains the variable exception and adds only the direct lambda-return case. Arbitrary deduced function returns remain governed by the other enumerated exceptions. The inner `decltype(auto)` return of `Entry`'s lambda forwards `std::invoke`'s result and is a distinct policy concern, outside this change.

## Critical files

- `Documents/C++StyleGuide.txt`: rule 15(e), the sole implementation target.
- `Common/Threading/ThreadLocal.h`: `common::ThreadLocal::Entry`, read-only evidence for the outer return case and its distinct inner return.
- `Common/Threading/AGENTS.md`: `Thread-Local State`, read-only evidence that the factory is required.

## In scope

- Extend `Documents/C++StyleGuide.txt` rule 15(e) with the recommended direct lambda-return exception and inline example.
- Check the resulting wording against the existing outer return of `common::ThreadLocal::Entry`.

## Out of scope

- C++ source changes, new return-deduction adoption sites, and changes to `Entry`, its inner `decltype(auto)`, or its callers.
- General permission for deduced function returns, forwarding-return exceptions, iterator formatter changes, and changes to the existing member-list exception.
- Other style rules, AGENTS.md edits, or duplicated policy guidance. No additional documentation change is justified beyond the owning style rule.
- Builds, runtime verification, benchmarks, and unit tests.

## Coordination

`Documents/Plans/Engine/ForwardingReturnDeduction.md` separately owns a distinct `decltype(auto)` forwarding-return exception in rule 15. Neither Plan depends on the other. Preserve that separate exception if present when editing rule 15(e); this Plan neither implements nor removes it.

## Risk and invariants

Tier 1: documentation-only policy clarification; no source, signature, runtime, data layout, or determinism change. Preserve all other rule 15 exceptions and the range-based-for `auto` prohibition. Preserve the existing distinction between returning a closure and forwarding a callable's result.

Performance is unchanged because executable code remains identical. There is no reason to introduce a C++14 spelling into existing source or replace a clearer C++23 construct.

## Acceptance criteria

- Rule 15(e) retains its direct variable-initialization case and explicitly allows a deduced return type for a function directly returning a lambda expression.
- The rule contains both inline examples shown in the design and applies to the outer `auto` return of `common::ThreadLocal::Entry`.
- No other rule or existing exception is altered; the range-based-for prohibition remains intact.
- The implementation diff changes only rule 15(e) in `Documents/C++StyleGuide.txt`; no source or AGENTS.md changes occur.

## Verification

Inspect the documentation diff and surrounding rule 15 to verify each acceptance criterion. Read `ThreadLocal::Entry` to confirm that its outer return directly returns a lambda expression and that the inner forwarding return is a separate case. Obtain independent documentation coherence review. The diff is decisive for this prose-only change; no build, harness run, benchmark, or unit test is needed.
