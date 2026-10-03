<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:09:42.571Z","dependsOn":[]} -->
# Encode RunExecutable handle cleanup in its local deleter type

## Context

`Common/WindowsUtils.cpp:142`, `common::RunExecutable`, defines `ScopedHandle` at line 146 as `std::unique_ptr<void, decltype(&CloseHandle)>`. Its six guards at lines 163-164, 172-173, and 221-222 repeat `&CloseHandle` even though every guard has the same immutable cleanup operation. This pre-existing redundancy is the accepted C++20 adoption opportunity: captureless closure default construction and lambda expressions in unevaluated contexts address the same implementation boundary and belong in one change. This is a clarity refactor, not a leak fix.

## Design

Recommendation: replace only the local alias with the following form, then remove the second argument from all six existing guard constructors:

```cpp
using ScopedHandle = std::unique_ptr<void, decltype([](HANDLE hHandle) noexcept
{
	CloseHandle(hHandle);
})>;
```

This represents the invariant cleanup operation once without a named runtime callable or shared abstraction. Keep the empty capture list exactly `[]`; do not convert the closure to a function pointer. The six objects remain `pStdInPipeRead`, `pStdInPipeWrite`, `pStdOutPipeRead`, `pStdOutPipeWrite`, `pProcess`, and `pThread` with their existing handle arguments and declaration positions.

The language basis is [P0315R4, lambdas in unevaluated contexts](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0315r4.pdf), [captureless closure default construction](https://eel.is/c++draft/expr.prim.lambda.closure), and the [unique_ptr pointer-only constructor](https://eel.is/c++draft/unique.ptr.single.ctor), which value-initializes a non-pointer default-constructible deleter. The repository already targets C++23.

Preserve nullptr as the sole invalid sentinel, the same one `CloseHandle` call for each non-null owned handle, the ignored BOOL result, every launch-failure return, and reverse destruction order. In particular, keep `pStdOutPipeWrite.reset()` and `pStdInPipeRead.reset()` before the read loop so EOF behavior stays identical; retain the attribute-list guard and its backing buffer order. No raw Win32 argument, handle inheritance flag, output collection, wait, or exit-code operation changes.

Performance reasoning: the closure contains no runtime state and its body is the existing OS call. Installed MSVC 14.51.36231 `include/memory` lines 3367-3399 and 3454-3457 implement default deleter construction and the same null-guarded invocation; `include/xmemory` lines 1542-1564 stores an empty deleter as a base, instead of the old function-pointer member. No allocation, loop, synchronization, lookup, or extra OS call is introduced. These are implementation observations, not measured speedup or size claims; record actual compiled size and optimized cleanup evidence during implementation verification.

## Critical files

- `Common/WindowsUtils.cpp:common::RunExecutable` — local alias and six owner constructions, the only proposed source edits.
- `Common/AGENTS.md` — shared-utility and Windows wrapper constraints; review-only.
- `Documents/C++StyleGuide.txt` — rules 2 (lambda braces) and 5 (RAII); review-only.
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj` and both `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox*.vcxproj` — existing consumers compiling WindowsUtils.cpp; verification-only.

## In scope

Only `common::RunExecutable`'s `ScopedHandle` alias and the deleter arguments of its six existing guard constructors. Existing comments may be reviewed for continued truth; the expected comment delta is none.

## Out of scope

Other Windows wrappers, call sites, public signatures, new general-purpose handle wrappers, invalid-sentinel changes, ownership ordering, error handling, captured deleters, comparator/hash conversions, closure assignment adoption, project membership, headers, unit tests, and runtime behavior changes.

## Acceptance criteria

1. Source review finds one local captureless lambda deleter type and exactly six pointer-only guard constructions, with no remaining `&CloseHandle` constructor argument in this function.
2. A before/after control-flow and lifetime review confirms the same acquired handles are closed once on all existing returns and resets, nullptr remains skipped, and the explicit EOF-establishing resets and attribute-list lifetime ordering are unchanged.
3. `/compile` successfully builds DataPacker and the BrokenEngineSandbox client and server in Release x64, the three existing projects compiling the changed translation unit. Follow that skill's target and data-mode rules; no project or build-policy edits are proposed.
4. During implementation, inspect optimized cleanup and record actual `sizeof(ScopedHandle)` versus the prior function-pointer-deleter type using temporary compiler/debugger evidence. Confirm no increased owner storage or added allocation/OS operation; do not require an instruction-count improvement or claim faster launches. Keep verification instrumentation out of tracked source.
5. Review documentation via `/update-claude-docs`, plus the normal C++ correctness, style, comment, and affected-code passes. The local implementation changes no documented contract or style policy; expect no AGENTS.md or style-guide update. Rules 2 and 5 already cover the proposed spelling and ownership.

## Notes

Change Workflow Tier 1: local behavior-preserving work with no public signature or invariant exposure. The private automatic guard representation is not persisted or exposed; determinism/CRC, wire formats, serialization, packed assets, save/replay compatibility, affinity, and threading remain unaffected. This is process-launch tooling and does not require game `/agent-harness` verification. Source lifetime review, normal compilation, and focused compiled cleanup evidence settle this bounded refactor; no unit tests are proposed.

Planning performed source and duplicate inspection only; no build or runtime result is claimed. Searches across existing Plans for WindowsUtils, RunExecutable, ScopedHandle, CloseHandle, captureless lambdas, and unevaluated contexts found no matching root cause and boundary. Existing lambda deduction and collection-lambda Plans concern different sites. No dependencies or Coordination constraints are needed. This one Plan owns both accepted adoption findings.
