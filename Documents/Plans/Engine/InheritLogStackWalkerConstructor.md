<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:24:25.704Z","dependsOn":[]} -->
# Inherit the LogStackWalker constructor

## Context

`Common/StackWalker.h` defines `common::LogStackWalker` as a stateless, single-base leaf that supplies the logging `Emit` override. Its public constructor repeats `FilteredStackWalker(ExceptType)` and forwards the sole argument with an empty body. Inheriting that constructor expresses the existing construction relationship directly and removes the repeated parameter and initializer. This uses inherited-constructor syntax introduced in C++11 with the C++17 semantics supported by the repository's C++23 build.

## Design

Replace the entire public `LogStackWalker(ExceptType eExceptType)` definition with:

```cpp
using FilteredStackWalker::FilteredStackWalker;
```

Keep the declaration in the existing public section and retain the protected `Emit` override verbatim. The direct base has exactly one declared application constructor, public `FilteredStackWalker(ExceptType eExceptType)`, with no default argument. It explicitly calls upstream `StackWalker(eExceptType)`. Upstream additionally offers defaulted arguments and other constructors; retain the direct base's explicit forwarding boundary so those upstream overloads do not become leaf construction options. The inherited declaration adds no member state or work, and special-member handling remains implicit. Do not add constructors, assertions, helper aliases, or explanatory comments.

## Critical files

- `Common/StackWalker.h`: `LogStackWalker` constructor replacement; inspect `FilteredStackWalker` and `CrashFileStackWalker` as unchanged boundaries.
- `Common/Determinism.cpp`: existing `LogStackWalker logStackWalker(StackWalker::NonExcept)` call followed by `ShowCallstack`, verification only.
- `ThirdParty/StackWalker/Main/StackWalker/StackWalker.h`: upstream constructor declarations, verification only; never edit ThirdParty.
- `Common/AGENTS.md` and `Documents/C++StyleGuide.txt` rule 23: unchanged owning contracts.

## In scope

Only the public forwarding constructor region of `common::LogStackWalker` in `Common/StackWalker.h` changes, to the inherited-constructor declaration above.

## Out of scope

Other constructor conversions; `FilteredStackWalker` implementation; `CrashFileStackWalker` and its `mpWriter` initialization; logging, error formatting, DbgHelp locking, and crash behavior; call-site edits; documentation or style-policy additions; unit tests and runtime instrumentation.

## Risk and invariants

Tier 1: local behavior-preserving declaration simplification with the same effective public construction signature. Preserve the required `ExceptType` argument, its accessibility and implicit-conversion behavior, and the absence of a usable default constructor. Preserve the single base, all virtual overrides, object layout, logging sink, and shared DbgHelp serialization. No simulation, CRC, serialization, persistence, or protocol code changes. The derived class has no data members or constructor side effects to lose. Retain `CrashFileStackWalker`'s explicit constructor because it initializes its writer member.

## Performance

Construction still initializes the same base with the same enum. There is no new storage, allocation, synchronization, or virtual dispatch. Source equivalence and compilation are sufficient; no benchmark or runtime crash exercise is warranted.

## Documentation and style

No documentation or style-policy edit is needed. `Common/AGENTS.md` describes serialized stack walking and exception handling, whose contracts remain unchanged. Style rule 23 prescribes aliases over typedefs; it does not govern constructor inheritance and should not be expanded for this one leaf. Preserve existing indentation and access-section spacing. Run the normal C++ style/comment review and documentation synchronization checks; the expected documentation result is no change.

## Verification

1. Inspect the diff: the only implementation change replaces the empty forwarding constructor with the specified public declaration. Confirm no added state or changes to `Emit`, `ShowCallstack`, or the other constructors.
2. Recheck the direct base constructor surface: one public required `ExceptType` parameter, no inherited upstream constructor declaration, and no default argument. Confirm the existing `Common/Determinism.cpp` construction remains unchanged.
3. Use `/compile` to build BrokenEngineSandbox client and server in Debug and Release. These builds verify the existing enum construction and header integration in both targets. Follow the skill's build driver and runtime-data policy; do not invent standalone build commands.
4. Complete the repository's applicable C++ correctness, propagation, style, comment, and acceptance checks. No unit tests or `/agent-harness` run is required for this declaration-only change.

## Acceptance criteria

- `LogStackWalker` publicly contains `using FilteredStackWalker::FilteredStackWalker;` and no explicit forwarding constructor.
- The sole existing `StackWalker::NonExcept` construction compiles unchanged in client/server Debug and Release.
- The leaf has no added fields, and its `Emit` body, direct base implementation, sibling writer initialization, upstream constructors, locking, and logging remain unchanged.
- The implementation diff is limited to the scoped constructor replacement; no documentation or style-policy edits are introduced.
- No runtime work, state layout, determinism, CRC, or serialized format changes result.
