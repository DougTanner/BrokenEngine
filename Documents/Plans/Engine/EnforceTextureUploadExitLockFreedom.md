<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:26:28.738Z","dependsOn":[]} -->
# Enforce lock-free texture upload exit polling

## Context

`TextureUploadManager::RethrowException` in `Engine/Source/Graphics/Managers/TextureUploadManager.cpp` returns after an acquire-load of `mbThreadExited` on the healthy path. Its local comment promises lock-free per-frame polling, and the member comment in `TextureUploadManager.h` repeats the guarantee. `Engine/Source/Main.cpp` calls this function immediately before rendering. The guarantee currently relies on an unchecked property of the atomic implementation.

The client project, `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`, supports Debug, Profile, and Release x64 with C++23 and `BT_CLIENT`. The manager header is already client-guarded. No existing Plan owns this assertion.

## Design

Immediately after the existing private member declaration `std::atomic<bool> mbThreadExited {false};` and its existing trailing comment, add this class-scope declaration, indented with one tab:

```cpp
static_assert(decltype(mbThreadExited)::is_always_lock_free);
```

The unparenthesized member name in `decltype` yields its declared atomic type. Its static constexpr `is_always_lock_free` property makes failure of the existing promise a compilation error. Referencing the member type keeps enforcement attached to the actual polling flag if its type changes. Preserve the declaration, initializer, and comment verbatim; place the assertion before `mUploadMutex`. Add no message string or helper.

## Critical files

- `Engine/Source/Graphics/Managers/TextureUploadManager.h`: sole implementation edit, immediately after `mbThreadExited`.
- `Engine/Source/Graphics/Managers/TextureUploadManager.cpp`: read-only evidence, `RethrowException`, exit publication, and `WaitIdle`.
- `Engine/Source/Main.cpp`: read-only evidence of the per-frame call.
- `Engine/Source/Graphics/Managers/AGENTS.md`, TextureUploadManager section: existing ownership, drain handshake, and fatal publication contract.
- `Documents/C++StyleGuide.txt`, rule 31: existing compile-time assertion convention.
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandbox.vcxproj`: supported client configurations and header membership.

## In scope

Add exactly one member-type lock-free `static_assert` adjacent to `TextureUploadManager::mbThreadExited` in its existing client-only class definition.

## Out of scope

Other atomics, engine-wide lock-freedom policy, runtime checks, replacing the flag with `atomic_flag`, synchronization redesign, project changes, new includes, comment cleanup, unit tests, and runtime instrumentation are excluded. Do not change the header guard, member initialization, memory ordering, exception publication, drain handshake, or polling call sites.

## Risk and invariants

Tier 1: a local compile-time assertion enforces an already documented property without changing a signature, object layout, or threading behavior. The only new failure mode is a compile error when the member type is not always lock-free. Supported client configurations must compile successfully; do not weaken the assertion or add a runtime fallback to accommodate failure.

Preserve all acquire/release operations, mutex ownership, exception consumption, and teardown ordering. Preserve simulation determinism, CRC, serialization, and save/replay compatibility; this client-only assertion has no runtime effect on those surfaces. The assertion does not claim that exceptional handling or all of `WaitIdle` is lock-free.

## Performance

The assertion generates no runtime instructions or storage and introduces no allocation, synchronization, or polling work. Its benefit is compile-time enforcement of an existing hot-path guarantee, not a claimed speedup.

## Documentation and style

No documentation or style-guide edit is needed. The existing member and `RethrowException` comments already state the precise guarantee; the Managers AGENTS.md already owns publication and drain behavior, which stay unchanged. Rule 31 already recommends `static_assert` where possible. Preserve surrounding comments and formatting rather than adding duplicate rationale or a general atomic policy. Run the workflow's applicable C++ style, comment, and documentation checks; a no-change documentation result is expected.

## Verification

During implementation, invoke `/compile` to build the BrokenEngineSandbox client in each existing Debug, Profile, and Release x64 configuration using the skill's supported build path. Successful compilation proves the actual member type meets the assertion on the supported toolchain. No server build is needed for this client-guarded declaration, and no `/agent-harness` run or unit test is needed for a compile-time-only change.

Inspect the final source diff to confirm the sole code change is the exact assertion following `mbThreadExited`, with no changes to initialization, layout-bearing declarations, operations, comments, or project membership. Do not run builds during plan authoring.

## Acceptance criteria

1. `TextureUploadManager.h` contains exactly `static_assert(decltype(mbThreadExited)::is_always_lock_free);` immediately after the existing `mbThreadExited` declaration and comment, inside its existing client guard.
2. Client Debug, Profile, and Release x64 builds through `/compile` succeed with the assertion enabled.
3. The source diff contains only that added declaration; runtime behavior, memory ordering, layout, and unrelated atomics remain unchanged.
4. Documentation/style review confirms existing comments, Managers AGENTS.md, and style rule 31 remain accurate without expansion.
