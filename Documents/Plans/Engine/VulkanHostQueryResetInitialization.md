<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T17:50:41.000Z","dependsOn":[]} -->
# Initialize fresh GPU timestamp pools with host query reset

## Context

At baseline `f9337a874c4a0736ae18fd80caf04d322ed715f6`, `ProfileManagerBase::Create` creates and names a fresh timestamp query pool, then allocates, records, submits, waits for, and frees a `OneShotCommandBuffer` solely to reset that pool. The pool has not been referenced by submitted work. `Graphics::Create` invokes profiler creation before creating and recording the rendering command buffers. Swapchain-tier teardown destroys the old pool after the existing device-work drain, so recreation also reaches a fresh pool.

Remove this initialization-only GPU submission. The concrete benefit is less initialization code and one fewer command-buffer allocation/submission/fence wait per actual pool creation. Any elapsed startup or recreation improvement is unmeasured; this change makes no steady-state frame-time promise.

## Design

Enable `.hostQueryReset = VK_TRUE` in the existing `VkPhysicalDeviceVulkan12Features` initializer in `DeviceManager::DeviceManager`, after `.scalarBlockLayout` in Vulkan declaration order. Add one matching `RequiredFeature` entry to `InstanceManager::ValidatePhysicalDeviceCapabilities`: pointer `&mVkPhysicalDeviceVulkan12Features.hostQueryReset`, name `"hostQueryReset"`, reason `"GPU timestamp pool initialization"`. Use the existing selected-device diagnostics and failure path; introduce no new capability object or probe chain.

Replace the local `OneShotCommandBuffer` construction, `vkCmdResetQueryPool`, and `Execute` in `ProfileManagerBase::Create` with:

```cpp
vkResetQueryPool(gpDeviceManager->mVkDevice, mVkQueryPool, 0, static_cast<uint32_t>(iQueryCount));
```

Keep its location after successful pool creation and naming, its full-pool range, and all existing profiling, client, already-created-pool, and timestamp-support guards. Use the core Vulkan 1.2 entry point. It returns `void`, so it has no `CHECK_VK` wrapper. The Vulkan contract requires enabled `hostQueryReset`, completed submitted references, and no concurrent host use of the reset range; a newly created unpublished pool meets the lifetime conditions. See [vkResetQueryPool](https://docs.vulkan.org/refpages/latest/refpages/source/vkResetQueryPool.html), VUIDs 02665, 02741, and 02742.

Recurring command-buffer resets stay GPU commands. Their ordering and reuse concerns are outside this initialization change.

## Critical files

| File | Role |
|---|---|
| `Engine/Source/Profile/ProfileManagerBase.cpp` | `Create` reset replacement and adjacent comments; `GpuReset`, `GpuRead`, and `Destroy` are invariant evidence. |
| `Engine/Source/Graphics/Managers/DeviceManager.cpp` | Existing Vulkan 1.2 feature enablement. |
| `Engine/Source/Graphics/Managers/InstanceManager.cpp` | Existing selected-device required-feature diagnostics. |
| `Engine/Source/Graphics/Graphics.cpp` | Read-only evidence for creation, drain, destruction, and recording order. |
| `Engine/Source/Graphics/OneShotCommandBuffer.cpp` | Read-only evidence for the removed allocation, submit, and fence wait. |
| `Engine/Source/Profile/AGENTS.md` | GPU timing contract owner. |
| `Engine/Source/Graphics/Managers/AGENTS.md` | Feature validation/enablement and shared one-shot ownership contracts. |

## In scope

- Only the three-statement initialization reset sequence and affected comments in `ProfileManagerBase::Create`.
- Only the `hostQueryReset` member in `DeviceManager::DeviceManager`'s existing Vulkan 1.2 feature initializer.
- Only one `hostQueryReset` entry in `InstanceManager::ValidatePhysicalDeviceCapabilities`'s existing required-feature array.

## Out of scope

- `GpuReset`, `GpuRead`, recurring reset ranges, timestamps, smoothing, query-result flags, and GPU timer enums.
- Shared `OneShotCommandBuffer`, its command pool/fence, or any other caller.
- New waits, barriers, locks, helpers, fallback paths, extension aliases, or optional capability handling.
- Unrelated feature bits, device selection, graphics lifecycle changes, or persistent performance instrumentation.
- Project membership, shader changes, deterministic simulation state, and unit tests.

## Invariants and risks

- The reset only follows successful creation of this fresh pool. The already-created-pool return must remain ahead of it; a later refactor moving reset onto a reused pool would require new lifetime analysis and is outside this plan.
- No submitted command or concurrent host reader/resetter may reference the new pool before this call. Preserve the existing create-before-record and teardown ordering; do not add synchronization to compensate for moving the call.
- Profiling-disabled, server, and unsupported-timestamp paths still create and reset no pool. Device feature enablement and its matching diagnostic are unconditional, like the surrounding required-feature entries.
- Keep query count, first index, device ownership, and creation/destruction pairing unchanged.
- Feature enablement is required even though the core command is available. The existing diagnostic must identify `hostQueryReset` if a device reports it unavailable; no fallback is authorized.
- Vulkan validation errors, startup failure on the supported test device, missing GPU samples after recreation, or a new wait in recurring profiling are acceptance failures.

## Implementation steps

1. Recheck the named creation/lifecycle sites against the baseline assumptions before editing. Follow the repository change workflow when this saved plan is claimed.
2. Add the one feature initializer member and matching diagnostic-table entry without modifying feature-chain structure.
3. Replace the initialization sequence with the core host call. Remove the obsolete `OneShotCommandBuffer` dependency from the `Create` precondition comment, shortening that comment to the actual manager-order precondition. Replace the reset comment with the local invariant: the fresh pool is reset before command-buffer recording. Do not narrate the old implementation.
4. Run the triggered affected-code, correctness, comment, style, and documentation checks over the changed ranges. No other production edits are expected.
5. Complete the acceptance evidence below and report any unavailable runtime configuration explicitly.

## Acceptance

| Criterion | Decisive evidence |
|---|---|
| Logical device enables the feature and reports its absence consistently | Diff shows the single enabled member and matching existing-table diagnostic. |
| Each actual fresh-pool creation resets the entire pool on the host | Diff and lifecycle inspection show the exact core call after creation, with unchanged guards and count. |
| Initialization loses one submit and fence wait | Compare the removed `Create` call path with `OneShotCommandBuffer::Execute`: one fewer allocation/free pair, queue submit, and fence wait per fresh pool. No benchmark threshold is required. |
| Recurring profiling behavior is unchanged | `GpuReset` and `GpuRead` remain unchanged; client validation run produces GPU timing samples before and after swapchain recreation. |
| Inactive paths remain inactive | Source guard inspection establishes no pool/reset on server, profiling-disabled, or zero timestamp-valid-bits paths; build/runtime evidence below covers configurations available on the test machine. |
| Build and startup remain valid | Client and server builds succeed; supported-device startup and recreation produce no related Vulkan validation errors. |

## Verification

- Use `/compile` for client and server builds; use the existing profiling-enabled configuration for client runtime checks. Also build the existing profiling-disabled configuration if available without adding a new build mode. Inspect the compile-time guards regardless.
- Use `/agent-harness` for startup, GPU-profile queries, logs, and a supported window-size change that causes swapchain recreation. Confirm successful rendering and populated GPU timing samples both before and after recreation, and inspect validation output for feature/reset/lifetime errors. Use validation-enabled execution without a RenderDoc launch for this check.
- Inspect the zero `timestampValidBits` early return and the already-created-pool return. If hardware exposing zero timestamp valid bits is available, confirm its existing warning and lack of GPU samples; do not add a runtime toggle, feature emulation, or production instrumentation solely to force this branch. Record source-only evidence when such hardware is unavailable.
- Establish the removed submission/wait count by source call-path comparison for each fresh-pool creation, rather than counting all unrelated startup submissions. Leave the shared one-shot implementation intact.
- Review the final diff against the exact scope; verify no recurring reset conversion, feature-chain restructuring, new synchronization, or unrelated edits.

## Documentation and style implications

The two local comments in `Create` need the scoped corrections described above under `Documents/C++StyleGuide.txt` rule 64. Preserve surrounding C++ formatting and designated-initializer order. `/update-claude-docs` should check the GPU Timing and manager contracts; their current wording remains accurate because recurring resets and ownership/order do not change, so no AGENTS.md edit is planned. No architecture diagram, project/filter, format version, or new documentation file is needed for the implementation.
