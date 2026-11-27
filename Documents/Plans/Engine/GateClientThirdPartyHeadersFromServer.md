<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T18:56:08.653Z","dependsOn":[]} -->
# Gate Client-Only Third-Party Headers and Include Paths Out of Server

## Context

`Common/ExternalHeaders.h` currently exposes client-only UI and graphics
third-party headers to `BT_SERVER` builds, and
`Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
adds client-only third-party include directories:

- `Common/ExternalHeaders.h:211-249` gates Dear ImGui (`imgui.h`,
  `backends/imgui_impl_win32.h`, `backends/imgui_impl_vulkan.h`), ImPlot
  (`implot.h`), and the `ImGui_ImplWin32_WndProcHandler` declaration under
  `#if defined(BT_ENGINE)` rather than `#if defined(BT_CLIENT)`, with an inner
  `#if defined(BT_CLIENT)` around `imgui_internal.h` at lines 239-242. Because
  `BrokenEngineSandboxServer.vcxproj` defines `BT_ENGINE;BT_SERVER`, the server
  PCH parses ImGui, ImPlot, and Volk on every build.
- `Common/ExternalHeaders.h:284-290` unguardedly includes `<Volk/volk.h>`,
  `<vma/vk_mem_alloc.h>`, and `<vulkan/vk_enum_string_helper.h>` for every
  target (`BT_CLIENT`, `BT_SERVER`, and `BT_DATA_PACKER`), and
  `Common/Log/LogFormatters.h:132-160` defines unguarded `std::formatter`
  specializations for `VkResult`, `VkFilter`, and `VkSamplerAddressMode` that
  call `string_VkResult`, `string_VkFilter`, and `string_VkSamplerAddressMode`
  from `<vulkan/vk_enum_string_helper.h>` (with zero `BT_SERVER` or
  `BT_DATA_PACKER` callers).
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj:44`
  includes `$(ProjectDir)\..\..\..\..\ThirdParty\imgui`,
  `$(ProjectDir)\..\..\..\..\ThirdParty\implot`, and
  `$(ProjectDir)\..\..\..\..\ThirdParty\DirectXTK\Inc` (plus
  `$(VK_SDK_PATH)\Include`) in `<AdditionalIncludeDirectories>`.

Note that `Common/DataFile.h:442,450,460` (`VkDescriptorSetLayoutBinding`,
`VkVertexInputAttributeDescription`, `VkFormat`, `VK_FORMAT_UNDEFINED`) and
`Common/TextureFormat.h:9,13` / `Common/TextureFormat.cpp:6,9,46` (`VkFormat`)
are compiled by `BrokenEngineSandboxServer.vcxproj` (lines 69, 98, 242) and
`DataPacker.vcxproj`. Those shared Common files need Vulkan core type
definitions (`<vulkan/vulkan_core.h>`) unless decoupled, but they do not need
Dear ImGui, ImPlot, DirectXTK, Volk, VMA, or `vk_enum_string_helper.h`.

This Plan enforces compile-time header and include-path isolation for
`BT_SERVER`; `Engine/Platforms/VisualStudio2026/Engine.props:69` continues to
pass the shared `ThirdParty.$(Configuration).lib` archive to the linker.

## Design

The recommended approach enforces compile-time header and include-path isolation
for `BT_SERVER` (leaving `ThirdParty.$(Configuration).lib` link inputs
unchanged):

1. In `Common/ExternalHeaders.h:211-249`, change the Dear ImGui / ImPlot guard
   from `#if defined(BT_ENGINE)` to `#if defined(BT_CLIENT)` and collapse the
   now-redundant inner `#if defined(BT_CLIENT)` at lines 239-242 (keeping
   `IMGUI_ENABLE_TEST_ENGINE` and `#include "imgui_internal.h"` inside the
   client-only block).
2. In `Common/ExternalHeaders.h:284-290`, gate `<Volk/volk.h>`,
   `<vma/vk_mem_alloc.h>`, and `<vulkan/vk_enum_string_helper.h>` (along with
   `VK_USE_PLATFORM_WIN32_KHR`) under `#if defined(BT_CLIENT)`. For non-client
   builds (`BT_SERVER` and `BT_DATA_PACKER`), include only the minimal
   `<vulkan/vulkan_core.h>` (with `VK_NO_PROTOTYPES` and
   `VK_USE_64_BIT_PTR_DEFINES 1`) required by `Common/DataFile.h` and
   `Common/TextureFormat.h/.cpp` — verifying during implementation whether any
   other `BT_SERVER` or `BT_DATA_PACKER` translation unit references a Vulkan
   helper outside `vulkan_core.h`.
3. In `Common/Log/LogFormatters.h:132-160`, wrap `std::formatter<VkResult>`,
   `std::formatter<VkFilter>`, and `std::formatter<VkSamplerAddressMode>` under
   `#if defined(BT_CLIENT)` so `BT_SERVER` and `BT_DATA_PACKER` do not call the
   `string_Vk*` helpers from `<vulkan/vk_enum_string_helper.h>`.
4. In
   `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj:44`,
   remove `$(ProjectDir)\..\..\..\..\ThirdParty\imgui`,
   `$(ProjectDir)\..\..\..\..\ThirdParty\implot`, and
   `$(ProjectDir)\..\..\..\..\ThirdParty\DirectXTK\Inc` from
   `<AdditionalIncludeDirectories>`. Keep `$(VK_SDK_PATH)\Include` while
   `Common/DataFile.h` and `Common/TextureFormat.h/.cpp` still include
   `<vulkan/vulkan_core.h>` on `BT_SERVER`.

## Critical files

- `Common/ExternalHeaders.h`
- `Common/Log/LogFormatters.h`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`

## In scope

- Changing `Common/ExternalHeaders.h:211-249` from `BT_ENGINE` to `BT_CLIENT`
  and collapsing its inner `BT_CLIENT` guard.
- Gating `<Volk/volk.h>`, `<vma/vk_mem_alloc.h>`, and
  `<vulkan/vk_enum_string_helper.h>` in `Common/ExternalHeaders.h:284-290`
  under `#if defined(BT_CLIENT)`, leaving minimal `<vulkan/vulkan_core.h>` for
  `BT_SERVER` and `BT_DATA_PACKER`.
- Wrapping `std::formatter<VkResult>`, `std::formatter<VkFilter>`, and
  `std::formatter<VkSamplerAddressMode>` in `Common/Log/LogFormatters.h:132-160`
  under `#if defined(BT_CLIENT)`.
- Removing `ThirdParty\imgui`, `ThirdParty\implot`, and
  `ThirdParty\DirectXTK\Inc` from `<AdditionalIncludeDirectories>` in
  `BrokenEngineSandboxServer.vcxproj`.

## Out of scope

- Replacing `VkFormat`, `VkDescriptorSetLayoutBinding`, or
  `VkVertexInputAttributeDescription` in `Common/DataFile.h` or
  `Common/TextureFormat.h/.cpp` (which would affect `.pack` data-format headers
  and `DataPacker`).
- Splitting `ThirdParty.$(Configuration).lib` into separate client/server link
  inputs, modifying `Engine/Platforms/VisualStudio2026/Engine.props:69`
  `<AdditionalDependencies>`, or changing `ThirdParty/Prebuilts` library
  packaging (this Plan enforces compile-time header and include-path isolation
  only).
- UI/wrapper or Engine affinity fixes already owned by the prerequisite Plans.

## Acceptance criteria

- Compile-time isolation: `BT_SERVER` PCH compilation does not include
  `imgui.h`, `implot.h`, `Volk/volk.h`, `vma/vk_mem_alloc.h`, or
  `vulkan/vk_enum_string_helper.h`, and `Common/Log/LogFormatters.h` does not
  expose `string_Vk*` formatters to `BT_SERVER` or `BT_DATA_PACKER`.
- `BrokenEngineSandboxServer.vcxproj` no longer lists `ThirdParty\imgui`,
  `ThirdParty\implot`, or `ThirdParty\DirectXTK\Inc` in
  `<AdditionalIncludeDirectories>`.
- `BrokenEngineSandbox` (client), `BrokenEngineSandboxServer`, and
  `ModelViewer` build cleanly (Debug and Release, x64), and `DataPacker` builds
  cleanly (`Release|x64`).

## Notes

- Change Workflow tier: Tier 2 (scoped build/header configuration in
  `Common/ExternalHeaders.h`, `Common/Log/LogFormatters.h`, and
  `BrokenEngineSandboxServer.vcxproj`; no runtime behavior, determinism/CRC,
  wire, serialization, save/replay, threading, or trust-boundary change).
