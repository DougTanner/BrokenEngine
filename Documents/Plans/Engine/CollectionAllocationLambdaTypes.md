<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:05:07.876Z","dependsOn":[]} -->
# Name the collection allocation callbacks' pointer type

## Context

`Engine/Source/Frame/Collections/CollectionMemory.h:32` (`CalculateBufferSize`), `:62` (`AssignAligned`), and `:76` (`AssignAndCopyAligned`) reconstruct the pointer type of an `auto& rElementPointer` callback parameter. The first nests `remove_reference_t` inside `remove_pointer_t`; the latter two introduce `ElementPtrType` for the stripped-reference type before extracting its pointee and casting. The opening comment at line 6 warns that reversing the traits yields pointer-sized storage. Naming the callback's deduced type directly removes this repeated machinery and its ordering trap. This is a bounded clarity refactor authorized by the C++20 adoption review, with no claimed performance improvement.

`ForEachMemberPointer` passes scalar pointers and individual array pointers as lvalues. `CalculateBufferSize` also receives const members, so preserving reference deduction and top-level pointer constness is essential.

## Design

The recommendation is to change exactly those three callbacks to `[&]<typename ELEMENT_PTR>(ELEMENT_PTR& rElementPointer)`. In each, define `ElementType` as `std::remove_pointer_t<ELEMENT_PTR>`. Remove the two `ElementPtrType` aliases and use `ELEMENT_PTR` as the target of their existing pointer casts. Retain capture lists, parameter reference category, every runtime expression, and all call sites.

An explicit template parameter names the same type that the current generic lambda invents: `ELEMENT_PTR` equals `std::remove_reference_t<decltype(rElementPointer)>`, including top-level constness. The pointee type, `sizeof` results, casts, arithmetic, conditional copy, and aliasing therefore remain identical. No runtime operation, allocation, indirection, or dispatch is added. This structural equivalence is the performance non-regression argument.

Primary language references supporting this reasoning are [lambda call operator templates](https://eel.is/c++draft/expr.prim.lambda.closure#5), [generic parameter placeholders](https://eel.is/c++draft/dcl.spec.auto.general#2), and [reference parameter deduction](https://eel.is/c++draft/temp.deduct.call#3).

For the opening comment, retain the existing visitation-order, CRC/layout ordering, pointer-reference, and constness contract. Remove the obsolete trait derivation recipe and reversal warning; the explicit type name makes the recipe unnecessary. Do not add narration about this refactor.

## Critical files

- `Engine/Source/Frame/Collections/CollectionMemory.h`: three allocation callbacks and opening comment.
- `Engine/Source/Frame/Collections/AGENTS.md` (`Core Contract`, `Identity, Serialization, and CRC`): read-only constraints.
- `Documents/C++StyleGuide.txt` rules 15 and 64: read-only style and comment policy.

## In scope

- Only the callback template parameter lists, pointee aliases, and pointer-cast type names inside `CalculateBufferSize`, `AssignAligned`, and `AssignAndCopyAligned`.
- Only the obsolete derivation recipe in the opening `ForEachMemberPointer` contract comment.

## Out of scope

- Changes to `ForEachMemberPointer`, other callbacks, `CopyMemberPointerRows`, helper APIs, call sites, constraints, by-value parameters, or `ELEMENT*` deduction patterns.
- Allocation, buffer sizing, alignment, growth, copy lengths, traversal, tuple membership, collection layout, serialization, CRC, replay, wire formats, or version changes.
- Conversion campaigns, new abstractions, benchmarks, unit tests, project membership changes, and unrelated cleanup.
- Changes to `ThreadLocal::Entry`, replay serialization, FrameUtils folds, or style-guide return-deduction permissions.

## Acceptance criteria

1. Exactly the three named callbacks use the explicit `ELEMENT_PTR&` parameter, their pointee aliases use `remove_pointer_t<ELEMENT_PTR>`, and the two cast sites use `ELEMENT_PTR` without a reconstructed alias.
2. Review confirms equivalent reference deduction for mutable and const pointer lvalues and pointer-array elements; captures and all runtime expressions remain unchanged apart from equivalent type spelling.
3. Buffer sizes, 64-byte rounding/alignment, pointer advancement, conditional `memcpy`, and byte counts retain their existing expressions and types. No change reaches the collection layout or deterministic computation contract.
4. The opening comment preserves its order and constness contract without the obsolete recipe. The diff stays inside the declared regions.
5. Client and server builds pass using `/compile` during implementation; record the exact build results then.

## Verification

Inspect the complete header diff and map old and new deduced types for scalar pointers, const pointer references used by sizing, and array pointer elements. Confirm all three replacements are the same typed computation and all other bodies and callers are untouched. Run client and server compilation through `/compile` to check actual shared-header instantiations. Apply the normal C++ correctness, code-style, comment, affected-code, and documentation review routes. No harness run is required for this type-spelling-only refactor; there is no new runtime acceptance criterion. Do not add unit tests.

## Coordination

No dependencies or mandatory coordination constraints. Searches by the three target functions, `CollectionMemory`, `ForEachMemberPointer`, pointer-type reconstruction, explicit lambda templates, and existing Coordination sections found no competing Plan. `LambdaFactoryReturnDeduction.md` and `ForwardingReturnDeduction.md` own distinct style-guide permissions and require no edits for this work.

## Notes

Risk tier: Tier 1, local behavior-preserving type-spelling work with no public signature or invariant change. The header serves invariant-sensitive allocation, but the recommendation changes neither computed types nor runtime operations; any proposed layout or computation change exceeds this Plan and requires reclassification.

Documentation/style decision: no style-guide update is warranted. Rule 15(f) permits the existing generic lambda parameters; explicit named template parameters introduce no new `auto` exception. Rule 64 governs the local comment removal. Collections and Frame documentation contracts remain accurate, so `/update-claude-docs` should confirm no AGENTS edit is needed. Preserve existing local names and formatting beyond the specified replacements. No build, runtime verification, or benchmark has been performed while authoring this Plan.
