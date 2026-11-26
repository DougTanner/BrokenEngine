<!-- broken-engine-plan/v2 {"createdUtc":"2026-11-25T00:01:58.475Z","dependsOn":[]} -->
# Enforce Exhaustive Enum Switches with MSVC C4062

## Context

`Engine/Platforms/VisualStudio2026/Engine.props` and the standalone tool and
DataPacker `.vcxproj` files compile with `/W4`, where MSVC warning `C4062`
(`enumerator 'identifier' in switch of enum 'enumeration' is not handled`) is
off by default. Unlike `C4061`, `C4062` fires only on `switch` statements that
have no `default:` label, so switches that intentionally handle a subset of an
external or protocol enum (`VkFormat`, Win32 `UINT`, partial `PacketType`
dispatchers) remain unaffected.

Of the 55 `switch` statements across `Common/`, `Engine/`, `DataPacker/`,
`Projects/BrokenEngineSandbox/`, and `Tools/`, 27 already omit `default:` on
enum types (`StatusChangeType`, `ShipType`, `ShipController`, `LogLevel`,
`SubscriptionChangeReason`, `AgentScriptKind`, `MeshResidency`, and others).
Without `/we4062`, adding an enumerator to any of those enums compiles cleanly
even when a switch omits the new case. Four additional switches over repo-owned
enums (`NetworkSimulationLevel` in `NetworkSimulation.h` and `ShipType` in
`ServerClientManager.cpp`) currently use `default:` where explicit cases would
let `C4062` catch newly added enumerators.

## Design

1. Add `/we4062` to `<AdditionalOptions>` in:
   - `Engine/Platforms/VisualStudio2026/Engine.props` (covers
     `BrokenEngineSandbox`, `BrokenEngineSandboxServer`, and `ModelViewer` in
     Debug, Profile, and Release);
   - `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj` (Debug and
     Release);
   - `Tools/AgentHarness/Platforms/VisualStudio2026/AgentHarness.vcxproj` (Debug
     and Release);
   - `Tools/ServerMonitor/Platforms/VisualStudio2026/ServerMonitor.vcxproj`
     (Debug and Release);
   - `Tools/WorktreeCli/Platforms/VisualStudio2026/WorktreeCli.vcxproj` (Debug
     and Release).
   Using `/we4062` rather than `/w14062` ensures unhandled enum cases fail the
   build in Debug configurations of standalone tools and DataPacker too (where
   `TreatWarningAsError` is `false`). External headers in
   `Common/ExternalHeaders.h` are already wrapped in `#pragma warning(push, 0)`.
2. Remove the unnecessary `default:` arms on the four repo-owned exhaustive enum
   switches so `C4062` guards them:
   - `Engine/Source/Network/NetworkSimulation.h`: in
     `GetNetworkSimulationConfiguration`, `GetNetworkSimulationName`, and
     `GetNetworkSimulationBounds`, replace `default:` with
     `case NetworkSimulationLevel::kDisabled:` and keep the disabled return
     value after the `switch` (matching `ShipTypeName` and
     `StatusChangeTypeName`).
   - `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`:
     in `switch (rMember.eType)`, replace `default:` with
     `case ShipType::kCount: break;` and place `ASSERT(false); std::unreachable();`
     after the `switch`.
3. Build every affected target and resolve any existing `C4062` diagnostics in
   `switch` statements without `default:` by adding explicit `case` labels
   (never by adding a catch-all `default:` on an exhaustive enum switch).
4. Record the exhaustive-switch rule in `Documents/C++StyleGuide.txt` beside the
   existing `switch` formatting rules: `/we4062` is enabled, so switches over
   repo-owned enums that intend to handle every enumerator omit `default:` and
   list sentinel values such as `kCount` explicitly.

## Critical files

- `Engine/Platforms/VisualStudio2026/Engine.props`
- `DataPacker/Platforms/VisualStudio2026/DataPacker.vcxproj`
- `Tools/AgentHarness/Platforms/VisualStudio2026/AgentHarness.vcxproj`
- `Tools/ServerMonitor/Platforms/VisualStudio2026/ServerMonitor.vcxproj`
- `Tools/WorktreeCli/Platforms/VisualStudio2026/WorktreeCli.vcxproj`
- `Engine/Source/Network/NetworkSimulation.h`
- `Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`
- `Documents/C++StyleGuide.txt`

## In scope

- Adding `/we4062` to `<AdditionalOptions>` in `Engine.props`,
  `DataPacker.vcxproj`, `AgentHarness.vcxproj`, `ServerMonitor.vcxproj`, and
  `WorktreeCli.vcxproj`.
- Replacing `default:` with explicit enumerator cases in
  `GetNetworkSimulationConfiguration`, `GetNetworkSimulationName`, and
  `GetNetworkSimulationBounds` (`Engine/Source/Network/NetworkSimulation.h`) and
  `switch (rMember.eType)`
  (`Projects/BrokenEngineSandbox/Source/Network/Server/ServerClientManager.cpp`).
- Fixing any compile errors surfaced by `/we4062` in existing `switch`
  statements across those targets by adding explicit `case` labels.
- Adding one concise rule in `Documents/C++StyleGuide.txt` documenting `/we4062`
  and omitting `default:` on exhaustive enum switches.

## Out of scope

- Enabling `C4061` (which warns even when `default:` is intentionally present).
- Removing `default:` from switches that intentionally handle a subset of an
  enum (such as `PacketType` / `GamePacketType` direction-specific dispatchers,
  `VkFormat`, or Win32 `UINT` message loops).
- Refactoring `Frame::Crcs()` / `LogDifferences()` to walk `Values()`, adding
  X-macros to `LogCategory`, or modifying `ThirdParty/`.

## Risk triggers and invariants

- Tier 1/2 mechanical build-flag and control-flow-preserving cleanup; no runtime
  behavior, wire format, serialization, or CRC change.
- Preserve existing behavior for out-of-range enum values cast from wire or save
  bytes (e.g., returning `"Unknown"` or hitting `ASSERT(false); std::unreachable()`
  after the `switch`).
- Do not suppress `C4062` globally or add `default:` to silence a newly exposed
  unhandled case on a repo-owned enum.

## Acceptance criteria

1. `Engine.props` and all four standalone `.vcxproj` files pass `/we4062` in
   every configuration.
2. `BrokenEngineSandbox`, `BrokenEngineSandboxServer`, `ModelViewer`,
   `DataPacker`, `AgentHarness`, `ServerMonitor`, and `WorktreeCli` all compile
   cleanly with zero `C4062` errors (Debug and Release where applicable).
3. `NetworkSimulation.h` (all three switches) and `ServerClientManager.cpp`
   (`switch (rMember.eType)`) omit `default:` and handle every enumerator
   explicitly.
4. `Documents/C++StyleGuide.txt` documents `/we4062` and the exhaustive-switch
   convention.
