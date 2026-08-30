<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-30T12:56:08.676Z","dependsOn":[]} -->
# Remove future-only shared shader layout declarations

## Context

The false required condition is that every declaration in the shared shader
layout header must remain available even without a current CPU or GLSL
consumer. `ObjectLayout` and `ModelCustomLayout` are declared in
`Engine/Data/Shaders/ShaderLayoutsBase.h:237-287`, but exact CPU/GLSL searches
find no object buffer, writer, binding, or shader use. Current model rendering
uses `ModelLayout`, and the shield path uses `HexShieldLayout`. The manual
`Documents/Features/Graphics/DestructionBuffer.md:57-61` document is a future
feature proposal, not a consumer.

The originating candidate is `CPS/shard-0003/003`. The user explicitly directs deletion of the two
future-only declarations while preserving live layouts, CPU/GLSL contracts,
and the manual feature document. The concern is pre-existing at session
baseline `80896f33661aaab99cf180a96db54600099be652`.

## Design

The author's recommendation is to delete only `ObjectLayout` and
`ModelCustomLayout` from `ShaderLayoutsBase.h`. Keep `ModelLayout`,
`HexShieldLayout`, all live shared constants, bindings, field order, scalar
layout, and the manual DestructionBuffer feature document unchanged. Treat
the feature document's mention of a possible `ModelCustomLayout` decision as
future planning prose, never as proof of a current consumer; do not rewrite it
into a capability or add a replacement declaration.

Compile both language sides of the shared header and every affected shader
after the deletion. If generated shader dependencies or comments mention one
of the removed names, update only those stale references; do not alter the
live model/shield layouts or generated binding numbers.

## Critical files

- `Engine/Data/Shaders/ShaderLayoutsBase.h:237-287` — the two declarations and adjacent live layouts.
- `Engine/Data/Shaders/AGENTS.md:5-14` — shared CPU/GLSL layout and binding contract.
- `Documents/Features/Graphics/DestructionBuffer.md:57-61` — manual future-feature reference, not a consumer.
- `Engine/Data/Shaders/Model/` and `Engine/Data/Shaders/Objects/` — compile/read-only verification of live layout consumers.

## In scope

- Removing `ObjectLayout` and `ModelCustomLayout` declarations from the shared
  C++/GLSL header.
- Rechecking all CPU and GLSL references, generated shader dependencies, and
  comments for stale names, with only meaning-preserving cleanup where needed.
- Compiling both `BT_ENGINE` language modes and the live Model/HexShield shader
  consumers after the header change.

## Out of scope

- `ModelLayout`, `HexShieldLayout`, `PbrMaterialLayout`, `BillboardLayout`,
  shared constants, bindings, field order, scalar layout, or live shader code.
- Implementing DestructionBuffer, extending either model layout, adding a
  consumer, changing CPU/GPU buffers, changing descriptor sets, or changing
  `.pack`, wire, save, replay, or CRC formats.
- The ownerless Billboards cleanup's coordinated `BillboardLayout` deletion,
  except for the shared-header coordination below.

## Risk tier and invariants

Expected Change Workflow Tier 3. Trigger: `ShaderLayoutsBase.h` is a dual-
language CPU/GPU contract consumed by client/server C++ and Vulkan GLSL;
deleting declarations must not shift or reinterpret any live layout or binding.

Preserve these invariants:

- C++ and GLSL preprocessing produce the same remaining declarations and live
  field layouts.
- `ModelLayout` and `HexShieldLayout` sizes, offsets, bindings, and descriptor
  writes remain unchanged.
- The manual DestructionBuffer document remains a manual future plan and does
  not become a current consumer or capability.
- No runtime resource, shader output, deterministic state, wire/save/replay
  format, or CRC behavior changes.

## Acceptance criteria

- Exact CPU/GLSL searches find no current `ObjectLayout` or
  `ModelCustomLayout` declaration/use outside explicitly preserved future
  documentation, and no new consumer or replacement is added.
- Client and server C++ compilation plus DataPacker shader compilation pass;
  live Model and HexShield shader reflection/layout checks remain unchanged.
- `ModelLayout` and `HexShieldLayout` field order/offsets and all descriptor
  bindings compare equal to the pre-change contract.
- The DestructionBuffer feature document remains present and clearly manual;
  it is not cited as a runtime consumer.
- No unit tests are added.

## Coordination

`Documents/Plans/Engine/OwnerlessBillboardsRemoval.md` also changes
`Engine/Data/Shaders/ShaderLayoutsBase.h` by removing the separately unused
`BillboardLayout`. Neither Plan depends on the other. Keep the two deletions
disjoint and review the shared header once, preserving all live layouts and
binding declarations.

## Notes

The CPS triage and feature document are provenance/reference evidence. The
shared-header and shader authority files above are the durable implementation
contract.
