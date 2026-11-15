<!-- broken-engine-plan/v1 {"createdUtc":"2026-11-14T23:21:26.422Z","dependsOn":[]} -->
# Instance and validation-layer clean-ups

## Context

Line numbers are those at commit `401e4b75`; locate by symbol if they moved.
`M/` abbreviates `Engine/Source/Graphics/Managers/` and `G/` abbreviates
`Engine/Source/Graphics/`. Most of this work runs only with the Vulkan debug
layers or RenderDoc switches on.

### Optional device features enabled without validation

`DeviceManager` enables four features that `InstanceManager` never checks:
`scalarBlockLayout` (`M/DeviceManager.cpp:174`), `hostQueryReset` (`:175`),
`drawIndirectFirstInstance` (`:220`), and `fragmentStoresAndAtomics`
(`:227`). A repository search finds those names nowhere else, so neither the
startup-error table (`M/InstanceManager.cpp:475-484`) nor the ASSERTs
(`:520-528`) cover them.

Verified against the Vulkan 1.2 specification:

- `scalarBlockLayout`, `fragmentStoresAndAtomics`, and
  `drawIndirectFirstInstance` are optional on a conformant 1.2 device.
- `hostQueryReset` is mandatory in Vulkan 1.2.
- `vkCreateDevice` fails with `VK_ERROR_FEATURE_NOT_PRESENT` when it is asked
  to enable a feature the physical device does not report.

So on a device that lacks one of the three optional features,
`vkCreateDevice` (`M/DeviceManager.cpp:242`) returns that error into
`CHECK_VK` instead of the named `ShowStartupError` path the table gives
every other required feature. The engine uses all three: the smoke and wind
deposit shaders call `atomicOr` on storage buffers in the fragment stage
(`Engine/Data/Shaders/Smoke/Smoke.frag:44`,
`Engine/Data/Shaders/Wind/WindDeposit.frag:51`), the comment at
`M/DeviceManager.cpp:217-219` records the nonzero `firstInstance` use in
indirect records, and the shared layouts use `scalar` blocks.

`M/AGENTS.md` `### InstanceManager` states "`DeviceManager` must enable every
feature required here", and that direction holds. The gap is the converse:
features enabled without being validated.

### Debug callback and layer settings in `InstanceManager.cpp`

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

What the C++ standard library does when `std::format_to_n` is given a null
`const char*` was not verified against an authoritative source; the reading
here is that the formatter requires a null-terminated string. The fix does
not depend on the answer.

### Write-only graphics debug-names set

`Graphics::mDebugNames` (`G/Graphics.h:158`) is written and never read. The
only uses in the tree are the declaration and `SetVkObjectName`
(`G/GraphicsUtils.cpp:81, 84`), which emplaces each formatted name so that
`pObjectName` (`:91`) points at NUL-terminated storage that lives as long as
the renderer. The set is never read or cleared, and the insert forces the
`// Heap:` allocation-tracking suppression at `:81-82`: a heap allocation per
distinct name in debug-layer builds.

Separately, `SetVkObjectName` has one caller, `VkName`
(`G/GraphicsUtils.h:20`), already inside
`if constexpr (kbVulkanDebugLayers)` (`:18`), so the same test at
`GraphicsUtils.cpp:66` and the three `[[maybe_unused]]` parameter attributes
at `:64` repeat it.

Still open (the investigation portion): whether
`vkSetDebugUtilsObjectNameEXT` copies `pObjectName`, so the string need not
outlive the call. The verification could not settle it from an authoritative
source. If the implementation keeps the pointer, the set is the string's
owner and is not removable. Also established: `common::Workbuffer::View()` is
not NUL-terminated (`Common/Workbuffer.cpp:72-76`), so the current workbuffer
contents cannot be passed as they are.

### Originating gaps and unmet criteria

The analyses of `Engine/Source/Graphics/Managers` (architecture phase, data
shape and cohesion lenses; in-function phase) and of `Engine/Source/Graphics`
(direct files only) at that commit. Unmet criteria:

- Every optional feature `DeviceManager` enables unconditionally is rejected
  by `ValidatePhysicalDeviceCapabilities` with the startup error when the
  device lacks it.
- `DebugUtilsCallback` never formats a null id name; each layer setting is
  stated once as a name and a value.
- No renderer-lifetime container that nothing reads, and no `// Heap:`
  suppression for a temporary. Authority:
  `.agents/references/cpp-conventions.md` (workbuffer for temporaries;
  `// Heap:` suppression only when unavoidable).

## Design

1. Optional features: add three rows to `pRequiredFeatures`
   (`M/InstanceManager.cpp:475-484`), one per optional feature, each with its
   name and a reason in the style of the existing rows.
   `mVkPhysicalDeviceVulkan12Features` and
   `mVkPhysicalDeviceFeatures2.features` are already filled at `:467`, so the
   rows read existing state. `hostQueryReset` gets no row: it is mandatory.
   The enable list in `DeviceManager` is not derived from a shared table:
   one table driving both steps would be a new abstraction over two
   different Vulkan structs for no duplication beyond the list itself.
2. Debug callback: at `M/InstanceManager.cpp:85`, pass an empty string when
   `pMessageIdName` is null. The six guards stay as they are: they validate
   a third-party result, which `.agents/references/cpp-conventions.md` tells
   code to do.
3. Layer settings: in `BuildValidationLayerSettings`, one file-local helper
   taking the setting name and the value pointer builds each
   `VkLayerSettingEXT`. The two file-scope constants stay, because the
   pointers must outlive the call (`:110`). The ASSERT at `:190` stays: it is
   the only statement that ties the setting count to the `[7]` array bound
   (`:115`).
4. RenderDoc load: invert the guard at `M/InstanceManager.cpp:204` in
   `TryLoadRenderDocDll` into an early return. This edit is cosmetic alone
   and is included only because this Plan already edits the file.
5. Debug names, investigation: settle the open fact first with
   `/verify-external-claims`: does `vkSetDebugUtilsObjectNameEXT`
   (`VK_EXT_debug_utils`, `VkDebugUtilsObjectNameInfoEXT`) copy
   `pObjectName` during the call?
6. If verified, recommended: in `SetVkObjectName`, append a terminator to the
   workbuffer frame opened at `G/GraphicsUtils.cpp:76` (`Append` takes a
   `std::string_view`, `Common/Workbuffer.h:42`), pass the frame's data
   pointer as `pObjectName` while that frame is open, and delete the
   `mDebugNames` member, the emplace, and the suppression with its `// Heap:`
   comment. If refuted or still unresolved, leave `mDebugNames`, the
   emplace, and the suppression unchanged and report that result; the set
   then owns storage the driver may read.
7. In either case, remove the repeated `if constexpr (kbVulkanDebugLayers)`
   test and the three `[[maybe_unused]]` attributes in `SetVkObjectName`.

## Critical files

- `Engine/Source/Graphics/Managers/InstanceManager.cpp`
- `Engine/Source/Graphics/GraphicsUtils.cpp`
- `Engine/Source/Graphics/Graphics.h`

## In scope

`Engine/Source/Graphics/Managers/InstanceManager.cpp`:

- Three new rows in `pRequiredFeatures` in
  `ValidatePhysicalDeviceCapabilities`.
- The `LOG` argument at `:85` in `DebugUtilsCallback`.
- The eight initializers in `BuildValidationLayerSettings` and one new
  file-local helper.
- The second guard of `TryLoadRenderDocDll`.

`Engine/Source/Graphics/GraphicsUtils.cpp` and
`Engine/Source/Graphics/Graphics.h`:

- The investigation of Design step 5.
- The body and parameter attributes of `SetVkObjectName`.
- The `mDebugNames` member of `Graphics`.

Documentation:

- `Engine/Source/Graphics/Managers/AGENTS.md` `### InstanceManager` or
  `### DeviceManager`, only if the documentation sync finds a sentence the
  new rows make incomplete.

## Out of scope

- `DeviceManager.cpp`: the enabled features and their order.
- A row for `hostQueryReset`.
- A table shared by validation and enabling.
- The `maxPerStageResources` ASSERT (`M/InstanceManager.cpp:463`) and every
  format check in `ValidatePhysicalDeviceCapabilities`.
- The six `pMessageIdName != nullptr` guards and the suppression list in
  `DebugUtilsCallback`.
- The ASSERT at the end of `BuildValidationLayerSettings` and the setting
  values.
- `SelectQueueFamilies` and its three surface-support loops.
- `VkName` and its callers.
- `common::Workbuffer`.
- Any other allocation-tracking suppression.

## Risk tier

Tier 2 — scoped behavior. Triggers: it adds checks inside one unit at an
existing boundary (physical-device capability validation), with the format
and the trust unchanged, which `.agents/references/risk-tiers.md` places in
Tier 2; runtime behavior of one subsystem's debug-layer path changes (a
message with no id name is logged instead of formatting a null pointer); and
Vulkan object naming in debug-layer builds changes how the name string is
stored. A device that lacks one of the three optional features now fails at
the named startup error instead of at `vkCreateDevice`; a device that has
them is unaffected. Release builds without the debug layers compile the
callback body out. No determinism/CRC, layout, serialization, wire, replay,
or threading exposure.

## Acceptance criteria

1. `pRequiredFeatures` has a row for each of `scalarBlockLayout`,
   `fragmentStoresAndAtomics`, and `drawIndirectFirstInstance`, and none for
   `hostQueryReset`.
2. No statement in `DebugUtilsCallback` formats or searches `pMessageIdName`
   without a null test.
3. Each layer setting is written once as a name and a value, and the ASSERT
   at the end of `BuildValidationLayerSettings` remains.
4. The verdict of Design step 5 is reported with its source.
5. If verified: no `mDebugNames` identifier remains in the tracked tree,
   `SetVkObjectName` holds no allocation-tracking suppression, and the string
   passed as `pObjectName` is NUL-terminated.
6. `SetVkObjectName` no longer tests `kbVulkanDebugLayers`.
7. The BrokenEngineSandbox client and ModelViewer build through `/compile` in
   a configuration with the Vulkan debug layers on.
8. One `/agent-harness` run of the client with validation layers on, on the
   development device: the client starts, it boots with the same layer
   settings in effect (no new validation message at start-up), and a
   RenderDoc capture taken in that run shows object names intact. The
   feature-rejection path and the null-name path are verified by code
   reading plus the build: the development device reports all three
   features, no harness command produces a message without an id name, and
   no harness command or temporary edit is added to trip either. The
   supplied evidence records no harness setup.

## Coordination

No directional dependencies.
`Documents/Plans/Engine/BufferAndRenderTargetCleanup.md` turns the
multisample member written at `M/InstanceManager.cpp:529-531` into a local,
in `ValidatePhysicalDeviceCapabilities`, the same function as the
`pRequiredFeatures` table. Neither Plan depends on the other; whichever lands
later re-derives its lines by symbol.
