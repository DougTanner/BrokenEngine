<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:27:41.757Z","dependsOn":[]} -->
# MakeAligned Fixed Alignment Assertion

## Context

`Common/AlignedMemory.h` defines `common::MakeAligned<T>(int64_t iCount)`. At baseline `d29fed456d3ede935c5e672f95f13d6733f0660c`, its body checks trivial default construction and destruction at compile time, calculates the byte count, checks count/overflow at runtime, and allocates with `_aligned_malloc(uiBytes, 64)`. A type requiring alignment above 64 cannot be guaranteed suitable storage by this allocator. Reject that unsupported instantiation explicitly.

The current instantiations are `std::byte` in `Engine/Source/Frame/Collections/CollectionMemory.h` (lines 136, 217, 340) and `XMMATRIX` in `Engine/Source/Graphics/AnimationData.cpp` (lines 165, 188, 195). The existing 64-byte allocation supports both. `Common/AGENTS.md` already documents 64-byte-aligned MakeAligned storage, and style rule 31 already favors `static_assert`; no documentation or style-policy amendment is warranted.

## Design

Add exactly one assertion beside the existing two type assertions in `common::MakeAligned<T>`:

```cpp
static_assert(alignof(T) <= 64, "MakeAligned requires a type with alignment no greater than 64 bytes");
```

Keep the literal limit consistent with the existing allocation expression. No helper, configuration, new constant, or runtime branch is needed. The assertion changes compile-time diagnostics for unsupported types and introduces no runtime work.

## Critical files

- `Common/AlignedMemory.h` — sole implementation edit, in `common::MakeAligned<T>`.
- `Engine/Source/Frame/Collections/CollectionMemory.h` and `Engine/Source/Graphics/AnimationData.cpp` — read-only call-site verification.
- `Common/AGENTS.md` and `Documents/C++StyleGuide.txt` rule 31 — existing contract and style authorities; read-only.

## In scope

- Add the stated fixed-alignment `static_assert` to the body of `common::MakeAligned<T>` immediately after its current type assertions.
- Reconfirm existing template instantiations remain supported and compile both game targets.

## Out of scope

- Changing the 64-byte allocation alignment, allocation size, signature, deleter, ownership, failure behavior, or existing count/overflow assertion.
- Replacing runtime count validation with a compile-time assertion: `iCount` is not a constant expression.
- Imposing trivial-copyability or any other additional type requirement.
- Changing `Workbuffer::PushBack`; its separate trivial-copy constraint adoption owns that work and is independent of this Plan.
- Broader assertion adoption, documentation/style policy changes, tests, data regeneration, or runtime instrumentation.

## Risk and invariants

Future implementation is Tier 2: it tightens the accepted type contract of a public Common template at its existing allocation boundary. Allocation behavior, formats, layout, trust, threading, and deterministic computations remain unchanged; no Tier-3 surface changes.

For supported types, retain the existing construction/destruction requirements, fixed allocation alignment, byte count and overflow validation, null-on-allocation-failure behavior, and `_aligned_free` ownership. There must be no additional generated runtime work or performance cost.

## Acceptance criteria

1. `MakeAligned<T>` contains the exact `alignof(T) <= 64` assertion and a diagnostic stating the 64-byte limit, alongside its existing type assertions.
2. The allocation expression, runtime count check, type constraints already present, signature, and deleter are unchanged.
3. Existing `std::byte` and `XMMATRIX` instantiations compile successfully in the client/server consumers.
4. The implementation diff is confined to this assertion; no documentation or style-policy amendment is introduced.

## Verification

- Inspect the diff to settle criteria 1, 2, and 4, including the absence of runtime expressions added by the change. Re-run a repository search for `MakeAligned<` to identify any intervening call-site additions.
- Through `/compile`, build BrokenEngineSandbox Client and Server in `Release|x64` to settle criterion 3. Use Shared data mode for this header-only compile-time contract change; it changes no asset or pack contract and requires no Local generation. No PREfast run is required.
- No `/agent-harness` scenario is needed: supported runtime behavior is unchanged, and the relevant new behavior is compile-time rejection. Do not add unit tests or permanent negative-compilation fixtures; the explicit constant-expression predicate and normal consumer builds provide decisive evidence.
- These are future implementation checks, not checks performed while writing this Plan.
