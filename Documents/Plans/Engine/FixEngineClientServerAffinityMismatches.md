<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T18:55:57.909Z","dependsOn":[]} -->
# Fix Engine Client/Server Affinity Mismatches

## Context

An audit of `Engine/Source/` client/server build affinity found four remaining
mismatches where client-only declarations, headers, or preprocessor guards leak
into `BT_SERVER` or confuse affinity classification:

1. `Engine/Source/Input/RawInputManager.h:1-82` is still listed in
   `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
   and `.vcxproj.filters` with only a partial `#if defined(BT_CLIENT)` guard at
   lines 45-80. `Engine/Source/Engine.h:59` already gates its include under
   `BT_CLIENT`, and zero server translation units include it.
2. `Engine/Source/Audio/AudioManager.cpp:1-6` places `#if defined(BT_DEBUG)` /
   `#include "Agent/Commands/AudioStreamingHarnessRig.h"` (lines 2-4) before
   `#if defined(BT_CLIENT)` (line 6). Because that directive precedes the
   `BT_CLIENT` guard, `Resolve-FileAffinity` in
   `.agents/scripts/SourceAffinity.psm1` cannot classify the file as `client`.
3. `Engine/Source/LaunchOptions.h:9,22,35-38` and
   `Engine/Source/LaunchOptions.cpp:61-64,139-170` expose client-only
   `LaunchOptionFlags::kRenderDoc`, `LaunchOptions::vkWindowedExtent` (a Vulkan
   `VkExtent2D` member), `PhysicalInputSuppressed()`, and `--renderdoc` /
   `--windowed` parsing in `BT_SERVER`. All readers of those symbols are
   client-only (`Main.cpp`, `RawInputManager.cpp`, `ImGuiManager.cpp`,
   `InstanceManager.cpp`, and `Projects/ModelViewer/Source/Main.cpp`).
4. `Engine/Source/Profile/ProfileManager.h:10-17,290-338,482` and
   `Engine/Source/Profile/ProfileManager.cpp:148-164` expose client-only
   `ProfileScreen` (including `kGpu`), `meProfileScreen`,
   `ToggleProfileText()`, and the Vulkan/renderer `BootTimers` /
   `kpBootTimerNames` entries in `BT_SERVER`. In addition,
   `Engine/Source/Profile/ProfileScreens.cpp:4` includes
   `"Graphics/CameraBase.h"` unguarded while every `gpCamera` use in that file
   (`:161,:326`) is inside `#if defined(BT_CLIENT)`. Note that
   `kBootTimerIslands` is used on `BT_SERVER` in
   `Engine/Source/Frame/IslandTerrain.cpp:229`, so it must remain visible to
   `BT_SERVER`.

## Design

The recommended approach is to tighten each of the four engine sites so
`BT_SERVER` no longer sees client-only input, RenderDoc/Vulkan launch options,
or GPU/Vulkan profile screen declarations:

1. `Engine/Source/Input/RawInputManager.h`: wrap the body after `#pragma once`
   in a single whole-file `#if defined(BT_CLIENT)` guard, collapsing the
   partial guard at lines 45-80, and remove `RawInputManager.h` from
   `BrokenEngineSandboxServer.vcxproj` and
   `BrokenEngineSandboxServer.vcxproj.filters` via `/update-vcxproj` rules.
2. `Engine/Source/Audio/AudioManager.cpp`: move the `#if defined(BT_DEBUG)`
   include of `Agent/Commands/AudioStreamingHarnessRig.h` inside
   `#if defined(BT_CLIENT)` (after line 6) so the whole file after
   `#include "AudioManager.h"` is guarded by `BT_CLIENT` and
   `Resolve-FileAffinity` classifies it as `client`.
3. `Engine/Source/LaunchOptions.h` and `Engine/Source/LaunchOptions.cpp`:
   guard `LaunchOptionFlags::kRenderDoc`, `LaunchOptions::vkWindowedExtent`,
   `PhysicalInputSuppressed()`, and the `--renderdoc` and `--windowed` branches
   in `ParseLaunchOptions()` under `#if defined(BT_CLIENT)`. On `BT_SERVER`,
   passing `--renderdoc` or `--windowed` falls through to the existing unknown
   launch-option warning path.
4. `Engine/Source/Profile/ProfileManager.h`,
   `Engine/Source/Profile/ProfileManager.cpp`, and
   `Engine/Source/Profile/ProfileScreens.cpp`:
   - Guard `enum class ProfileScreen`, `meProfileScreen`, and
     `ToggleProfileText()` (declaration and definition) under
     `#if defined(BT_CLIENT)`.
   - Guard the client-only `BootTimers` entries and their matching rows in
     `kpBootTimerNames` under `#if defined(BT_CLIENT)`, leaving exactly three
     shared entries in `BT_SERVER`: `kBootTimerTotal` (`"Total"`),
     `kBootTimerWaitForDataFile` (`"    Wait for data file"`), and
     `kBootTimerIslands` (`"      Islands"`). Guard two contiguous spans in both
     `BootTimers` and `kpBootTimerNames`:
     1. `kBootTimerWaitForTexturesFile` through `kBootTimerBufferManager`
        (`kBootTimerWaitForTexturesFile`, `kBootTimerWaitForPriorityTextures`,
        `kBootTimerWaitForIslands`, `kBootTimerVulkan`,
        `kBootTimerInstanceManager`, `kBootTimerDeviceManager`,
        `kBootTimerSwapchainManager`, `kBootTimerParticleManager`,
        `kBootTimerPipelineManager`, `kBootTimerCommandBufferManager`, and
        `kBootTimerBufferManager` — `kBootTimerWaitForTexturesFile` has zero
        callers, and the rest are called only under `BT_CLIENT`).
     2. `kBootTimerTextureManager` through `kBootTimerRenderPresent`
        (`kBootTimerTextureManager`, `kBootTimerTextureUpload`,
        `kModelTexturesGeneration`, `kBootTimerRecordCommandBuffers`, and
        `kBootTimerRenderPresent`).
     Keep
     `static_assert(std::size(kpBootTimerNames) == static_cast<size_t>(kBootTimerCount))`
     passing in both `BT_CLIENT` and `BT_SERVER`.
   - Guard `#include "Graphics/CameraBase.h"` at
     `Engine/Source/Profile/ProfileScreens.cpp:4` under
     `#if defined(BT_CLIENT)`.

## Critical files

- `Engine/Source/Input/RawInputManager.h`
- `Engine/Source/Audio/AudioManager.cpp`
- `Engine/Source/LaunchOptions.h`
- `Engine/Source/LaunchOptions.cpp`
- `Engine/Source/Profile/ProfileManager.h`
- `Engine/Source/Profile/ProfileManager.cpp`
- `Engine/Source/Profile/ProfileScreens.cpp`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj`
- `Projects/BrokenEngineSandbox/Platforms/VisualStudio2026/BrokenEngineSandboxServer.vcxproj.filters`

## In scope

- Whole-file `BT_CLIENT` wrapping of `Engine/Source/Input/RawInputManager.h`
  and its removal from `BrokenEngineSandboxServer.vcxproj` and
  `BrokenEngineSandboxServer.vcxproj.filters`.
- Moving the `BT_DEBUG` include inside `#if defined(BT_CLIENT)` in
  `Engine/Source/Audio/AudioManager.cpp`.
- Guarding `kRenderDoc`, `vkWindowedExtent`, `PhysicalInputSuppressed()`, and
  their `ParseLaunchOptions()` branches under `#if defined(BT_CLIENT)` in
  `Engine/Source/LaunchOptions.h` and `Engine/Source/LaunchOptions.cpp`.
- Guarding `ProfileScreen`, `meProfileScreen`, `ToggleProfileText()`, the
  client-only Vulkan `BootTimers` / `kpBootTimerNames` entries, and
  `ProfileScreens.cpp:4` `#include "Graphics/CameraBase.h"` under
  `#if defined(BT_CLIENT)`.

## Out of scope

- UI/Wrapper layer cleanup (the UI/wrapper layer is already whole-file
  `BT_CLIENT`-guarded and absent from `BrokenEngineSandboxServer.vcxproj`).
- `LaunchOptions::scenePath` (`ModelViewer` and `LaunchOptions.cpp` shared CLI
  surface) unless verified unused on `BT_SERVER` and approved separately.
- `Common/ExternalHeaders.h` third-party header gating and server include-path
  removal (separate Plan).
- Static-check automation for affinity and reachability (separate Plan).

## Acceptance criteria

- `Engine/Source/Input/RawInputManager.h` is absent from
  `BrokenEngineSandboxServer.vcxproj` and
  `BrokenEngineSandboxServer.vcxproj.filters`, and `/update-vcxproj` pair check
  passes.
- Both `Engine/Source/Input/RawInputManager.h` and
  `Engine/Source/Audio/AudioManager.cpp` classify as `client` under
  `.agents/scripts/SourceAffinity.psm1`.
- `Engine/Source/LaunchOptions.h` and `Engine/Source/LaunchOptions.cpp`
  reference no `VkExtent2D`, `kRenderDoc`, or `PhysicalInputSuppressed()` under
  `BT_SERVER`.
- `Engine/Source/Profile/ProfileManager.h` compiles on both `BT_CLIENT` and
  `BT_SERVER` with its `kpBootTimerNames` `static_assert` passing and only
  `kBootTimerTotal`, `kBootTimerWaitForDataFile`, and `kBootTimerIslands`
  exposed to `BT_SERVER`.
- `BrokenEngineSandbox` (client), `BrokenEngineSandboxServer`, and
  `ModelViewer` build cleanly (Debug and Release, x64).

## Notes

- Change Workflow tier: Tier 2 (scoped behavior — `ParseLaunchOptions()` on
  `BT_SERVER` stops parsing `--renderdoc` and `--windowed` and treats them as
  unknown options; no determinism/CRC, wire, serialization, save/replay,
  threading, or trust-boundary change).
