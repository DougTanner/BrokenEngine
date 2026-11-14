<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:20:50.942Z","dependsOn":[]} -->
# Guard the null message id name in the Vulkan debug callback log

## Context

Paths are in `Engine/Source/Graphics/Managers/InstanceManager.cpp`, at commit
`401e4b75`. All three items are in code that runs only with the debug
layers or RenderDoc switches on.

- `DebugUtilsCallback` tests `pVkCallbackData->pMessageIdName != nullptr`
  before each of six `std::strstr` uses (`:34`, `:43`, `:48`, `:58`, `:64`,
  `:69`), and then passes the same pointer to `LOG` as a `{}` argument with
  no null test (`:85`). `pMessageIdName` in
  `VkDebugUtilsMessengerCallbackDataEXT` may be NULL (verified against the
  Vulkan specification), so the six guards are right and `:85` is the
  defect: every warning or error message that carries no id name formats a
  null `const char*`. `LOG` gives no protection: `common::Log`
  (`Common/Log/Log.h:139-161`) hands its arguments straight to
  `std::format_to_n` (`:153`) and nothing in `Common/Log` substitutes text
  for a null pointer. This is the path that reports validation errors.
- `BuildValidationLayerSettings` (`:117-187`) writes eight `VkLayerSettingEXT`
  initializers that differ only in `pSettingName` and in which of two
  file-scope constants (`:111-112`) `pValues` names.
- `TryLoadRenderDocDll` has an early-return guard at `:199-202` and then
  wraps `:205-242` in a second guard (`:204`) instead of returning;
  `Documents/C++StyleGuide.txt` rule 62 asks for early returns in guard
  sequences.

Originating gap: the in-function phase of an analysis of
`Engine/Source/Graphics/Managers` at that commit. Unmet criteria:
`DebugUtilsCallback` never formats a null id name; each layer setting is
stated once as a name and a value.

## Design

Recommended:

- At `:85`, pass an empty string when `pMessageIdName` is null. The six
  guards stay as they are: they validate a third-party result, which
  `.agents/references/cpp-conventions.md` tells code to do.
- In `BuildValidationLayerSettings`, one file-local helper taking the
  setting name and the value pointer builds each `VkLayerSettingEXT`. The
  two file-scope constants stay, because the pointers must outlive the call
  (`:110`). The ASSERT at `:190` stays: it is the only statement that ties
  the setting count to the `[7]` array bound (`:115`).
- Invert the guard at `:204` into an early return. This edit is cosmetic
  alone and is included only because this Plan already edits the file.

## Critical files

- `Engine/Source/Graphics/Managers/InstanceManager.cpp`

## In scope

- The `LOG` argument at `:85`.
- The eight initializers in `BuildValidationLayerSettings` and one new
  file-local helper.
- The second guard of `TryLoadRenderDocDll`.

## Out of scope

- The six `pMessageIdName != nullptr` guards and the suppression list.
- The ASSERT at `:190` and the setting values.
- `SelectQueueFamilies` and its three surface-support loops.
- `ValidatePhysicalDeviceCapabilities`.

## Risk tier

Tier 2 — scoped behavior. Trigger: runtime behavior of one subsystem's
debug-layer path changes (a message with no id name is logged instead of
formatting a null pointer). No determinism, layout, wire, or threading
exposure; release builds without the debug layers compile the callback body
out.

## Acceptance criteria

1. No statement in `DebugUtilsCallback` formats or searches
   `pMessageIdName` without a null test.
2. Each layer setting is written once as a name and a value, and the ASSERT
   at the end of `BuildValidationLayerSettings` remains.
3. The BrokenEngineSandbox client and ModelViewer build through `/compile`
   in a configuration with the Vulkan debug layers on.
4. Through `/agent-harness`, a client run with validation layers on boots
   with the same layer settings in effect (no new validation message at
   start-up). The null-name path itself is verified by code reading plus
   the build: no harness command produces a message without an id name. The
   supplied evidence records no harness setup.

## Coordination

No directional dependencies or mandatory nondirectional coordination.
`Documents/Plans/Engine/ValidateEnabledOptionalDeviceFeatures.md` and
`Documents/Plans/Engine/BufferManagerDeadStateAndDuplicates.md` edit
`ValidatePhysicalDeviceCapabilities`, a different function of this file.

## Notes

- What the C++ standard library does when `std::format_to_n` is given a
  null `const char*` was not verified against an authoritative source; the
  author reads the formatter as requiring a null-terminated string. The fix
  does not depend on the answer.
