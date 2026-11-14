<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:17:06.058Z","dependsOn":[]} -->
# Create the UI prepass indirect buffer through Buffer::CreateBuffer

## Context

Lines are those at commit `401e4b75`.

`ImGuiManager::CreateUiPrepassIndirectBuffer`
(`Engine/Source/Graphics/Managers/ImGuiManager.cpp:313-338`) fills the buffer
and allocation create infos by hand (`:316-331`). `Buffer::CreateBuffer`
(`Engine/Source/Graphics/Objects/Buffer.cpp:10-54`) with usage
`INDIRECT_BUFFER` and `HOST_VISIBLE | HOST_COHERENT` produces the same create
info: `VMA_MEMORY_USAGE_AUTO`, `requiredFlags` equal to the memory flags, the
`HOST_ACCESS_SEQUENTIAL_WRITE | MAPPED` allocation flags (`Buffer.cpp:41`,
because the usage has no `TRANSFER_DST`), exclusive sharing, `VkName`, and the
mapped pointer through the optional `VmaAllocationInfo`. The one difference:
the helper rounds the size up to `nonCoherentAtomSize` (`Buffer.cpp:12`).

Sibling host-visible buffers already use the helper
(`Engine/Source/Graphics/Islands.cpp:60`,
`Engine/Source/Graphics/Objects/PipelineCreator.cpp:90`). This is the one
host-visible allocation in the Managers directory that encodes the flag
contract of `Engine/Source/Graphics/Objects/AGENTS.md`
`## Resource Contracts` outside its owner.

Originating gap: the architecture phase (cohesion lens) of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criterion: the buffer
is created through `Buffer::CreateBuffer`.

## Design

Recommended: replace `:316-331` with one `Buffer::CreateBuffer` call that
passes a `VmaAllocationInfo` and takes the mapped pointer from it. The
destroy at `ImGuiManager.cpp:294-296` is unchanged. A buffer rounded up to
the atom size is harmless here: the extra bytes are never read.

## Critical files

- `Engine/Source/Graphics/Managers/ImGuiManager.cpp`

## In scope

- The creation statements in `ImGuiManager::CreateUiPrepassIndirectBuffer`.

## Out of scope

- The buffer's destroy path, its contents, and its consumers.
- `Buffer::CreateBuffer` itself.

## Risk tier

Tier 2 — scoped behavior. Trigger: the allocation request of one buffer
changes (its size may round up), which is runtime behavior of one subsystem
and more than Tier 1's behavior-preserving work. No threading, determinism,
layout, or wire exposure.

## Acceptance criteria

1. `CreateUiPrepassIndirectBuffer` calls `Buffer::CreateBuffer` and sets no
   create-info field itself.
2. The BrokenEngineSandbox client and ModelViewer build through `/compile`.
3. Through `/agent-harness`, a client run with validation layers on shows the
   UI over a world frame, before and after one window resize, with no
   validation error. The supplied evidence records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
