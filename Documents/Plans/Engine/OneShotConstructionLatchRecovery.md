<!-- broken-engine-plan/v1 {"createdUtc":"2026-08-26T23:32:16.726Z","dependsOn":[]} -->
# Fix: Roll back the one-shot latch when construction fails

## Context

The accepted survivor `CAI/shard-0027/004` shows that
`OneShotCommandBuffer` sets the process-wide `sbInUse` latch before Vulkan
allocation and begin, while only the destructor clears it
(`Engine/Source/Graphics/OneShotCommandBuffer.cpp:8-44`).  A throwing
`CHECK_VK` during either constructor call skips the destructor.  Graphics boot
and recreation then construct another one-shot buffer for placeholder or
texture uploads, so the stale latch triggers the assertion before recovery can
complete.

The source report is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/shard-0027.md:113`;
the consolidated selector is
`Temp/CppAdversarialInvariantAudit/76d303f0eeeb86c1ed241edc81634e60070ba5a5/consolidated-index.md:799`.
Its 14 frozen target rows matched and successful one-shot lifetimes were
otherwise serialized.  The issue is pre-existing at frozen commit
`76d303f0eeeb86c1ed241edc81634e60070ba5a5`; this routing session has not
edited source.

Impact: one recoverable allocation/begin failure permanently poisons the shared
one-shot pool and turns the next Graphics construction into client termination.

## Design

Author's recommendation: wrap the constructor's post-latch work in a local
rollback guard.  If allocation or begin throws, free a non-null command buffer
through the existing command-pool cleanup call, clear `sbInUse`, and rethrow;
release the guard only after the command buffer is fully begun.  Keep the
destructor's normal free-and-clear path for successful construction and retain
the single-owner assertion during execution.

## Critical files

- `Engine/Source/Graphics/OneShotCommandBuffer.cpp:8-50` — latch acquisition,
  Vulkan construction, and destructor release.
- `Engine/Source/Graphics/OneShotCommandBuffer.h:8-23` — null handle state and
  RAII surface.
- `Engine/Source/Graphics/Managers/DeviceManager.cpp:236-260` — shared pool
  and fence lifetime (reference for cleanup ordering).

## In scope

- Exception-safe rollback for `sbInUse` when
  `vkAllocateCommandBuffers` or `vkBeginCommandBuffer` fails.
- Cleanup of the partially allocated command buffer when the handle is
  non-null, without changing the shared pool/fence ownership model.
- Preserving the successful `Execute`/destructor serialization and client-only
  one-shot API.

## Out of scope

- Graphics full-teardown creation, Vulkan result classification, one-shot
  scheduling or parallelism, and texture upload policy.
- Adding a second command pool, changing fence ownership, or repairing a stale
  latch from an unrelated caller.
- Server code, simulation state, wire/save/replay formats, and unit tests.

## Risk tier and invariants

Expected Change Workflow Tier 2 (downgraded from Tier 3). Trigger: construction
failure crosses Vulkan resource lifetime, a process-wide ownership latch,
Graphics recovery, and single-threaded command-pool use; threading and
device-recovery integration are higher-risk surfaces.

Preserve these invariants:

- At most one live one-shot command buffer owns the shared pool/fence.
- Every failed constructor leaves the latch clear and does not leak a command
  buffer that remains usable by a later attempt.
- A successful constructor keeps the latch until normal destruction, including
  `Execute` and fence completion.
- No client/server deterministic, wire, save, replay, or `.pack` data changes.

Tier rationale: the Design fully specifies a local rollback guard inside one
constructor that runs only on an already-failing path. No ownership model,
threading structure, or serialized data changes, and every successful
construction keeps its current latch and destructor behavior.

## Acceptance criteria

- A forced failure from command-buffer allocation or begin leaves
  `sbInUse == false`, frees any partial handle, and rethrows the original
  failure.
- The next one-shot construction on a replacement Graphics/device succeeds
  instead of hitting the stale-latch assertion.
- Normal overlapping construction still asserts/rejects, and successful
  execute/destruction still releases the latch once.
- Client Debug and Release builds pass `/compile`; a harness device-loss or
  upload-retry scenario, where exposed, completes the next one-shot upload with
  no stale-latch error.

## Notes

The cleanup must remain valid during the existing device-loss teardown path;
do not add a second recovery reset that could hide overlapping live ownership.
