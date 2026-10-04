<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T19:42:09.100Z","dependsOn":[]} -->
# Use std::ignore for existing explicit result discards

## Context

At baseline `a0cb6acb`, four calls explicitly discard a non-void result with `static_cast<void>`. The accepted P2968R2 adoption is a small readability improvement, not a bug fix or a claimed speedup. `std::ignore` already expresses the same intent elsewhere in the engine, including `Engine/Source/Main.cpp:199` and `Engine/Source/Graphics/Managers/TextureUploadManager.cpp`.

[P2968R2, section 8](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2968r2.html#wording) specifies assignment through a const reference with a constexpr, noexcept operation that returns the ignore object without examining the argument. For these four non-volatile object results, the assignment adds no copy, allocation, or exception. The right-hand call still executes once and can still throw. No C++ language-mode change is needed; the repository already uses this library idiom.

## Design

The author recommends replacing only the four existing `static_cast<void>(call());` expressions with `std::ignore = call();`, because this states the intentional discard directly without changing which results the program checks.

| Existing site | Concrete use and preserved behavior |
|---|---|
| `Engine/Source/Main.cpp:80`, `HandleEagerLoadCompletion` | Discard `gpFileManager->mpPackChunks->GetEagerChunkMap()`'s map reference while retaining eager-load completion and its surrounding exception handling. |
| `Engine/Source/File/PackChunks.cpp:587`, `PackChunks::WaitForChunks` | Discard `GetEagerChunkMap()`'s map reference before the unchanged loader wait; retain eager-load failure propagation. |
| `Engine/Source/Agent/Commands/AudioStreamingFixture.cpp:735`, `AudioStreamingFixture::RunInvalid` | Discard only the initial valid `TryReadChunkData` status between the missing-CRC and wrong-CRC checks. Preserve request submission, buffers, subsequent comparisons and reset. This code is client Debug only. |
| `Engine/Source/File/PackChunkLoader.cpp:400`, `PackChunkLoader::LoadChunk` | Discard `ReadFile`'s existing Boolean result while preserving its arguments, output byte count, copy-size calculation and following assertion. |

The map expressions bind directly to the ignore assignment's const-reference parameter; do not introduce a local map value or other intermediate. The scalar return values need no ownership or lifetime changes. Existing `Common/ExternalHeaders.h:109,113` includes `<tuple>` and `<utility>`, so no include or PCH change is proposed.

The author recommends appending one sentence to `Documents/C++StyleGuide.txt` rule 11: "For an intentionally discarded non-void function result, use std::ignore = call(); rather than a cast to void; retain required result checks." This belongs beside the cast convention and avoids turning a syntax preference into permission to ignore errors. No AGENTS.md invariant changes: no contract or routing changes here.

## Critical files

- `Engine/Source/Main.cpp` — `HandleEagerLoadCompletion`.
- `Engine/Source/File/PackChunks.cpp` — `PackChunks::WaitForChunks`.
- `Engine/Source/Agent/Commands/AudioStreamingFixture.cpp` — `AudioStreamingFixture::RunInvalid`.
- `Engine/Source/File/PackChunkLoader.cpp` — existing `ReadFile` discard in the whole-chunk read loop.
- `Documents/C++StyleGuide.txt` — rule 11 only.

## In scope

- The four discard expressions identified above, keeping each called expression byte-for-byte apart from its discard wrapper.
- The single rule-11 sentence described in Design.

## Out of scope

- Every currently unchecked call, all checked results, and other discard expressions.
- The lifetime marker `(void)pLifetime` in `Projects/BrokenEngineSandbox/Source/Agent/Commands/ClientSubscriptionFixtures.cpp:317`; it is not a non-void call result.
- Error handling, I/O policy, request lifetime, assertion changes, and performance refactors.
- Library wrappers, feature-test macros, language-mode/toolchain changes, new headers, project membership, and AGENTS.md edits.
- Unit tests, new harness scenarios, asset generation, and broad style cleanup.

## Acceptance criteria

1. Exactly the four listed calls use `std::ignore =` instead of a cast to void. All arguments, ordering, conditional compilation, exception handlers, result comparisons, byte-count handling and assertions remain unchanged.
2. The two map-reference results bind without map construction, copy, move, or allocation; scalar results are discarded without added work in optimized production code.
3. Rule 11 contains the narrow intentional-result-discard convention; no additional style rule or instruction-file change is introduced.
4. BrokenEngineSandbox client and server build in Release x64, and the client builds in Debug x64 to compile `AudioStreamingFixture::RunInvalid`.

## Verification

Before editing, record the implementation baseline. Inspect the installed STL ignore-assignment definition and the two `GetEagerChunkMap` return signatures to confirm reference binding and the no-op assignment body. Review the diff against the four expressions and their surrounding statements. Build the targets above through `/compile`, using Shared runtime data because this source-only edit changes no assets or data layout.

For the optimized no-regression check, compare before/after Release call-site disassembly for the two eager-map calls and the `ReadFile` call using the same compiler/settings, retaining evidence under `Temp/`. Resolve relocated addresses symbolically; confirm unchanged calls/control flow and no added ignore-assignment call, map copy, or allocation. The Debug-only fixture is covered by its Debug build and the unchanged-expression review, not a production benchmark. No live runtime check is required when the source proof and optimized check agree; no timing claim is made.

Run the applicable C++ correctness, affected-code, comment and style review routes, and coherence review for the style guide. `/update-claude-docs` verifies that unchanged contracts need no AGENTS.md edit. Future landing follows `/finalize-changes` after applicable acceptance verification.

## Notes

- Change Workflow Tier 1: four local behavior-preserving expressions and a narrow style clarification, with no public-signature or invariant exposure. Existing I/O and asynchronous side effects remain in place; determinism/CRC, threading, wire, save/replay, serialization and `.pack` formats do not change.
- Dependencies: none. No Coordination constraint is needed. `Documents/Plans/Engine/RemoveEngineAnonymousNamespaces.md` overlaps two files but owns separate linkage/namespace edits; neither plan requires the other and symbol-based locations survive either order.
- This plan is self-contained and does not depend on an investigation report remaining present.
