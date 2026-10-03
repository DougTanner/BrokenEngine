<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T17:40:20.964Z","dependsOn":[]} -->
# Inherit the logging stack walker's constructor

## Context

The L26 adoption review identified `common::LogStackWalker` in `Common/StackWalker.h` as a suitable inherited-constructor use. Its constructor accepts `ExceptType`, forwards it unchanged to `FilteredStackWalker`, and performs no other work. The leaf owns no data members. `FilteredStackWalker` currently provides one ordinary constructor taking `ExceptType` and no default constructor. `Common/Determinism.cpp` constructs the leaf with `StackWalker::NonExcept`.

The independent adversarial review returned PASS/ADOPT for this site. Removing the redundant forwarding definition makes the leaf's sole responsibility, logging emitted frames, clearer. This is a source-maintenance change with no expected runtime or performance change.

## Design

Replace only the explicit `LogStackWalker(ExceptType eExceptType)` forwarding constructor definition with `using FilteredStackWalker::FilteredStackWalker;` in the existing public section. Preserve the protected `Emit` override verbatim. Do not inherit constructors into `FilteredStackWalker` itself.

The leaf intentionally follows the base constructor surface: exposure of a future base overload is acceptable for this stateless logging sink. At the current source baseline, the replacement adds no callable overload and retains the existing construction path. If the leaf acquires state or construction work before implementation, re-evaluate this premise instead of deleting that work.

No style-guide or AGENTS.md amendment is warranted. This local use establishes no new repository-wide policy and changes no documented stack-walking contract.

## Critical files

- `Common/StackWalker.h`: `common::LogStackWalker` public constructor definition, the sole edit target; `FilteredStackWalker` and `CrashFileStackWalker` are comparison evidence.
- `Common/Determinism.cpp`: the `LogStackWalker` construction with `StackWalker::NonExcept`, read-only caller evidence.
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj` and both BrokenEngineSandbox client/server project files: read-only evidence that all three targets compile `Common/Determinism.cpp`.

## In scope

- Replace the pure forwarding constructor of `common::LogStackWalker` with the public inherited-constructor declaration specified above.
- Inspect the base constructor set, leaf members, and current caller to confirm the substitution still meets its stated premises.
- Perform the applicable C++ change reviews and compile checks listed below.

## Out of scope

- Changes to `FilteredStackWalker`, `LogStackWalker::Emit`, stack frame filtering, DbgHelp locking, log sinks, or callers.
- Changes to `CrashFileStackWalker`, whose explicit constructor initializes `mpWriter`; `Export*` constructors that initialize `kiVersion`; and game `Camera` constructors that perform setup.
- Broad constructor sweeps, new abstractions, compatibility paths, policy amendments, project membership edits, and unrelated cleanup.
- Unit tests, intentional crash injection, harness scenarios, asset generation, PREfast, and benchmarks.

## Risk and invariants

Future implementation is Tier 1: a local behavior-preserving replacement with no change to the current callable public signature set or invariant exposure. The location participates in crash diagnostics, but this edit changes neither crash behavior nor the existing locking or allocation rules. Reclassify if source drift would change that conclusion.

Preserve public construction from `ExceptType`, lack of a usable default constructor, the argument passed to the same base constructor, and the logging override. Preserve the data layout, fault-path allocation behavior, DbgHelp serialization, and emitted text. No simulation, CRC, wire, save/replay, or packed-data format changes occur.

## Acceptance criteria

| Criterion | Required evidence |
| --- | --- |
| The forwarding definition is replaced by the exact public inherited-constructor declaration. | Focused diff of `Common/StackWalker.h`. |
| The current constructor surface and base initialization remain equivalent; the leaf still has no state or construction work. | Read the current base/leaf declarations and existing `NonExcept` caller; successful consumer builds. |
| Logging, filtering, locking, other constructors, and documentation remain unchanged. | Final source diff changes only the specified constructor region. |
| All consumers compiling the existing call accept the inherited constructor. | Successful `/compile` results for DataPacker, BrokenEngineSandbox client, and BrokenEngineSandbox server, each Release\|x64. |

## Verification

Inspect the diff and constructor/caller evidence to settle the source and invariant criteria. Run the repository's applicable C++ correctness, comment, style, affected-code, and documentation checks; report no-op propagation and documentation outcomes where appropriate. Do not expand the edit for optional cleanup.

Use `/compile` through its delegated builder for DataPacker Release|x64, BrokenEngineSandbox client Release|x64, and BrokenEngineSandbox server Release|x64. All three compile `Common/Determinism.cpp`; AgentHarness and WorktreeCli are not affected build targets. The planned source change qualifies for Shared runtime data mode because it touches no asset, exporter, generated-header, or packed-data contract. No Local generation or PREfast is requested. No runtime harness scenario is needed: the unchanged emission implementation and constructor equivalence are settled by inspection and compilation.

Plan authoring validates only this executable Plan's metadata and content. Implementation, reviews, and builds above remain future work and must not be reported as already passed.
