<!-- broken-engine-plan/v1 {"createdUtc":"2026-10-02T18:51:22.296Z","dependsOn":[]} -->
# Simplify Vulkan instance-version startup checks

## Context

`Engine/Source/Graphics/Graphics.cpp:82-119`, `engine::CheckVulkan12Support`, currently combines a failed `vkEnumerateInstanceVersion` result with a successfully queried version below 1.2. It reports both as an unsupported graphics driver, potentially displaying the initial zero version after a failed query. The absent-entry-point message also asserts a physical-device driver version that this check does not establish. These conditions exist at baseline `270007c8dde36b36a7b46583d5c2670c89bb5138`.

The accepted improvement is clearer failure classification with less custom startup code. The concrete consumer is `Graphics::Graphics`, which calls the preflight immediately after `CHECK_VK(volkInitialize())` and before `Create`. This is a one-time startup simplification; no frame-time or measured performance improvement is claimed.

## Design

The recommendation is to reuse the existing `CHECK_VK` policy because `GraphicsUtils.h:53` and `CheckVkFailed` in `GraphicsUtils.cpp:33-64` already preserve the failing expression and result and throw for this query's documented failure results. The query returns instance-level support, not a selected physical device's version; see the [Khronos vkEnumerateInstanceVersion reference](https://docs.vulkan.org/refpages/latest/refpages/source/vkEnumerateInstanceVersion.html).

1. Retain the null-entry-point guard. Recommend the log text `Vulkan 1.2 required; vkEnumerateInstanceVersion is unavailable` and dialog text `Vulkan 1.2 or higher is required.\n\nThe Vulkan instance-version query is unavailable.` These state the observed limitation without attributing a device version.
2. Replace the local `VkResult` assignment with `CHECK_VK(vkEnumerateInstanceVersion(&uiApiVersion));`. Keep initialization of `uiApiVersion` and make the following condition solely `uiApiVersion < VK_API_VERSION_1_2`.
3. Recommend the insufficient-version log `Vulkan 1.2 required; supported Vulkan instance version is {}.{}` and dialog prefix `Vulkan 1.2 or higher is required.\n\nThe supported Vulkan instance version is `. Keep the existing major/minor formatting and fatal exception in this branch.
4. Preserve both `!(gLaunchOptions.iAgentPort != 0)` dialog guards and the fatal null-entry-point branch. Remove the two obvious narration comment blocks immediately before the guard and query; the code states these operations directly.

The deliberate behavior change is that a failed query takes the established result-bearing fatal path, including its debug break, instead of the bespoke insufficient-version modal. No change to global error handling is recommended.

## Critical files

| File | Role |
|---|---|
| `Engine/Source/Graphics/Graphics.cpp` | Only production edit: `CheckVulkan12Support`; constructor ordering is read-only evidence. |
| `Engine/Source/Graphics/GraphicsUtils.h` and `GraphicsUtils.cpp` | Read-only `CHECK_VK` and `CheckVkFailed` policy evidence. |
| `Engine/Source/Graphics/Managers/InstanceManager.cpp` | Read-only application API request and physical-device version gate. |
| `Engine/Source/Graphics/AGENTS.md` | Existing Frame and Resource Lifecycle error-handling policy. |

## In scope

Only `CheckVulkan12Support`: query-result handling, the two log/dialog messages, and adjacent preflight narration comments.

## Out of scope

Instance/device creation, selected-device validation, extension and feature negotiation, Vulkan minimum version, supported-device policy, fallback paths, helpers, success logging, identity queries, global `CHECK_VK` policy, rendering, shaders, project membership, benchmarks, instrumentation, and unit tests.

## Acceptance criteria

| Criterion | Evidence |
|---|---|
| Absent entry point remains fatal before invocation | Static control-flow trace of null guard, preserved throw, and dialog suppression. |
| Failed query is not interpreted as an insufficient version | Query passes through `CHECK_VK`; inspect `CheckVkFailed` for each documented failure result and verify it throws before version inspection. |
| Only successful versions below 1.2 reach the version diagnostic | Diff shows version-only condition; messages identify instance support and retain major/minor formatting. |
| Supported startup policy is preserved | Successful 1.2+ proceeds to unchanged `Create`; `InstanceManager.cpp:287` still requests 1.2 and its physical-device gate at lines 450-458 remains intact. |
| Agent launches retain modal suppression | Both preflight dialogs remain inside `!(gLaunchOptions.iAgentPort != 0)` guards; query failure uses existing fatal policy. |
| Simplification stays local | One query invocation remains; local result variable and combined predicate disappear; no frame-path edits or new helpers. |
| C++ integration remains valid | Client build succeeds through `/compile`. |

## Verification

Trace the four cases statically: absent pointer, documented query failure, successful version below 1.2, and successful version at least 1.2. Inspect the existing macro and failure handler rather than assuming every `CHECK_VK` failure throws: its returning surface/swapchain cases are not documented results of this query. Build the client using `/compile`. No new fault-injection framework or unit tests are needed, and no runtime or performance experiment is required to establish this local classification change. Run the normal scoped affected-code, correctness, comment, style, and documentation checks for the eventual C++ implementation.

## Notes

- Future implementation is Change Workflow Tier 2: scoped Graphics startup error behavior changes, with no determinism/CRC, wire, serialization, `.pack`, save/replay, threading, or trust-boundary exposure. Preserve client-only affinity and allocation/lifecycle ordering.
- Documentation decision: no AGENTS.md or architecture addition is warranted; the Graphics error policy already directs Vulkan results through `CHECK_VK`. `/update-claude-docs` should confirm that existing policy remains accurate. Style work is limited to the changed ranges, retaining existing naming and Allman formatting and removing redundant comments under style-guide rule 64.
- Duplicate search found no owner for this function or failure classification. `VulkanHostQueryResetInitialization.md` owns profiler initialization and `GraphicsFullTeardownNoRecreate.md` owns resource teardown; neither shares this implementation boundary. No dependencies or mandatory Coordination constraints are needed.
