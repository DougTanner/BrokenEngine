<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:31:47.866Z","dependsOn":[]} -->
# Express Vulkan sample-count selection with std::bit_floor

## Context

`Engine/Source/Graphics/Managers/InstanceManager.cpp:93`, static `SelectSampleCount(VkSampleCountFlags)`, contains six descending bit tests for sample counts 64 through 2 followed by a return of 1. The repeated branches obscure a single operation: choose the highest available sample-count bit, retaining 1 for a zero mask. This is an authorized clarity refactor; it claims no speedup.

The two helper calls are `InstanceManager::SelectSupportedSampleCount` at line 133 and maximum multisample selection during device initialization at line 561. The former intersects the supported color/depth mask with the requested count and lower bits, then ORs in the existing 4 fallback. The latter passes the color/depth intersection directly. Neither producer introduces bits outside Vulkan's sample-count set.

## Design

The recommendation is to replace only the helper body with:

```cpp
return static_cast<VkSampleCountFlagBits>(std::bit_floor(eVkSampleCountFlags | VK_SAMPLE_COUNT_1_BIT));
```

Retain the helper signature and both callers unchanged. OR-ing in 1 is necessary: `std::bit_floor(0)` alone would change the fallback from 1 to 0. Preserve the supported-setting early return in `SelectSupportedSampleCount` and its existing 4 fallback, including an unsupported request of 2.

[Khronos defines the sample-count bits](https://docs.vulkan.org/refpages/latest/refpages/source/VkSampleCountFlagBits.html) as 1, 2, 4, 8, 16, 32, and 64. The [C++ bit_floor contract](https://eel.is/c++draft/bit.pow.two) returns the largest power of two no greater than a nonzero unsigned input. Thus, for every mask composed of those bits, OR-ing in 1 followed by `bit_floor` reproduces the descending tests. Installed Vulkan 1.4.341.1 `vulkan_core.h:97,2764` defines `VkFlags` as `uint32_t` and `VkSampleCountFlags` as `VkFlags`, satisfying the unsigned template constraint. `Common/ExternalHeaders.h:62` already includes `<bit>`.

Invalid masks with bits above 64 would differ from the old helper, which ignores those bits. Such masks are outside the reachable input domain established by the two producers; the recommendation deliberately adds no mask, validation, or assertion for speculative inputs.

## Critical files

- `Engine/Source/Graphics/Managers/InstanceManager.cpp`: change only static `SelectSampleCount`; inspect `SelectSupportedSampleCount` and the device-initialization assignment to `meMaxMultisampleCount` as unchanged producers.
- `Engine/Source/Graphics/Graphics.cpp:520`, `Graphics::Refresh`: inspect the unchanged supported-setting fast path and correction writeback as performance evidence.
- `Common/ExternalHeaders.h:62`: confirm the existing centralized `<bit>` include; no edit.

## In scope

- Replace the body of static `SelectSampleCount` with the single standard-library expression above.
- Verify equivalence over the valid input domain and preserve both callers verbatim.
- Perform the required C++ review, style/comment review, affected-code check, documentation synchronization assessment, and client compilation for that localized change.

## Out of scope

- Changes to sample-count settings, unsupported-request policy, the 4 fallback, Vulkan capability validation, helper signatures, or callers.
- Additional `bit_ceil`, `bit_width`, `has_single_bit`, or unrelated C++20 adoption.
- New headers, utility abstractions, benchmarks, unit tests, runtime validation layers, and invalid-mask handling.
- Shader, GPU-resource, simulation, serialization, replay, wire, data layout, allocation, or threading changes.

## Risk and invariants

Change Workflow Tier 1: local behavior-preserving work with no public signature or invariant exposure. Valid masks, including zero, retain identical outputs. This code is client-only and has no deterministic simulation or CRC exposure; no format/version or project-membership change is needed.

The helper runs during initialization and unsupported-setting correction. `Graphics::Refresh` invokes `SelectSupportedSampleCount`, but supported settings return before reaching this helper, and correction writes the supported result back. The normal frame path therefore remains unchanged. Installed MSVC 14.51.36231 `include/bit:127-132` implements `bit_floor` through a zero check and a shift based on `countl_zero`; `include/__msvc_bit_utils.hpp:347` dispatches runtime x64 counting to its intrinsic implementation. The replacement is bounded integer work with no allocation or additional GPU work. This supports non-regression, without asserting a measured performance gain.

## Acceptance criteria

| Criterion | Verification |
|---|---|
| Every mask from 0 through 127 produces the same result as before. | Review the algebra: zero/one become 1; for each highest set bit 2 through 64, OR-ing in 1 leaves that bit highest and `bit_floor` returns it. This covers every subset of the seven valid flags without adding tests. |
| Both producers, the supported-setting early return, and the 4 fallback remain unchanged. | Inspect the focused diff and both call sites; confirm no new input bits or changed settings behavior. |
| The implementation resolves with the repository's Vulkan types and existing `<bit>` include. | Build the BrokenEngineSandbox client through `/compile`; report the actual build result during implementation. |
| No steady-state frame work, allocation, or GPU operation is added. | Inspect the unchanged `Graphics::Refresh` correction path and the replacement helper. |
| Scope is limited to the helper body. | Review the final diff and required C++ review results. |

## Documentation and style

No style-guide or AGENTS.md edit is proposed: the change preserves the existing interfaces, contracts, policy, and centralized include convention. `Documents/C++StyleGuide.txt` rule 11 already covers the explicit `static_cast`; rule 13 already requires unsigned bitwise operations and API-appropriate types. No new comment is needed under rule 64 because the standard operation expresses the local intent. Run `/update-claude-docs` to assess synchronization and `/code-style-review` and `/comment-review` during implementation; avoid unrelated documentation or style changes.

## Coordination

No dependencies or mandatory coordination constraints. Searches of existing Plans by `SelectSampleCount`, `SelectSupportedSampleCount`, `InstanceManager`, sample-count/MSAA terminology, highest-set-bit/power-of-two terminology, `bit_floor`, and Coordination sections found no Plan owning this helper refactor.

## Notes

This Plan authorizes no implementation during its creation. No build or runtime verification has been performed. A client compile and source equivalence review are sufficient for this bounded replacement; no live harness run or benchmark is proposed. Follow the normal implementation and finalization workflow when executing the Plan.
