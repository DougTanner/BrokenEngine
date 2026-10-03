<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:10:20.711Z","dependsOn":[]} -->
# Require trivially copyable Workbuffer elements

## Context

`common::Workbuffer::PushBack<T>(const T&)` in `Common/Workbuffer.h:70` appends `sizeof(T)` bytes with `std::memcpy` but does not constrain `T`. A non-trivially-copyable type can therefore instantiate a byte-copy operation that does not implement its copy semantics. `Common/Serialization.h:66` already uses `static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable")` for its typed binary-copy contract.

The current typed append callers are `Engine/Source/Network/Client/ClientSessionRuntime.cpp:532,541` (`GridCoord`), `Engine/Source/Network/Server/ServerTransferManager.cpp:112,169,359` (`ClientTransferInfo` and `common::crc_t`), and `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.cpp:22,29` (`ReceivedPlayerEvent`). These are scalar values or aggregates of trivially copyable values. Default member initializers do not require trivial default construction. The forwarding `ScopedWorkbufferArena::PushBack` reaches the same implementation.

## Design

Insert `static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");` as the first statement in `Workbuffer::PushBack`, before the existing depth assertion and byte copy. Keep the assertion at this single implementation boundary; the arena wrapper needs no duplicate. This introduces compile-time rejection without changing any runtime instruction, byte representation, allocation, or size accounting.

Add this sentence to the first paragraph of `Common/AGENTS.md` `## Allocation-Free Scratch`: "`PushBack<T>()` requires a trivially copyable element type because it appends the object's bytes."

No amendment to `Documents/C++StyleGuide.txt` is needed: rule 31 already favors compile-time assertions. Do not impose `is_trivial`, `is_pod`, or `is_standard_layout`; none describes the required byte-copy property as precisely. The assertion does not validate alignment, padding, or unrelated object-lifetime conditions.

## Critical files

- `Common/Workbuffer.h` — `Workbuffer::PushBack<T>` is the sole code edit; inspect `ScopedWorkbufferArena::PushBack<T>` to confirm forwarding.
- `Common/AGENTS.md` — the allocation-free scratch contract owns the documentation update.
- `Common/Serialization.h` — read-only precedent for the trait and diagnostic.
- The three caller files named in Context and their payload declarations in `Engine/Source/Frame/GridCoord.h`, `Engine/Source/Network/Server/ServerTransferManager.h`, `Projects/BrokenEngineSandbox/Source/Network/PlayerEvents.h`, `Common/Crc.h`, `Engine/Source/Frame/Collections/CollectionId.h`, and `Engine/Source/Network/NetworkProtocol.h` — read-only compatibility evidence.

## In scope

- Add the single specified assertion to `common::Workbuffer::PushBack<T>`.
- Add the specified public contract sentence to `Common/AGENTS.md` `## Allocation-Free Scratch`.
- Inspect all current typed append instantiations and compile the client and server that instantiate them.

## Out of scope

- Changes to `PushBuffer`, `Data`, `Span`, `Count`, the arena wrapper, or any caller or payload type.
- Other assertion adoption, including layout/offset assertions; `GraphicsSettings.cpp`; repository-wide trait policy or style-guide changes.
- Alignment, padding, lifetime, capacity, overflow, or Workbuffer allocation changes; serialization, wire, save, replay, and CRC changes.
- New tests, runtime harness scenarios, benchmarks, data generation, and project membership changes.

## Risk and invariants

Future implementation is Tier 2: this tightens the accepted type contract of a public Common template at its existing copy boundary. It exposes an invariant and is therefore beyond Tier 1, while formats, trust, runtime behavior, and independently owned consumers stay unchanged.

Preserve the function signature, wrapper forwarding, `memcpy`, growth branch, depth check, size accounting, byte contents, layout, allocation behavior, and all valid callers. Preserve support for trivially copyable types with default member initializers. Do not broaden implementation to repair an unexpected incompatible caller: report it for scope reassessment. No prerequisites or coordination with other assertion plans are required.

## Acceptance criteria

1. The only C++ delta is the exact `std::is_trivially_copyable_v<T>` assertion in `Workbuffer::PushBack`; any non-trivially-copyable instantiation encounters that unconditional compile-time assertion.
2. Existing client and server typed append instantiations compile without caller or payload changes.
3. The precise contract is documented once in `Common/AGENTS.md`; the style guide remains unchanged.
4. Runtime operations, byte/layout contracts, and allocation behavior are unchanged by inspection.

## Verification

At implementation time, search all `PushBack` calls to confirm the caller inventory, inspect their payload members, and review the scoped diff against criteria 1, 3, and 4. The unconditional assertion itself is decisive rejection evidence; do not add a negative-compilation fixture or unit test.

Use `/compile` to build `BrokenEngineSandbox` and `BrokenEngineSandboxServer`, each `Release|x64`, to settle criterion 2. Use Shared runtime data because the scoped compile-time assertion and documentation edit cannot change generated assets or serialized bytes; no Local generation or PREfast is required. No `/agent-harness` run or benchmark is needed because there is no runtime-observable change or runtime overhead. Complete the applicable C++ and documentation workflow reviews when implementing.

These are future required checks, not results claimed by this Plan.
